/***************************************************************************
 *   Realistic (Tinkercad-style) part artwork registry.                     *
 *   Maps a component type to an SVG resource (resources/parts) and to the   *
 *   rectangle (in component coordinates) the art is fitted into, so pins    *
 *   keep their positions.                                                   *
 ***************************************************************************/

#ifndef PARTART_H
#define PARTART_H

#include <QColor>
#include <QIcon>
#include <QLineF>
#include <QList>
#include <QRectF>
#include <QString>

class QPainter;
class QSvgRenderer;

// Moves a region of the art by `shift` (both in viewBox units). Used to line up
// a group of header holes with the package pins when the real board layout
// differs from the SimulIDE package (see the Arduino boards).
struct PartArtWarp {
    QRectF src;    // Region to move
    QPointF shift; // Offset to apply
    QColor fill;   // Colour left behind where the region was
};

struct PartArtInfo {
    QString res; // Qt resource path of the SVG
    QRectF rect; // Target rect in component coords (art fitted, aspect kept,
                 // centered)
    int rot;     // Rotation of the art (degrees, multiple of 90)
    QList<QLineF> leads;      // Metal wires drawn under the art (leg -> pin)
    bool canvas;              // false: art used only as selector icon
    QList<PartArtWarp> warps; // Regions of the art moved to match the pins
    double leadW;             // Width of the leads (px)
    bool noLegs; // Module / chip: no protruding pin legs, pins sit on the art
                 // (header holes, terminals, leg tips)
};

class PartArt
{
  public:
    static bool enabled();
    static void setEnabled(bool en); // Persists in settings, refreshes circuit

    static bool has(const QString &type);     // Has canvas art
    static bool hasIcon(const QString &type); // Has any art
    static bool noLegs(const QString &type);  // Pin legs hidden (see noLegs)
    static const PartArtInfo *info(const QString &type);

    // Rect actually covered by the art (component coords)
    static QRectF artRect(const QString &type);

    // Paint art for type; returns rect painted (component coords)
    static QRectF paint(QPainter *p, const QString &type,
                        const QString &res = QString());

    // Paint an arbitrary art resource fitted into rect
    static QRectF
    paintRes(QPainter *p, const QString &res, const QRectF &rect, int rot = 0,
             const QList<PartArtWarp> &warps = QList<PartArtWarp>());

    // Icon for component selector cards (null icon if no art)
    static QIcon icon(const QString &type);

    // Selector card icon, never empty: part art, else iconFile, else a
    // generated DIP chip (pin count from the item's package) or a
    // category icon.
    static QIcon cardIcon(const QString &type, const QString &name,
                          const QString &category, const QString &iconFile);

  private:
    static void init();
    static int packagePins(const QString &name);
    static QIcon chipIcon(int pins, const QString &text);
    static QSvgRenderer *renderer(const QString &res);
    static QRectF fitRect(QSvgRenderer *r, const QRectF &target, int rot);
};

#endif
