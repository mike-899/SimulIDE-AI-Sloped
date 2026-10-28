/***************************************************************************
 *   Copyright (C) 2026 SimulIDE contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "spiceengine.h"

#include "clock-base.h"
#include "e-element.h"
#include "e-node.h"
#include "mainwindow.h"
#include "mcucomponent.h"
#include "ngspice-api.h"
#include "simulator.h"
#include "spicecosim.h"

#include <QCoreApplication>
#include <QLibrary>
#include <QRegularExpression>
#include <QSettings>
#include <QThread>
#include <cmath>
#include <iostream>

static bool s_enabled     = false;
static bool s_enabledRead = false;

SpiceEngine *SpiceEngine::self()
{
    static SpiceEngine engine;
    return &engine;
}

SpiceEngine::SpiceEngine() {}

bool SpiceEngine::enabled()
{
    if (!s_enabledRead) {
        s_enabledRead          = true;
        const QStringList args = QCoreApplication::arguments();
        if (args.contains("--spice"))
            s_enabled = true;
        else if (args.contains("--no-spice"))
            s_enabled = false;
        else if (qEnvironmentVariableIsSet("SIMULIDE_SPICE"))
            s_enabled = qgetenv("SIMULIDE_SPICE") != "0";
        else if (MainWindow::self())
            s_enabled =
                MainWindow::self()->settings()->value("spice/enabled").toBool();
    }
    return s_enabled;
}

void SpiceEngine::setEnabled(bool en)
{
    s_enabledRead = true;
    s_enabled     = en;
    if (MainWindow::self())
        MainWindow::self()->settings()->setValue("spice/enabled", en);
}

//-------------------------------------------------------------- callbacks --

int SpiceEngine::cbSendChar(char *text, int, void *user)
{
    SpiceEngine *e = static_cast<SpiceEngine *>(user);
    QString line   = QString::fromLocal8Bit(text);
    QMutexLocker lock(&e->m_mutex);
    if (line.contains("Error", Qt::CaseInsensitive) &&
        !line.contains("no circuit loaded", Qt::CaseInsensitive) &&
        !line.contains("remcirc", Qt::CaseInsensitive))
        e->m_failed = true;
    e->m_log.append(line);
    if (e->m_log.size() > 300)
        e->m_log.removeFirst();
    return 0;
}

int SpiceEngine::cbSendStat(char *, int, void *)
{
    return 0;
}

int SpiceEngine::cbExit(int status, bool, bool, int, void *user)
{
    SpiceEngine *e = static_cast<SpiceEngine *>(user);
    QMutexLocker lock(&e->m_mutex);
    e->m_log.append(QString("ngspice exit request, status %1").arg(status));
    e->m_failed = true;
    return 0;
}

int SpiceEngine::cbInitData(void *, int, void *)
{
    return 0;
}

int SpiceEngine::cbBgRunning(bool notRunning, int, void *user)
{
    // ngspice sends 'true' when the background thread is NOT running
    SpiceEngine *e = static_cast<SpiceEngine *>(user);
    e->m_bgRunning = !notRunning; // Informative only, see step()
    if (notRunning && e->m_cosim && SpiceCosim::s_active)
        SpiceCosim::self()->onBgEnded(); // Chunk finished (or failed)
    return 0;
}

int SpiceEngine::cbVsrc(double *value, double time, char *name, int, void *user)
{
    SpiceEngine *e = static_cast<SpiceEngine *>(user);
    *value         = 0;
    if (e->m_cosim)
        *value = SpiceCosim::self()->sourceValue(name, time + e->m_chunkOffset);
    return 0;
}

int SpiceEngine::cbSync(double time, double *delta, double, int redostep, int,
                        int loc, void *user)
{
    // Called by ngspice before every time step (loc 0, after accepting a
    // point) and after a step was computed (loc 1). Only the
    // co-simulation limits the step; ngspice decides otherwise.
    SpiceEngine *e = static_cast<SpiceEngine *>(user);
    if (e->m_cosim && !e->m_halt)
        SpiceCosim::self()->onSync(time + e->m_chunkOffset, delta, redostep,
                                   loc);
    return 0;
}

int SpiceEngine::cbData(void *data, int, int, void *user)
{
    SpiceEngine *e    = static_cast<SpiceEngine *>(user);
    pvecvaluesall all = static_cast<pvecvaluesall>(data);
    if (e->m_halt)
        return 0;

    if (!e->m_mapDone) // Map vector names to slots
    {
        e->m_vecToSlot.assign(all->veccount, -1);
        e->m_timeIndex = -1;
        for (int i = 0; i < all->veccount; i++) {
            QString name = QString(all->vecsa[i]->name).toLower();
            if (name == "time") {
                e->m_timeIndex = i;
                continue;
            }
            if (name.startsWith("v(") && name.endsWith(")"))
                name = name.mid(2, name.size() - 3);
            else if (name.startsWith("i(") && name.endsWith(")"))
                name = name.mid(2, name.size() - 3) + "#branch";
            e->m_vecToSlot[i] = e->m_slotNames.indexOf(name);
        }
        e->m_mapDone = true;
    }
    if (e->m_timeIndex < 0)
        return 0;

    Point p;
    p.t = all->vecsa[e->m_timeIndex]->creal + e->m_chunkOffset;
    p.v.assign(e->m_slotNames.size(), 0.0);
    for (int i = 0; i < all->veccount; i++) {
        int s = e->m_vecToSlot[i];
        if (s >= 0)
            p.v[s] = all->vecsa[i]->creal;
    }
    e->m_lastTime = p.t;
    if (e->m_cosim) // Lockstep: apply now, the MCU thread is waiting
    {
        e->m_last = p.v;
        e->m_chunkPoints++;
        e->applyPoint(p);
        SpiceCosim::self()->onPoint(p.t);
        return 0;
    }
    {
        QMutexLocker lock(&e->m_mutex);
        e->m_last = p.v;
        if (e->m_queue.size() > 50000) // GUI not consuming: keep it bounded
            e->m_queue.erase(e->m_queue.begin(), e->m_queue.begin() + 25000);
        e->m_queue.push_back(std::move(p));
    }
    // Real time pacing: block the ngspice thread while ahead of the clock
    double t = e->m_lastTime;
    while (!e->m_halt) {
        double wall = e->m_wallOffset + e->m_wall.nsecsElapsed() * 1e-9;
        if (t <= wall)
            break;
        double dt = std::min(t - wall, 0.01);
        QThread::usleep((unsigned long)(dt * 1e6) + 1);
    }
    return 0;
}

// The C callbacks with the exact ngspice signatures
extern "C" {
static int ng_char(char *s, int id, void *u)
{
    return SpiceEngine::cbSendChar(s, id, u);
}
static int ng_stat(char *s, int id, void *u)
{
    return SpiceEngine::cbSendStat(s, id, u);
}
static int ng_exit(int st, NG_BOOL a, NG_BOOL b, int id, void *u)
{
    return SpiceEngine::cbExit(st, a, b, id, u);
}
static int ng_data(pvecvaluesall d, int n, int id, void *u)
{
    return SpiceEngine::cbData(d, n, id, u);
}
static int ng_init(pvecinfoall d, int id, void *u)
{
    return SpiceEngine::cbInitData(d, id, u);
}
static int ng_bg(NG_BOOL r, int id, void *u)
{
    return SpiceEngine::cbBgRunning(r, id, u);
}
static int ng_vsrc(double *v, double t, char *n, int id, void *u)
{
    return SpiceEngine::cbVsrc(v, t, n, id, u);
}
static int ng_sync(double t, double *d, double od, int rs, int id, int loc,
                   void *u)
{
    return SpiceEngine::cbSync(t, d, od, rs, id, loc, u);
}
}

//------------------------------------------------------------------ engine --

bool SpiceEngine::load(QString *error)
{
    if (m_initialized)
        return true;

    if (!m_lib) {
        QString path;
        if (MainWindow::self())
            path = MainWindow::self()
                       ->settings()
                       ->value("spice/library")
                       .toString();
        m_lib =
            path.isEmpty() ? new QLibrary("ngspice", 0) : new QLibrary(path);
        if (!m_lib->load()) {
            delete m_lib;
            m_lib = new QLibrary("ngspice"); // Unversioned (Windows, macOS)
        }
    }
    if (!m_lib->isLoaded() && !m_lib->load()) {
        if (error)
            *error = "libngspice not found (" + m_lib->errorString() +
                     "). Install ngspice (e.g. apt install libngspice0)";
        return false;
    }
    m_init      = (void *)m_lib->resolve("ngSpice_Init");
    m_circ      = (void *)m_lib->resolve("ngSpice_Circ");
    m_cmd       = (void *)m_lib->resolve("ngSpice_Command");
    m_runningFn = (void *)m_lib->resolve("ngSpice_running");
    if (!m_init || !m_circ || !m_cmd || !m_runningFn) {
        if (error)
            *error = "libngspice: missing API symbols";
        return false;
    }
    ((ngSpice_Init_t)m_init)(ng_char, ng_stat, ng_exit, ng_data, ng_init, ng_bg,
                             this);
    // EXTERNAL sources and time step control for the MCU co-simulation
    m_initSync = (void *)m_lib->resolve("ngSpice_Init_Sync");
    if (m_initSync)
        ((ngSpice_Init_Sync_t)m_initSync)(ng_vsrc, nullptr, ng_sync, nullptr,
                                          this);
    m_initialized = true;
    return true;
}

bool SpiceEngine::available(QString *error)
{
    return load(error);
}

QString SpiceEngine::version()
{
    if (!m_version.isEmpty() || !load(nullptr) || m_running)
        return m_version;
    command("version -s");
    QMutexLocker lock(&m_mutex);
    QRegularExpression re("ngspice-([0-9A-Za-z.+]+)");
    for (const QString &l : m_log) {
        QRegularExpressionMatch m = re.match(l);
        if (m.hasMatch()) {
            m_version = m.captured(1);
            break;
        }
    }
    return m_version;
}

void SpiceEngine::command(const char *cmd)
{
    if (!m_initialized)
        return;
    QByteArray c(cmd);
    ((ngSpice_Command_t)m_cmd)(c.data());
}

void SpiceEngine::haltBg()
{
    if (!m_initialized)
        return;
    if (((ngSpice_running_t)m_runningFn)()) {
        command("bg_halt");
        for (int i = 0; i < 400; i++) {
            if (!((ngSpice_running_t)m_runningFn)())
                break;
            QThread::msleep(5);
        }
    }
}

QString SpiceEngine::netlistText(QString *error)
{
    SpiceNetlist net;
    QString err;
    bool ok = net.build(&err, true); // MCU pins as EXTERNAL sources
    if (error)
        *error = err;
    QString text = net.lines(m_chunkLen, m_maxStep).join("\n");
    return ok ? text : QString();
}

bool SpiceEngine::runChunk(const QHash<eNode *, double> *ic, QString *error)
{
    haltBg();
    m_halt = true;
    command("destroy all");
    command("remcirc");

    QStringList lines = m_net.lines(m_chunkLen, m_maxStep, ic, m_cosim);
    std::vector<QByteArray> store;
    std::vector<char *> ptrs;
    for (const QString &l : lines)
        store.push_back(l.toLatin1());
    for (QByteArray &b : store)
        ptrs.push_back(b.data());
    ptrs.push_back(nullptr);

    {
        QMutexLocker lock(&m_mutex);
        m_failed = false;
    }
    m_mapDone     = false;
    m_chunkDone   = false;
    m_chunkPoints = 0;
    ((ngSpice_Circ_t)m_circ)(ptrs.data());

    bool failed;
    {
        QMutexLocker lock(&m_mutex);
        failed = m_failed;
    }
    if (failed) {
        if (error)
            *error = "ngspice could not parse the netlist:\n" + log();
        return false;
    }
    m_halt      = false;
    m_idleTicks = 0;
    command("bg_run");
    return true;
}

void SpiceEngine::mapSlots()
{
    m_slotNodes.clear();
    m_slotNames.clear();
    for (eNode *n : m_net.nodes()) {
        m_slotNodes.append(n);
        m_slotNames.append(m_net.nodeName(n));
    }
    for (const SpiceNetlist::Ammeter &a : m_net.ammeters())
        m_slotNames.append(a.vector);
}

static double setting(const char *key, double def, double min, double max)
{
    MainWindow *mw = MainWindow::self();
    if (!mw)
        return def;
    double v = mw->settings()->value(key, def).toDouble();
    return (v >= min && v <= max) ? v : def;
}

bool SpiceEngine::start(QString *error)
{
    stop();
    if (!load(error))
        return false;

    // Microcontrollers run in lockstep with ngspice (SpiceCosim)
    m_cosim = !McuComponent::mcuList().isEmpty();
    if (m_cosim && !m_initSync) {
        if (error)
            *error = "this libngspice has no ngSpice_Init_Sync (needed to "
                     "co-simulate microcontrollers)";
        m_cosim = false;
        return false;
    }
    if (!m_net.build(error, m_cosim)) {
        m_cosim = false;
        return false;
    }
    m_maxStep = setting("spice/maxstep", 20e-6, 1e-9, 1e-2);
    mapSlots();

    m_clockEls.clear(); // Elements sampled every simulation step (scopes)
    for (eElement *el : Simulator::self()->m_simuClock)
        if (!dynamic_cast<ClockBase *>(el))
            m_clockEls.append(el);

    {
        QMutexLocker lock(&m_mutex);
        m_queue.clear();
        m_last.clear();
        m_log.clear();
    }
    m_chunkOffset = 0;
    m_wallOffset  = 0;
    m_lastTime    = 0;
    m_wall.start();

    SpiceCosim *cs = SpiceCosim::self();
    if (m_cosim) {
        // The MCUs run at most one "horizon" ahead of ngspice (latency of
        // asynchronous inputs such as pin change interrupts); ngspice may
        // reach the next sync time in one step (max step = 1.25 horizon).
        double horizon = setting("spice/cosimHorizon", 50e-6, 1e-7, 1e-2);
        m_maxStep      = 1.25 * horizon;
        m_chunkLen     = 0.25;
        cs->setParams(horizon, setting("spice/cosimRise", 10e-9, 1e-12, 1e-6),
                      setting("spice/cosimInputRes", 2e-6, 0, 1e-3));
        cs->begin(0);
        cs->setup(this, m_net.mcuPins());
        cs->prepareChunk(0, m_chunkLen, 0);
    } else
        m_chunkLen = 2;

    if (!runChunk(nullptr, error) || (m_cosim && !cs->waitParked())) {
        if (m_cosim) {
            if (error && cs->failed())
                *error = cs->error() + "\n" + log();
            stop();
        }
        return false;
    }
    m_running = true;
    std::cout << "\n    ngspice engine running (" << m_net.nodes().size()
              << " nodes";
    if (m_cosim)
        std::cout << ", " << McuComponent::mcuList().size()
                  << " MCU co-simulated, " << m_net.mcuPins().size()
                  << " MCU pins";
    std::cout << ")\n" << std::endl;
    return true;
}

void SpiceEngine::halt()
{
    m_halt    = true;
    m_running = false;
    if (m_cosim) {
        SpiceCosim::s_active = false;
        SpiceCosim::self()->release(); // ngspice is parked in cbSync
    }
    haltBg();
    QMutexLocker lock(&m_mutex);
    m_queue.clear();
}

void SpiceEngine::stop()
{
    halt();
    if (m_cosim)
        SpiceCosim::self()->end();
    m_cosim = false;
}

void SpiceEngine::restart()
{
    QHash<eNode *, double> ic;
    QString err;
    halt();
    if (!m_net.build(&err, m_cosim)) {
        std::cout << "ngspice: " << err.toStdString() << std::endl;
        stop();
        return;
    }
    for (eNode *n : m_net.nodes())
        ic.insert(n, n->getVolt());
    mapSlots();

    SpiceCosim *cs = SpiceCosim::self();
    if (m_cosim) {
        m_lastTime = cs->time();
        cs->setup(this, m_net.mcuPins());
        SpiceCosim::s_active = true;
        cs->prepareChunk(m_lastTime, m_chunkLen, m_lastTime);
    }
    m_chunkOffset = m_lastTime;
    m_wallOffset  = m_lastTime;
    m_wall.restart();
    if (runChunk(&ic, &err) && (!m_cosim || cs->waitParked()))
        m_running = true;
    else {
        std::cout << "ngspice: " << err.toStdString() << std::endl;
        if (m_cosim)
            stop();
    }
}

//---------------------------------------------------- MCU co-simulation --

double SpiceEngine::cosimBatchEnd(double tick)
{
    double simT = SpiceCosim::self()->time();
    double wall = m_wallOffset + m_wall.nsecsElapsed() * 1e-9;
    // As fast as possible up to real time; when slower, simply slower
    return std::min(simT + tick, wall + tick);
}

void SpiceEngine::cosimRun()
{
    SpiceCosim *cs     = SpiceCosim::self();
    Simulator *sim     = Simulator::self();
    const double slice = 0.02; // Simulated / wall seconds per batch
    while (m_running && m_cosim && sim->isRunning() && !cs->failed()) {
        double tEnd = cosimBatchEnd(slice);
        if (tEnd - cs->time() < 1e-4) // Ahead of real time: wait
        {
            QThread::usleep(1000);
            continue;
        }
        cs->runBatch(tEnd, slice);
    }
    if (cs->failed()) {
        std::cout << "ngspice co-simulation error: "
                  << cs->error().toStdString() << "\n"
                  << log().toStdString() << std::endl;
        m_running    = false;
        sim->m_error = true; // GUI stops the simulation
    }
}

bool SpiceEngine::cosimNextChunk()
{
    // Keep the stored transient small: the next chunk is as long as
    // about 200000 points (1 ms .. 1 s)
    double len = m_chunkLen;
    if (m_chunkPoints > 0)
        len = m_chunkLen * 200000.0 / m_chunkPoints;
    m_chunkLen = std::max(1e-3, std::min(1.0, len));

    QHash<eNode *, double> ic;
    {
        QMutexLocker lock(&m_mutex);
        for (int i = 0; i < m_slotNodes.size() && i < (int)m_last.size(); i++)
            ic.insert(m_slotNodes[i], m_last[i]);
    }
    SpiceCosim *cs = SpiceCosim::self();
    double t       = cs->time();
    m_chunkOffset  = t;
    cs->prepareChunk(t, m_chunkLen, std::max(t, cs->target()));
    QString err;
    if (!runChunk(&ic, &err)) {
        std::cout << "ngspice: " << err.toStdString() << std::endl;
        return false;
    }
    return true;
}

void SpiceEngine::applyPoint(const Point &p)
{
    Simulator *sim                          = Simulator::self();
    const QList<SpiceNetlist::Ammeter> &ams = m_net.ammeters();
    int nNodes                              = m_slotNodes.size();
    double stepsPerS                        = sim->m_stepsPerus * 1e6;

    for (int i = 0; i < nNodes; i++)
        m_slotNodes[i]->setVolt(p.v[i]);
    for (int k = 0; k < ams.size(); k++) {
        double I = p.v[nNodes + k];
        ams[k].rNode->setVolt(ams[k].lNode->getVolt() - I * ams[k].res);
    }
    uint64_t target = (uint64_t)(p.t * stepsPerS + 0.5);
    while (sim->m_step < target) {
        for (eElement *el : m_clockEls)
            el->simuClockStep();
        sim->m_step++;
    }
    sim->m_circTime = sim->m_step * sim->m_stepNS;
    // Native solver lists filled by setVolt() are not used here
    sim->m_changedFast.clear();
    sim->m_reactive.clear();
    sim->m_nonLinear.clear();
    sim->m_eChangedNodeList.clear();
}

void SpiceEngine::step()
{
    if (!m_running || m_cosim)
        return;
    Simulator *sim = Simulator::self();

    bool bgRunning = ((ngSpice_running_t)m_runningFn)();
    if (!bgRunning && (m_lastTime >= m_chunkOffset + m_chunkLen * 0.999))
        m_chunkDone = true;
    else if (!bgRunning && ++m_idleTicks > 40) // 2 s without progress
    {
        std::cout << "ngspice: transient stopped unexpectedly\n"
                  << log().toStdString() << std::endl;
        m_running = false;
        return;
    }
    if (bgRunning)
        m_idleTicks = 0;

    if (m_chunkDone) // Continue with a new transient from the last values
    {
        m_idleTicks = 0;
        m_chunkDone = false;
        QHash<eNode *, double> ic;
        {
            QMutexLocker lock(&m_mutex);
            for (int i = 0; i < m_slotNodes.size() && i < (int)m_last.size();
                 i++)
                ic.insert(m_slotNodes[i], m_last[i]);
        }
        m_chunkOffset = m_lastTime;
        QString err;
        if (!runChunk(&ic, &err)) {
            std::cout << "ngspice: " << err.toStdString() << std::endl;
            m_running = false;
            return;
        }
    }
    std::vector<Point> pts;
    {
        QMutexLocker lock(&m_mutex);
        pts.swap(m_queue);
    }
    uint64_t maxSteps = sim->m_step + 4 * sim->m_circuitRate + 1;
    double stepsPerS  = sim->m_stepsPerus * 1e6;

    for (const Point &p : pts) {
        if ((uint64_t)(p.t * stepsPerS) > maxSteps) // GUI was late
        {
            Point q = p;
            q.t     = maxSteps / stepsPerS;
            applyPoint(q);
        } else
            applyPoint(p);
    }
}
