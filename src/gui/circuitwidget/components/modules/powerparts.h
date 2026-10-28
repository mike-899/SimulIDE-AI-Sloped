/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef POWERPARTS_H
#define POWERPARTS_H

#include "sensors.h"

// Battery / cell: voltage source with internal resistance
class MAINMODULE_EXPORT Battery : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(double Voltage READ voltage WRITE setVoltage DESIGNABLE true USER
                   true)
    Q_PROPERTY(double Internal_Res READ intRes WRITE setIntRes DESIGNABLE true
                   USER true)
  public:
    Battery(QObject *parent, QString type, QString id, int kind);
    double voltage()
    {
        return m_volt;
    }
    void setVoltage(double v);
    double intRes()
    {
        return m_ri;
    }
    void setIntRes(double r);

  protected:
    virtual void paintPart(QPainter *p);
    int m_kind; // 0: 9V, 1: 2xAA, 2: 4xAA, 3: CR2032
    int m_cell;
    double m_volt, m_ri;
};

#define BATTERY_CLASS(Cls, kind)                                               \
    class MAINMODULE_EXPORT Cls : public Battery                               \
    {                                                                          \
        Q_OBJECT                                                               \
        MODULE_FACTORY(Cls)                                                    \
      public:                                                                  \
        Cls(QObject *parent, QString type, QString id) :                       \
            Battery(parent, type, id, kind)                                    \
        {                                                                      \
        }                                                                      \
    };
BATTERY_CLASS(Battery9V, 0)
BATTERY_CLASS(Battery2AA, 1)
BATTERY_CLASS(Battery4AA, 2)
BATTERY_CLASS(CoinCell, 3)

// Fuse: opens when I^2*t exceeds its rating, stays open until reset
class MAINMODULE_EXPORT Fuse : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(double Rating READ rating WRITE setRating DESIGNABLE true USER
                   true)
    Q_PROPERTY(bool Blown READ blown WRITE setBlown DESIGNABLE true USER true)
    MODULE_FACTORY(Fuse)
  public:
    Fuse(QObject *parent, QString type, QString id);
    double rating()
    {
        return m_rating;
    }
    void setRating(double a)
    {
        m_rating = a < 0.001 ? 0.001 : a;
    }
    bool blown()
    {
        return m_blown;
    }
    void setBlown(bool b);
    virtual void updateStep();
    virtual void resetState();

  protected:
    virtual void zoneEvent(int i, bool on);
    virtual void evaluate();
    virtual void paintOver(QPainter *p);
    virtual void paintPart(QPainter *p);
    double m_rating, m_heat;
    bool m_blown;
    uint64_t m_lastT;
    int m_zone;
};

#endif
