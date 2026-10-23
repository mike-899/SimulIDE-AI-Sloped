/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef DCMOTOR_H
#define DCMOTOR_H

#include "component.h"
#include "e-element.h"
#include "e-resistor.h"
#include "itemlibrary.h"
#include "pin.h"

// Brushed DC motor: winding resistance + back-EMF (Ke * speed).
// Mechanics: first order, dw/dt = (V/Ke - w)/Tau - w/TauFriction
class MAINMODULE_EXPORT DcMotor
    : public Component
    , public eElement
{
    Q_OBJECT
    Q_PROPERTY(
        double Resistance READ res WRITE setRes DESIGNABLE true USER true)
    Q_PROPERTY(
        double Max_Rpm READ maxRpm WRITE setMaxRpm DESIGNABLE true USER true)
    Q_PROPERTY(double Rated_Volt READ ratedVolt WRITE setRatedVolt
                   DESIGNABLE true USER true)
    Q_PROPERTY(double Time_Const READ timeConst WRITE setTimeConst
                   DESIGNABLE true USER true)
    Q_PROPERTY(double Rpm READ rpm WRITE setRpm DESIGNABLE true USER true)

  public:
    DcMotor(QObject *parent, QString type, QString id);
    ~DcMotor();

    static Component *construct(QObject *parent, QString type, QString id);
    static LibraryItem *libraryItem();

    double res()
    {
        return m_resist;
    }
    void setRes(double r);

    double maxRpm()
    {
        return m_maxRpm;
    }
    void setMaxRpm(double rpm);

    double ratedVolt()
    {
        return m_ratedVolt;
    }
    void setRatedVolt(double v);

    double timeConst()
    {
        return m_tau;
    }
    void setTimeConst(double t);

    double rpm()
    {
        return m_rpm;
    }
    void setRpm(double)
    {
        ;
    } // Read only (simulation state)

    virtual void resetState();
    virtual void stamp();
    virtual void setVChanged();
    virtual void updateStep();

    virtual void paint(QPainter *p, const QStyleOptionGraphicsItem *option,
                       QWidget *widget);

  public slots:
    virtual void remove();

  private:
    void integrate();
    double ke(); // Back-EMF constant V/RPM

    double m_resist;
    double m_maxRpm;
    double m_ratedVolt;
    double m_tau;
    double m_tauF; // Friction (coast) time constant

    double m_rpm;
    double m_volt;  // Terminal voltage since last event
    double m_angle; // Visual shaft angle (degrees)
    uint64_t m_lastTime;
    uint64_t m_lastPaint;

    eResistor m_winding;
    Pin m_pinA;
    Pin m_pinB;
};

#endif
