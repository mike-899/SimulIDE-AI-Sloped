/***************************************************************************
 *   Copyright (C) 2026 SimulIDE contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// "Accurate analog (ngspice)" engine: runs the circuit as a transient
// analysis in libngspice (loaded at runtime) in its background thread,
// paced to real time, and streams node voltages back into the eNodes so the
// existing UI (probes, meters, oscilloscope, LED brightness) keeps working.
//
// ngspice (http://ngspice.sourceforge.net) is BSD-3-Clause licensed;
// see NOTICE.

#ifndef SPICEENGINE_H
#define SPICEENGINE_H

#include "spicenetlist.h"

#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <atomic>
#include <vector>

class QLibrary;
class eElement;

class MAINMODULE_EXPORT SpiceEngine
{
  public:
    static SpiceEngine *self();

    // User toggle (simulide.ini "spice/enabled", --spice, MCP tool)
    static bool enabled();
    static void setEnabled(bool en);

    bool available(QString *error = nullptr); // libngspice can be loaded
    QString version();

    // Netlist of the current circuit (for inspection / export)
    QString netlistText(QString *error = nullptr);

    bool start(QString *error); // Called after Simulator::startSim()
    void stop();                // Stop (ends a MCU co-simulation)
    void halt();                // Pause: MCU co-simulation state is kept
    void restart();             // Circuit or property changed while running
    bool isRunning()
    {
        return m_running;
    }

    // Lockstep co-simulation with microcontrollers (SpiceCosim)
    bool cosim()
    {
        return m_cosim;
    }
    bool cosimAvailable()
    {
        return m_initSync != nullptr;
    }
    // GUI thread: simulated time the next batch must reach (real time
    // pacing: never more than one tick ahead of the wall clock)
    double cosimBatchEnd(double tick);
    void cosimRun();       // MCU thread: runs until the simulation stops
    bool cosimNextChunk(); // MCU thread

    void step(); // GUI thread, every timer tick: apply streamed results

    QString log()
    {
        return m_log.join("\n");
    }
    double simTime()
    {
        return m_lastTime;
    }

    // ngspice callbacks
    static int cbSendChar(char *, int, void *);
    static int cbSendStat(char *, int, void *);
    static int cbExit(int, bool, bool, int, void *);
    static int cbData(void *, int, int, void *);
    static int cbInitData(void *, int, void *);
    static int cbBgRunning(bool, int, void *);
    static int cbVsrc(double *, double, char *, int, void *);
    static int cbSync(double, double *, double, int, int, int, void *);

  private:
    SpiceEngine();

    bool load(QString *error);
    bool runChunk(const QHash<eNode *, double> *ic, QString *error);
    void haltBg();
    void command(const char *cmd);

    struct Point {
        double t;
        std::vector<double> v;
    };
    void mapSlots();
    void applyPoint(const Point &p); // Point -> eNodes, step counter

    QLibrary *m_lib    = nullptr;
    void *m_init       = nullptr;
    void *m_circ       = nullptr;
    void *m_cmd        = nullptr;
    void *m_runningFn  = nullptr;
    void *m_initSync   = nullptr;
    bool m_initialized = false;

    SpiceNetlist m_net;

    // Vector slots: node voltages then ammeter currents
    QList<eNode *> m_slotNodes;
    QStringList m_slotNames;
    std::vector<int> m_vecToSlot; // ngspice vector index -> slot
    int m_timeIndex = -1;
    bool m_mapDone  = false;

    QMutex m_mutex;
    std::vector<Point> m_queue;
    std::vector<double> m_last; // Last values (for .ic continuation)

    QElapsedTimer m_wall;
    double m_wallOffset  = 0; // Sim time when m_wall restarted
    double m_chunkOffset = 0; // Sim time at start of the running chunk
    double m_chunkLen    = 2; // Seconds per ngspice transient run
    double m_maxStep     = 20e-6;
    std::atomic<double> m_lastTime{ 0 };

    std::atomic<bool> m_running{ false };
    std::atomic<bool> m_bgRunning{ false };
    std::atomic<bool> m_chunkDone{ false };
    std::atomic<bool> m_halt{ false };
    bool m_cosim           = false;
    uint64_t m_chunkPoints = 0; // Points in the running chunk (cosim)
    bool m_failed          = false;
    int m_idleTicks        = 0;

    QStringList m_log;
    QString m_version;
    QList<eElement *> m_clockEls;
};

#endif
