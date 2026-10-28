/***************************************************************************
 *   Copyright (C) 2016 by santiago González                               *
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

#include "e-mosfet.h"

#include "e-nlstamp.h"
#include "e-node.h"
#include "simulator.h"

#include <cmath>

static const double VTOL = 1e-4; // Convergence tolerance [V]

enum { D = 0, S = 1, G = 2 }; // ePin order

eMosfet::eMosfet(std::string id) : eResistor(id)
{
    m_Pchannel  = false;
    m_depletion = false;
    m_model     = "Custom";

    m_gateV     = 0;
    m_resist    = 1;
    m_RDSon     = 1;
    m_threshold = 3;
    m_drainCurr = 0;

    m_ePin.resize(3);
    refreshModel();
    resetState();
}
eMosfet::~eMosfet() {}

// Model or parameter changed: reload it safely while the simulation runs and
// make sure the device is evaluated again
void eMosfet::refreshModel()
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

void eMosfet::loadModel()
{
    if ((m_model == "Custom") || !Semi::mosPreset(m_model, m_p))
        m_p = Semi::customMos(m_threshold, m_RDSon, m_Pchannel, m_depletion);
    else
        m_depletion = false;
    m_Pchannel = m_p.pch;

    m_pRev    = m_p;
    m_pRev.rs = m_p.rd;
    m_pRev.rd = m_p.rs;

    m_vcritB   = Semi::vcrit(Semi::VT, m_p.isb);
    m_vgsStamp = 1e9; // Force restamp
}

void eMosfet::setModelName(const QString &model)
{
    bool pauseSim = Simulator::self()->isRunning();
    if (pauseSim)
        Simulator::self()->pauseSim();

    m_model = model;
    refreshModel();

    if (pauseSim)
        Simulator::self()->resumeSim();
}

void eMosfet::stamp()
{
    if ((m_ePin[0]->isConnected()) && (m_ePin[1]->isConnected()) &&
        (m_ePin[2]->isConnected())) {
        for (int i = 0; i < 3; i++) {
            eNode *node = m_ePin[i]->getEnode();
            node->addToNoLinList(this);
            node->setSwitched(true);
        }
    }
    m_vgs      = 0;
    m_vds      = 0;
    m_vgsStamp = 1e9;
    setVChanged(); // Initial linearization
}

void eMosfet::resetState()
{
    m_gateV     = 0;
    m_drainCurr = 0;
    m_vgs       = 0;
    m_vds       = 0;
    m_idGuess   = 0;
    m_vgsStamp  = 1e9;
    m_vdsStamp  = 1e9;
}

void eMosfet::setVChanged()
{
    if (!m_ePin[D]->isConnected() || !m_ePin[S]->isConnected() ||
        !m_ePin[G]->isConnected())
        return;

    int it = Simulator::self()->nlIteration();
    if (it > NEWTON_MAX) // Not converging here: continue in the next step
    {
        Simulator::self()->addToNoLinNextStep(this);
        return;
    }

    double t  = m_Pchannel ? -1 : 1;
    double vd = m_ePin[D]->getVolt();
    double vs = m_ePin[S]->getVolt();
    double vg = m_ePin[G]->getVolt();

    double vgs  = t * (vg - vs);
    double vds  = t * (vd - vs);
    double vgd  = vgs - vds;
    double vgsT = vgs, vdsT = vds;

    // SPICE3 FET voltage limiting
    if (m_vds >= 0) {
        vgs = Semi::fetlim(vgs, m_vgs, m_p.vto);
        vds = vgs - vgd;
        vds = Semi::limvds(vds, m_vds);
    } else {
        vgd = Semi::fetlim(vgd, m_vgs - m_vds, m_p.vto);
        vds = vgs - vgd;
        vds = -Semi::limvds(-vds, -m_vds);
        vgs = vgd + vds;
    }
    if (-vds > m_vcritB) // Body diode forward: junction limiting
    {
        double vsd = Semi::pnjlim(-vds, -m_vds, Semi::VT, m_vcritB);
        if (vsd < -vds)
            vds = -vsd;
    }
    if (it > NEWTON_DAMP) // Newton not converging: damped Newton
    {
        double k = 0.5;
        vgs      = m_vgs + k * (vgs - m_vgs);
        vds      = m_vds + k * (vds - m_vds);
    }
    m_vgs   = vgs;
    m_vds   = vds;
    m_gateV = vgs - m_p.vto;

    if ((std::fabs(vgs - vgsT) > VTOL) || (std::fabs(vds - vdsT) > VTOL))
        Simulator::self()->addToNoLinList(this); // Limited: iterate again

    if ((std::fabs(vgs - m_vgsStamp) < VTOL) &&
        (std::fabs(vds - m_vdsStamp) < VTOL))
        return; // Converged
    m_vgsStamp = vgs;
    m_vdsStamp = vds;

    NLStamp<3> s;
    s.clear();
    s.v0[S] = vs;
    s.v0[G] = vs + t * vgs;
    s.v0[D] = vs + t * vds;

    Semi::MosOp op;
    if (vds >= 0) // Normal mode
    {
        Semi::mosEval(m_p, vgs, vds, m_idGuess, op);
        m_idGuess = op.id;
        s.J[D][G] = op.gm;
        s.J[D][D] = op.gds;
        s.J[D][S] = -(op.gm + op.gds);
        s.I[D]    = t * op.id;
    } else // Reverse mode: source and drain exchanged
    {
        Semi::mosEval(m_pRev, vgs - vds, -vds, m_idGuess, op);
        m_idGuess = op.id;
        s.J[D][G] = -op.gm;
        s.J[D][S] = -op.gds;
        s.J[D][D] = op.gm + op.gds;
        s.I[D]    = -t * op.id;
    }
    for (int j = 0; j < 3; j++)
        s.J[S][j] = -s.J[D][j];
    s.I[S] = -s.I[D];

    // Body diode (bulk tied to source): anode S, cathode D (N channel)
    Semi::DiodeParams body;
    body.is = m_p.isb;
    double ib, gb;
    Semi::diodeJunction(body, -vds, ib, gb);
    s.J[S][S] += gb;
    s.J[S][D] -= gb;
    s.J[D][D] += gb;
    s.J[D][S] -= gb;
    s.I[S] += t * ib;
    s.I[D] -= t * ib;

    s.addG(G, S, Semi::GMIN); // Gate leakage keeps the gate node connected
    s.addG(G, D, Semi::GMIN);

    m_drainCurr = s.I[D];
    s.stamp(&m_ePin[0]);
}

bool eMosfet::pChannel()
{
    return m_Pchannel;
}

void eMosfet::setPchannel(bool pc)
{
    if ((m_model != "Custom") && (pc != m_p.pch) && !m_p.complement.isEmpty())
        m_model = m_p.complement; // Same family, other polarity
    m_Pchannel = pc;
    refreshModel();
    if (m_Pchannel != pc) {
        m_model    = "Custom";
        m_Pchannel = pc;
        refreshModel();
    }
}

bool eMosfet::depletion()
{
    return m_depletion;
}

void eMosfet::setDepletion(bool dep)
{
    m_depletion = dep;
    if (dep)
        m_model = "Custom"; // No depletion presets
    refreshModel();
}

double eMosfet::RDSon()
{
    return m_RDSon;
}

void eMosfet::setRDSon(double rdson)
{
    if (rdson < cero_doub)
        rdson = cero_doub;
    if (rdson > 1000)
        rdson = 1000;
    m_RDSon = rdson;
    if (m_model == "Custom")
        refreshModel();
}

double eMosfet::threshold()
{
    return m_threshold;
}

void eMosfet::setThreshold(double th)
{
    m_threshold = th;
    if (m_model == "Custom")
        refreshModel();
}

ePin *eMosfet::getEpin(QString pinName)
{
    ePin *pin = 0l;
    if (pinName == "Dren")
        pin = m_ePin[0];
    else if (pinName == "Sour")
        pin = m_ePin[1];
    else if (pinName == "Gate")
        pin = m_ePin[2];
    return pin;
}

void eMosfet::initEpins()
{
    setNumEpins(3);
}
