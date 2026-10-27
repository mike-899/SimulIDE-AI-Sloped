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
#include "ngspice-api.h"
#include "simulator.h"

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
    bool ok = net.build(&err);
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

    QStringList lines = m_net.lines(m_chunkLen, m_maxStep, ic);
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
    m_mapDone   = false;
    m_chunkDone = false;
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

bool SpiceEngine::start(QString *error)
{
    stop();
    if (!load(error))
        return false;
    if (!m_net.build(error))
        return false;

    MainWindow *mw = MainWindow::self();
    if (mw) {
        double ms = mw->settings()->value("spice/maxstep", 20e-6).toDouble();
        if (ms > 1e-9 && ms < 1e-2)
            m_maxStep = ms;
    }
    m_slotNodes.clear();
    m_slotNames.clear();
    for (eNode *n : m_net.nodes()) {
        m_slotNodes.append(n);
        m_slotNames.append(m_net.nodeName(n));
    }
    for (const SpiceNetlist::Ammeter &a : m_net.ammeters())
        m_slotNames.append(a.vector);

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

    if (!runChunk(nullptr, error))
        return false;
    m_running = true;
    std::cout << "\n    ngspice engine running (" << m_net.nodes().size()
              << " nodes)\n"
              << std::endl;
    return true;
}

void SpiceEngine::stop()
{
    m_halt    = true;
    m_running = false;
    haltBg();
    QMutexLocker lock(&m_mutex);
    m_queue.clear();
}

void SpiceEngine::restart()
{
    QHash<eNode *, double> ic;
    QString err;
    stop();
    if (!m_net.build(&err)) {
        std::cout << "ngspice: " << err.toStdString() << std::endl;
        return;
    }
    for (eNode *n : m_net.nodes())
        ic.insert(n, n->getVolt());

    m_slotNodes.clear();
    m_slotNames.clear();
    for (eNode *n : m_net.nodes()) {
        m_slotNodes.append(n);
        m_slotNames.append(m_net.nodeName(n));
    }
    for (const SpiceNetlist::Ammeter &a : m_net.ammeters())
        m_slotNames.append(a.vector);

    m_chunkOffset = m_lastTime;
    m_wallOffset  = m_lastTime;
    m_wall.restart();
    if (runChunk(&ic, &err))
        m_running = true;
    else
        std::cout << "ngspice: " << err.toStdString() << std::endl;
}

void SpiceEngine::step()
{
    if (!m_running)
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
    const QList<SpiceNetlist::Ammeter> &ams = m_net.ammeters();
    int nNodes                              = m_slotNodes.size();
    double stepsPerS                        = sim->m_stepsPerus * 1e6;
    uint64_t maxSteps = sim->m_step + 4 * sim->m_circuitRate + 1;

    for (const Point &p : pts) {
        for (int i = 0; i < nNodes; i++)
            m_slotNodes[i]->setVolt(p.v[i]);
        for (int k = 0; k < ams.size(); k++) {
            double I = p.v[nNodes + k];
            ams[k].rNode->setVolt(ams[k].lNode->getVolt() - I * ams[k].res);
        }
        uint64_t target = (uint64_t)(p.t * stepsPerS);
        if (target > maxSteps)
            target = maxSteps;
        while (sim->m_step < target) {
            for (eElement *el : m_clockEls)
                el->simuClockStep();
            sim->m_step++;
        }
    }
    sim->m_circTime = sim->m_step * sim->m_stepNS;
}
