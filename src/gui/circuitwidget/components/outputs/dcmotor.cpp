/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "dcmotor.h"

#include "circuit.h"
#include "connector.h"
#include "e-node.h"
#include "partart.h"
#include "simulator.h"

#include <math.h>

static const char *DcMotor_properties[] = {
    QT_TRANSLATE_NOOP("App::Property", "Resistance"),
    QT_TRANSLATE_NOOP("App::Property", "Max Rpm"),
    QT_TRANSLATE_NOOP("App::Property", "Rated Volt"),
    QT_TRANSLATE_NOOP("App::Property", "Time Const"),
    QT_TRANSLATE_NOOP("App::Property", "Rpm")
};

Component *DcMotor::construct(QObject *parent, QString type, QString id)
{
    return new DcMotor(parent, type, id);
}

LibraryItem *DcMotor::libraryItem()
{
    return new LibraryItem(tr("DC Motor"), tr("Motors"), "steeper.png",
                           "DcMotor", DcMotor::construct);
}

DcMotor::DcMotor(QObject *parent, QString type, QString id) :
    Component(parent, type, id), eElement((id + "-eElement").toStdString()),
    m_winding((id + "-eWinding").toStdString()),
    m_pinA(180, QPoint(-40, -8), id + "-PinA", 0, this),
    m_pinB(180, QPoint(-40, 8), id + "-PinB", 1, this)
{
    Q_UNUSED(DcMotor_properties);

    m_area  = QRectF(-32, -20, 72, 51);
    m_color = QColor(50, 50, 70);

    m_pinA.setLabelText(" +");
    m_pinB.setLabelText(" -");

    m_resist    = 10;
    m_maxRpm    = 200;
    m_ratedVolt = 6;
    m_tau       = 0.1;
    m_tauF      = 0.5;
    m_angle     = 0;

    m_winding.setEpin(0, &m_pinA);
    m_winding.setEpin(1, &m_pinB);
    m_winding.setRes(m_resist);

    resetState();

    Simulator::self()->addToUpdateList(this);

    setLabelPos(-28, -36, 0);
    setShowId(true);
}
DcMotor::~DcMotor() {}

double DcMotor::ke()
{
    // Chosen so steady no-load speed at rated voltage is Max_Rpm
    return m_ratedVolt / (m_maxRpm * (1 + m_tau / m_tauF));
}

void DcMotor::setRes(double r)
{
    if (r < 1e-3)
        r = 1e-3;
    m_resist = r;
    m_winding.setResSafe(r);
}

void DcMotor::setMaxRpm(double rpm)
{
    if (rpm < 1)
        rpm = 1;
    m_maxRpm = rpm;
}

void DcMotor::setRatedVolt(double v)
{
    if (v < 0.1)
        v = 0.1;
    m_ratedVolt = v;
}

void DcMotor::setTimeConst(double t)
{
    if (t < 1e-3)
        t = 1e-3;
    m_tau = t;
}

void DcMotor::resetState()
{
    m_rpm       = 0;
    m_volt      = 0;
    m_lastTime  = 0;
    m_lastPaint = 0;
}

void DcMotor::stamp()
{
    eNode *enode = m_pinA.getEnode(); // Register for changes callback
    if (enode)
        enode->addToChangedFast(this);
    enode = m_pinB.getEnode();
    if (enode)
        enode->addToChangedFast(this);

    m_winding.stampCurrent(0); // No back-EMF at rest
}

void DcMotor::integrate()
{
    uint64_t now = Simulator::self()->circTime(); // ns
    if (now <= m_lastTime) {
        m_lastTime = now;
        return;
    }
    double dt  = (now - m_lastTime) * 1e-9;
    m_lastTime = now;

    double k    = 1 / m_tau + 1 / m_tauF;
    double wInf = (m_volt / (ke() * m_tau)) / k;
    m_rpm       = wInf + (m_rpm - wInf) * exp(-k * dt);
    if (fabs(m_rpm) < 1e-3)
        m_rpm = 0;
}

void DcMotor::setVChanged() // Terminal voltage changed
{
    integrate();
    if (m_pinA.isConnected() && m_pinB.isConnected())
        m_volt = m_pinA.getVolt() - m_pinB.getVolt();
    else
        m_volt = ke() * m_rpm; // Open: free running
}

void DcMotor::updateStep() // Circuit thread is idle here
{
    if (Simulator::self()->isRunning()) {
        integrate();
        if (m_pinA.isConnected() && m_pinB.isConnected()) {
            // Back-EMF as Norton current source in parallel with winding
            m_winding.stampCurrent(ke() * m_rpm / m_resist);
            m_volt = m_pinA.getVolt() - m_pinB.getVolt();
        }
    }
    uint64_t now = Simulator::self()->circTime();
    if (now > m_lastPaint) // Visual speed: 1 deg/s per rpm
        m_angle += m_rpm * (now - m_lastPaint) * 1e-9;
    m_lastPaint = now;
    m_angle     = fmod(m_angle, 360);
    update();
}

void DcMotor::remove()
{
    if (m_pinA.isConnected())
        m_pinA.connector()->remove();
    if (m_pinB.isConnected())
        m_pinB.connector()->remove();

    Simulator::self()->remFromUpdateList(this);

    Component::remove();
}

void DcMotor::paint(QPainter *p, const QStyleOptionGraphicsItem *option,
                    QWidget *widget)
{
    Component::paint(p, option, widget);

    QPointF hub(20, 0);
    if (!paintArt(p)) {
        p->setBrush(QColor(230, 190, 40));
        p->drawRoundedRect(QRectF(-4, -18, 42, 36), 3, 3);
        p->setBrush(QColor(180, 180, 185));
        p->drawRoundedRect(QRectF(-31, -12, 28, 24), 4, 4);
    }
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->translate(hub);

    // Wheel hub rotating with shaft (direction = sign of rpm)
    p->setPen(QPen(QColor(30, 30, 30), 1));
    p->setBrush(QColor(250, 250, 250));
    p->drawEllipse(QPointF(0, 0), 9, 9);
    p->rotate(m_angle);
    p->setBrush(QColor(220, 60, 50));
    for (int i = 0; i < 3; i++) {
        p->drawRect(QRectF(-1.2, -8.5, 2.4, 7));
        p->rotate(120);
    }
    p->setBrush(QColor(60, 60, 60));
    p->drawEllipse(QPointF(0, 0), 2.5, 2.5);
    p->restore();

    // RPM label
    p->save();
    QFont f = p->font();
    f.setPixelSize(7);
    f.setBold(true);
    p->setFont(f);
    QString txt = QString::number(qRound(m_rpm)) + " rpm";
    QRectF tr(-30, 21, 70, 9);
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(20, 20, 20, 170));
    p->drawRoundedRect(tr, 2, 2);
    p->setPen(m_rpm > 0.5    ? QColor(120, 240, 120)
              : m_rpm < -0.5 ? QColor(250, 170, 80)
                             : QColor(220, 220, 220));
    QString dir = m_rpm > 0.5    ? QString::fromUtf8("↻ ")
                  : m_rpm < -0.5 ? QString::fromUtf8("↺ ")
                                 : QString();
    p->drawText(tr, Qt::AlignCenter, dir + txt);
    p->restore();
}

#include "moc_dcmotor.cpp"
