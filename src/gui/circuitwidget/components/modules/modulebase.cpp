/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "modulebase.h"

#include "circuit.h"
#include "connector.h"
#include "e-node.h"
#include "simulator.h"

#include <QGraphicsProxyWidget>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QSlider>
#include <math.h>

static const double offAdmit = 1e-9; // Open switch (keeps matrix solvable)

ModuleBase::ModuleBase(QObject *parent, QString type, QString id) :
    Component(parent, type, id), eElement((id + "-eElement").toStdString())
{
    m_changed      = true;
    m_inputChanged = false;
    m_color        = QColor(50, 50, 70);
    Simulator::self()->addToUpdateList(this);
    setShowId(true);
}
ModuleBase::~ModuleBase()
{
    for (Res &r : m_res) {
        delete r.r;
        delete r.ea;
        delete r.eb;
    }
    for (Out &o : m_outs)
        delete o.src;
}

Pin *ModuleBase::addPin(QString name, int angle, QPoint pos, QString label)
{
    Pin *pin = new Pin(angle, pos, m_id + "-" + name, m_pins.size(), this);
    pin->setLabelText(label);
    pin->setLabelColor(QColor(250, 250, 250));
    pin->setFontSize(4);
    m_pins.append(pin);
    return pin;
}

int ModuleBase::addRes(Pin *a, Pin *b, double res)
{
    Res r;
    r.a             = a;
    r.b             = b;
    r.diode         = false;
    std::string rid = (m_id + "-r" + QString::number(m_res.size())).toStdString();
    r.ea            = new ePin(rid + "-a", 0);
    r.eb            = new ePin(rid + "-b", 1);
    r.r             = new eResistor(rid);
    r.r->setEpin(0, r.ea);
    r.r->setEpin(1, r.eb);
    r.r->setRes(res);
    m_res.append(r);
    return m_res.size() - 1;
}

int ModuleBase::addDiode(Pin *anode, Pin *cathode, double vf)
{
    Res r;
    r.a             = anode;
    r.b             = cathode;
    r.diode         = true;
    std::string rid = (m_id + "-d" + QString::number(m_res.size())).toStdString();
    r.ea            = new ePin(rid + "-a", 0);
    r.eb            = new ePin(rid + "-b", 1);
    eDiode *d       = new eDiode(rid);
    d->setEpin(0, r.ea);
    d->setEpin(1, r.eb);
    d->setThreshold(vf);
    r.r = d;
    m_res.append(r);
    return m_res.size() - 1;
}

int ModuleBase::addOut(Pin *pin, double imp)
{
    Out o;
    o.pin = pin;
    o.volt = 0;
    o.src  = new eSource(
        (m_id + "-out" + QString::number(m_outs.size())).toStdString(), pin);
    o.src->setVoltHigh(0);
    o.src->setVoltLow(0);
    o.src->setImp(imp);
    o.src->setOut(true);
    m_outs.append(o);
    return m_outs.size() - 1;
}

int ModuleBase::addCell(Pin *pos, Pin *neg, double volt, double res)
{
    Cell c;
    c.res  = addRes(pos, neg, res);
    c.volt = volt;
    m_cells.append(c);
    return m_cells.size() - 1;
}

int ModuleBase::addPot(Pin *a, Pin *w, Pin *b, double total)
{
    PotModel p;
    p.w     = w;
    p.total = total;
    p.frac  = 0.5;
    p.aw    = addRes(a, w, total / 2);
    p.wb    = addRes(w, b, total / 2);
    p.ab    = addRes(a, b, total);
    m_pots.append(p);
    return m_pots.size() - 1;
}

void ModuleBase::attach() // Internal ePins follow the real pin nodes
{
    for (Res &r : m_res) {
        eNode *na = r.a->getEnode();
        eNode *nb = r.b->getEnode();
        r.ea->setEnode(nb ? na : 0l);
        r.eb->setEnode(na ? nb : 0l);
    }
}

void ModuleBase::stamp()
{
    for (Res &r : m_res) {
        if (r.ea->getEnode())
            r.ea->getEnode()->setSwitched(true);
        if (r.eb->getEnode())
            r.eb->getEnode()->setSwitched(true);
    }
    for (Pin *pin : m_watch) {
        eNode *enode = pin->getEnode();
        if (enode)
            enode->addToChangedFast(this);
    }
    for (Cell &c : m_cells) {
        Res &r = m_res[c.res];
        if (r.ea->getEnode())
            r.r->stampCurrent(c.volt / r.r->res());
    }
    for (int i = 0; i < m_pots.size(); i++)
        setPot(i, m_pots[i].frac);
    evaluate();
    m_changed = true;
}

void ModuleBase::resetState()
{
    m_changed = true;
}

void ModuleBase::setVChanged()
{
    evaluate();
}

void ModuleBase::controlChanged()
{
    m_inputChanged = true;
    m_changed      = true;
    update();
}

void ModuleBase::updateStep()
{
    if (m_inputChanged) {
        m_inputChanged = false;
        if (Simulator::self()->isRunning())
            evaluate();
    }
    if (m_changed) {
        m_changed = false;
        update();
    }
}

void ModuleBase::setResAdmit(int i, double admit)
{
    eResistor *r = m_res[i].r;
    if (r->admit() == admit)
        return;
    r->setAdmit(admit);
    m_changed = true;
}

void ModuleBase::setResValue(int i, double res)
{
    if (res < 1e-3)
        res = 1e-3;
    eResistor *r = m_res[i].r;
    if (fabs(r->res() - res) <= res * 1e-6)
        return;
    r->setRes(res);
    for (Cell &c : m_cells)
        if (c.res == i && m_res[i].ea->getEnode())
            r->stampCurrent(c.volt / res);
    m_changed = true;
}

void ModuleBase::setCell(int i, double volt, double res)
{
    Cell &c = m_cells[i];
    c.volt  = volt;
    Res &r  = m_res[c.res];
    if (res < 1e-3)
        res = 1e-3;
    r.r->setRes(res);
    if (r.ea->getEnode())
        r.r->stampCurrent(volt / res);
    m_changed = true;
}

// frac: wiper position from A (0) to B (1)
void ModuleBase::setPot(int i, double frac, double total)
{
    PotModel &p = m_pots[i];
    if (total > 0)
        p.total = total;
    p.frac = qBound(0.0, frac, 1.0);
    if (p.w->isConnected()) {
        setResValue(p.aw, p.total * p.frac);
        setResValue(p.wb, p.total * (1 - p.frac));
        setResAdmit(p.ab, offAdmit);
    } else // Wiper floating: plain resistor between the ends
    {
        setResAdmit(p.aw, offAdmit);
        setResAdmit(p.wb, offAdmit);
        setResValue(p.ab, p.total);
    }
}

double ModuleBase::resCurrent(int i) // From a to b
{
    Res &r = m_res[i];
    if (!r.ea->getEnode() || !r.eb->getEnode())
        return 0;
    return (r.ea->getVolt() - r.eb->getVolt()) * r.r->admit();
}

void ModuleBase::setOutVolt(int i, double v)
{
    Out &o = m_outs[i];
    if (fabs(o.volt - v) < 1e-4)
        return;
    o.volt = v;
    o.src->setVoltHigh(v);
    o.src->setOut(true);
    o.src->stampOutput();
    m_changed = true;
}

bool ModuleBase::powered(Pin *vcc, Pin *gnd, double min)
{
    return vcc->isConnected() && gnd->isConnected() &&
           (vcc->getVolt() - gnd->getVolt()) > min;
}

QSlider *ModuleBase::addSlider(QRect geom, int min, int max, int val,
                               Qt::Orientation o)
{
    QSlider *s = new QSlider(o);
    s->setRange(min, max);
    s->setValue(val);
    s->setFixedSize(geom.size());
    s->setCursor(Qt::PointingHandCursor);
    s->setAttribute(Qt::WA_TranslucentBackground);
    s->setStyleSheet(
        "QSlider{background:transparent;}"
        "QSlider::groove:horizontal{height:3px;background:#2a2a2e;"
        "border-radius:1px;}"
        "QSlider::groove:vertical{width:3px;background:#2a2a2e;"
        "border-radius:1px;}"
        "QSlider::handle:horizontal{width:7px;margin:-3px 0;"
        "background:#f0f0f0;border:1px solid #555;border-radius:2px;}"
        "QSlider::handle:vertical{height:7px;margin:0 -3px;"
        "background:#f0f0f0;border:1px solid #555;border-radius:2px;}");
    QGraphicsProxyWidget *proxy = Circuit::self()->addWidget(s);
    proxy->setParentItem(this);
    proxy->setPos(geom.topLeft());
    m_proxys.append(proxy);
    return s;
}

int ModuleBase::addZone(QRectF r, QString text, bool checkable, bool visible)
{
    Zone z;
    z.r         = r;
    z.text      = text;
    z.checkable = checkable;
    z.visible   = visible;
    z.on        = false;
    z.down      = false;
    m_zones.append(z);
    return m_zones.size() - 1;
}

void ModuleBase::setZoneOn(int i, bool on)
{
    if (m_zones[i].on == on)
        return;
    m_zones[i].on = on;
    update();
}

QRectF ModuleBase::boundingRect() const
{
    QRectF br = Component::boundingRect();
    for (const Zone &z : m_zones)
        br = br.united(z.r.adjusted(-2, -2, 2, 2));
    return br;
}

void ModuleBase::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        for (int i = 0; i < m_zones.size(); i++) {
            Zone &z = m_zones[i];
            if (!z.r.contains(e->pos()))
                continue;
            if (z.checkable) {
                z.on = !z.on;
                zoneEvent(i, z.on);
            } else {
                z.on   = true;
                z.down = true;
                zoneEvent(i, true);
            }
            update();
            e->accept();
            return;
        }
    }
    Component::mousePressEvent(e);
}

void ModuleBase::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    bool any = false;
    for (int i = 0; i < m_zones.size(); i++) {
        Zone &z = m_zones[i];
        if (z.down) {
            z.down = false;
            z.on   = false;
            zoneEvent(i, false);
            any = true;
        }
    }
    if (any) {
        update();
        return;
    }
    Component::mouseReleaseEvent(e);
}

void ModuleBase::setSliderValue(QSlider *s, int v)
{
    if (s->value() == v)
        return;
    s->blockSignals(true);
    s->setValue(v);
    s->blockSignals(false);
}

void ModuleBase::drawLed(QPainter *p, QPointF c, QColor col, bool on, double r)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    if (on) {
        QRadialGradient g(c, r * 2.6);
        QColor glow = col;
        glow.setAlpha(150);
        g.setColorAt(0, glow);
        glow.setAlpha(0);
        g.setColorAt(1, glow);
        p->setPen(Qt::NoPen);
        p->setBrush(g);
        p->drawEllipse(c, r * 2.6, r * 2.6);
    }
    p->setPen(QPen(QColor(30, 30, 30), 0.4));
    p->setBrush(on ? col.lighter(130) : col.darker(260));
    p->drawEllipse(c, r, r * 0.8);
    p->restore();
}

void ModuleBase::drawText(QPainter *p, QRectF r, QString txt, QColor col,
                          int px, int flags)
{
    p->save();
    QFont f = p->font();
    f.setPixelSize(px);
    f.setBold(true);
    p->setFont(f);
    p->setPen(col);
    p->drawText(r, flags, txt);
    p->restore();
}

void ModuleBase::remove()
{
    for (Pin *pin : m_pins)
        if (pin->isConnected() && pin->connector())
            pin->connector()->remove();
    Simulator::self()->remFromUpdateList(this);
    Component::remove();
}

void ModuleBase::paintPart(QPainter *p)
{
    p->drawRoundedRect(m_area, 2, 2);
}

void ModuleBase::paint(QPainter *p, const QStyleOptionGraphicsItem *option,
                       QWidget *widget)
{
    Component::paint(p, option, widget);
    if (paintArt(p))
        paintOver(p);
    else
        paintPart(p);

    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    for (const Zone &z : m_zones) {
        if (!z.visible)
            continue;
        p->setPen(QPen(QColor(15, 15, 18), 0.6));
        p->setBrush(z.on ? QColor(47, 143, 70) : QColor(58, 58, 64));
        p->drawRoundedRect(z.r, 2, 2);
        drawText(p, z.r, z.text, QColor(235, 235, 235), 6);
    }
    p->restore();
}

#include "moc_modulebase.cpp"
