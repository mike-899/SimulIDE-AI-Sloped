/***************************************************************************
 *   QML (Qt Quick) application shell: shared engine and widget factory    *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#include "qmlui.h"

#include "circuitwidget.h"
#include "componentmodel.h"
#include "componentselector.h"
#include "inspector.h"
#include "mainwindow.h"
#include "qmlcontrollers.h"
#include "themetokens.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>

QmlUi *QmlUi::self()
{
    static QmlUi *inst = 0l;
    if (!inst)
        inst = new QmlUi(qApp);
    return inst;
}

QmlUi::QmlUi(QObject *parent) : QObject(parent)
{
    m_engine    = 0l;
    m_inspector = 0l;
}

bool QmlUi::enabled(bool settingValue)
{
    if (QCoreApplication::arguments().contains("--widgets-ui"))
        return false;
    if (QCoreApplication::arguments().contains("--qml-ui"))
        return true;
    if (qEnvironmentVariable("SIMULIDE_UI") == "widgets")
        return false;
    return settingValue;
}

void QmlUi::init(MainWindow *window, CircuitWidget *circuit,
                 ComponentSelector *selector)
{
    if (m_engine)
        return;
    m_engine = new QQmlEngine(this);
    m_engine->addImportPath("qrc:/qml");
    m_engine->addImageProvider("parts", new PartImageProvider(selector));
    m_engine->addImageProvider("icon", new IconImageProvider());

    m_inspector = new Inspector(this);

    QQmlContext *ctx = m_engine->rootContext();
    ctx->setContextProperty("Theme", new ThemeTokens(this));
    ctx->setContextProperty("Components", new ComponentModel(selector, this));
    ctx->setContextProperty("Sim", new SimController(circuit, this));
    ctx->setContextProperty("Files", new FileController(window, circuit, this));
    ctx->setContextProperty("Mcus", new McuModel(this));
    ctx->setContextProperty("Inspector", m_inspector);
    ctx->setContextProperty("Shell",
                            new ShellController(window, circuit, this));
}

QQuickWidget *QmlUi::createWidget(const QString &file, QWidget *parent,
                                  bool overlay)
{
    QQuickWidget *w = new QQuickWidget(m_engine, parent);
    w->setObjectName("qml_" + QString(file).remove(".qml"));
    w->setFocusPolicy(Qt::ClickFocus);
    if (overlay) {
        w->setAttribute(Qt::WA_AlwaysStackOnTop);
        w->setAttribute(Qt::WA_TranslucentBackground);
        w->setClearColor(Qt::transparent);
        w->setResizeMode(QQuickWidget::SizeViewToRootObject);
    } else
        w->setResizeMode(QQuickWidget::SizeRootObjectToView);

    connect(w, &QQuickWidget::statusChanged, w,
            [w, file](QQuickWidget::Status status) {
                if (status == QQuickWidget::Error)
                    for (const QQmlError &e : w->errors())
                        qWarning() << "QML" << file << e.toString();
            });
    // SIMULIDE_QML_DIR=<src/qml> loads the QML from disk (live editing)
    QString dir = qEnvironmentVariable("SIMULIDE_QML_DIR");
    w->setSource(dir.isEmpty() ? QUrl("qrc:/qml/" + file)
                               : QUrl::fromLocalFile(dir + "/" + file));
    m_widgets.append(w);
    return w;
}

QPoint QmlUi::globalPos(QQuickItem *item, qreal x, qreal y) const
{
    if (!item)
        return QCursor::pos();
    for (const QPointer<QQuickWidget> &w : m_widgets)
        if (w && w->quickWindow() == item->window())
            return w->mapToGlobal(item->mapToScene(QPointF(x, y)).toPoint());
    return QCursor::pos();
}

#include "moc_qmlui.cpp"
