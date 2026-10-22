/***************************************************************************
 *   Realistic (Tinkercad-style) part artwork registry.                     *
 *   Maps a component type to an SVG resource (resources/parts) and to the   *
 *   rectangle (in component coordinates) the art is fitted into, so pins    *
 *   keep their positions.                                                   *
 ***************************************************************************/

#ifndef PARTART_H
#define PARTART_H

#include <QIcon>
#include <QLineF>
#include <QList>
#include <QRectF>
#include <QString>

class QPainter;
class QSvgRenderer;

struct PartArtInfo {
    QString res; // Qt resource path of the SVG
    QRectF rect; // Target rect in component coords (art fitted, aspect kept,
                 // centered)
    int rot;     // Rotation of the art (degrees, multiple of 90)
    QList<QLineF> leads; // Metal wires drawn under the art (leg -> pin)
    bool canvas;         // false: art used only as selector icon
};

class PartArt
{
  public:
    static bool enabled();
    static void setEnabled(bool en); // Persists in settings, refreshes circuit

    static bool has(const QString &type);     // Has canvas art
    static bool hasIcon(const QString &type); // Has any art
    static const PartArtInfo *info(const QString &type);

    // Rect actually covered by the art (component coords)
    static QRectF artRect(const QString &type);

    // Paint art for type; returns rect painted (component coords)
    static QRectF paint(QPainter *p, const QString &type,
                        const QString &res = QString());

    // Paint an arbitrary art resource fitted into rect
    static QRectF paintRes(QPainter *p, const QString &res, const QRectF &rect,
                           int rot = 0);

    // Icon for component selector cards (null icon if no art)
    static QIcon icon(const QString &type);

  private:
    static void init();
    static QSvgRenderer *renderer(const QString &res);
    static QRectF fitRect(QSvgRenderer *r, const QRectF &target, int rot);
};

#endif
