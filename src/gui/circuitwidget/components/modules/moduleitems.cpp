/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// Library registration of the module / sensor / power parts and of the named
// presets (transistors, diodes, regulators, buzzers, stepper) built from the
// existing parts. Art: see the "modules" block of PartArt::init.

#include "moduleitems.h"

#include "audio_out.h"
#include "bjt.h"
#include "diode.h"
#include "hbridge.h"
#include "icmodules.h"
#include "itemlibrary.h"
#include "partart.h"
#include "powerparts.h"
#include "sensors.h"
#include "stepper.h"
#include "switchparts.h"
#include "volt_reg.h"

#include <QPainter>

//---------------------------------------------------------------- Regulators
// 7805 / LM317: VoltReg with a fixed reference voltage (the reference pin
// is GND for the 7805 and ADJ for the LM317) and TO-220 art
class Reg7805 : public VoltReg
{
  public:
    Reg7805(QObject *parent, QString type, QString id) :
        VoltReg(parent, type, id)
    {
        setVRef(5);
        for (Pin *p : m_pin)
            p->setLabelText("");
    }
};

class Lm317 : public VoltReg
{
  public:
    Lm317(QObject *parent, QString type, QString id) : VoltReg(parent, type, id)
    {
        setVRef(1.25);
        // TO-220 front view, left to right: ADJ OUT IN
        m_pin[2]->setPos(-16, 0);
        m_pin[2]->setPinAngle(180);
        m_pin[1]->setPos(0, 16);
        m_pin[1]->setPinAngle(270);
        m_pin[0]->setPos(16, 0);
        m_pin[0]->setPinAngle(0);
        for (Pin *p : m_pin) {
            p->setLength(6);
            p->setLabelText("");
        }
    }
};

//------------------------------------------------------------ 28BYJ-48 motor
class Stepper28BYJ : public Stepper
{
  public:
    Stepper28BYJ(QObject *parent, QString type, QString id) :
        Stepper(parent, type, id)
    {
        setRes(50); // 50 ohm per coil, 5 V
        m_area = QRectF(-64, -48, 128, 96);
    }
    virtual void paint(QPainter *p, const QStyleOptionGraphicsItem *option,
                       QWidget *widget)
    {
        if (!artActive()) {
            Stepper::paint(p, option, widget);
            return;
        }
        Component::paint(p, option, widget);
        paintArt(p);
        // Rotor mark on the blue hub
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->translate(21, 0);
        p->rotate(-m_ang / 16.0);
        p->setPen(QPen(QColor(255, 255, 255), 2, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(QPointF(0, -5), QPointF(0, -20));
        p->restore();
    }
};

template<typename T>
static Component *make(QObject *parent, QString type, QString id)
{
    return new T(parent, type, id);
}

static Component *makeBjt(QObject *parent, QString type, QString id,
                          BJT::BjtModel model)
{
    BJT *q = new BJT(parent, type, id);
    q->setModel(model);
    return q;
}

static Component *makeDiode(QObject *parent, QString type, QString id,
                            Diode::DiodeModel model)
{
    Diode *d = new Diode(parent, type, id);
    d->setModel(model);
    return d;
}

void addModuleItems(ItemLibrary *lib)
{
    // Sensors
    lib->addItem(Ldr::libraryItem());
    lib->addItem(Thermistor::libraryItem());
    lib->addItem(SlidePot::libraryItem());
    lib->addItem(PirSensor::libraryItem());
    lib->addItem(IrObstacle::libraryItem());
    lib->addItem(SoilMoisture::libraryItem());
    lib->addItem(SoundSensor::libraryItem());
    lib->addItem(Joystick::libraryItem());
    lib->addItem(RotaryEncoder::libraryItem());
    // Power
    lib->addItem(Battery9V::libraryItem());
    lib->addItem(Battery2AA::libraryItem());
    lib->addItem(Battery4AA::libraryItem());
    lib->addItem(CoinCell::libraryItem());
    lib->addItem(Fuse::libraryItem());
    lib->addItem(new LibraryItem(QObject::tr("7805 Regulator"),
                                 QObject::tr("Power"), "voltreg.png", "Reg7805",
                                 make<Reg7805>));
    lib->addItem(new LibraryItem(QObject::tr("LM317 Regulator"),
                                 QObject::tr("Power"), "voltreg.png", "Lm317",
                                 make<Lm317>));
    // Switches
    lib->addItem(TactileButton::libraryItem());
    lib->addItem(SlideSwitch::libraryItem());
    lib->addItem(ToggleSwitch::libraryItem());
    lib->addItem(ReedSwitch::libraryItem());
    // Motors and drivers
    lib->addItem(TB6612::libraryItem());
    lib->addItem(Uln2003::libraryItem());
    lib->addItem(new LibraryItem(QObject::tr("Stepper 28BYJ-48"),
                                 QObject::tr("Motors"), "steeper.png",
                                 "Stepper28BYJ", make<Stepper28BYJ>));
    // Modules
    lib->addItem(RelayModule::libraryItem());
    lib->addItem(MosfetModule::libraryItem());
    // Semiconductors
    lib->addItem(new LibraryItem(
        QObject::tr("2N2222 NPN"), QObject::tr("Semiconductors"), "bjt.png",
        "Q2N2222", [](QObject *p, QString t, QString i) {
            return makeBjt(p, t, i, BJT::Q2N2222);
        }));
    lib->addItem(new LibraryItem(QObject::tr("BC547 NPN"),
                                 QObject::tr("Semiconductors"), "bjt.png",
                                 "BC547", [](QObject *p, QString t, QString i) {
                                     return makeBjt(p, t, i, BJT::BC547);
                                 }));
    lib->addItem(new LibraryItem(
        QObject::tr("TIP120 Darlington"), QObject::tr("Semiconductors"),
        "bjt.png", "TIP120", [](QObject *p, QString t, QString i) {
            return makeBjt(p, t, i, BJT::TIP120);
        }));
    lib->addItem(new LibraryItem(
        QObject::tr("1N4007 Diode"), QObject::tr("Semiconductors"), "diode.png",
        "D1N4007", [](QObject *p, QString t, QString i) {
            return makeDiode(p, t, i, Diode::D1N4007);
        }));
    lib->addItem(new LibraryItem(
        QObject::tr("1N4148 Diode"), QObject::tr("Semiconductors"), "diode.png",
        "D1N4148", [](QObject *p, QString t, QString i) {
            return makeDiode(p, t, i, Diode::D1N4148);
        }));
    lib->addItem(new LibraryItem(
        QObject::tr("Zener 5.1V 1N4733A"), QObject::tr("Semiconductors"),
        "diode.png", "D1N4733A", [](QObject *p, QString t, QString i) {
            return makeDiode(p, t, i, Diode::D1N4733A);
        }));
    lib->addItem(Optocoupler::libraryItem());
    // Buzzers
    lib->addItem(new LibraryItem(
        QObject::tr("Active Buzzer"), QObject::tr("Outputs"), "audio_out.png",
        "ActiveBuzzer", [](QObject *p, QString t, QString i) -> Component * {
            AudioOut *a = new AudioOut(p, t, i);
            a->setBuzzer(true); // Built-in oscillator: sounds when powered
            a->setBuzzFreq(2700);
            return a;
        }));
    lib->addItem(new LibraryItem(
        QObject::tr("Passive Buzzer"), QObject::tr("Outputs"), "audio_out.png",
        "PassiveBuzzer", [](QObject *p, QString t, QString i) -> Component * {
            AudioOut *a = new AudioOut(p, t, i); // Plays the driving signal
            a->setBuzzer(false);
            a->setResSafe(16);
            return a;
        }));
}
