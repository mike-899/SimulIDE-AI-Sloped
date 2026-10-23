/***************************************************************************
 *   Realistic (Tinkercad-style) part artwork registry.                     *
 *   Artwork: resources/parts/mit (MIT) and resources/parts/fritzing        *
 *   (CC-BY-SA 3.0), see the NOTICE files there.                            *
 ***************************************************************************/

#include "partart.h"

#include "circuit.h"
#include "component.h"
#include "mainwindow.h"

#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QSvgRenderer>

static QHash<QString, PartArtInfo> s_arts;
static QHash<QString, QSvgRenderer *> s_renderers;
static int s_enabled = -1; // -1 = not read from settings yet

static void add(const QString &type, const QString &res, QRectF rect,
                int rot = 0, QList<QLineF> leads = QList<QLineF>())
{
    PartArtInfo info;
    info.res     = ":/parts/" + res;
    info.rect    = rect;
    info.rot     = rot;
    info.leads   = leads;
    info.canvas  = true;
    s_arts[type] = info;
}

void PartArt::init()
{
    if (!s_arts.isEmpty())
        return;

    // Two terminal parts, pins at x=+-16 (inner end at +-8)
    add("Resistor", "mit/resistor.svg", QRectF(-16, -4, 32, 8));
    add("Diode", "fritzing/diode.svg", QRectF(-16, -5, 32, 10));
    add("Led", "mit/led.svg", QRectF(-6.6, -26, 13.2, 26), 0,
        { QLineF(-8, 0, -2.6, 0), QLineF(8, 0, 3.3, 0) });

    // Capacitors / inductor, pins at x=+-24 (inner end at +-16)
    add("Capacitor", "fritzing/capacitor.svg", QRectF(-5.3, -26, 10.6, 26), 0,
        { QLineF(-16, 0, -2.6, 0), QLineF(16, 0, 2.3, 0) });
    add("elCapacitor", "fritzing/elcapacitor.svg", QRectF(-4.65, -30, 9.3, 30),
        0, { QLineF(-16, 0, -1.4, 0), QLineF(16, 0, 1.1, 0) });
    add("Inductor", "fritzing/inductor.svg", QRectF(-24, -7, 48, 14));

    // Switches, pins at (+-16,0)
    add("Push", "mit/push_button.svg", QRectF(-16, -14.6, 32, 21.55));
    add("Switch", "mit/switch.svg", QRectF(-8.3, -17, 16.6, 18), 0,
        { QLineF(-8, 0, -3.8, 0), QLineF(8, 0, 3.8, 0) });

    // Potentiometer: pins A(-16,0) M(0,16) B(16,0)
    add("Potentiometer", "mit/potentiometer.svg", QRectF(-10, -6, 20, 20), 0,
        { QLineF(-8, 0, -10, 0), QLineF(8, 0, 10, 0) });

    // Sources
    add("Ground", "mit/ground.svg", QRectF(-7, -9, 14, 19));
    add("Rail", "mit/rail.svg", QRectF(-12.3, -8, 20.6, 16));

    // Active (TO-92 / TO-220 upright), pins B/G(-16,0) C/D(8,-16) E/S(8,16)
    add("BJT", "fritzing/bjt.svg", QRectF(-7, -22, 12.3, 24), 0,
        { QLineF(-8, 0, -5.2, 0), QLineF(8, -8, 8, 1), QLineF(8, 1, 3.45, 1),
          QLineF(8, 8, -0.85, 8), QLineF(-0.85, 8, -0.85, 1.5) });
    add("Mosfet", "fritzing/mosfet.svg", QRectF(-6, -22, 12.1, 24), 0,
        { QLineF(-8, 0, -2.4, 0), QLineF(8, -8, 8, 1), QLineF(8, 1, 2.5, 1),
          QLineF(8, 8, 0.05, 8), QLineF(0.05, 8, 0.05, 1.5) });

    // Parts fitted into their own m_area (null rect) or painting themselves
    add("AudioOut", "mit/buzzer.svg", QRectF());
    add("Servo", "mit/servo.svg", QRectF());
    add("RelaySPST", "fritzing/relay.svg", QRectF());
    add("WS2812", "mit/ws2812.svg", QRectF());

    // Motors / drivers (pins line up with the art edges)
    add("DcMotor", "mit/dcmotor.svg", QRectF(-32, -20, 72, 40));
    add("L298N", "mit/l298n.svg", QRectF(-48, -48, 96, 96));
    add("L293D", "mit/l293d.svg", QRectF(-20, -36, 40, 72));

    // Icon-only art (component keeps its own drawing on canvas)
    add("Seven Segment", "mit/sevensegment.svg", QRectF());
    add("KeyPad", "mit/keypad.svg", QRectF());
    add("LedBar", "fritzing/ledbar.svg", QRectF());
    add("Hd44780", "mit/hd44780.svg", QRectF());
    add("Lm555", "fritzing/lm555.svg", QRectF());
    add("VoltReg", "fritzing/voltreg.svg", QRectF());
    add("Pcd8544", "mit/pcd8544.svg", QRectF());
    add("Frequencimeter", "mit/frequencimeter.svg", QRectF());
    add("Current Source", "mit/currsource.svg", QRectF());
    add("SwitchDip", "mit/switchdip.svg", QRectF());
    add("Memory", "mit/memory.svg", QRectF());

    for (const QString &t : { "Seven Segment", "KeyPad", "LedBar", "Hd44780",
                              "Lm555", "VoltReg", "Pcd8544", "Frequencimeter",
                              "Current Source", "SwitchDip", "Memory" })
        s_arts[t].canvas = false;
}

bool PartArt::enabled()
{
    if (s_enabled < 0) {
        s_enabled = 1;
        if (MainWindow::self())
            s_enabled = MainWindow::self()
                                ->settings()
                                ->value("realisticParts", true)
                                .toBool()
                            ? 1
                            : 0;
    }
    return s_enabled == 1;
}

void PartArt::setEnabled(bool en)
{
    if (MainWindow::self())
        MainWindow::self()->settings()->setValue("realisticParts", en);

    Circuit *circ = Circuit::self();
    QList<Component *> comps;
    if (circ)
        comps = *circ->compList();

    for (Component *c : comps)
        c->artGeometryAboutToChange();
    s_enabled = en ? 1 : 0;
    for (Component *c : comps) {
        c->update();
        for (QGraphicsItem *ch : c->childItems())
            ch->update(); // Pins
    }
    if (circ)
        circ->update();
}

bool PartArt::has(const QString &type)
{
    init();
    auto it = s_arts.constFind(type);
    return (it != s_arts.constEnd()) && it.value().canvas;
}

bool PartArt::hasIcon(const QString &type)
{
    init();
    return s_arts.contains(type);
}

const PartArtInfo *PartArt::info(const QString &type)
{
    init();
    auto it = s_arts.constFind(type);
    if (it == s_arts.constEnd())
        return 0l;
    return &it.value();
}

QSvgRenderer *PartArt::renderer(const QString &res)
{
    QSvgRenderer *r = s_renderers.value(res);
    if (!r) {
        r                = new QSvgRenderer(res);
        s_renderers[res] = r;
    }
    return r->isValid() ? r : 0l;
}

QRectF PartArt::fitRect(QSvgRenderer *r, const QRectF &target, int rot)
{
    QSizeF s = r->viewBoxF().size();
    if (s.isEmpty())
        s = r->defaultSize();
    if ((rot / 90) % 2)
        s.transpose();

    double scale =
        qMin(target.width() / s.width(), target.height() / s.height());
    QSizeF size = s * scale;
    return QRectF(target.center().x() - size.width() / 2,
                  target.center().y() - size.height() / 2, size.width(),
                  size.height());
}

QRectF PartArt::artRect(const QString &type)
{
    const PartArtInfo *inf = info(type);
    if (!inf || inf->rect.isNull())
        return QRectF();
    QSvgRenderer *r = renderer(inf->res);
    if (!r)
        return QRectF();
    return fitRect(r, inf->rect, inf->rot);
}

QRectF PartArt::paintRes(QPainter *p, const QString &res, const QRectF &rect,
                         int rot)
{
    QSvgRenderer *r = renderer(res);
    if (!r)
        return QRectF();

    QRectF fr = fitRect(r, rect, rot);
    p->save();
    if (rot) {
        p->translate(fr.center());
        p->rotate(rot);
        QSizeF s = fr.size();
        if ((rot / 90) % 2)
            s.transpose();
        r->render(
            p, QRectF(-s.width() / 2, -s.height() / 2, s.width(), s.height()));
    } else
        r->render(p, fr);
    p->restore();
    return fr;
}

QRectF PartArt::paint(QPainter *p, const QString &type, const QString &res)
{
    const PartArtInfo *inf = info(type);
    if (!inf)
        return QRectF();

    if (!inf->leads.isEmpty()) {
        p->save();
        p->setPen(
            QPen(QColor(150, 150, 150), 1.6, Qt::SolidLine, Qt::RoundCap));
        for (const QLineF &l : inf->leads)
            p->drawLine(l);
        p->restore();
    }
    return paintRes(p, res.isEmpty() ? inf->res : res, inf->rect, inf->rot);
}

QIcon PartArt::icon(const QString &type)
{
    const PartArtInfo *inf = info(type);
    if (!inf)
        return QIcon();
    QSvgRenderer *r = renderer(inf->res);
    if (!r)
        return QIcon();

    QIcon ic;
    for (int dpr = 1; dpr <= 2; dpr++) // Card icon size is 40x30
    {
        const int W = 40 * dpr, H = 30 * dpr;
        QPixmap pm(W, H);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        r->render(&p,
                  fitRect(r, QRectF(dpr, dpr, W - 2 * dpr, H - 5 * dpr), 0));
        p.end();
        pm.setDevicePixelRatio(dpr);
        ic.addPixmap(pm);
    }
    return ic;
}
