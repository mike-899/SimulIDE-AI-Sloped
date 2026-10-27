/***************************************************************************
 *   QML (Qt Quick) application shell: shared engine and widget factory    *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#ifndef QMLUI_H
#define QMLUI_H

#include <QList>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>

class QQmlEngine;
class QQuickWidget;
class QWidget;
class MainWindow;
class CircuitWidget;
class ComponentSelector;
class Inspector;
class QQuickItem;

// All chrome parts (top bar, component panel, inspector, toolbar, code
// panel header) are QQuickWidgets sharing one QQmlEngine, so they share
// the C++ models exposed as context properties:
//   Theme, Components, Sim, Files, Mcus, Inspector, Shell
// See docs/QML_UI.md.
class QmlUi : public QObject
{
    Q_OBJECT

  public:
    static QmlUi *self();

    // QML shell enabled? Setting "ui/qml" (default true); "--widgets-ui"
    // or SIMULIDE_UI=widgets force the classic Widgets chrome.
    static bool enabled(bool settingValue);

    void init(MainWindow *window, CircuitWidget *circuit,
              ComponentSelector *selector);

    // Loads qrc:/qml/<file>. Overlays get a transparent background and
    // are sized to their root item (SizeViewToRootObject).
    QQuickWidget *createWidget(const QString &file, QWidget *parent,
                               bool overlay = false);

    // Global screen position of a point of a QML item, whatever
    // QQuickWidget hosts it.
    QPoint globalPos(QQuickItem *item, qreal x, qreal y) const;

    QQmlEngine *engine() const
    {
        return m_engine;
    }
    Inspector *inspector() const
    {
        return m_inspector;
    }

  private:
    explicit QmlUi(QObject *parent);

    QQmlEngine *m_engine;
    Inspector *m_inspector;
    QList<QPointer<QQuickWidget>> m_widgets;
};

#endif
