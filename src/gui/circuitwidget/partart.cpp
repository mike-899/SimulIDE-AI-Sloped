/***************************************************************************
 *   Realistic (Tinkercad-style) part artwork registry.                     *
 *   Artwork: resources/parts/mit (MIT) and resources/parts/fritzing        *
 *   (CC-BY-SA 3.0), see the NOTICE files there.                            *
 ***************************************************************************/

#include "partart.h"

#include "circuit.h"
#include "component.h"
#include "mainwindow.h"
#include "thememanager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRegularExpression>
#include <QSettings>
#include <QSvgRenderer>

static QHash<QString, PartArtInfo> s_arts;
static QHash<QString, QSvgRenderer *> s_renderers;
static int s_enabled = -1; // -1 = not read from settings yet

static void add(const QString &type, const QString &res, QRectF rect,
                int rot = 0, QList<QLineF> leads = QList<QLineF>(),
                QList<PartArtWarp> warps = QList<PartArtWarp>())
{
    PartArtInfo info;
    info.res     = ":/parts/" + res;
    info.rect    = rect;
    info.rot     = rot;
    info.leads   = leads;
    info.canvas  = true;
    info.warps   = warps;
    info.leadW   = 1.6;
    info.noLegs  = false;
    s_arts[type] = info;
}

// Arduino boards (Fritzing art). The art is rotated and scaled (pitch 7.2 ->
// 8 px) so the header holes sit on the package pins. Where the real board has a
// different hole layout than the SimulIDE package a header strip is moved.
static void addBoard(const QString &name, const QString &res, QRectF rect,
                     QList<QLineF> leads, QList<PartArtWarp> warps,
                     int rot = 270)
{
    add("board:" + name, res, rect, rot, leads, warps);
    s_arts["board:" + name].canvas = false;
}

void PartArt::init()
{
    if (!s_arts.isEmpty())
        return;

    // Two terminal parts, pins at x=+-16 (inner end at +-8)
    add("Resistor", "mit/resistor.svg", QRectF(-16, -4, 32, 8));
    add("Diode", "fritzing/diode.svg", QRectF(-16, -5, 32, 10));
    // Dome only: the legs are drawn as straight leads from the pins
    add("Led", "mit/led_red.svg", QRectF(-6.6, -26.4, 13.2, 20.76), 0,
        { QLineF(-8, 0, -2.8, -8), QLineF(8, 0, 2.8, -8) });

    // Capacitors / inductor, pins at x=+-24 (inner end at +-16)
    add("Capacitor", "fritzing/capacitor.svg", QRectF(-5.3, -26, 10.6, 26), 0,
        { QLineF(-16, 0, -2.6, -5), QLineF(16, 0, 2.3, -5) });
    add("elCapacitor", "fritzing/elcapacitor.svg", QRectF(-4.65, -30, 9.3, 30),
        0, { QLineF(-16, 0, -1.4, -5), QLineF(16, 0, 1.1, -5) });
    add("Inductor", "fritzing/inductor.svg", QRectF(-24, -7, 48, 14));

    // Switches, pins at (+-16,0)
    // Body only; the pins end on its sides (the click button covers it)
    add("Push", "mit/push_button.svg", QRectF(-8, -8, 16, 16));
    add("Switch", "mit/switch.svg", QRectF(-8.3, -17, 16.6, 18), 0,
        { QLineF(-8, 0, -3.8, -4), QLineF(8, 0, 3.8, -4) });

    // Potentiometer: pins A(-16,0) M(0,16) B(16,0)
    add("Potentiometer", "mit/potentiometer.svg", QRectF(-10, -6, 20, 20), 0,
        { QLineF(-8, 0, -10, 0), QLineF(8, 0, 10, 0) });

    // Sources
    add("Ground", "mit/ground.svg", QRectF(-7, -9, 14, 19));
    add("Rail", "mit/rail.svg", QRectF(-12.3, -8, 20.6, 16));

    // Active (TO-92 / TO-220 upright), pins B/G(-16,0) C/D(8,-16) E/S(8,16)
    add("BJT", "fritzing/bjt.svg", QRectF(-7, -22, 12.3, 24), 0,
        { QLineF(-8, 0, -5.2, 0), QLineF(8, -8, 3.45, 1),
          QLineF(8, 8, -0.85, 1.5) });
    add("Mosfet", "fritzing/mosfet.svg", QRectF(-6, -22, 12.1, 24), 0,
        { QLineF(-8, 0, -2.4, 0), QLineF(8, -8, 2.5, 1),
          QLineF(8, 8, 0.05, 1.5) });

    // Parts fitted into their own m_area (null rect) or painting themselves
    add("AudioOut", "mit/buzzer.svg", QRectF());
    add("Servo", "fritzing/servo.svg", QRectF(-40, -18, 93.6, 36));
    add("RelaySPST", "fritzing/relay.svg", QRectF());
    add("WS2812", "mit/ws2812.svg", QRectF());

    // Arduino boards
    {
        const QColor pcb(0x0f, 0x73, 0x91);
        const QPointF d8(-2.88, 0); // Digital 8..13 header: 0.16" gap
        const QList<PartArtWarp> unoWarps = {
            { QRectF(67.0, 3.0, 74.6, 8.5), d8, pcb },
            { QRectF(84.0, 11.5, 57.6, 13.0), d8, pcb }
        };
        addBoard("arduino_uno", "fritzing/arduino_uno.svg",
                 QRectF(-4, 0, 168, 235.969), QList<QLineF>(), unoWarps);
        addBoard("arduino_duemilanove", "fritzing/arduino_uno.svg",
                 QRectF(-4, 0, 168, 235.969), QList<QLineF>(), unoWarps);
        addBoard("arduino_leonardo", "fritzing/arduino_leonardo.svg",
                 QRectF(-4, 0, 168, 221.68), QList<QLineF>(),
                 { { QRectF(54.0, 3.0, 74.4, 8.5), d8, pcb },
                   { QRectF(55.0, 11.5, 73.4, 13.0), d8, pcb } });

        // Mega: the analog and power headers sit one pitch off the package
        const QPointF one(-7.2, 0);
        addBoard("arduino_mega", "fritzing/arduino_mega.svg",
                 QRectF(-4, 0, 168, 339.97), QList<QLineF>(),
                 { { QRectF(67.0, 3.0, 74.5, 8.5), d8, pcb },
                   { QRectF(83.9, 11.5, 57.6, 16.3), d8, pcb },
                   { QRectF(108.0, 121.0, 45.0, 28.5), one, pcb },
                   { QRectF(157.5, 133.9, 60.5, 15.6), one, pcb },
                   { QRectF(221.5, 133.9, 59.5, 15.6), one, pcb } });

        // Nano: pin ends are 8 px from the holes
        QList<QLineF> nano;
        for (int pos = 2; pos <= 16; pos++) {
            nano << QLineF(0, 8 * pos, 8, 8 * pos);
            nano << QLineF(64, 8 * pos, 56, 8 * pos);
        }
        addBoard("arduino_nano", "fritzing/arduino_nano.svg",
                 QRectF(4, 4, 56, 141.76), nano, {}, 0);
    }

    // Motors / drivers (pins line up with the art edges)
    add("DcMotor", "fritzing/dcmotor.svg",
        QRectF(-34.72, -14.615, 61.42, 29.59));
    add("L298N", "mit/l298n.svg", QRectF(-48, -48, 96, 96));
    {
        // DIP-16 turned 90 degrees: legs (pitch 8 px) end 7 px short of the
        // pins
        QList<QLineF> legs;
        for (int i = 0; i < 8; i++) {
            double y = -28 + 8 * i;
            legs << QLineF(-20, y, -12, y) << QLineF(20, y, 12, y);
        }
        add("L293D", "fritzing/l293d.svg", QRectF(-13.2, -32, 26.4, 64), 90,
            legs);
        s_arts["L293D"].leadW = 2.4;
    }

    // ---- Modules, sensors, power parts and named presets ------------------
    // (original art generated by resources/parts/tools/modules.py; to use
    // other art just change the file name / rect here)
    // noLegs: pins sit on the art (header holes, terminals, leg tips)
    auto mod = [](const QString &type, const QString &file, QRectF rect,
                  bool noLegs, QList<QLineF> leads = QList<QLineF>()) {
        add(type, "modules/" + file + ".svg", rect, 0, leads);
        s_arts[type].noLegs = noLegs;
    };
    s_arts["L298N"].noLegs = true;
    s_arts["L293D"].noLegs = true;
    mod("TB6612", "tb6612", QRectF(-28, -36, 56, 72), true);
    mod("Uln2003", "uln2003", QRectF(-20, -36, 40, 72), true);
    mod("Stepper28BYJ", "stepper28byj", QRectF(-64, -48, 128, 96), false);
    mod("ActiveBuzzer", "buzzer_active", QRectF(), false);
    mod("PassiveBuzzer", "buzzer_passive", QRectF(), false);
    mod("Ldr", "ldr", QRectF(-16, -10, 32, 20), false);
    mod("Thermistor", "ntc", QRectF(-16, -8, 32, 16), false);
    mod("SlidePot", "slidepot", QRectF(-12, -6, 24, 16), false);
    mod("PirSensor", "pir", QRectF(-24, -28, 48, 56), true);
    mod("IrObstacle", "irsensor", QRectF(-20, -24, 40, 48), true);
    mod("SoilMoisture", "soil", QRectF(-24, -36, 48, 72), true);
    mod("SoundSensor", "sound", QRectF(-24, -28, 48, 56), true);
    mod("Joystick", "joystick", QRectF(-28, -28, 56, 56), true);
    mod("RotaryEncoder", "encoder", QRectF(-28, -24, 56, 48), true);
    mod("Battery9V", "battery9v", QRectF(-14, -24, 28, 48), true);
    mod("Battery2AA", "aa2", QRectF(-32, -12, 72, 24), true);
    mod("Battery4AA", "aa4", QRectF(-32, -20, 72, 40), true);
    mod("CoinCell", "cr2032", QRectF(-16, -16, 32, 32), true);
    mod("TactileButton", "tactile", QRectF(-12, -12, 24, 24), true);
    mod("SlideSwitch", "slideswitch", QRectF(-12, -8, 24, 12), false);
    mod("ToggleSwitch", "toggle", QRectF(-12, -24, 24, 28), false);
    mod("ReedSwitch", "reed", QRectF(-16, -5, 32, 10), false);
    mod("Fuse", "fuse", QRectF(-16, -5, 32, 10), false);
    mod("Optocoupler", "pc817", QRectF(-12, -8, 24, 16), true);
    mod("RelayModule", "relaymod", QRectF(-32, -24, 64, 48), true);
    mod("MosfetModule", "irf520", QRectF(-32, -28, 64, 56), true);
    // Named presets of existing parts (same pins as the generic part)
    const QList<QLineF> bjtLeads = { QLineF(-8, 0, -5.2, 0),
                                     QLineF(8, -8, 8, 1),
                                     QLineF(8, 1, 3.45, 1),
                                     QLineF(8, 8, -0.85, 8),
                                     QLineF(-0.85, 8, -0.85, 1.5) };
    mod("Q2N2222", "t2n2222", QRectF(-7, -22, 12.3, 24), false, bjtLeads);
    mod("BC547", "bc547", QRectF(-7, -22, 12.3, 24), false, bjtLeads);
    mod("TIP120", "tip120", QRectF(-8, -24, 16, 26), false,
        { QLineF(-8, 0, -5, 0), QLineF(8, -8, 8, 4.5), QLineF(8, 4.5, 0, 4.5),
          QLineF(0, 4.5, 0, 1.5), QLineF(8, 8, 5, 8), QLineF(5, 8, 5, 6),
          QLineF(5, 3, 5, 1.5) }); // C wire hops over the E wire
    mod("D1N4007", "d1n4007", QRectF(-16, -5, 32, 10), false);
    mod("D1N4148", "d1n4148", QRectF(-16, -5, 32, 10), false);
    mod("D1N4733A", "zener", QRectF(-16, -5, 32, 10), false);
    const QList<QLineF> regLeads = { QLineF(-10, 0, -5, 0), QLineF(10, 0, 5, 0),
                                     QLineF(0, 10, 0, 1.5) };
    mod("Reg7805", "reg7805", QRectF(-10, -24, 20, 26), false, regLeads);
    mod("Lm317", "lm317", QRectF(-10, -24, 20, 26), false, regLeads);

    // Icon-only art (component keeps its own drawing on canvas)
    add("Seven Segment", "fritzing/sevensegment.svg", QRectF());
    add("KeyPad", "fritzing/keypad.svg", QRectF());
    add("LedBar", "fritzing/ledbar.svg", QRectF());
    add("Hd44780", "fritzing/hd44780.svg", QRectF());
    add("Lm555", "fritzing/lm555.svg", QRectF());
    add("VoltReg", "fritzing/voltreg.svg", QRectF());
    add("Pcd8544", "mit/pcd8544.svg", QRectF());
    add("Frequencimeter", "mit/frequencimeter.svg", QRectF());
    add("Current Source", "mit/currsource.svg", QRectF());
    add("SwitchDip", "mit/switchdip.svg", QRectF());
    add("Memory", "fritzing/memory.svg", QRectF());
    add("Ks0108", "fritzing/glcd_128x64.svg", QRectF());
    add("Voltage Source", "mit/psu.svg", QRectF());

    for (const QString &t :
         { "Seven Segment", "KeyPad", "LedBar", "Hd44780", "Lm555", "VoltReg",
           "Pcd8544", "Frequencimeter", "Current Source", "SwitchDip", "Memory",
           "Ks0108", "Voltage Source" })
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

bool PartArt::noLegs(const QString &type)
{
    init();
    auto it = s_arts.constFind(type);
    return (it != s_arts.constEnd()) && it.value().canvas && it.value().noLegs;
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
                         int rot, const QList<PartArtWarp> &warps)
{
    QSvgRenderer *r = renderer(res);
    if (!r)
        return QRectF();

    QRectF fr = fitRect(r, rect, rot);
    p->save();
    if (rot) {
        p->translate(fr.center());
        p->rotate(rot);
    }
    QSizeF s = fr.size();
    if ((rot / 90) % 2)
        s.transpose();
    QRectF box =
        rot ? QRectF(-s.width() / 2, -s.height() / 2, s.width(), s.height())
            : fr;

    if (warps.isEmpty())
        r->render(p, box);
    else {
        // Work in art (viewBox) units, draw the art without the moved regions,
        // then draw those regions again at their new place
        QRectF vb = r->viewBoxF();
        double sc = box.width() / vb.width();
        p->translate(box.center());
        p->scale(sc, sc);
        p->translate(-vb.center());

        QPainterPath keep;
        keep.addRect(vb);
        QPainterPath moved;
        for (const PartArtWarp &w : warps) {
            moved.addRect(w.src);
            moved.addRect(w.src.translated(w.shift));
        }
        p->setClipPath(keep.subtracted(moved));
        r->render(p, vb);
        for (const PartArtWarp &w : warps) {
            p->setClipRect(w.src);
            p->fillRect(w.src, w.fill);
            p->setClipRect(w.src.translated(w.shift));
            p->save();
            p->translate(w.shift);
            r->render(p, vb);
            p->restore();
        }
    }
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
        p->setPen(QPen(QColor(150, 150, 150), inf->leadW, Qt::SolidLine,
                       Qt::FlatCap));
        for (const QLineF &l : inf->leads)
            p->drawLine(l);
        p->restore();
    }
    return paintRes(p, res.isEmpty() ? inf->res : res, inf->rect, inf->rot,
                    inf->warps);
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

int PartArt::packagePins(const QString &name)
{
    // name -> pin count, from the item lists in the data folder
    static QHash<QString, int> pins;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        QDir dataDir(QApplication::applicationDirPath());
        dataDir.cd("../share/simulide/data");
        QRegularExpression itemRe(
            "<item\\s+name=\"([^\"]+)\"[^>]*?package=\"([^\"]+)\"");
        QRegularExpression pinsRe("<package[^>]*?pins=\"(\\d+)\"");
        QHash<QString, int> pkgPins;
        for (const QString &xml : dataDir.entryList({ "*.xml" }, QDir::Files)) {
            QFile f(dataDir.filePath(xml));
            if (!f.open(QIODevice::ReadOnly))
                continue;
            QString txt = QString::fromUtf8(f.readAll());
            auto it     = itemRe.globalMatch(txt);
            while (it.hasNext()) {
                auto m      = it.next();
                QString pkg = m.captured(2);
                if (!pkgPins.contains(pkg)) {
                    QFile pf(dataDir.filePath(pkg + ".package"));
                    int n = 0;
                    if (pf.open(QIODevice::ReadOnly)) {
                        auto pm =
                            pinsRe.match(QString::fromUtf8(pf.read(4096)));
                        if (pm.hasMatch())
                            n = pm.captured(1).toInt();
                    }
                    pkgPins[pkg] = n;
                }
                pins[m.captured(1)] = pkgPins[pkg];
            }
        }
    }
    return pins.value(name, 0);
}

QIcon PartArt::chipIcon(int pins, const QString &text)
{
    // Generic DIP package: black body, silver legs, notch and label
    int perSide = qBound(2, (pins > 0 ? pins : 16) / 2, 20);
    QIcon ic;
    for (int dpr = 1; dpr <= 2; dpr++) {
        QPixmap pm(40 * dpr, 30 * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);

        double bw = qBound(14.0, perSide * 2.0 + 2, 38.0);
        QRectF body(20 - bw / 2, 8, bw, 13);
        double step = (bw - 2) / perSide;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(190, 190, 195));
        for (int i = 0; i < perSide; i++) {
            double x = body.left() + 1 + step * (i + 0.5) - step * 0.22;
            p.drawRect(QRectF(x, body.top() - 3, step * 0.44, 3));
            p.drawRect(QRectF(x, body.bottom(), step * 0.44, 3));
        }
        QLinearGradient g(body.topLeft(), body.bottomLeft());
        g.setColorAt(0, QColor(70, 72, 76));
        g.setColorAt(1, QColor(28, 29, 31));
        p.setBrush(g);
        p.setPen(QPen(QColor(10, 10, 10), 0.6));
        p.drawRoundedRect(body, 1.2, 1.2);
        p.setBrush(QColor(15, 15, 15));
        p.setPen(Qt::NoPen);
        p.drawPie(QRectF(body.left() - 2.2, body.center().y() - 2.2, 4.4, 4.4),
                  -90 * 16, 180 * 16);

        QFont f = p.font();
        f.setPixelSize(6);
        f.setBold(true);
        p.setFont(f);
        p.setPen(QColor(220, 220, 220));
        for (int px = 6; px > 3; px--) { // Shrink label to fit the body
            f.setPixelSize(px);
            p.setFont(f);
            if (p.fontMetrics().boundingRect(text).width() <= body.width() - 4)
                break;
        }
        p.drawText(body.adjusted(2, 0, 0, 0), Qt::AlignCenter, text);

        if (pins > 0) { // Pin count under the chip
            f.setPixelSize(5);
            f.setBold(false);
            p.setFont(f);
            p.setPen(ThemeManager::self()->textColor());
            p.drawText(QRectF(0, 24, 40, 6), Qt::AlignCenter,
                       QString("DIP-%1").arg(pins));
        }
        p.end();
        ic.addPixmap(pm);
    }
    return ic;
}

QIcon PartArt::cardIcon(const QString &type, const QString &name,
                        const QString &category, const QString &iconFile)
{
    if (hasIcon(type))
        return icon(type); // Realistic art

    if (type == "Arduino") // One card per board
    {
        static const QHash<QString, QString> boards = {
            { "Arduino Uno", "board:arduino_uno" },
            { "Arduino Mega", "board:arduino_mega" },
            { "Arduino Nano", "board:arduino_nano" },
            { "Arduino Leonardo", "board:arduino_leonardo" },
            { "Duemilanove (m168)", "board:arduino_duemilanove" }
        };
        QString key = boards.value(name);
        if (hasIcon(key))
            return icon(key);
    }

    if (!iconFile.isEmpty() && QFile::exists(iconFile)) {
        QIcon ic(iconFile);
        if (!ic.availableSizes().isEmpty())
            return ic;
    }
    int pins     = packagePins(name);
    QString cat  = category.split("/").first();
    QString root = cat.toUpper();
    bool chip    = pins > 0 || type == "PIC" || type == "AVR" ||
                type == "Subcircuit" || type.isEmpty() ||
                root.contains("LOGIC") || root.contains("IC") ||
                root.contains("MICRO") || root.contains("MEMOR") ||
                root.contains("PIC") || root.contains("AVR");
    if (chip) {
        QString text = (type == "PIC" || type == "AVR") ? type : name;
        if (text.size() > 7)
            text = text.left(7);
        return chipIcon(pins, text);
    }
    // Category icon (Material Symbols)
    QString mat = "widgets";
    if (root.contains("SOURCE"))
        mat = "input";
    else if (root.contains("SWITCH") || root.contains("PASSIVE"))
        mat = "tune";
    else if (root.contains("METER") || root.contains("OUTPUT"))
        mat = "image";
    return ThemeManager::icon(mat);
}
