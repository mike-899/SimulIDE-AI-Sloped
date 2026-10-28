/***************************************************************************
 *   Copyright (C) 2026 SimulIDE contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "spicecosim.h"

#include "avrprocessor.h"
#include "baseprocessor.h"
#include "e-node.h"
#include "mcucomponent.h"
#include "mcucomponentpin.h"
#include "simulator.h"
#include "spiceengine.h"

#include <QElapsedTimer>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <thread>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#    include <intrin.h>
#endif

// Two times closer than this are the same time point
static const double kEps = 1e-12;

static thread_local bool tl_inMcu = false; // Running inside a MCU batch

bool SpiceCosim::s_active = false;

struct SpiceCosim::PinRead {
    SpiceCosim *cosim;
    int proc;
    avr_t *avr;
    int io;
    avr_io_read_t origC;
    void *origParam;
};

SpiceCosim *SpiceCosim::self()
{
    static SpiceCosim cosim;
    return &cosim;
}

static inline void cpuRelax()
{
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    _mm_pause();
#elif defined(__aarch64__)
    asm volatile("yield");
#else
    std::this_thread::yield();
#endif
}

//---------------------------------------------------------------- handoff --
// Strict alternation: the side that holds the turn runs, the other waits.
// Spin for a few microseconds (a handoff usually comes back that fast),
// then sleep on the condition variable.

void SpiceCosim::pass(int side)
{
    m_turn.store(side);
    if (m_sleepers.load()) {
        std::lock_guard<std::mutex> lock(m_mx);
        m_cv.notify_all();
    }
}

void SpiceCosim::waitTurn(int side)
{
    // The other side usually answers within a few microseconds (one
    // ngspice time point, or the MCU running up to its next event), much
    // less than a sleep/wake up of the thread costs: spin (yielding now and
    // then, other threads may need this core) for up to 200 us, then sleep.
    if (m_turn.load(std::memory_order_acquire) == side)
        return;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 1;; i++) {
        if (m_turn.load(std::memory_order_acquire) == side)
            return;
        if ((i & 63) == 0) {
            if (std::chrono::steady_clock::now() - t0 >
                std::chrono::microseconds(200))
                break;
            std::this_thread::yield();
        } else
            cpuRelax();
    }
    std::unique_lock<std::mutex> lock(m_mx);
    m_sleepers++;
    m_cv.wait(lock, [&] { return m_turn.load() == side; });
    m_sleepers--;
}

//------------------------------------------------------------------ setup --

void SpiceCosim::setup(SpiceEngine *eng, const QList<McuComponentPin *> &pins)
{
    m_eng = eng;
    clearSlots();

    for (int k = 0; k < pins.size(); k++) {
        McuComponentPin *pin = pins[k];
        Slot s;
        s.pin   = pin;
        s.guard = pin;
        s.node  = pin->getEpin()->getEnode();
        s.proc  = -1;
        if (pin->mcu())
            s.proc = m_procIndex.value(pin->mcu()->processor(), -1);
        s.t0     = 0;
        s.v1     = pin->voltOut();
        s.g1     = 1 / std::max(pin->imp(), 1e-4);
        s.v0     = s.v1;
        s.g0     = s.g1;
        s.lastIn = s.node ? s.node->getVolt() : 0;
        m_slots.push_back(s);
        pin->setCosimSlot(k);
    }
}

static uint8_t pinReadHook(avr_t *avr, avr_io_addr_t addr, void *param)
{
    SpiceCosim::PinRead *r = static_cast<SpiceCosim::PinRead *>(param);
    r->cosim->inputSyncIdx(r->proc);
    return r->origC(avr, addr, r->origParam);
}

void SpiceCosim::begin(double t0)
{
    end();
    m_procs.clear();
    m_procIndex.clear();
    for (BaseProcessor *p : Simulator::self()->mcuList()) {
        Proc pr;
        pr.p         = p;
        pr.stepS     = p->stepNS() * 1e-9;
        pr.lastCycle = p->getLoadStatus() ? p->cycle() : 0;
        pr.time      = t0;
        if (pr.stepS <= 0)
            pr.stepS = 62.5e-9;
        m_procIndex.insert(p, (int)m_procs.size());
        m_procs.push_back(pr);
    }
    // Digital reads (PINx) bring ngspice to the read time (AVR)
    for (int i = 0; i < (int)m_procs.size(); i++) {
        AvrProcessor *ap = dynamic_cast<AvrProcessor *>(m_procs[i].p);
        if (!ap || !ap->getCpu())
            continue;
        avr_t *avr = ap->getCpu();
        for (char port = 'A'; port <= 'L'; port++) {
            int addr = ap->getRegAddress(QString("PIN") + port);
            if (addr < 32 || addr >= 32 + MAX_IOs)
                continue;
            int io = AVR_DATA_TO_IO(addr);
            if (!avr->io[io].r.c)
                continue;
            PinRead *r          = new PinRead;
            r->cosim            = this;
            r->proc             = i;
            r->avr              = avr;
            r->io               = io;
            r->origC            = avr->io[io].r.c;
            r->origParam        = avr->io[io].r.param;
            avr->io[io].r.c     = pinReadHook;
            avr->io[io].r.param = r;
            m_pinReads.push_back(r);
        }
    }
    m_ts     = t0;
    m_target = t0;
    m_failed = false;
    m_error.clear();
    m_points = m_syncs = m_edges = 0;
    m_ngCompute                  = 0;
    m_rejected                   = 0;
    m_batchWall = m_spiceWall = 0;
    s_active                  = true;
}

void SpiceCosim::clearSlots()
{
    for (Slot &s : m_slots)
        if (s.guard)
            s.guard->setCosimSlot(-1);
    m_slots.clear();
    m_rampEnds.clear();
}

void SpiceCosim::end()
{
    s_active = false;
    // Restore the PINx read callbacks of the AVR cores that still exist
    QList<BaseProcessor *> live = Simulator::self()->mcuList();
    for (PinRead *r : m_pinReads) {
        BaseProcessor *p = m_procs[r->proc].p;
        AvrProcessor *ap =
            live.contains(p) ? dynamic_cast<AvrProcessor *>(p) : nullptr;
        if (ap && ap->getCpu() == r->avr && r->avr->io[r->io].r.param == r) {
            r->avr->io[r->io].r.c     = r->origC;
            r->avr->io[r->io].r.param = r->origParam;
        }
        delete r;
    }
    m_pinReads.clear();
    clearSlots();
}

void SpiceCosim::prepareChunk(double t, double chunkLen, double target)
{
    m_target        = target;
    m_chunkEnd      = t + chunkLen;
    m_ts            = t;
    m_releasing     = false;
    m_needChunk     = false;
    m_propDelta     = 0;
    m_lastExitValid = false;
    m_rampEnds.clear(); // A restart completes the running ramps
    for (Slot &s : m_slots) {
        s.v0 = s.v1;
        s.g0 = s.g1;
        s.t0 = t;
    }
    m_turn = 1; // ngspice runs until it parks at t
}

bool SpiceCosim::waitParked()
{
    waitTurn(0);
    return !m_failed;
}

void SpiceCosim::release()
{
    m_releasing = true;
    pass(1);
}

//--------------------------------------------------------- ngspice thread --

double SpiceCosim::value(const Slot &s, bool g, double T) const
{
    double a = g ? s.g0 : s.v0;
    double b = g ? s.g1 : s.v1;
    if (T <= s.t0)
        return a;
    if (T >= s.t0 + m_rise)
        return b;
    return a + (b - a) * (T - s.t0) / m_rise;
}

double SpiceCosim::sourceValue(const char *name, double T)
{
    // "vmv<k>" Thevenin voltage, "vmg<k>" conductance of slot k
    if (!name || !name[0] || !name[1] || !name[2])
        return 0;
    int k = atoi(name + 3);
    if (k < 0 || k >= (int)m_slots.size())
        return 0;
    bool g = (name[2] == 'g' || name[2] == 'G');
    return value(m_slots[k], g, T);
}

void SpiceCosim::onSync(double T, double *delta, int redostep, int loc)
{
    if (m_releasing)
        return;
    auto now = std::chrono::steady_clock::now();
    if (m_lastExitValid)
        m_ngCompute += std::chrono::duration<double>(now - m_lastExit).count();
    m_lastExit      = now;
    m_lastExitValid = true;
    if (loc == 0) // T is an accepted time point
    {
        m_ts = T;
        if (T >= m_target - kEps) // Reached: give the turn to the MCUs
        {
            pass(0);
            waitTurn(1);
            m_lastExit = std::chrono::steady_clock::now();
            if (m_releasing)
                return;
        }
    } else if (!redostep)
        return; // Converged, the next call (loc 0) limits the step
    else
        m_rejected++;

    if (loc == 0)
        m_propDelta = *delta; // ngspice's own choice (accuracy based)

    // Land exactly on the requested time and on the end of every ramp
    double lim = m_target - T;
    if (!m_rampEnds.empty()) {
        int n = 0;
        for (double re : m_rampEnds) {
            if (re > T + kEps) {
                m_rampEnds[n++] = re;
                lim             = std::min(lim, re - T);
            }
        }
        m_rampEnds.resize(n);
    }
    if (lim > kEps && *delta > lim)
        *delta = lim;
}

void SpiceCosim::onPoint(double T)
{
    m_points++;
    m_ts = T;
    // Inputs (and supply/reference pins) see the new node voltages
    for (Slot &s : m_slots) {
        if (!s.node)
            continue;
        McuComponentPin *pin = s.pin;
        if (pin->pinType() == 1 && !pin->isInput())
            continue;
        double v = s.node->getVolt();
        if (std::fabs(v - s.lastIn) > 2e-3) { // 2 mV (PIC ADC inputs)
            s.lastIn = v;
            pin->setVChanged();
        }
    }
}

void SpiceCosim::onBgEnded()
{
    if (m_releasing)
        return;
    if (m_ts >= m_chunkEnd - 1e-9)
        m_needChunk = true; // End of this transient chunk: continue
    else {
        m_error  = "ngspice stopped at t = " + QString::number(m_ts) + " s";
        m_failed = true;
    }
    pass(0);
}

//------------------------------------------------------------- MCU thread --

void SpiceCosim::syncTo(double t)
{
    if (t <= m_ts + kEps || m_failed)
        return;
    m_syncs++;
    auto w0 = std::chrono::steady_clock::now();
    struct WaitTime { // Time the MCU thread waits for ngspice
        std::chrono::steady_clock::time_point t0;
        double *acc;
        ~WaitTime()
        {
            *acc += std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
        }
    } wt{ w0, &m_spiceWall };
    for (;;) {
        m_target = t;
        if (m_needChunk) // Previous transient finished: next one
        {
            m_needChunk = false;
            if (!m_eng->cosimNextChunk()) {
                m_failed = true;
                m_error  = "ngspice could not continue the transient";
                return;
            }
        } else
            pass(1);
        waitTurn(0);
        if (m_failed)
            return;
        if (m_needChunk)
            continue;
        if (m_ts >= t - kEps)
            return;
    }
}

void SpiceCosim::stepProc(Proc &p)
{
    p.p->stepCpu();
    uint64_t c = p.p->cycle();
    uint64_t d = (c >= p.lastCycle) ? c - p.lastCycle : c; // Reset: from 0
    if (d == 0)
        d = 1;
    p.lastCycle = c;
    p.time += d * p.stepS;
}

void SpiceCosim::runBatch(double tEnd, double budgetS)
{
    if (m_failed || m_procs.empty())
        return;
    tl_inMcu = true;
    QElapsedTimer wall;
    wall.start();
    struct BatchTime {
        QElapsedTimer *w;
        double *acc;
        ~BatchTime()
        {
            *acc += w->nsecsElapsed() * 1e-9;
        }
    } bt{ &wall, &m_batchWall };
    Simulator *sim   = Simulator::self();
    int64_t budgetNs = (int64_t)(budgetS * 1e9);
    int n            = 0;

    for (;;) {
        if (m_failed || !sim->isRunning())
            break;
        // The MCU that is furthest behind runs next
        int cur     = -1;
        double tmin = 1e300, t2 = 1e300;
        for (int i = 0; i < (int)m_procs.size(); i++) {
            double t = m_procs[i].time;
            if (t < tmin) {
                t2   = tmin;
                tmin = t;
                cur  = i;
            } else if (t < t2)
                t2 = t;
        }
        if (cur < 0 || tmin >= tEnd)
            break;
        if (tmin > m_ts + m_horizon) // Do not run too far ahead of ngspice
            syncTo(tmin);

        // Run it up to the next MCU (at least one instruction), the end of
        // the batch or the horizon
        double lim        = std::min(std::min(t2, tEnd), m_ts + m_horizon);
        Proc &p           = m_procs[cur];
        BaseProcessor *bp = p.p;
        if (!bp->getLoadStatus() || bp->inReset()) // Idle: nothing to run
        {
            p.time =
                std::max(p.time + p.stepS, std::min(tEnd, m_ts + m_horizon));
            if (bp->getLoadStatus())
                p.lastCycle = bp->cycle();
        } else {
            do
                stepProc(p);
            while (p.time < lim && !m_failed);
        }
        if ((++n & 15) == 0 && wall.nsecsElapsed() > budgetNs)
            break;
    }
    // ngspice catches up, so the GUI shows a consistent state
    double tmin = 1e300;
    for (const Proc &p : m_procs)
        tmin = std::min(tmin, p.time);
    if (!m_failed && sim->isRunning())
        syncTo(std::min(tmin, tEnd));
    tl_inMcu = false;
}

void SpiceCosim::pinChanged(McuComponentPin *pin)
{
    int k = pin->cosimSlot();
    if (k < 0 || k >= (int)m_slots.size())
        return;
    Slot &s  = m_slots[k];
    double v = pin->voltOut();
    double g = 1 / std::max(pin->imp(), 1e-4);
    if (pin->isInput())
        s.lastIn = -1e9; // Deliver the input level on the next point
    if (v == s.v1 && g == s.g1)
        return;

    double t = m_ts;
    if (tl_inMcu && s.proc >= 0) // Edge at the MCU time: ngspice first
    {
        syncTo(m_procs[s.proc].time);
        t = std::max(m_procs[s.proc].time, (double)m_ts);
    }
    s.v0 = value(s, false, t);
    s.g0 = value(s, true, t);
    s.t0 = t;
    s.v1 = v;
    s.g1 = g;
    m_rampEnds.push_back(t + m_rise);
    m_edges++;
}

void SpiceCosim::inputSyncIdx(int proc)
{
    if (!tl_inMcu || proc < 0 || proc >= (int)m_procs.size())
        return;
    // A digital read needs ngspice at the read time only when the analog
    // solution may have moved meanwhile: not more than the step ngspice
    // itself proposed (its accuracy based resolution), at least inputRes.
    double t = m_procs[proc].time;
    double q = std::max(m_inputRes, std::min(m_propDelta, m_horizon));
    if (t - m_ts > q)
        syncTo(t);
}

void SpiceCosim::inputSync(BaseProcessor *proc, double quantum)
{
    if (!tl_inMcu)
        return;
    int i = m_procIndex.value(proc, -1);
    if (i < 0)
        return;
    double t = m_procs[i].time;
    if (t - m_ts > quantum)
        syncTo(t);
}
