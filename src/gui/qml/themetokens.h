/***************************************************************************
 *   Material 3 theme tokens for the QML shell                             *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#ifndef THEMETOKENS_H
#define THEMETOKENS_H

#include <QColor>
#include <QQmlPropertyMap>

// Exposed to QML as "Theme". Every Material 3 colour role of the active
// ThemeManager palette is a key (Theme.primary, Theme.surfaceContainerHigh,
// ...), plus the shape scale (Theme.shapeL ...) and Theme.dark. Keys are
// refreshed in place when the theme changes so bindings update (QML
// animates the colours with ColorAnimation).
class ThemeTokens : public QQmlPropertyMap
{
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY darkChanged)

  public:
    explicit ThemeTokens(QObject *parent = 0l);

    bool dark() const;
    void setDark(bool dark);

    Q_INVOKABLE void toggle();
    // Colour with alpha, for state layers: Theme.alpha(Theme.onSurface, .08)
    Q_INVOKABLE QColor alpha(const QColor &c, qreal a) const;

  signals:
    void darkChanged();

  private:
    void refresh();
};

#endif
