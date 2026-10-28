/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "icmodules.h"

#include <QPainter>
#include <math.h>

#define LIB_ITEM(Cls, name, cat, type)                                         \
    LibraryItem *Cls::libraryItem()                                            \
    {                                                                          \
        return new LibraryItem(tr(name), tr(cat), "subc.png", type,            \
                               Cls::construct);                                \
    }

static const char *IcModule_properties[] = {
    QT_TRANSLATE_NOOP("App::Property", "CTR"),
    QT_TRANSLATE_NOOP("App::Property", "Active Low")
};

//-------------------------------------------------------------- ULN2003 ---
LIB_ITEM(Uln2003, "ULN2003", "Motors", "Uln2003")

Uln2003::Uln2003(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    Q_UNUSED(IcModule_properties);
    m_area = QRectF(-16, -36, 32, 72);
    Pin *c[7];
    for (int i = 0; i < 7; i++)
        m_in[i] = addPin(QString("%1B").arg(i + 1), 180,
                         QPoint(-20, -28 + 8 * i), QString("%1B").arg(i + 1));
    m_gnd = addPin("GND", 180, QPoint(-20, 28), "GND");
    for (int i = 0; i < 7; i++)
        c[i] = addPin(QString("%1C").arg(i + 1), 0, QPoint(20, -28 + 8 * i),
                      QString("%1C").arg(i + 1));
    Pin *com = addPin("COM", 0, QPoint(20, 28), "COM");
    for (Pin *p : m_pins) {
        p->setLength(3);
        p->setLabelColor(QColor(250, 250, 200));
    }
    for (int i = 0; i < 7; i++) {
        addRes(m_in[i], m_gnd, 3.6e3); // Input resistance
        m_sw[i] = addRes(c[i], m_gnd, 3);
        setResAdmit(m_sw[i], 1e-9);
        m_on[i] = false;
        addDiode(c[i], com, 0.7); // Flyback clamp
        watch(m_in[i]);
    }
    watch(m_gnd);
    setLabelPos(-16, -52, 0);
}
void Uln2003::evaluate()
{
    double g = volt(m_gnd);
    for (int i = 0; i < 7; i++) {
        bool on = m_gnd->isConnected() && m_in[i]->isConnected() &&
                  (m_in[i]->getVolt() - g) > 1.4;
        m_on[i] = on;
        setResAdmit(m_sw[i], on ? 1 / 3.0 : 1e-9);
    }
}
void Uln2003::paintPart(QPainter *p)
{
    p->drawRect(m_area);
    drawText(p, m_area, "ULN2003", Qt::white, 6);
}

//-------------------------------------------------------- Optocoupler ---
LIB_ITEM(Optocoupler, "PC817 Optocoupler", "Semiconductors", "Optocoupler")

Optocoupler::Optocoupler(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    m_area  = QRectF(-8, -8, 16, 16);
    m_ctr   = 100;
    Pin *a  = addPin("A", 180, QPoint(-12, -4), "A");
    Pin *k  = addPin("K", 180, QPoint(-12, 4), "K");
    m_c     = addPin("C", 0, QPoint(12, -4), "C");
    m_e     = addPin("E", 0, QPoint(12, 4), "E");
    for (Pin *p : m_pins) {
        p->setLength(3);
        p->setLabelColor(QColor(250, 250, 200));
    }
    m_led = addDiode(a, k, 1.2);
    m_ce  = addRes(m_c, m_e, 1e9);
    setResAdmit(m_ce, 1e-9);
    watch(a);
    watch(k);
    watch(m_c);
    watch(m_e);
    setLabelPos(-12, -22, 0);
}
void Optocoupler::evaluate()
{
    double ic0 = 0;
    if (m_res[m_led].ea->getEnode() && m_res[m_led].eb->getEnode()) {
        double i = m_res[m_led].r->current();
        if (i > 0)
            ic0 = m_ctr / 100.0 * i;
    }
    double vce = volt(m_c) - volt(m_e);
    double g   = 1e-9;
    if (ic0 > 1e-7 && vce > 1e-4)
        g = ic0 * (1 - exp(-vce / 0.25)) / vce; // Saturating current source
    if (g > 1 / 5.0)
        g = 1 / 5.0;
    if (g < 1e-9)
        g = 1e-9;
    double old = m_res[m_ce].r->admit();
    if (fabs(g - old) > 0.02 * old || (g <= 1e-9) != (old <= 1e-9))
        setResAdmit(m_ce, g);
}
void Optocoupler::paintPart(QPainter *p)
{
    p->drawRect(m_area);
    drawText(p, m_area, "PC817", Qt::white, 5);
}

//------------------------------------------------------------ RelayModule ---
LIB_ITEM(RelayModule, "Relay Module", "Modules", "RelayModule")

RelayModule::RelayModule(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    m_area       = QRectF(-32, -24, 64, 48);
    m_activeLow  = true;
    m_energized  = false;
    m_powered    = false;
    m_vcc        = addPin("VCC", 180, QPoint(-28, -8), "VCC");
    m_gnd        = addPin("GND", 180, QPoint(-28, 0), "GND");
    m_in         = addPin("IN", 180, QPoint(-28, 8), "IN");
    Pin *nc      = addPin("NC", 0, QPoint(28, -8), "NC");
    Pin *com     = addPin("COM", 0, QPoint(28, 0), "COM");
    Pin *no      = addPin("NO", 0, QPoint(28, 8), "NO");
    for (Pin *p : m_pins)
        p->setLength(3);
    m_coil = addRes(m_vcc, m_gnd, 1500);
    m_no   = addRes(com, no, 1);
    m_nc   = addRes(com, nc, 1);
    setSwitch(m_no, false);
    setSwitch(m_nc, true);
    watch(m_vcc);
    watch(m_gnd);
    watch(m_in);
    setLabelPos(-32, -40, 0);
}
void RelayModule::evaluate()
{
    m_powered = powered(m_vcc, m_gnd, 3.0);
    bool act  = false;
    if (m_powered && m_in->isConnected()) {
        double v = m_in->getVolt() - m_gnd->getVolt();
        double h = m_vcc->getVolt() - m_gnd->getVolt();
        act = m_activeLow ? (v < h * 0.3) : (v > h * 0.5);
    }
    m_energized = act;
    setResAdmit(m_coil, act ? 1 / 66.0 : 1 / 1500.0); // 70 mA coil + LED
    setSwitch(m_no, act);
    setSwitch(m_nc, !act);
}
void RelayModule::paintOver(QPainter *p)
{
    drawLed(p, QPointF(-22, 7), QColor(230, 40, 40), m_powered, 1.9);
    drawLed(p, QPointF(-22, 14), QColor(40, 220, 80), m_energized, 1.9);
}
void RelayModule::paintPart(QPainter *p)
{
    p->drawRoundedRect(m_area, 2, 2);
    drawText(p, m_area, "RELAY", Qt::white, 8);
    paintOver(p);
}

//----------------------------------------------------------- MosfetModule ---
LIB_ITEM(MosfetModule, "IRF520 MOSFET Module", "Modules", "MosfetModule")

MosfetModule::MosfetModule(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    m_area       = QRectF(-32, -28, 64, 56);
    m_vgs        = 0;
    Pin *vinp    = addPin("VINP", 180, QPoint(-24, -8), "VIN+");
    Pin *vinn    = addPin("VINN", 180, QPoint(-24, 8), "VIN-");
    Pin *vp      = addPin("VP", 0, QPoint(24, -8), "V+");
    Pin *vn      = addPin("VN", 0, QPoint(24, 8), "V-");
    m_sig        = addPin("SIG", 270, QPoint(-8, 24), "SIG");
    Pin *vcc     = addPin("VCC", 270, QPoint(0, 24), "VCC");
    m_gnd        = addPin("GND", 270, QPoint(8, 24), "GND");
    Q_UNUSED(vcc);
    for (Pin *p : m_pins)
        p->setLength(3);
    int l1 = addRes(vinp, vp, 1);
    int l2 = addRes(m_gnd, vinn, 1);
    setSwitch(l1, true);
    setSwitch(l2, true);
    addRes(m_sig, m_gnd, 10e3); // Gate pull-down
    m_sw = addRes(vn, vinn, 1);
    setResAdmit(m_sw, 1e-9);
    watch(m_sig);
    watch(m_gnd);
    setLabelPos(-32, -44, 0);
}
void MosfetModule::evaluate()
{
    m_vgs = m_sig->isConnected() ? m_sig->getVolt() - volt(m_gnd) : 0;
    double g = 1e-9;
    if (m_vgs > 2.0) { // Vgs(th) 2..4 V, Rds(on) 0.27 ohm at 10 V
        double r = 0.27 + 5.7 * exp(-(m_vgs - 2.0) / 1.2);
        g        = 1 / r;
    }
    double old = m_res[m_sw].r->admit();
    if (fabs(g - old) > 0.01 * old || (g <= 1e-9) != (old <= 1e-9))
        setResAdmit(m_sw, g);
}
void MosfetModule::paintOver(QPainter *p)
{
    drawLed(p, QPointF(-18, 12), QColor(230, 40, 40), m_vgs > 2.0, 2);
}
void MosfetModule::paintPart(QPainter *p)
{
    p->drawRoundedRect(m_area, 2, 2);
    drawText(p, m_area, "IRF520", Qt::white, 8);
    paintOver(p);
}

#include "moc_icmodules.cpp"
