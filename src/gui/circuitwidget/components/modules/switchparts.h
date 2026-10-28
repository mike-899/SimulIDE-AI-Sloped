/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SWITCHPARTS_H
#define SWITCHPARTS_H

#include "sensors.h"

// 4 pin tactile switch: pins 1+3 (left) and 2+4 (right) are joined inside,
// pressing joins the two sides
class MAINMODULE_EXPORT TactileButton : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(
        bool Pressed READ pressed WRITE setPressed DESIGNABLE true USER true)
    MODULE_FACTORY(TactileButton)
  public:
    TactileButton(QObject *parent, QString type, QString id);
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
    int m_link1, m_link2, m_main, m_zone;
    int m_cross[4];
    bool m_pressed;
};

// SPDT switch: common (middle pin) to pin 1 (Position 0) or pin 3 (1)
class MAINMODULE_EXPORT SpdtSwitch : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(
        int Position READ position WRITE setPosition DESIGNABLE true USER true)
  public:
    SpdtSwitch(QObject *parent, QString type, QString id, bool toggle);
    int position()
    {
        return m_pos;
    }
    void setPosition(int p);

  protected:
    virtual void zoneEvent(int i, bool on);
    virtual void evaluate();
    virtual void paintOver(QPainter *p);
    virtual void paintPart(QPainter *p);
    bool m_toggle;
    int m_pos;
    int m_r1, m_r3, m_zone;
};

class MAINMODULE_EXPORT SlideSwitch : public SpdtSwitch
{
    Q_OBJECT
    MODULE_FACTORY(SlideSwitch)
  public:
    SlideSwitch(QObject *parent, QString type, QString id) :
        SpdtSwitch(parent, type, id, false)
    {
    }
};

class MAINMODULE_EXPORT ToggleSwitch : public SpdtSwitch
{
    Q_OBJECT
    MODULE_FACTORY(ToggleSwitch)
  public:
    ToggleSwitch(QObject *parent, QString type, QString id) :
        SpdtSwitch(parent, type, id, true)
    {
    }
};

// Normally open reed switch closed by a magnet
class MAINMODULE_EXPORT ReedSwitch : public ModuleBase
{
    Q_OBJECT
    Q_PROPERTY(
        bool Magnet READ magnet WRITE setMagnet DESIGNABLE true USER true)
    MODULE_FACTORY(ReedSwitch)
  public:
    ReedSwitch(QObject *parent, QString type, QString id);
    bool magnet()
    {
        return m_magnet;
    }
    void setMagnet(bool m);

  protected:
    virtual void zoneEvent(int i, bool on);
    virtual void evaluate();
    virtual void paintOver(QPainter *p);
    virtual void paintPart(QPainter *p);
    bool m_magnet;
    int m_zone;
};

#endif
