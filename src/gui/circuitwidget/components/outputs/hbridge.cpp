/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "hbridge.h"

#include "circuit.h"
#include "connector.h"
#include "e-node.h"
#include "simulator.h"

static const char *HBridge_properties[] = { QT_TRANSLATE_NOOP(
    "App::Property", "On Resistance") };

static const double offAdmit = 1e-9; // Open switch (keeps matrix solvable)

enum { stOff = 0, stHigh, stLow };

HBridge::HBridge(QObject *parent, QString type, QString id) :
    Component(parent, type, id), eElement((id + "-eElement").toStdString())
{
    Q_UNUSED(HBridge_properties);

    m_ron       = 1.5;
    m_threshold = 2.3;
    m_vsPin     = 0l;
    m_color     = QColor(50, 50, 70);
}
HBridge::~HBridge()
{
    for (HBridgeChannel &ch : m_chan) {
        delete ch.hi;
        delete ch.lo;
        delete ch.hiOut;
        delete ch.hiVs;
        delete ch.loOut;
        delete ch.loGnd;
    }
}

Pin *HBridge::addPin(QString name, int angle, QPoint pos, QString label)
{
    Pin *pin = new Pin(angle, pos, m_id + "-" + name, m_pins.size(), this);
    pin->setLabelText(label);
    m_pins.append(pin);
    return pin;
}

void HBridge::addChannel(Pin *in, int en, Pin *out)
{
    HBridgeChannel ch;
    ch.in    = in;
    ch.out   = out;
    ch.en    = en;
    ch.state = -1;

    std::string id =
        (m_id + "-ch" + QString::number(m_chan.size())).toStdString();
    ch.hiOut = new ePin(id + "-hiOut", 0);
    ch.hiVs  = new ePin(id + "-hiVs", 1);
    ch.loOut = new ePin(id + "-loOut", 0);
    ch.loGnd = new ePin(id + "-loGnd", 1);
    ch.hi    = new eResistor(id + "-hi");
    ch.lo    = new eResistor(id + "-lo");
    ch.hi->setEpin(0, ch.hiOut);
    ch.hi->setEpin(1, ch.hiVs);
    ch.lo->setEpin(0, ch.loOut);
    ch.lo->setEpin(1, ch.loGnd);
    ch.hi->setAdmit(offAdmit);
    ch.lo->setAdmit(offAdmit);
    m_chan.append(ch);
}

void HBridge::setRon(double r)
{
    if (r < 1e-3)
        r = 1e-3;
    m_ron         = r;
    bool pauseSim = Simulator::self()->isRunning();
    if (pauseSim)
        Simulator::self()->pauseSim();
    for (HBridgeChannel &ch : m_chan)
        ch.state = -1;
    setVChanged();
    if (pauseSim)
        Simulator::self()->resumeSim();
}

eNode *HBridge::gndNode()
{
    for (Pin *pin : m_gndPins)
        if (pin->getEnode())
            return pin->getEnode();
    return 0l;
}

void HBridge::attach() // Place internal switch pins on the real nodes
{
    eNode *vsNode = m_vsPin ? m_vsPin->getEnode() : 0l;
    eNode *gnd    = gndNode();
    for (HBridgeChannel &ch : m_chan) {
        eNode *out = ch.out->getEnode();
        ch.hiOut->setEnode(out);
        ch.loOut->setEnode(out);
        ch.hiVs->setEnode(out ? vsNode : 0l);
        ch.loGnd->setEnode(out ? gnd : 0l);
    }
}

void HBridge::resetState()
{
    for (HBridgeChannel &ch : m_chan)
        ch.state = -1;
}

void HBridge::stamp()
{
    for (Pin *pin : m_pins) {
        eNode *enode = pin->getEnode();
        if (!enode)
            continue;
        bool isOut = false;
        for (HBridgeChannel &ch : m_chan)
            if (ch.out == pin)
                isOut = true;
        if (isOut || pin == m_vsPin || m_gndPins.contains(pin))
            enode->setSwitched(true);
        if (!isOut)
            enode->addToChangedFast(this); // Inputs, enables, supplies
    }
    for (HBridgeChannel &ch : m_chan) {
        ch.state = -1;
        setState(ch, stOff);
    }
}

bool HBridge::isHigh(Pin *pin, double gnd)
{
    return (pin->getVolt() - gnd) > m_threshold;
}

void HBridge::setState(HBridgeChannel &ch, int state)
{
    if (state == ch.state)
        return;
    ch.state  = state;
    double on = 1 / m_ron;
    ch.hi->setAdmit(state == stHigh ? on : offAdmit);
    ch.lo->setAdmit(state == stLow ? on : offAdmit);
}

void HBridge::setVChanged()
{
    eNode *gnd  = gndNode();
    double gndV = gnd ? gnd->getVolt() : 0;
    bool power  = m_vsPin && m_vsPin->isConnected() && gnd;

    for (HBridgeChannel &ch : m_chan) {
        int state = stOff;
        Pin *en   = m_enPins.value(ch.en);
        // Unconnected EN = enabled (module jumper / TTL input floats high)
        bool enabled = !en || !en->isConnected() || isHigh(en, gndV);
        if (power && enabled && ch.out->isConnected())
            state = isHigh(ch.in, gndV) ? stHigh : stLow;
        setState(ch, state);
    }
}

void HBridge::remove()
{
    for (Pin *pin : m_pins)
        if (pin->isConnected())
            pin->connector()->remove();
    Component::remove();
}

void HBridge::paint(QPainter *p, const QStyleOptionGraphicsItem *option,
                    QWidget *widget)
{
    Component::paint(p, option, widget);
    if (paintArt(p))
        return; // Realistic part art

    p->drawRoundedRect(m_area, 2, 2);
}

//---------------------------------------------------------------------------
// L298N dual H-bridge module

Component *L298N::construct(QObject *parent, QString type, QString id)
{
    return new L298N(parent, type, id);
}

LibraryItem *L298N::libraryItem()
{
    return new LibraryItem(tr("L298N Module"), tr("Motors"), "subc.png",
                           "L298N", L298N::construct);
}

L298N::L298N(QObject *parent, QString type, QString id) :
    HBridge(parent, type, id)
{
    m_area = QRectF(-48, -48, 96, 96);
    m_ron  = 1.5;

    Pin *out1 = addPin("OUT1", 180, QPoint(-56, -16), "OUT1");
    Pin *out2 = addPin("OUT2", 180, QPoint(-56, 0), "OUT2");
    Pin *out3 = addPin("OUT3", 0, QPoint(56, -16), "OUT3");
    Pin *out4 = addPin("OUT4", 0, QPoint(56, 0), "OUT4");
    m_vsPin   = addPin("VS", 270, QPoint(-40, 56), "12V");
    m_gndPins.append(addPin("GND", 270, QPoint(-32, 56), "GND"));
    addPin("5V", 270, QPoint(-24, 56), "5V");
    m_enPins.append(addPin("ENA", 270, QPoint(0, 56), "ENA"));
    Pin *in1 = addPin("IN1", 270, QPoint(8, 56), "IN1");
    Pin *in2 = addPin("IN2", 270, QPoint(16, 56), "IN2");
    Pin *in3 = addPin("IN3", 270, QPoint(24, 56), "IN3");
    Pin *in4 = addPin("IN4", 270, QPoint(32, 56), "IN4");
    m_enPins.append(addPin("ENB", 270, QPoint(40, 56), "ENB"));

    for (Pin *pin : m_pins) {
        pin->setLabelColor(QColor(250, 250, 250));
        pin->setFontSize(4);
    }

    addChannel(in1, 0, out1);
    addChannel(in2, 0, out2);
    addChannel(in3, 1, out3);
    addChannel(in4, 1, out4);

    setLabelPos(-48, -64, 0);
    setShowId(true);
}
L298N::~L298N() {}

//---------------------------------------------------------------------------
// L293D quadruple half-H driver, DIP-16

Component *L293D::construct(QObject *parent, QString type, QString id)
{
    return new L293D(parent, type, id);
}

LibraryItem *L293D::libraryItem()
{
    return new LibraryItem(tr("L293D"), tr("Motors"), "subc.png", "L293D",
                           L293D::construct);
}

L293D::L293D(QObject *parent, QString type, QString id) :
    HBridge(parent, type, id)
{
    m_area = QRectF(-20, -36, 40, 72);
    m_ron  = 2;

    // Left side pins 1-8 (top to bottom), right side 16-9
    const char *names[16]  = { "EN12", "1A", "1Y",   "GND1", "GND2", "2Y",
                               "2A",   "VS", "EN34", "3A",   "3Y",   "GND3",
                               "GND4", "4Y", "4A",   "VCC" };
    const char *labels[16] = {
        "EN", "1A", "1Y", "GND", "GND", "2Y", "2A", "VS",
        "EN", "3A", "3Y", "GND", "GND", "4Y", "4A", "VCC"
    };
    Pin *pins[16];
    for (int i = 0; i < 16; i++) {
        QPoint pos =
            (i < 8) ? QPoint(-28, -28 + 8 * i) : QPoint(28, 28 - 8 * (i - 8));
        pins[i] = addPin(names[i], (i < 8) ? 180 : 0, pos, labels[i]);
        pins[i]->setLabelColor(QColor(250, 250, 200));
        pins[i]->setFontSize(4);
    }
    m_vsPin = pins[7];
    m_gndPins << pins[3] << pins[4] << pins[11] << pins[12];
    m_enPins << pins[0] << pins[8];

    addChannel(pins[1], 0, pins[2]);   // 1A -> 1Y
    addChannel(pins[6], 0, pins[5]);   // 2A -> 2Y
    addChannel(pins[9], 1, pins[10]);  // 3A -> 3Y
    addChannel(pins[14], 1, pins[13]); // 4A -> 4Y

    setLabelPos(-20, -52, 0);
    setShowId(true);
}
L293D::~L293D() {}

#include "moc_hbridge.cpp"
