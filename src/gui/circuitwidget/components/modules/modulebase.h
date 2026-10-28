/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MODULEBASE_H
#define MODULEBASE_H

#include "component.h"
#include "e-diode.h"
#include "e-element.h"
#include "e-resistor.h"
#include "e-source.h"
#include "itemlibrary.h"
#include "pin.h"

class QSlider;
class QGraphicsSceneMouseEvent;
class QGraphicsProxyWidget;

// Building blocks for breakout modules and simple discrete parts:
//  - pins at any position (on the art's header holes / terminals)
//  - resistors, diodes and cells (voltage source + internal resistance)
//    between two pins, potentiometer models
//  - outputs: a pin driven to a voltage (relative to circuit ground)
// Subclasses implement evaluate() (runs in the simulation on input changes
// and when a control on the part is changed).
class MAINMODULE_EXPORT ModuleBase
    : public Component
    , public eElement
{
    Q_OBJECT
  public:
    ModuleBase(QObject *parent, QString type, QString id);
    ~ModuleBase();

    virtual void attach();
    virtual void stamp();
    virtual void resetState();
    virtual void setVChanged();
    virtual void updateStep();

    virtual void paint(QPainter *p, const QStyleOptionGraphicsItem *option,
                       QWidget *widget);
    virtual QRectF boundingRect() const;

  public slots:
    virtual void remove();
    void controlChanged(); // A widget on the part changed

  protected:
    struct Res {
        Pin *a;
        Pin *b;
        ePin *ea;
        ePin *eb;
        eResistor *r; // eDiode when diode
        bool diode;
    };
    struct Out {
        Pin *pin;
        eSource *src;
        double volt;
    };
    struct Cell {
        int res;
        double volt;
    };
    // Clickable (optionally drawn) rectangles on the part: push buttons,
    // toggles, or invisible hot spots over the art
    struct Zone {
        QRectF r;
        QString text;
        bool checkable;
        bool visible;
        bool on;
        bool down;
    };
    struct PotModel {
        Pin *w;
        int aw, wb, ab; // Resistor indexes
        double total;
        double frac;
    };

    Pin *addPin(QString name, int angle, QPoint pos, QString label);
    int addRes(Pin *a, Pin *b, double res);
    int addDiode(Pin *anode, Pin *cathode, double vf = 0.7);
    int addOut(Pin *pin, double imp);
    int addCell(Pin *pos, Pin *neg, double volt, double res);
    int addPot(Pin *a, Pin *w, Pin *b, double total);
    void watch(Pin *pin) // evaluate() when this pin's voltage changes
    {
        m_watch.append(pin);
    }

    void setResAdmit(int i, double admit);
    void setResValue(int i, double res);
    void setSwitch(int i, bool closed)
    {
        setResAdmit(i, closed ? 1e3 : 1e-9);
    }
    void setOutVolt(int i, double v);
    void setCell(int i, double volt, double res);
    void setPot(int i, double frac, double total = -1);
    double resCurrent(int i); // a -> b
    double volt(Pin *pin)     // 0 if unconnected
    {
        return pin->isConnected() ? pin->getVolt() : 0;
    }
    bool powered(Pin *vcc, Pin *gnd, double min = 2.5);

    virtual void evaluate()
    {
        ;
    }
    virtual void paintPart(QPainter *p); // Fallback when art is disabled
    virtual void paintOver(QPainter *p)  // Drawn on top of the art
    {
        Q_UNUSED(p);
    }

    // Control widgets on the part
    QSlider *addSlider(QRect geom, int min, int max, int val,
                       Qt::Orientation o = Qt::Horizontal);
    int addZone(QRectF r, QString text, bool checkable, bool visible = true);
    void setZoneOn(int i, bool on); // Without zoneEvent()
    // Checkable: toggled to `on`. Momentary: pressed (true) / released (false)
    virtual void zoneEvent(int i, bool on)
    {
        Q_UNUSED(i);
        Q_UNUSED(on);
    }
    virtual void mousePressEvent(QGraphicsSceneMouseEvent *e);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *e);
    void setSliderValue(QSlider *s, int v);
    void drawLed(QPainter *p, QPointF c, QColor col, bool on, double r = 2.2);
    void drawText(QPainter *p, QRectF r, QString txt, QColor col = Qt::white,
                  int px = 6, int flags = Qt::AlignCenter);

    QList<Pin *> m_pins;
    QList<Res> m_res;
    QList<Out> m_outs;
    QList<Cell> m_cells;
    QList<PotModel> m_pots;
    QList<Pin *> m_watch;
    QList<QGraphicsProxyWidget *> m_proxys;
    QList<Zone> m_zones;

    bool m_changed;      // Repaint needed
    bool m_inputChanged; // A control changed: evaluate() again
};

#endif
