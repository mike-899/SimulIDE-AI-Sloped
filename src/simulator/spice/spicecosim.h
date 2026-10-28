/***************************************************************************
 *   Copyright (C) 2026 SimulIDE contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// Lockstep co-simulation of microcontrollers with ngspice, in simulated
// time (see docs/ANALOG_ENGINE.md, "MCU co-simulation").
//
// Two threads run in strict alternation (never at the same time):
//  * the MCU thread (one QtConcurrent batch per GUI tick) executes the
//    firmware instruction by instruction, ahead of ngspice;
//  * the ngspice background thread runs the transient. In the GetSyncData
//    callback it parks after every accepted time point that reached the
//    time requested by the MCU thread, and otherwise clamps its next time
//    step so it lands exactly on that time (and on the end of the ramp of
//    every MCU edge).
// The MCU thread asks ngspice to catch up ("syncTo") when an output pin
// changes (the change is applied as a 10 ns ramp of an EXTERNAL source at
// that exact time), when the firmware samples an input (ADC conversion,
// PINx read) and at least every "horizon" (max step) of simulated time.

#ifndef SPICECOSIM_H
#define SPICECOSIM_H

#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

class BaseProcessor;
class McuComponentPin;
class SpiceEngine;
class eNode;

class MAINMODULE_EXPORT SpiceCosim
{
  public:
    static SpiceCosim *self();

    // True while a co-simulation runs (pin hooks check it first)
    static bool s_active;

    //---- SpiceEngine, GUI thread (ngspice not running) ----------------
    // New netlist: one slot per exported MCU pin (SpiceNetlist order)
    void setup(SpiceEngine *eng, const QList<McuComponentPin *> &pins);
    void begin(double t0); // First run: processors, PINx read hooks
    void end();            // Remove hooks, forget slots
    // Before / after starting an ngspice chunk that begins at time t and
    // must run up to time target
    void prepareChunk(double t, double chunkLen, double target);
    bool waitParked(); // false: ngspice failed before reaching t
    void release();    // Unblock a parked ngspice thread (stop / halt)

    void setParams(double horizon, double rise, double inputRes)
    {
        m_horizon  = horizon;
        m_rise     = rise;
        m_inputRes = inputRes;
    }

    //---- ngspice thread -----------------------------------------------
    void onSync(double T, double *delta, int redostep, int loc);
    void onPoint(double T); // Accepted point, eNodes already updated
    double sourceValue(const char *name, double T);
    void onBgEnded();

    //---- MCU thread ---------------------------------------------------
    // Run the MCUs (and ngspice behind them) until simulated time tEnd
    // or until budgetS seconds of wall time were used.
    void runBatch(double tEnd, double budgetS);
    void pinChanged(McuComponentPin *pin);
    // Firmware samples an input of proc: bring ngspice to the MCU time
    // if it is more than quantum seconds behind
    void inputSync(BaseProcessor *proc, double quantum);
    void inputSyncIdx(int proc);

    //---- status -------------------------------------------------------
    double time()
    {
        return m_ts.load(std::memory_order_relaxed);
    }
    bool failed()
    {
        return m_failed;
    }
    QString error()
    {
        return m_error;
    }
    double target()
    {
        return m_target;
    }
    bool needChunk()
    {
        return m_needChunk;
    }
    uint64_t points()
    {
        return m_points;
    }
    uint64_t syncs()
    {
        return m_syncs;
    }
    uint64_t edges()
    {
        return m_edges;
    }
    // Wall time of the MCU batches and the part spent waiting for ngspice
    double batchWall()
    {
        return m_batchWall;
    }
    double spiceWall()
    {
        return m_spiceWall;
    }
    double ngCompute()
    {
        return m_ngCompute;
    }
    uint64_t rejected()
    {
        return m_rejected;
    }
    double horizon()
    {
        return m_horizon;
    }

  private:
    SpiceCosim() {}

    struct Slot {
        McuComponentPin *pin;
        QPointer<McuComponentPin> guard; // The MCU may be deleted meanwhile
        eNode *node;
        int proc;
        double t0; // Ramp start (global simulated time)
        double v0, g0, v1, g1;
        double lastIn; // Last voltage given to the MCU input
    };
    struct Proc {
        BaseProcessor *p;
        double stepS;       // Seconds per MCU clock cycle
        uint64_t lastCycle; // cycle() at the last update
        double time;        // Simulated time of the next instruction
    };

  public:
    struct PinRead; // Wrapped simavr PINx read callback (spicecosim.cpp)

  private:
    void syncTo(double t);
    void stepProc(Proc &p);
    void clearSlots();
    double value(const Slot &s, bool g, double T) const;
    void pass(int side);
    void waitTurn(int side);

    SpiceEngine *m_eng = nullptr;
    std::vector<Slot> m_slots;
    std::vector<Proc> m_procs;
    std::vector<PinRead *> m_pinReads;
    QHash<BaseProcessor *, int> m_procIndex;
    std::vector<double> m_rampEnds;

    // Handoff between the MCU thread (side 0) and ngspice (side 1)
    std::atomic<int> m_turn{ 0 };
    std::atomic<int> m_sleepers{ 0 };
    std::mutex m_mx;
    std::condition_variable m_cv;

    std::atomic<double> m_ts{ 0 }; // Last accepted ngspice time
    double m_target    = 0;        // Time ngspice must reach
    double m_chunkEnd  = 0;
    double m_propDelta = 0; // Last time step proposed by ngspice
    std::atomic<bool> m_releasing{ false };
    std::atomic<bool> m_needChunk{ false };
    std::atomic<bool> m_failed{ false };
    QString m_error;

    double m_horizon  = 20e-6; // Max ngspice lag behind the MCUs
    double m_rise     = 10e-9; // MCU pin edge ramp
    double m_inputRes = 2e-6;  // PINx read sync quantum

    uint64_t m_points   = 0;
    uint64_t m_syncs    = 0;
    uint64_t m_edges    = 0;
    double m_batchWall  = 0;
    double m_ngCompute  = 0; // ngspice thread busy time (without waits)
    uint64_t m_rejected = 0; // Time steps redone by ngspice
    std::chrono::steady_clock::time_point m_lastExit;
    bool m_lastExitValid = false;
    double m_spiceWall   = 0;
};

#endif
