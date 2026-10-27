/***************************************************************************
 *   Copyright (C) 2026 SimulIDE contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "spicenetlist.h"

#include "baseprocessor.h"
#include "clock-base.h"
#include "component.h"
#include "e-bjt.h"
#include "e-capacitor.h"
#include "e-diode.h"
#include "e-inductor.h"
#include "e-mosfet.h"
#include "e-node.h"
#include "e-source.h"
#include "mcucomponent.h"
#include "simulator.h"
#include "wavegen.h"

#include <cmath>

static QString num(double v)
{
    return QString::number(v, 'g', 9);
}

// SI prefix of a unit string like " mV", " kΩ"
static double unitMult(const QString &unit)
{
    QString u = unit.trimmed();
    if (u.size() < 2)
        return 1;
    switch (u[0].toLatin1()) {
        case 'p':
            return 1e-12;
        case 'n':
            return 1e-9;
        case 'u':
            return 1e-6;
        case 'm':
            return 1e-3;
        case 'k':
            return 1e3;
        case 'M':
            return 1e6;
        case 'G':
            return 1e9;
    }
    if (u[0] == QChar(0x00B5)) // micro sign
        return 1e-6;
    return 1;
}

static ePin *pinOf(eElement *el, int i) // Avoids getEpin() name hiding
{
    return (i < el->numEpins()) ? el->getEpin(i) : nullptr;
}

// Clock/WaveGen owning an eSource (its id is "<component id>-outnod-eSource")
static ClockBase *clockOf(eElement *src)
{
    QString id = QString::fromStdString(src->getId());
    if (!id.endsWith("-outnod-eSource"))
        return nullptr;
    QString compId = id.left(id.size() - 15);
    for (eElement *el : Simulator::self()->elementList()) {
        ClockBase *ck = dynamic_cast<ClockBase *>(el);
        if (ck && ck->objectName() == compId)
            return ck;
    }
    return nullptr;
}

QString SpiceNetlist::node(ePin *pin)
{
    if (!pin || !pin->isConnected() || !pin->getEnode())
        return QString("nc%1").arg(m_nc++); // Dangling terminal

    eNode *enode = pin->getEnode();
    QString name = m_nodeNames.value(enode);
    if (name.isEmpty()) {
        name = QString("n%1").arg(m_nodeNames.size() + 1);
        m_nodeNames.insert(enode, name);
        m_vecNodes.insert(name, enode);
    }
    return name;
}

QString SpiceNetlist::model(const QString &text, const QString &prefix)
{
    // text is a ".model NAME ..." line built with a placeholder name
    QString body = text.section(' ', 2); // Without ".model NAME"
    QString name = m_modelNames.value(body);
    if (name.isEmpty()) {
        name = QString("%1_%2").arg(prefix).arg(m_modelNames.size() + 1);
        m_modelNames.insert(body, name);
        m_models.append(".model " + name + " " + body);
    }
    return name;
}

bool SpiceNetlist::build(QString *error)
{
    m_nodeNames.clear();
    m_vecNodes.clear();
    m_modelNames.clear();
    m_ammeters.clear();
    m_elements.clear();
    m_models.clear();
    m_nc = 0;

    QStringList unsupported;

    if (BaseProcessor::self() || McuComponent::self())
        unsupported.append("microcontroller (MCUs cannot run inside ngspice)");

    int idx = 0;
    for (eElement *el : Simulator::self()->elementList()) {
        idx++;
        QString id   = QString::number(idx);
        Component *c = dynamic_cast<Component *>(el);

        if (eMosfet *m = dynamic_cast<eMosfet *>(el)) {
            ePin *d = pinOf(m, 0), *s = pinOf(m, 1), *g = pinOf(m, 2);
            const Semi::MosParams &p = m->params();
            QString mn               = model(Semi::spiceModel(p, "X"), "MOS");
            QString sn               = node(s);
            m_elements.append("M" + id + " " + node(d) + " " + node(g) + " " +
                              sn + " " + sn + " " + mn + " W=100u L=100u");
            continue;
        }
        if (eBJT *q = dynamic_cast<eBJT *>(el)) {
            ePin *cp = pinOf(q, 0), *e = pinOf(q, 1), *b = pinOf(q, 2);
            QString mn = model(Semi::spiceModel(q->params(), "X"), "BJT");
            m_elements.append("Q" + id + " " + node(cp) + " " + node(b) + " " +
                              node(e) + " " + mn);
            continue;
        }
        if (eDiode *dd = dynamic_cast<eDiode *>(el)) {
            QString mn = model(Semi::spiceModel(dd->params(), "X"), "D");
            m_elements.append("D" + id + " " + node(pinOf(dd, 0)) + " " +
                              node(pinOf(dd, 1)) + " " + mn);
            continue;
        }
        if (eCapacitor *cap = dynamic_cast<eCapacitor *>(el)) {
            m_elements.append("C" + id + " " + node(pinOf(cap, 0)) + " " +
                              node(pinOf(cap, 1)) + " " + num(cap->cap()));
            continue;
        }
        if (eInductor *ind = dynamic_cast<eInductor *>(el)) {
            m_elements.append("L" + id + " " + node(pinOf(ind, 0)) + " " +
                              node(pinOf(ind, 1)) + " " + num(ind->ind()));
            continue;
        }
        if (eResistor *r = dynamic_cast<eResistor *>(el)) {
            ePin *a = pinOf(r, 0), *b = pinOf(r, 1);
            if (!a || !b)
                continue;
            if (c && c->inherits("Amperimeter")) {
                QString vn = "VAM" + id;
                m_elements.append(vn + " " + node(a) + " " + node(b) + " 0");
                if (a->isConnected() && b->isConnected())
                    m_ammeters.append({ a->getEnode(), b->getEnode(),
                                        vn.toLower() + "#branch", r->res() });
                continue;
            }
            double g = r->admit();
            if (g < 1e-12) // Open switch contact or unconnected meter
                continue;
            m_elements.append("R" + id + " " + node(a) + " " + node(b) + " " +
                              num(1 / g));
            continue;
        }
        if (eSource *src = dynamic_cast<eSource *>(el)) {
            ePin *pin = src->getEpin();
            if (!pin || !pin->isConnected())
                continue;
            double imp  = std::max(src->imp(), 1e-4);
            double v    = src->out() ? src->voltHight() : src->voltLow();
            QString n   = node(pin);
            QString in  = "s" + id;
            QString val = "DC " + num(v);

            ClockBase *ck    = clockOf(src);
            Component *owner = ck;
            WaveGen *wg      = dynamic_cast<WaveGen *>(ck);
            if (ck && ck->running() && ck->freq() > 0) {
                double f = ck->freq(), T = 1 / f;
                double vpp = owner->property("Voltage").toDouble() *
                             unitMult(owner->property("Unit").toString());
                if (wg) {
                    double vb = wg->voltBase();
                    switch (wg->waveType()) {
                        case WaveGen::Sine:
                            val = QString("DC %1 SIN(%2 %3 %4)")
                                      .arg(num(vb + vpp / 2), num(vb + vpp / 2),
                                           num(vpp / 2), num(f));
                            break;
                        case WaveGen::Saw:
                            val = QString("PULSE(%1 %2 0 %3 1n 0 %4)")
                                      .arg(num(vb), num(vb + vpp),
                                           num(T - 2e-9), num(T));
                            break;
                        case WaveGen::Triangle:
                            val = QString("PULSE(%1 %2 0 %3 %3 0 %4)")
                                      .arg(num(vb), num(vb + vpp), num(T / 2),
                                           num(T));
                            break;
                        case WaveGen::Square:
                            val = QString("PULSE(%1 %2 0 1n 1n %3 %4)")
                                      .arg(num(vb), num(vb + vpp),
                                           num(T * wg->duty() / 100), num(T));
                            break;
                        default:
                            break;
                    }
                } else // Square clock 0..V, 50 % duty
                    val = QString("PULSE(0 %1 0 1n 1n %2 %3)")
                              .arg(num(vpp), num(T / 2), num(T));
            }
            m_elements.append("V" + id + " " + in + " 0 " + val);
            m_elements.append("RS" + id + " " + n + " " + in + " " + num(imp));
            continue;
        }
        // Components whose electrical behaviour is fully described by their
        // internal elements (sources, resistors) or that only observe
        if (c && (c->inherits("LogicInput") || c->inherits("Rail") ||
                  c->inherits("VoltSource") || c->inherits("Potentiometer") ||
                  c->inherits("Probe") || c->inherits("Oscope") ||
                  c->inherits("Frequencimeter") ||
                  (c->inherits("MechContact") && !c->inherits("RelayBase"))))
            continue;

        bool connected = false;
        for (int i = 0; i < el->numEpins(); i++) {
            ePin *p = el->getEpin(i);
            if (p && p->isConnected())
                connected = true;
        }
        if (connected) {
            QString what =
                c ? c->objectName() : QString::fromStdString(el->getId());
            if (!unsupported.contains(what))
                unsupported.append(what);
        }
    }
    if (!unsupported.isEmpty()) {
        if (error)
            *error = "Not supported by the ngspice engine: " +
                     unsupported.join(", ");
        return false;
    }
    if (m_elements.isEmpty()) {
        if (error)
            *error = "Empty circuit";
        return false;
    }
    return true;
}

QStringList SpiceNetlist::lines(double tstop, double maxStep,
                                const QHash<eNode *, double> *ic) const
{
    QStringList l;
    l << "* SimulIDE circuit exported for ngspice";
    l << m_elements;
    l << m_models;
    if (ic && !ic->isEmpty()) {
        QString s = ".ic";
        for (auto it = ic->constBegin(); it != ic->constEnd(); ++it) {
            QString n = m_nodeNames.value(it.key());
            if (!n.isEmpty())
                s += " v(" + n + ")=" + num(it.value());
        }
        l << s;
    }
    l << ".options rshunt=1e12 itl4=50 noacct";
    l << QString(".tran %1 %2 0 %1 uic").arg(num(maxStep), num(tstop));
    l << ".end";
    return l;
}
