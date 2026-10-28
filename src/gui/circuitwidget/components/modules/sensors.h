/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SENSORS_H
#define SENSORS_H

#include "modulebase.h"

#include <QElapsedTimer>

#define MODULE_FACTORY(Cls)                                                \
  public:                                                                  \
    static Component *construct(QObject *parent, QString type, QString id) \
    {                                                                      \
        return new Cls(parent, type, id);                                  \
    }                                                                      \
    static LibraryItem *libraryItem();

// Light dependent resistor: R = R10 * (10 lux / Lux) ^ Gamma
class MAINMODULE_EXPORT Ldr : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(double Lux READ lux WRITE setLux DESIGNABLE true USER true)
    Q_PROPERTY(double R_10lux READ r10 WRITE setR10 DESIGNABLE true USER true)
    Q_PROPERTY(double Gamma READ gamma WRITE setGamma DESIGNABLE true USER true)
    MODULE_FACTORY(Ldr)
  public:
    Ldr(QObject *parent, QString type, QString id);
    double lux()
    {
        return m_lux;
    }
    void setLux(double l);
    double r10()
    {
        return m_r10;
    }
    void setR10(double r);
    double gamma()
    {
        return m_gamma;
    }
    void setGamma(double g);

  protected:
    virtual void evaluate();
    virtual void paintOver(QPainter *p);
    virtual void paintPart(QPainter *p);
    double m_lux, m_r10, m_gamma;
    QSlider *m_slider;
};

// NTC thermistor: R = R25 * exp(Beta * (1/T - 1/298.15))
class MAINMODULE_EXPORT Thermistor : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(
        double Temperature READ temp WRITE setTemp DESIGNABLE true USER true)
    Q_PROPERTY(double R25 READ r25 WRITE setR25 DESIGNABLE true USER true)
    Q_PROPERTY(double Beta READ beta WRITE setBeta DESIGNABLE true USER true)
    MODULE_FACTORY(Thermistor)
  public:
    Thermistor(QObject *parent, QString type, QString id);
    double temp()
    {
        return m_temp;
    }
    void setTemp(double t);
    double r25()
    {
        return m_r25;
    }
    void setR25(double r);
    double beta()
    {
        return m_beta;
    }
    void setBeta(double b);

  protected:
    virtual void evaluate();
    virtual void paintOver(QPainter *p);
    virtual void paintPart(QPainter *p);
    double m_temp, m_r25, m_beta;
    QSlider *m_slider;
};

// Slide potentiometer (A, wiper, B)
class MAINMODULE_EXPORT SlidePot : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(
        double Resistance READ res WRITE setRes DESIGNABLE true USER true)
    Q_PROPERTY(int Position READ pos WRITE setPos DESIGNABLE true USER true)
    MODULE_FACTORY(SlidePot)
  public:
    SlidePot(QObject *parent, QString type, QString id);
    double res()
    {
        return m_res;
    }
    void setRes(double r);
    int pos()
    {
        return m_pos;
    }
    void setPos(int p);

  protected:
    virtual void evaluate();
    virtual void paintPart(QPainter *p);
    double m_res;
    int m_pos; // 0..100 % from A to B
    int m_pot;
    QSlider *m_slider;
};

// HC-SR501 PIR: OUT high while motion is present (+ hold time)
class MAINMODULE_EXPORT PirSensor : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(
        bool Motion READ motion WRITE setMotion DESIGNABLE true USER true)
    Q_PROPERTY(
        double Hold_Time READ hold WRITE setHold DESIGNABLE true USER true)
    Q_PROPERTY(double Out_Volt READ outLevel WRITE setOutLevel
                   DESIGNABLE true USER true)
    MODULE_FACTORY(PirSensor)
  public:
    PirSensor(QObject *parent, QString type, QString id);
    bool motion()
    {
        return m_motion;
    }
    void setMotion(bool m);
    double hold()
    {
        return m_hold;
    }
    void setHold(double h)
    {
        m_hold = h < 0 ? 0 : h;
    }
    double outLevel()
    {
        return m_outV;
    }
    void setOutLevel(double v)
    {
        m_outV = v;
        controlChanged();
    }
    virtual void updateStep();

  protected:
    virtual void zoneEvent(int i, bool on);
    virtual void evaluate();
    virtual void paintOver(QPainter *p);
    virtual void paintPart(QPainter *p);
    Pin *m_vcc, *m_gnd;
    int m_out;
    bool m_motion, m_active, m_powered;
    double m_hold, m_outV;
    QElapsedTimer m_timer;
    bool m_timing;
    int m_zone;
};

// FC-51 IR obstacle sensor: OUT low when an obstacle is detected
class MAINMODULE_EXPORT IrObstacle : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(
        bool Obstacle READ obstacle WRITE setObstacle DESIGNABLE true USER true)
    MODULE_FACTORY(IrObstacle)
  public:
    IrObstacle(QObject *parent, QString type, QString id);
    bool obstacle()
    {
        return m_obst;
    }
    void setObstacle(bool o);

  protected:
    virtual void zoneEvent(int i, bool on);
    virtual void evaluate();
    virtual void paintOver(QPainter *p);
    virtual void paintPart(QPainter *p);
    Pin *m_out, *m_gnd, *m_vcc;
    int m_o;
    bool m_obst, m_powered;
    int m_zone;
};

// Analog sensor module with comparator: soil moisture / sound
class MAINMODULE_EXPORT AnalogSensor : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(int Level READ level WRITE setLevel DESIGNABLE true USER true)
    Q_PROPERTY(int Threshold READ threshold WRITE setThreshold
                   DESIGNABLE true USER true)
  public:
    AnalogSensor(QObject *parent, QString type, QString id, bool soil);
    int level()
    {
        return m_level;
    }
    void setLevel(int l);
    int threshold()
    {
        return m_thr;
    }
    void setThreshold(int t);

  protected:
    virtual void evaluate();
    virtual void paintOver(QPainter *p);
    virtual void paintPart(QPainter *p);
    Pin *m_vcc, *m_gnd, *m_ao, *m_do;
    int m_oa, m_od;
    bool m_soil, m_powered;
    int m_level, m_thr; // % (0..100)
    QSlider *m_slider;
};

class MAINMODULE_EXPORT SoilMoisture : public AnalogSensor
{
    Q_OBJECT
    MODULE_FACTORY(SoilMoisture)
  public:
    SoilMoisture(QObject *parent, QString type, QString id) :
        AnalogSensor(parent, type, id, true)
    {
    }
};

class MAINMODULE_EXPORT SoundSensor : public AnalogSensor
{
    Q_OBJECT
    MODULE_FACTORY(SoundSensor)
  public:
    SoundSensor(QObject *parent, QString type, QString id) :
        AnalogSensor(parent, type, id, false)
    {
    }
};

// KY-023 joystick: two 10k pots (GND..+5V) and a push switch to GND
class MAINMODULE_EXPORT Joystick : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(int X READ x WRITE setX DESIGNABLE true USER true)
    Q_PROPERTY(int Y READ y WRITE setY DESIGNABLE true USER true)
    Q_PROPERTY(
        bool Pressed READ pressed WRITE setPressed DESIGNABLE true USER true)
    MODULE_FACTORY(Joystick)
  public:
    Joystick(QObject *parent, QString type, QString id);
    int x()
    {
        return m_x;
    }
    void setX(int v);
    int y()
    {
        return m_y;
    }
    void setY(int v);
    bool pressed()
    {
        return m_pressed;
    }
    void setPressed(bool p);

  protected:
    virtual void zoneEvent(int i, bool on);
    virtual void evaluate();
    virtual void paintOver(QPainter *p);
    virtual void paintPart(QPainter *p);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *e);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *e);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *e);
    void drag(QPointF pos);
    int m_potX, m_potY, m_sw;
    int m_x, m_y; // 0..100, 50 = centre
    bool m_pressed, m_drag;
    int m_zone;
};

// KY-040 rotary encoder: CLK / DT / SW with pull-ups to +
class MAINMODULE_EXPORT RotaryEncoder : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(
        int Position READ position WRITE setPosition DESIGNABLE true USER true)
    MODULE_FACTORY(RotaryEncoder)
  public:
    RotaryEncoder(QObject *parent, QString type, QString id);
    int position()
    {
        return m_pos;
    }
    void setPosition(int p);
    virtual void updateStep();

  protected:
    virtual void zoneEvent(int i, bool on);
    virtual void evaluate();
    virtual void paintOver(QPainter *p);
    virtual void paintPart(QPainter *p);
    void rotate(int dir); // +1 clockwise, -1 counter clockwise
    int m_clk, m_dt, m_sw;
    int m_zCcw, m_zCw, m_zSw;
    int m_pos;
    int m_phase; // Gray code state 0..3
    QList<int> m_queue;
    QElapsedTimer m_timer;
    bool m_swDown;
};

#endif
