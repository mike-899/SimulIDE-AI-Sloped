/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef ICMODULES_H
#define ICMODULES_H

#include "sensors.h"

// ULN2003A: 7 Darlington open collector drivers (input threshold 1.4 V,
// ~1 V saturation) with flyback diodes to COM
class MAINMODULE_EXPORT Uln2003 : public ModuleBase
{
    Q_OBJECT
    MODULE_FACTORY(Uln2003)
  public:
    Uln2003(QObject *parent, QString type, QString id);

  protected:
    virtual void evaluate();
    virtual void paintPart(QPainter *p);
    Pin *m_in[7];
    Pin *m_gnd;
    int m_sw[7];
    bool m_on[7];
};

// PC817 optocoupler: LED (Vf 1.2 V) drives a phototransistor,
// Ic = CTR * If with ~0.2 V saturation
class MAINMODULE_EXPORT Optocoupler : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(double CTR READ ctr WRITE setCtr DESIGNABLE true USER true)
    MODULE_FACTORY(Optocoupler)
  public:
    Optocoupler(QObject *parent, QString type, QString id);
    double ctr()
    {
        return m_ctr;
    }
    void setCtr(double c)
    {
        m_ctr = c < 1 ? 1 : c;
        controlChanged();
    }

  protected:
    virtual void evaluate();
    virtual void paintPart(QPainter *p);
    Pin *m_c, *m_e;
    int m_led, m_ce;
    double m_ctr; // %
};

// Relay module: VCC / GND / IN and COM / NO / NC, with LEDs
class MAINMODULE_EXPORT RelayModule : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(bool Active_Low READ activeLow WRITE setActiveLow
                   DESIGNABLE true USER true)
    MODULE_FACTORY(RelayModule)
  public:
    RelayModule(QObject *parent, QString type, QString id);
    bool activeLow()
    {
        return m_activeLow;
    }
    void setActiveLow(bool a)
    {
        m_activeLow = a;
        controlChanged();
    }

  protected:
    virtual void evaluate();
    virtual void paintOver(QPainter *p);
    virtual void paintPart(QPainter *p);
    Pin *m_vcc, *m_gnd, *m_in;
    int m_coil, m_no, m_nc;
    bool m_activeLow, m_energized, m_powered;
};

// IRF520 MOSFET driver module: SIG drives the low side switch V- / VIN-
class MAINMODULE_EXPORT MosfetModule : public ModuleBase
{
    Q_OBJECT
    MODULE_FACTORY(MosfetModule)
  public:
    MosfetModule(QObject *parent, QString type, QString id);

  protected:
    virtual void evaluate();
    virtual void paintOver(QPainter *p);
    virtual void paintPart(QPainter *p);
    Pin *m_sig, *m_gnd;
    int m_sw;
    double m_vgs;
};

#endif
