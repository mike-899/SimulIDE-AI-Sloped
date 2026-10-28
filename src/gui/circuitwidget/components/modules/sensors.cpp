/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "sensors.h"

#include "partart.h"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QSlider>
#include <math.h>

#define LIB_ITEM(Cls, name, cat, type)                                         \
    LibraryItem *Cls::libraryItem()                                            \
    {                                                                          \
        return new LibraryItem(tr(name), tr(cat), "subc.png", type,            \
                               Cls::construct);                                \
    }

static const char *Sensor_properties[] = {
    QT_TRANSLATE_NOOP("App::Property", "Lux"),
    QT_TRANSLATE_NOOP("App::Property", "R 10lux"),
    QT_TRANSLATE_NOOP("App::Property", "Gamma"),
    QT_TRANSLATE_NOOP("App::Property", "Temperature"),
    QT_TRANSLATE_NOOP("App::Property", "R25"),
    QT_TRANSLATE_NOOP("App::Property", "Beta"),
    QT_TRANSLATE_NOOP("App::Property", "Resistance"),
    QT_TRANSLATE_NOOP("App::Property", "Position"),
    QT_TRANSLATE_NOOP("App::Property", "Motion"),
    QT_TRANSLATE_NOOP("App::Property", "Hold Time"),
    QT_TRANSLATE_NOOP("App::Property", "Out Volt"),
    QT_TRANSLATE_NOOP("App::Property", "Obstacle"),
    QT_TRANSLATE_NOOP("App::Property", "Level"),
    QT_TRANSLATE_NOOP("App::Property", "Threshold"),
    QT_TRANSLATE_NOOP("App::Property", "Pressed")
};

static QString fmtR(double r)
{
    if (r >= 1e6)
        return QString::number(r / 1e6, 'g', 3) + "M";
    if (r >= 1e3)
        return QString::number(r / 1e3, 'g', 3) + "k";
    return QString::number(r, 'g', 3);
}

//------------------------------------------------------------------- LDR ---
LIB_ITEM(Ldr, "LDR", "Sensors", "Ldr")

Ldr::Ldr(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    Q_UNUSED(Sensor_properties);
    m_area  = QRectF(-10, -10, 20, 20);
    m_lux   = 100;
    m_r10   = 20e3;
    m_gamma = 0.7;
    Pin *a  = addPin("A", 180, QPoint(-16, 0), "");
    Pin *b  = addPin("B", 0, QPoint(16, 0), "");
    addRes(a, b, 1e3);
    m_slider = addSlider(QRect(-14, 12, 28, 8), 0, 100, 50);
    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        m_lux = pow(10, v / 25.0 - 1);
        controlChanged();
    });
    setLabelPos(-16, -34, 0);
    setSliderValue(m_slider, 50);
    m_lux = pow(10, 50 / 25.0 - 1);
}
void Ldr::setLux(double l)
{
    m_lux = qBound(0.1, l, 1000.0);
    setSliderValue(m_slider, qRound(25 * (log10(m_lux) + 1)));
    controlChanged();
}
void Ldr::setR10(double r)
{
    m_r10 = r < 1 ? 1 : r;
    controlChanged();
}
void Ldr::setGamma(double g)
{
    m_gamma = qBound(0.1, g, 2.0);
    controlChanged();
}
void Ldr::evaluate()
{
    double r = m_r10 * pow(10.0 / m_lux, m_gamma);
    setResValue(0, qBound(100.0, r, 5e6));
}
void Ldr::paintOver(QPainter *p)
{
    drawText(p, QRectF(-20, -22, 40, 10),
             QString::number(m_lux, 'g', 3) + " lx / " +
                 fmtR(m_res[0].r->res()),
             QColor(60, 60, 60), 5);
}
void Ldr::paintPart(QPainter *p)
{
    p->drawEllipse(QPointF(0, 0), 9, 9);
    p->drawLine(-3, -4, 3, 0);
    paintOver(p);
}

//------------------------------------------------------------ Thermistor ---
LIB_ITEM(Thermistor, "NTC Thermistor", "Sensors", "Thermistor")

Thermistor::Thermistor(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    m_area  = QRectF(-10, -8, 20, 16);
    m_temp  = 25;
    m_r25   = 10e3;
    m_beta  = 3950;
    Pin *a  = addPin("A", 180, QPoint(-16, 0), "");
    Pin *b  = addPin("B", 0, QPoint(16, 0), "");
    addRes(a, b, 10e3);
    m_slider = addSlider(QRect(-14, 12, 28, 8), -40, 125, 25);
    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        m_temp = v;
        controlChanged();
    });
    setLabelPos(-16, -34, 0);
}
void Thermistor::setTemp(double t)
{
    m_temp = qBound(-40.0, t, 125.0);
    setSliderValue(m_slider, qRound(m_temp));
    controlChanged();
}
void Thermistor::setR25(double r)
{
    m_r25 = r < 1 ? 1 : r;
    controlChanged();
}
void Thermistor::setBeta(double b)
{
    m_beta = qBound(500.0, b, 10000.0);
    controlChanged();
}
void Thermistor::evaluate()
{
    double r = m_r25 * exp(m_beta * (1 / (m_temp + 273.15) - 1 / 298.15));
    setResValue(0, qBound(10.0, r, 50e6));
}
void Thermistor::paintOver(QPainter *p)
{
    drawText(p, QRectF(-24, -22, 48, 10),
             QString::number(m_temp, 'g', 3) + QString::fromUtf8("°C / ") +
                 fmtR(m_res[0].r->res()),
             QColor(60, 60, 60), 5);
}
void Thermistor::paintPart(QPainter *p)
{
    p->drawEllipse(QPointF(0, 0), 7, 6);
    paintOver(p);
}

//---------------------------------------------------------------- SlidePot ---
LIB_ITEM(SlidePot, "Slide Potentiometer", "Sensors", "SlidePot")

SlidePot::SlidePot(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    m_area = QRectF(-12, -6, 24, 12);
    m_res  = 10e3;
    m_pos  = 50;
    Pin *a = addPin("A", 180, QPoint(-16, 0), "");
    Pin *w = addPin("W", 270, QPoint(0, 16), "");
    Pin *b = addPin("B", 0, QPoint(16, 0), "");
    m_pot  = addPot(a, w, b, m_res);
    m_slider = addSlider(QRect(-11, -3, 22, 6), 0, 100, 50);
    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        m_pos = v;
        controlChanged();
    });
    setLabelPos(-16, -30, 0);
}
void SlidePot::setRes(double r)
{
    m_res = r < 1 ? 1 : r;
    controlChanged();
}
void SlidePot::setPos(int p)
{
    m_pos = qBound(0, p, 100);
    setSliderValue(m_slider, m_pos);
    controlChanged();
}
void SlidePot::evaluate()
{
    setPot(m_pot, m_pos / 100.0, m_res);
}
void SlidePot::paintPart(QPainter *p)
{
    p->drawRect(m_area);
    p->drawLine(0, 6, 0, 8);
}

//-------------------------------------------------------------------- PIR ---
LIB_ITEM(PirSensor, "PIR Motion Sensor", "Sensors", "PirSensor")

PirSensor::PirSensor(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    m_area    = QRectF(-24, -28, 48, 56);
    m_motion  = false;
    m_active  = false;
    m_powered = false;
    m_timing  = false;
    m_hold    = 2;
    m_outV    = 3.3;
    m_vcc     = addPin("VCC", 270, QPoint(-8, 24), "VCC");
    Pin *out  = addPin("OUT", 270, QPoint(0, 24), "OUT");
    m_gnd     = addPin("GND", 270, QPoint(8, 24), "GND");
    for (Pin *p : m_pins)
        p->setLength(3);
    m_out = addOut(out, 100);
    watch(m_vcc);
    watch(m_gnd);
    m_zone = addZone(QRectF(-16, 32, 32, 9), "MOTION", true);
    setLabelPos(-24, -44, 0);
}
void PirSensor::setMotion(bool m)
{
    if (m && !m_motion) {
        m_timer.restart();
        m_timing = true;
    }
    if (!m && m_motion) {
        m_timer.restart();
        m_timing = true;
    }
    m_motion = m;
    setZoneOn(m_zone, m);
    controlChanged();
}
void PirSensor::zoneEvent(int, bool on)
{
    setMotion(on);
}
void PirSensor::updateStep()
{
    bool act = m_motion ||
               (m_timing && m_timer.elapsed() < m_hold * 1000);
    if (!act)
        m_timing = false;
    if (act != m_active) {
        m_active = act;
        controlChanged();
    }
    ModuleBase::updateStep();
}
void PirSensor::evaluate()
{
    m_powered = powered(m_vcc, m_gnd, 3.0);
    setOutVolt(m_out, (m_powered && m_active) ? m_outV : 0);
}
void PirSensor::paintOver(QPainter *p)
{
    drawLed(p, QPointF(18, 6), QColor(255, 60, 40), m_powered && m_active, 2);
}
void PirSensor::paintPart(QPainter *p)
{
    p->drawRoundedRect(m_area, 2, 2);
    p->drawEllipse(QPointF(0, -6), 14, 14);
    drawText(p, QRectF(-24, 8, 48, 10), "PIR", Qt::white, 6);
    paintOver(p);
}

//--------------------------------------------------------------- IrObstacle ---
LIB_ITEM(IrObstacle, "IR Obstacle Sensor", "Sensors", "IrObstacle")

IrObstacle::IrObstacle(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    m_area    = QRectF(-20, -24, 40, 48);
    m_obst    = false;
    m_powered = false;
    m_out     = addPin("OUT", 270, QPoint(-8, 20), "OUT");
    m_gnd     = addPin("GND", 270, QPoint(0, 20), "GND");
    m_vcc     = addPin("VCC", 270, QPoint(8, 20), "VCC");
    for (Pin *p : m_pins)
        p->setLength(3);
    m_o = addOut(m_out, 100);
    watch(m_vcc);
    watch(m_gnd);
    m_zone = addZone(QRectF(-18, 28, 36, 9), "OBSTACLE", true);
    setLabelPos(-20, -40, 0);
}
void IrObstacle::setObstacle(bool o)
{
    m_obst = o;
    setZoneOn(m_zone, o);
    controlChanged();
}
void IrObstacle::zoneEvent(int, bool on)
{
    setObstacle(on);
}
void IrObstacle::evaluate()
{
    m_powered = powered(m_vcc, m_gnd, 2.5);
    double v  = m_powered ? m_vcc->getVolt() - m_gnd->getVolt() : 0;
    setOutVolt(m_o, m_obst ? 0 : v);
}
void IrObstacle::paintOver(QPainter *p)
{
    drawLed(p, QPointF(-13, 9), QColor(230, 40, 40), m_powered, 1.8);
    drawLed(p, QPointF(13, 9), QColor(40, 220, 80), m_powered && m_obst, 1.8);
}
void IrObstacle::paintPart(QPainter *p)
{
    p->drawRoundedRect(m_area, 2, 2);
    drawText(p, m_area, "IR", Qt::white, 8);
    paintOver(p);
}

//----------------------------------------------------------- AnalogSensor ---
AnalogSensor::AnalogSensor(QObject *parent, QString type, QString id,
                           bool soil) : ModuleBase(parent, type, id)
{
    m_soil    = soil;
    m_powered = false;
    m_level   = soil ? 30 : 20;
    m_thr     = 50;
    if (soil) {
        m_area = QRectF(-24, -36, 48, 72);
        m_vcc  = addPin("VCC", 270, QPoint(-12, 30), "VCC");
        m_gnd  = addPin("GND", 270, QPoint(-4, 30), "GND");
        m_ao   = addPin("AO", 270, QPoint(4, 30), "AO");
        m_do   = addPin("DO", 270, QPoint(12, 30), "DO");
    } else {
        m_area = QRectF(-24, -28, 48, 56);
        m_ao   = addPin("AO", 270, QPoint(-12, 24), "AO");
        m_gnd  = addPin("GND", 270, QPoint(-4, 24), "G");
        m_vcc  = addPin("VCC", 270, QPoint(4, 24), "+");
        m_do   = addPin("DO", 270, QPoint(12, 24), "DO");
    }
    for (Pin *p : m_pins)
        p->setLength(3);
    m_oa = addOut(m_ao, 100);
    m_od = addOut(m_do, 100);
    watch(m_vcc);
    watch(m_gnd);
    int sy = soil ? 40 : 32;
    m_slider = addSlider(QRect(-20, sy, 40, 8), 0, 100, m_level);
    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        m_level = v;
        controlChanged();
    });
    setLabelPos(-24, soil ? -52 : -44, 0);
}
void AnalogSensor::setLevel(int l)
{
    m_level = qBound(0, l, 100);
    setSliderValue(m_slider, m_level);
    controlChanged();
}
void AnalogSensor::setThreshold(int t)
{
    m_thr = qBound(0, t, 100);
    controlChanged();
}
// Soil: AO falls with moisture (resistive probe), DO low when wet enough.
// Sound: AO rises with the sound level, DO high above the threshold.
void AnalogSensor::evaluate()
{
    m_powered = powered(m_vcc, m_gnd, 2.5);
    double v  = m_powered ? m_vcc->getVolt() - m_gnd->getVolt() : 0;
    double ao, d;
    if (m_soil) {
        ao = v * (1 - 0.7 * m_level / 100.0);
        d  = (m_level >= m_thr) ? 0 : v;
    } else {
        ao = v * (0.2 + 0.6 * m_level / 100.0);
        d  = (m_level >= m_thr) ? v : 0;
    }
    setOutVolt(m_oa, ao);
    setOutVolt(m_od, d);
}
void AnalogSensor::paintOver(QPainter *p)
{
    bool on = m_level >= m_thr;
    if (m_soil) {
        drawLed(p, QPointF(-17, 16), QColor(230, 40, 40), m_powered, 1.6);
        drawLed(p, QPointF(-17, 22), QColor(40, 220, 80), m_powered && on, 1.6);
    } else {
        drawLed(p, QPointF(-18, 5), QColor(230, 40, 40), m_powered, 1.7);
        drawLed(p, QPointF(18, 5), QColor(40, 220, 80), m_powered && on, 1.7);
    }
}
void AnalogSensor::paintPart(QPainter *p)
{
    p->drawRoundedRect(m_area, 2, 2);
    drawText(p, m_area, m_soil ? "SOIL" : "SOUND", Qt::white, 7);
    paintOver(p);
}

LIB_ITEM(SoilMoisture, "Soil Moisture Sensor", "Sensors", "SoilMoisture")
LIB_ITEM(SoundSensor, "Sound Sensor", "Sensors", "SoundSensor")

//-------------------------------------------------------------- Joystick ---
LIB_ITEM(Joystick, "Joystick Module", "Sensors", "Joystick")

Joystick::Joystick(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    m_area    = QRectF(-28, -28, 56, 56);
    m_x = m_y = 50;
    m_pressed = false;
    m_drag    = false;
    Pin *gnd  = addPin("GND", 270, QPoint(-16, 24), "GND");
    Pin *vcc  = addPin("VCC", 270, QPoint(-8, 24), "+5V");
    Pin *vrx  = addPin("VRX", 270, QPoint(0, 24), "VRx");
    Pin *vry  = addPin("VRY", 270, QPoint(8, 24), "VRy");
    Pin *sw   = addPin("SW", 270, QPoint(16, 24), "SW");
    for (Pin *p : m_pins)
        p->setLength(3);
    m_potX = addPot(gnd, vrx, vcc, 10e3);
    m_potY = addPot(gnd, vry, vcc, 10e3);
    m_sw   = addRes(sw, gnd, 1);
    setSwitch(m_sw, false);
    m_zone = addZone(QRectF(-14, 32, 28, 9), "PRESS", false);
    setLabelPos(-28, -44, 0);
}
void Joystick::zoneEvent(int, bool on)
{
    setPressed(on);
}
void Joystick::setX(int v)
{
    m_x = qBound(0, v, 100);
    controlChanged();
}
void Joystick::setY(int v)
{
    m_y = qBound(0, v, 100);
    controlChanged();
}
void Joystick::setPressed(bool p)
{
    m_pressed = p;
    controlChanged();
}
void Joystick::evaluate()
{
    setPot(m_potX, m_x / 100.0);
    setPot(m_potY, m_y / 100.0);
    setSwitch(m_sw, m_pressed);
}
void Joystick::drag(QPointF pos)
{
    QPointF d = pos - QPointF(0, -4);
    double l  = sqrt(d.x() * d.x() + d.y() * d.y());
    if (l > 14)
        d *= 14 / l;
    setX(qRound(50 + d.x() / 14 * 50));
    setY(qRound(50 + d.y() / 14 * 50)); // Down = high
}
void Joystick::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    QPointF d = e->pos() - QPointF(0, -4);
    if (e->button() == Qt::LeftButton && (d.x() * d.x() + d.y() * d.y()) < 144) {
        m_drag = true;
        drag(e->pos());
        e->accept();
        return;
    }
    ModuleBase::mousePressEvent(e);
}
void Joystick::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    if (m_drag) {
        drag(e->pos());
        return;
    }
    Component::mouseMoveEvent(e);
}
void Joystick::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    if (m_drag) {
        m_drag = false;
        setX(50);
        setY(50);
        return;
    }
    ModuleBase::mouseReleaseEvent(e);
}
void Joystick::paintOver(QPainter *p)
{
    // Stick cap follows the position
    QPointF c(0 + (m_x - 50) * 0.14, -4 + (m_y - 50) * 0.14);
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(QPen(QColor(10, 10, 10), 0.8));
    QRadialGradient g(c - QPointF(2, 2), 9);
    g.setColorAt(0, QColor(120, 120, 128));
    g.setColorAt(1, QColor(25, 25, 28));
    p->setBrush(m_pressed ? QBrush(QColor(60, 60, 70)) : QBrush(g));
    p->drawEllipse(c, m_pressed ? 7.5 : 8.5, m_pressed ? 7.5 : 8.5);
    p->restore();
}
void Joystick::paintPart(QPainter *p)
{
    p->drawRoundedRect(m_area, 2, 2);
    p->drawEllipse(QPointF(0, -4), 14, 14);
    paintOver(p);
}

//---------------------------------------------------------- RotaryEncoder ---
LIB_ITEM(RotaryEncoder, "Rotary Encoder", "Sensors", "RotaryEncoder")

static const int grayA[4] = { 1, 0, 0, 1 }; // CW: A leads B
static const int grayB[4] = { 1, 1, 0, 0 };

RotaryEncoder::RotaryEncoder(QObject *parent, QString type, QString id) :
    ModuleBase(parent, type, id)
{
    m_area  = QRectF(-28, -24, 56, 48);
    m_pos   = 0;
    m_phase = 0;
    m_swDown = false;
    Pin *clk = addPin("CLK", 270, QPoint(-16, 20), "CLK");
    Pin *dt  = addPin("DT", 270, QPoint(-8, 20), "DT");
    Pin *sw  = addPin("SW", 270, QPoint(0, 20), "SW");
    Pin *vcc = addPin("VCC", 270, QPoint(8, 20), "+");
    Pin *gnd = addPin("GND", 270, QPoint(16, 20), "GND");
    for (Pin *p : m_pins)
        p->setLength(3);
    addRes(vcc, clk, 10e3);
    addRes(vcc, dt, 10e3);
    addRes(vcc, sw, 10e3);
    m_clk = addRes(clk, gnd, 1);
    m_dt  = addRes(dt, gnd, 1);
    m_sw  = addRes(sw, gnd, 1);
    setSwitch(m_clk, false);
    setSwitch(m_dt, false);
    setSwitch(m_sw, false);

    m_zCcw = addZone(QRectF(-26, 26, 16, 9), "CCW", false);
    m_zCw  = addZone(QRectF(-8, 26, 16, 9), "CW", false);
    m_zSw  = addZone(QRectF(10, 26, 16, 9), "SW", false);
    setLabelPos(-28, -40, 0);
}
void RotaryEncoder::zoneEvent(int i, bool on)
{
    if (i == m_zSw) {
        m_swDown = on;
        controlChanged();
    } else if (on)
        rotate(i == m_zCw ? 1 : -1);
}
void RotaryEncoder::rotate(int dir) // One detent = a full Gray code cycle
{
    for (int k = 1; k <= 4; k++)
        m_queue.append(dir > 0 ? k % 4 : (4 - k) % 4);
    m_pos += dir;
    m_changed = true;
}
void RotaryEncoder::setPosition(int p)
{
    int n = 0;
    while (m_pos != p && n++ < 2000)
        rotate(p > m_pos ? 1 : -1);
}
void RotaryEncoder::updateStep()
{
    if (!m_queue.isEmpty() &&
        (!m_timer.isValid() || m_timer.elapsed() >= 15)) {
        m_phase = m_queue.takeFirst();
        m_timer.restart();
        controlChanged();
    }
    ModuleBase::updateStep();
}
void RotaryEncoder::evaluate()
{
    setSwitch(m_clk, !grayA[m_phase]); // Contact closed = line low
    setSwitch(m_dt, !grayB[m_phase]);
    setSwitch(m_sw, m_swDown);
}
void RotaryEncoder::paintOver(QPainter *p)
{
    // Knob marker follows the position (18 deg per detent)
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->translate(0, -6);
    p->rotate(m_pos * 18.0);
    p->setPen(QPen(QColor(30, 30, 30), 1.4, Qt::SolidLine, Qt::RoundCap));
    p->drawLine(QPointF(0, -7), QPointF(0, -2));
    p->restore();
    drawText(p, QRectF(-28, 10, 56, 7), QString::number(m_pos), Qt::white, 5);
}
void RotaryEncoder::paintPart(QPainter *p)
{
    p->drawRoundedRect(m_area, 2, 2);
    p->drawEllipse(QPointF(0, -6), 11, 11);
    paintOver(p);
}

#include "moc_sensors.cpp"
