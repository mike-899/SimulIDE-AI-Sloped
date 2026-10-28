/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "switchparts.h"

#include <QPainter>

#define LIB_ITEM(Cls, name, cat, type)                              \
    LibraryItem *Cls::libraryItem()                                 \
    {                                                               \
        return new LibraryItem(tr(name), tr(cat), "subc.png", type, \
                               Cls::construct);                     \
    }

static const char *SwitchPart_properties[] = {
    QT_TRANSLATE_NOOP("App::Property", "Pressed"),
    QT_TRANSLATE_NOOP("App::Property", "Position"),
    QT_TRANSLATE_NOOP("App::Property", "Magnet")
};

//----------------------------------------------------------- TactileButton ---
LIB_ITEM(TactileButton, "Tactile Button", "Switches", "TactileButton")

TactileButton::TactileButton(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    Q_UNUSED(SwitchPart_properties);
    m_area    = QRectF(-12, -12, 24, 24);
    m_pressed = false;
    Pin *p1   = addPin("P1", 180, QPoint(-12, -8), "");
    Pin *p2   = addPin("P2", 0, QPoint(12, -8), "");
    Pin *p3   = addPin("P3", 180, QPoint(-12, 8), "");
    Pin *p4   = addPin("P4", 0, QPoint(12, 8), "");
    for (Pin *p : m_pins)
        p->setLength(3);
    m_link1 = addRes(p1, p3, 1);
    m_link2 = addRes(p2, p4, 1);
    m_main  = addRes(p1, p2, 1);
    // Pressed: every left pin to every right pin (works with unused pins)
    m_cross[0] = m_main;
    m_cross[1] = addRes(p1, p4, 1);
    m_cross[2] = addRes(p3, p2, 1);
    m_cross[3] = addRes(p3, p4, 1);
    setSwitch(m_link1, true);
    setSwitch(m_link2, true);
    for (int i = 0; i < 4; i++)
        setSwitch(m_cross[i], false);
    m_zone = addZone(QRectF(-8, -8, 16, 16), "", false, false);
    setLabelPos(-12, -28, 0);
}
void TactileButton::zoneEvent(int, bool on)
{
    setPressed(on);
}
void TactileButton::setPressed(bool p)
{
    m_pressed = p;
    controlChanged();
}
void TactileButton::evaluate()
{
    setSwitch(m_link1, true);
    setSwitch(m_link2, true);
    for (int i = 0; i < 4; i++)
        setSwitch(m_cross[i], m_pressed);
}
void TactileButton::paintOver(QPainter *p)
{
    if (!m_pressed)
        return;
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    QRadialGradient g(QPointF(0, 0), 7);
    g.setColorAt(0, QColor(0, 0, 0, 40));
    g.setColorAt(1, QColor(0, 0, 0, 150));
    p->setBrush(g);
    p->drawEllipse(QPointF(0, 0), 6.5, 6.5);
    p->restore();
}
void TactileButton::paintPart(QPainter *p)
{
    p->drawRoundedRect(m_area, 2, 2);
    p->drawEllipse(QPointF(0, 0), 6, 6);
    paintOver(p);
}

//------------------------------------------------------------- SpdtSwitch ---
SpdtSwitch::SpdtSwitch(QObject *parent, QString type, QString id, bool toggle) :
    ModuleBase(parent, type, id)
{
    m_toggle = toggle;
    m_pos    = 0;
    m_area   = toggle ? QRectF(-12, -24, 24, 28) : QRectF(-12, -8, 24, 12);
    Pin *p1  = addPin("P1", 270, QPoint(-8, 12), "");
    Pin *pc  = addPin("C", 270, QPoint(0, 12), "");
    Pin *p3  = addPin("P3", 270, QPoint(8, 12), "");
    m_r1     = addRes(pc, p1, 1);
    m_r3     = addRes(pc, p3, 1);
    m_zone =
        addZone(toggle ? QRectF(-10, -24, 20, 24) : QRectF(-12, -8, 24, 12), "",
                false, false);
    setLabelPos(-12, toggle ? -40 : -24, 0);
    setSwitch(m_r1, true);
    setSwitch(m_r3, false);
}
void SpdtSwitch::zoneEvent(int, bool on)
{
    if (on)
        setPosition(m_pos ? 0 : 1);
}
void SpdtSwitch::setPosition(int p)
{
    m_pos = p ? 1 : 0;
    controlChanged();
}
void SpdtSwitch::evaluate()
{
    setSwitch(m_r1, m_pos == 0);
    setSwitch(m_r3, m_pos == 1);
}
void SpdtSwitch::paintOver(QPainter *p)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    if (m_toggle) { // Lever tilts toward the connected pin
        QPointF base(0, -10);
        QPointF tip(m_pos ? 7 : -7, -22);
        p->setPen(
            QPen(QColor(120, 124, 130), 3.4, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(base, tip);
        p->setPen(
            QPen(QColor(215, 218, 222), 1.6, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(base, tip);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(200, 204, 210));
        p->drawEllipse(tip, 2.6, 2.6);
    } else {
        double x = m_pos ? 0 : -8;
        p->setPen(QPen(QColor(40, 40, 44), 0.5));
        p->setBrush(QColor(225, 227, 230));
        p->drawRoundedRect(QRectF(x + 0.5, -6.5, 7, 7.5), 1.2, 1.2);
        p->setPen(QPen(QColor(120, 120, 124), 0.5));
        for (int i = 0; i < 3; i++)
            p->drawLine(QPointF(x + 2 + i * 2, -5),
                        QPointF(x + 2 + i * 2, -0.5));
    }
    p->restore();
}
void SpdtSwitch::paintPart(QPainter *p)
{
    p->drawRoundedRect(m_area, 2, 2);
    p->drawLine(0, 4, 0, 12);
    paintOver(p);
}

LIB_ITEM(SlideSwitch, "Slide Switch SPDT", "Switches", "SlideSwitch")
LIB_ITEM(ToggleSwitch, "Toggle Switch SPDT", "Switches", "ToggleSwitch")

//------------------------------------------------------------- ReedSwitch ---
LIB_ITEM(ReedSwitch, "Reed Switch", "Switches", "ReedSwitch")

ReedSwitch::ReedSwitch(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    m_area   = QRectF(-12, -5, 24, 10);
    m_magnet = false;
    Pin *a   = addPin("A", 180, QPoint(-16, 0), "");
    Pin *b   = addPin("B", 0, QPoint(16, 0), "");
    addRes(a, b, 1);
    setSwitch(0, false);
    m_zone = addZone(QRectF(-14, 10, 28, 8), "MAGNET", true);
    setLabelPos(-16, -22, 0);
}
void ReedSwitch::setMagnet(bool m)
{
    m_magnet = m;
    setZoneOn(m_zone, m);
    controlChanged();
}
void ReedSwitch::zoneEvent(int, bool on)
{
    setMagnet(on);
}
void ReedSwitch::evaluate()
{
    setSwitch(0, m_magnet);
}
void ReedSwitch::paintOver(QPainter *p)
{
    if (!m_magnet)
        return;
    p->save();
    p->setPen(QPen(QColor(30, 30, 30), 0.5));
    p->setBrush(QColor(210, 40, 40));
    p->drawRect(QRectF(-6, -9, 6, 4));
    p->setBrush(QColor(60, 90, 220));
    p->drawRect(QRectF(0, -9, 6, 4));
    p->restore();
}
void ReedSwitch::paintPart(QPainter *p)
{
    p->drawRoundedRect(m_area, 4, 4);
    paintOver(p);
}

#include "moc_switchparts.cpp"
