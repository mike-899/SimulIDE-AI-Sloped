/***************************************************************************
 *   Copyright (C) 2012 by santiago González                               *
 *   santigoro@gmail.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.  *
 *                                                                         *
 ***************************************************************************/

#include "e-diode.h"

#include "e-nlstamp.h"
#include "e-node.h"
#include "simulator.h"

#include <algorithm>
#include <cmath>
#include <iostream>

// Junction voltage convergence tolerance: skipping a restamp below it leaves
// a relative current error of about (VTOL/(N*Vt))^2/2 < 1e-6
static const double VTOL = 5e-5;
// Current tolerance (SPICE style): a new stamp is only needed when the
// present linearization mispredicts the device current
static const double ITOL_ABS = 1e-9;
static const double ITOL_REL = 1e-4;

eDiode::eDiode(std::string id) : eResistor(id)
{
    m_imped     = 0.6;
    m_threshold = 0.7;
    m_zenerV    = 0;
    m_emission  = 1.8;
    m_model     = "Custom";
    m_voltPN    = 0;
    m_vd        = 0;
    m_vdStamp   = 1e9;
    m_vtEval    = 1e9;
    m_limited   = false;
    refreshModel();
}
eDiode::~eDiode() {}

Semi::DiodeParams eDiode::autoParams()
{
    Semi::DiodeParams p;
    Semi::diodePreset("1N4148", p);
    return p;
}

// Model or parameter changed: reload it safely while the simulation runs and
// make sure the device is evaluated again
void eDiode::refreshModel()
{
    Simulator *sim = Simulator::self();
    bool running   = sim->isRunning();
    if (running)
        sim->pauseSim();

    loadModel();
    if (running || sim->isPaused())
        sim->addToNoLinList(this);

    if (running)
        sim->resumeSim();
}

void eDiode::loadModel()
{
    if (m_model == "Auto")
        m_p = autoParams();
    else if ((m_model == "Custom") || !Semi::diodePreset(m_model, m_p))
        m_p = Semi::customDiode(m_threshold, m_imped, m_zenerV, m_emission);

    m_vcrit   = Semi::vcrit(m_p.n * Semi::VT, m_p.is);
    m_vcritBv = Semi::vcrit(m_p.nbv * Semi::VT, m_p.ibv > 0 ? m_p.ibv : 1e-3);
    m_vdStamp = 1e9; // Force restamp
    m_vtEval  = 1e9;
}

void eDiode::setModelName(const QString &model)
{
    bool pauseSim = Simulator::self()->isRunning();
    if (pauseSim)
        Simulator::self()->pauseSim();

    m_model = model;
    refreshModel();

    if (pauseSim)
        Simulator::self()->resumeSim();
}

double eDiode::effectiveZenerV()
{
    // Breakdown of rectifiers (BV >= 40 V) is not drawn as a zener
    if (m_p.bv > 0 && m_p.bv < 40)
        return m_p.bv + m_p.ibv * m_p.rs;
    return 0;
}

void eDiode::stamp()
{
    for (int i = 0; i < 2; i++) {
        if (m_ePin[i]->isConnected()) {
            eNode *node = m_ePin[i]->getEnode();
            node->addToNoLinList(this);
            node->setSwitched(true);
        }
    }
    m_vd      = 0;
    m_vdStamp = 1e9;
    m_vtEval  = 1e9;
    setVChanged(); // Initial linearization at the present voltage
}

void eDiode::resetState()
{
    m_resist  = high_imp;
    m_admit   = 0;
    m_voltPN  = 0;
    m_current = 0;
    m_vd      = 0;
    m_vdStamp = 1e9;
    m_vtEval  = 1e9;
    m_limited = false;
}

void eDiode::setVChanged()
{
    if (!m_ePin[0]->isConnected() || !m_ePin[1]->isConnected())
        return;

    int it = Simulator::self()->nlIteration();
    if (it > NEWTON_MAX) // Not converging here: continue in the next step
    {
        Simulator::self()->addToNoLinNextStep(this);
        return;
    }

    double vt = m_ePin[0]->getVolt() - m_ePin[1]->getVolt();
    m_voltPN  = vt;
    if (!m_limited && (std::fabs(vt - m_vtEval) < 1e-6))
        return; // Terminal voltage did not really change
    m_vtEval = vt;

    double vdOld    = m_vd;
    double vd       = Semi::diodeSolveRs(m_p, vt, vdOld);
    double nvt      = m_p.n * Semi::VT;
    double vdTarget = vd;

    // Forward limiting. With series resistance the composite I(V) curve
    // grows at most like V/Rs, so Newton can't overflow and converges much
    // faster without junction limiting.
    if (m_p.rs < 0.01)
        vd = Semi::pnjlim(vd, vdOld, nvt, m_vcrit);

    if (m_p.bv > 0) // Breakdown limiting
    {
        double nbvt = m_p.nbv * Semi::VT;
        if (vd < std::min(0.0, -m_p.bv + 10 * nbvt)) {
            double vtemp = -(vd + m_p.bv);
            double vtold = -(vdOld + m_p.bv);
            vtemp        = Semi::pnjlim(vtemp, vtold, nbvt, m_vcritBv);
            vd           = -(vtemp + m_p.bv);
        }
    }
    if (it > NEWTON_DAMP) // Newton not converging: damped Newton
        vd = vdOld + (0.5) * (vd - vdOld);
    m_vd = vd;

    // Step was limited: the node voltages may not change enough to call us
    // again, so schedule another Newton iteration
    m_limited = (std::fabs(vd - vdTarget) > VTOL);
    if (m_limited)
        Simulator::self()->addToNoLinList(this);

    if (std::fabs(vd - m_vdStamp) < VTOL)
        return; // Converged

    double id, gd;
    Semi::diodeJunction(m_p, vd, id, gd);
    double vlin = vd + m_p.rs * id; // Terminal voltage consistent with vd
    m_current   = id;

    if (!m_limited && (m_vdStamp < 1e8)) // Current already well predicted?
    {
        double iPred = m_iStamp + m_gStamp * (vlin - m_vlinStamp);
        if (std::fabs(id - iPred) < ITOL_ABS + ITOL_REL * std::fabs(id))
            return;
    }
    double g    = gd / (1 + gd * m_p.rs);
    m_vdStamp   = vd;
    m_vlinStamp = vlin;
    m_iStamp    = id;
    m_gStamp    = g;
    m_admit     = g;
    m_resist    = 1 / g;

    NLStamp<2> s;
    s.clear();
    s.v0[0]   = vlin;
    s.v0[1]   = 0;
    s.J[0][0] = g;
    s.J[0][1] = -g;
    s.J[1][1] = g;
    s.J[1][0] = -g;
    s.I[0]    = id;
    s.I[1]    = -id;
    s.stamp(&m_ePin[0]);
}

void eDiode::setThreshold(double threshold)
{
    if (threshold < 0.05)
        threshold = 0.05;
    m_threshold = threshold;
    if (m_model == "Custom")
        refreshModel();
}

double eDiode::res()
{
    return m_imped;
}

void eDiode::setRes(double resist)
{
    bool pauseSim = Simulator::self()->isRunning();
    if (pauseSim)
        Simulator::self()->pauseSim();

    if (resist < 0)
        resist = 0;
    m_imped = resist;
    if (m_model == "Custom")
        refreshModel();

    if (pauseSim)
        Simulator::self()->resumeSim();
}

void eDiode::setZenerV(double zenerV)
{
    if (zenerV > 0)
        m_zenerV = zenerV;
    else
        m_zenerV = 0;
    if (m_model == "Custom")
        refreshModel();
}

void eDiode::updateVI()
{
    m_current = 0;
    if (m_ePin[0]->isConnected() && m_ePin[1]->isConnected()) {
        double vt = m_ePin[0]->getVolt() - m_ePin[1]->getVolt();
        double vd = m_vd;
        m_current = Semi::diodeCurrent(m_p, vt, &vd);
    }
}
