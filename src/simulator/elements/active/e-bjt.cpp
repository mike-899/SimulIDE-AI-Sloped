/***************************************************************************
 *   Copyright (C) 2018 by santiago González                               *
 *   santigoro@gmail.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
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

#include "e-bjt.h"

#include "e-nlstamp.h"
#include "e-node.h"
#include "simulator.h"

#include <cmath>
#include <iostream>
#include <sstream>

static const double VTOL     = 5e-5;
static const double ITOL_ABS = 1e-10; // Current tolerance, see e-diode.cpp
static const double ITOL_REL = 1e-4;  // Junction voltage convergence tolerance

enum { C = 0, E = 1, B = 2 }; // ePin order

eBJT::eBJT(std::string id) : eResistor(id)
{
    m_ePin.resize(3);

    m_gain      = 100;
    m_BEthr     = 0.7;
    m_PNP       = false;
    m_BCdiodeOn = false;
    m_model     = "Custom";
    m_baseCurr  = 0;
    m_collCurr  = 0;

    refreshModel();
    resetState();
}
eBJT::~eBJT() {}

// Model or parameter changed: reload it safely while the simulation runs and
// make sure the device is evaluated again
void eBJT::refreshModel()
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

void eBJT::loadModel()
{
    if ((m_model == "Custom") || !Semi::bjtPreset(m_model, m_p))
        m_p = Semi::customBjt(m_gain, m_BEthr, m_PNP);
    m_PNP = m_p.pnp;

    m_vcritBE  = Semi::vcrit(m_p.nf * Semi::VT, m_p.is);
    m_vcritBC  = Semi::vcrit(m_p.nr * Semi::VT, m_p.is);
    m_vbeStamp = 1e9; // Force restamp
}

void eBJT::setModelName(const QString &model)
{
    bool pauseSim = Simulator::self()->isRunning();
    if (pauseSim)
        Simulator::self()->pauseSim();

    m_model = model;
    refreshModel();

    if (pauseSim)
        Simulator::self()->resumeSim();
}

void eBJT::setGain(double gain)
{
    m_gain = gain;
    if (m_model == "Custom")
        refreshModel();
}

double eBJT::BEthr()
{
    return m_BEthr;
}

void eBJT::setBEthr(double thr)
{
    if (thr == 0)
        thr = 0.7;
    m_BEthr = thr;
    if (m_model == "Custom")
        refreshModel();
}

void eBJT::setPnp(double pnp)
{
    bool p = (pnp != 0);
    if ((m_model != "Custom") && (p != m_p.pnp) && !m_p.complement.isEmpty())
        m_model = m_p.complement; // Same family, other polarity
    m_PNP = p;
    refreshModel();
    if (m_PNP != p) // Preset without complement: fall back to custom
    {
        m_model = "Custom";
        m_PNP   = p;
        refreshModel();
    }
}

void eBJT::stamp()
{
    for (int i = 0; i < 3; i++) {
        if (m_ePin[i]->isConnected()) {
            eNode *node = m_ePin[i]->getEnode();
            node->addToNoLinList(this);
            node->setSwitched(true);
        }
    }
    m_vbe      = 0;
    m_vbc      = 0;
    m_vbeStamp = 1e9;
    setVChanged(); // Initial linearization
}

void eBJT::resetState()
{
    m_baseCurr = 0;
    m_collCurr = 0;
    m_vbe      = 0;
    m_vbc      = 0;
    m_vbeStamp = 1e9;
    m_vbcStamp = 1e9;
}

void eBJT::setVChanged()
{
    if (!m_ePin[C]->isConnected() || !m_ePin[E]->isConnected() ||
        !m_ePin[B]->isConnected())
        return;

    int it = Simulator::self()->nlIteration();
    if (it > NEWTON_MAX) // Not converging here: continue in the next step
    {
        Simulator::self()->addToNoLinNextStep(this);
        return;
    }

    double t  = m_PNP ? -1 : 1;
    double vc = m_ePin[C]->getVolt();
    double ve = m_ePin[E]->getVolt();
    double vb = m_ePin[B]->getVolt();

    double vbe = t * (vb - ve);
    double vbc = t * (vb - vc);

    double vbeT = vbe, vbcT = vbc;
    vbe = Semi::pnjlim(vbe, m_vbe, m_p.nf * Semi::VT, m_vcritBE);
    vbc = Semi::pnjlim(vbc, m_vbc, m_p.nr * Semi::VT, m_vcritBC);

    if (it > NEWTON_DAMP) // Newton not converging (e.g. latch at a switching
                          // instant)
    {
        double k = 0.5; // Damped Newton
        vbe      = m_vbe + k * (vbe - m_vbe);
        vbc      = m_vbc + k * (vbc - m_vbc);
    }
    m_vbe = vbe;
    m_vbc = vbc;

    if ((std::fabs(vbe - vbeT) > VTOL) || (std::fabs(vbc - vbcT) > VTOL))
        Simulator::self()->addToNoLinList(this); // Limited: iterate again

    if ((std::fabs(vbe - m_vbeStamp) < VTOL) &&
        (std::fabs(vbc - m_vbcStamp) < VTOL))
        return; // Converged

    Semi::BjtOp op;
    Semi::bjtEval(m_p, vbe, vbc, op);

    bool limited =
        (std::fabs(vbe - vbeT) > VTOL) || (std::fabs(vbc - vbcT) > VTOL);
    if (!limited && (m_vbeStamp < 1e8)) // Currents already well predicted?
    {
        double dbe = vbe - m_vbeStamp, dbc = vbc - m_vbcStamp;
        double icP = m_opS.ic + m_opS.dicVbe * dbe + m_opS.dicVbc * dbc;
        double ibP = m_opS.ib + m_opS.dibVbe * dbe + m_opS.dibVbc * dbc;
        if ((std::fabs(op.ic - icP) < ITOL_ABS + ITOL_REL * std::fabs(op.ic)) &&
            (std::fabs(op.ib - ibP) < ITOL_ABS + ITOL_REL * std::fabs(op.ib)))
            return;
    }
    m_vbeStamp = vbe;
    m_vbcStamp = vbc;
    m_opS      = op;

    NLStamp<3> s;
    s.clear();
    s.v0[E] = ve; // Linearization point consistent with limited vbe, vbc
    s.v0[B] = ve + t * vbe;
    s.v0[C] = s.v0[B] - t * vbc;

    // Partial derivatives are polarity independent (t*t = 1)
    s.J[C][B] = op.dicVbe + op.dicVbc;
    s.J[C][E] = -op.dicVbe;
    s.J[C][C] = -op.dicVbc;
    s.J[B][B] = op.dibVbe + op.dibVbc;
    s.J[B][E] = -op.dibVbe;
    s.J[B][C] = -op.dibVbc;
    for (int j = 0; j < 3; j++)
        s.J[E][j] = -(s.J[C][j] + s.J[B][j]);

    s.I[C] = t * op.ic;
    s.I[B] = t * op.ib;
    s.I[E] = -(s.I[C] + s.I[B]);

    s.addG(C, E, Semi::GMIN); // Keeps the C-E pair always connected

    m_baseCurr = s.I[B];
    m_collCurr = s.I[C];

    s.stamp(&m_ePin[0]);
}

void eBJT::initEpins()
{
    std::stringstream sd;
    sd << m_elmId << "-collector";
    m_ePin[0] = new ePin(sd.str(), 0);

    std::stringstream ss;
    ss << m_elmId << "-emiter";
    m_ePin[1] = new ePin(ss.str(), 1);

    std::stringstream sg;
    sg << m_elmId << "-base";
    m_ePin[2] = new ePin(sg.str(), 2);
}

ePin *eBJT::getEpin(QString pinName)
{
    ePin *pin = 0l;
    if (pinName == "collector")
        pin = m_ePin[0];
    else if (pinName == "emiter")
        pin = m_ePin[1];
    else if (pinName == "base")
        pin = m_ePin[2];
    return pin;
}
