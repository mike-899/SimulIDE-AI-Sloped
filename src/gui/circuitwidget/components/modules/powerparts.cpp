/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "powerparts.h"

#include "simulator.h"

#include <QPainter>
#include <math.h>

#define LIB_ITEM(Cls, name, cat, type)                              \
    LibraryItem *Cls::libraryItem()                                 \
    {                                                               \
        return new LibraryItem(tr(name), tr(cat), "subc.png", type, \
                               Cls::construct);                     \
    }

static const char *Power_properties[] = {
    QT_TRANSLATE_NOOP("App::Property", "Voltage"),
    QT_TRANSLATE_NOOP("App::Property", "Internal Res"),
    QT_TRANSLATE_NOOP("App::Property", "Rating"),
    QT_TRANSLATE_NOOP("App::Property", "Blown")
};

Battery::Battery(QObject *parent, QString type, QString id, int kind) :
    ModuleBase(parent, type, id)
{
    Q_UNUSED(Power_properties);
    m_kind = kind;
    Pin *pos, *neg;
    switch (kind) {
        case 0: // PP3
            m_area = QRectF(-14, -24, 28, 48);
            m_volt = 9;
            m_ri   = 1.5;
            pos    = addPin("PLUS", 90, QPoint(-8, -24), "+");
            neg    = addPin("MINUS", 90, QPoint(8, -24), "-");
            break;
        case 1: // 2xAA holder with flying leads
        case 2: // 4xAA
            m_area =
                QRectF(-32, kind == 1 ? -12 : -20, 56, kind == 1 ? 24 : 40);
            m_volt = kind == 1 ? 3 : 6;
            m_ri   = kind == 1 ? 0.3 : 0.6;
            pos    = addPin("PLUS", 0, QPoint(40, -4), "+");
            neg    = addPin("MINUS", 0, QPoint(40, 4), "-");
            break;
        default: // CR2032
            m_area = QRectF(-16, -16, 32, 32);
            m_volt = 3;
            m_ri   = 15;
            neg    = addPin("MINUS", 180, QPoint(-16, 0), "-");
            pos    = addPin("PLUS", 0, QPoint(16, 0), "+");
    }
    for (Pin *p : m_pins) {
        p->setLength(3);
        p->setLabelColor(QColor(40, 40, 40));
    }
    m_cell = addCell(pos, neg, m_volt, m_ri);
    setLabelPos(m_area.x(), m_area.y() - 14, 0);
}
void Battery::setVoltage(double v)
{
    m_volt = v;
    setCell(m_cell, m_volt, m_ri);
}
void Battery::setIntRes(double r)
{
    m_ri = r < 1e-3 ? 1e-3 : r;
    setCell(m_cell, m_volt, m_ri);
}
void Battery::paintPart(QPainter *p)
{
    p->drawRoundedRect(m_area, 3, 3);
    drawText(p, m_area, QString::number(m_volt, 'g', 3) + "V", Qt::white, 8);
}

LIB_ITEM(Battery9V, "Battery 9V", "Power", "Battery9V")
LIB_ITEM(Battery2AA, "Battery 2xAA", "Power", "Battery2AA")
LIB_ITEM(Battery4AA, "Battery 4xAA", "Power", "Battery4AA")
LIB_ITEM(CoinCell, "Coin Cell CR2032", "Power", "CoinCell")

//------------------------------------------------------------------- Fuse ---
LIB_ITEM(Fuse, "Fuse", "Power", "Fuse")

Fuse::Fuse(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    m_area   = QRectF(-12, -5, 24, 10);
    m_rating = 1.0;
    m_heat   = 0;
    m_blown  = false;
    m_lastT  = 0;
    Pin *a   = addPin("A", 180, QPoint(-16, 0), "");
    Pin *b   = addPin("B", 0, QPoint(16, 0), "");
    addRes(a, b, 0.05);
    m_zone = addZone(QRectF(-14, 10, 28, 8), "RESET", false);
    setLabelPos(-16, -22, 0);
}
void Fuse::zoneEvent(int, bool on)
{
    if (on)
        setBlown(false);
}
void Fuse::setBlown(bool b)
{
    m_blown = b;
    m_heat  = 0;
    controlChanged();
}
void Fuse::resetState()
{
    m_blown = false;
    m_heat  = 0;
    m_lastT = 0;
    ModuleBase::resetState();
}
void Fuse::evaluate()
{
    setResAdmit(0, m_blown ? 1e-9 : 20.0); // 50 mOhm
}
// Thermal model: heat += (I^2 - Irated^2) * dt, blows at 0.3 s * Irated^2
// (2x rating blows in 0.1 s, 1.5x in 0.24 s, 1.1x in 1.4 s)
void Fuse::updateStep()
{
    uint64_t t = Simulator::self()->circTime();
    double dt  = (t > m_lastT) ? (t - m_lastT) * 1e-9 : 0;
    m_lastT    = t;
    if (!m_blown && Simulator::self()->isRunning()) {
        double i  = fabs(resCurrent(0));
        double i2 = m_rating * m_rating;
        m_heat += (i * i - i2) * dt;
        if (m_heat < 0)
            m_heat = 0;
        if (m_heat > 0.3 * i2) {
            m_blown = true;
            controlChanged();
        }
    }
    ModuleBase::updateStep();
}
void Fuse::paintOver(QPainter *p)
{
    drawText(p, QRectF(-20, -17, 40, 8),
             QString::number(m_rating, 'g', 3) + "A", QColor(60, 60, 60), 5);
    if (m_blown) { // Burnt filament
        p->save();
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(20, 20, 20, 190));
        p->drawRoundedRect(QRectF(-8, -3.8, 16, 7.6), 2, 2);
        p->restore();
    }
}
void Fuse::paintPart(QPainter *p)
{
    p->drawRect(m_area);
    if (!m_blown)
        p->drawLine(-8, 0, 8, 0);
    paintOver(p);
}

#include "moc_powerparts.cpp"
