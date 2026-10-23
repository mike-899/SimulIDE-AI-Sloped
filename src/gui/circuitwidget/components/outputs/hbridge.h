/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef HBRIDGE_H
#define HBRIDGE_H

#include "component.h"
#include "e-element.h"
#include "e-resistor.h"
#include "itemlibrary.h"
#include "pin.h"

// Half-bridge channel: OUT switched to VS (IN high) or GND (IN low)
// through On_Resistance while EN is high; EN low => both switches open.
struct HBridgeChannel {
    Pin *in;
    Pin *out;
    int en; // Index in m_enPins
    int state;
    eResistor *hi;
    eResistor *lo;
    ePin *hiOut;
    ePin *hiVs;
    ePin *loOut;
    ePin *loGnd;
};

class MAINMODULE_EXPORT HBridge
    : public Component
    , public eElement
{
    Q_OBJECT
    Q_PROPERTY(
        double On_Resistance READ ron WRITE setRon DESIGNABLE true USER true)

  public:
    HBridge(QObject *parent, QString type, QString id);
    ~HBridge();

    double ron()
    {
        return m_ron;
    }
    void setRon(double r);

    virtual void attach();
    virtual void stamp();
    virtual void resetState();
    virtual void setVChanged();

    virtual void paint(QPainter *p, const QStyleOptionGraphicsItem *option,
                       QWidget *widget);

  public slots:
    virtual void remove();

  protected:
    Pin *addPin(QString name, int angle, QPoint pos, QString label);
    void addChannel(Pin *in, int en, Pin *out);
    eNode *gndNode();
    bool isHigh(Pin *pin, double gnd);
    void setState(HBridgeChannel &ch, int state);

    double m_ron;
    double m_threshold;

    QList<Pin *> m_pins;
    QList<Pin *> m_enPins;
    QList<Pin *> m_gndPins;
    Pin *m_vsPin;
    QList<HBridgeChannel> m_chan;
};

class MAINMODULE_EXPORT L298N : public HBridge
{
    Q_OBJECT
  public:
    L298N(QObject *parent, QString type, QString id);
    ~L298N();

    static Component *construct(QObject *parent, QString type, QString id);
    static LibraryItem *libraryItem();
};

class MAINMODULE_EXPORT L293D : public HBridge
{
    Q_OBJECT
  public:
    L293D(QObject *parent, QString type, QString id);
    ~L293D();

    static Component *construct(QObject *parent, QString type, QString id);
    static LibraryItem *libraryItem();
};

#endif
