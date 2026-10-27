/***************************************************************************
 *   Controllers exposed to the QML shell                                  *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#include "qmlcontrollers.h"

#include "circuitwidget.h"
#include "mainwindow.h"
#include "mcucomponent.h"
#include "qmlui.h"
#include "thememanager.h"

#include <QMenu>
#include <QQmlPropertyMap>
#include <QQuickItem>
#include <QToolTip>

//------------------------------------------------------------------------
// SimController

SimController::SimController(CircuitWidget *circuit, QObject *parent) :
    QObject(parent), m_circuit(circuit)
{
    m_speed = 0;
    connect(m_circuit, &CircuitWidget::simStateChanged, this,
            &SimController::stateChanged);
    connect(m_circuit->powerCircAct, &QAction::changed, this,
            &SimController::stateChanged);
    connect(m_circuit->pauseSimAct, &QAction::changed, this,
            &SimController::stateChanged);
    connect(m_circuit, &CircuitWidget::rateChanged, this, [this](int rate) {
        if (rate == m_speed)
            return;
        m_speed = rate;
        emit speedChanged();
    });
}

QString SimController::state() const
{
    return m_circuit->simState();
}

bool SimController::canToggle() const
{
    return m_circuit->powerCircAct->isEnabled() && state() != "Debug";
}

bool SimController::canPause() const
{
    return m_circuit->pauseSimAct->isEnabled();
}

void SimController::toggle()
{
    if (canToggle())
        m_circuit->powerCirc();
}

void SimController::start()
{
    if (state() == "Off")
        m_circuit->powerCircOn();
}

void SimController::stop()
{
    if (state() != "Off")
        m_circuit->powerCircOff();
}

void SimController::pause()
{
    m_circuit->pauseSimAct->trigger();
}

//------------------------------------------------------------------------
// FileController

FileController::FileController(MainWindow *window, CircuitWidget *circuit,
                               QObject *parent) :
    QObject(parent), m_window(window), m_circuit(circuit)
{
    connect(m_window, &QWidget::windowTitleChanged, this,
            &FileController::titleChanged);
}

QString FileController::title() const
{
    QString t = m_window->windowTitle().section("  -  ", 1);
    if (t.endsWith('*'))
        t.chop(1);
    return t;
}

bool FileController::modified() const
{
    return m_window->windowTitle().endsWith('*');
}

void FileController::newCircuit()
{
    m_circuit->newCircAct->trigger();
}

void FileController::open()
{
    m_circuit->openCircAct->trigger();
}

void FileController::save()
{
    m_circuit->saveCircAct->trigger();
}

void FileController::saveAs()
{
    m_circuit->saveCircAsAct->trigger();
}

//------------------------------------------------------------------------
// McuModel

McuModel::McuModel(QObject *parent) : QObject(parent)
{
    m_current = -1;
    connect(McuSignals::self(), SIGNAL(mcuListChanged()), this, SLOT(update()));
    connect(McuSignals::self(), SIGNAL(activeMcuChanged(McuComponent *)), this,
            SLOT(update()));
    update();
}

void McuModel::update()
{
    m_mcus.clear();
    m_current = -1;
    for (McuComponent *mcu : McuComponent::mcuList()) {
        QVariantMap m;
        m["id"]     = mcu->itemID();
        m["device"] = mcu->device();
        m["label"]  = mcu->itemID() + " (" + mcu->device() + ")";
        if (mcu == McuComponent::self())
            m_current = m_mcus.size();
        m_mcus.append(m);
    }
    emit changed();
}

void McuModel::select(int index)
{
    if (index < 0 || index >= m_mcus.size())
        return;
    MainWindow::self()->selectMcu(
        m_mcus.at(index).toMap().value("id").toString());
}

//------------------------------------------------------------------------
// ShellController

ShellController::ShellController(MainWindow *window, CircuitWidget *circuit,
                                 QObject *parent) :
    QObject(parent), m_window(window), m_circuit(circuit)
{
    QQmlPropertyMap *acts = new QQmlPropertyMap(this);
    const QList<QPair<const char *, QAction *>> list = {
        { "copy", circuit->copyAct },
        { "paste", circuit->pasteAct },
        { "remove", circuit->deleteAct },
        { "undo", circuit->undoAct },
        { "redo", circuit->redoAct },
        { "rotateCW", circuit->rotateCWAct },
        { "rotateCCW", circuit->rotateCCWAct },
        { "hFlip", circuit->hFlipAct },
        { "vFlip", circuit->vFlipAct },
        { "zoomIn", circuit->zoomInAct },
        { "zoomOut", circuit->zoomOutAct },
        { "zoomFit", circuit->zoomFitAct },
        { "zoomReset", circuit->zoomResetAct },
        { "pause", circuit->pauseSimAct },
        { "newCirc", circuit->newCircAct },
        { "openCirc", circuit->openCircAct },
        { "saveCirc", circuit->saveCircAct },
    };
    for (auto &p : list)
        acts->insert(p.first, QVariant::fromValue<QObject *>(p.second));
    m_actions = acts;

    connect(m_window, &MainWindow::panelsChanged, this,
            &ShellController::panelsChanged);
    connect(m_window->realisticPartsAction(), &QAction::toggled, this,
            &ShellController::togglesChanged);
    connect(m_window->spiceAction(), &QAction::toggled, this,
            &ShellController::togglesChanged);
}

bool ShellController::componentsVisible() const
{
    return m_window->isComponentPanelVisible();
}

void ShellController::setComponentsVisible(bool v)
{
    if (v && m_window->isCodePanelVisible())
        m_window->setCodePanelVisible(false);
    m_window->setComponentPanelVisible(v);
}

bool ShellController::codeVisible() const
{
    return m_window->isCodePanelVisible();
}

void ShellController::setCodeVisible(bool v)
{
    m_window->setCodePanelVisible(v);
}

int ShellController::sidePage() const
{
    return m_window->sidePage();
}

void ShellController::setSidePage(int page)
{
    m_window->setSidePage(page);
}

bool ShellController::realisticParts() const
{
    return m_window->realisticPartsAction()->isChecked();
}

void ShellController::setRealisticParts(bool on)
{
    m_window->realisticPartsAction()->setChecked(on);
}

bool ShellController::spice() const
{
    return m_window->spiceAction()->isChecked();
}

void ShellController::setSpice(bool on)
{
    m_window->spiceAction()->setChecked(on);
}

QString ShellController::version() const
{
    return QString(APP_VERSION);
}

void ShellController::popupMenu(QQuickItem *anchor)
{
    hideToolTip();
    QPoint pos =
        QmlUi::self()->globalPos(anchor, 0, anchor ? anchor->height() + 4 : 0);
    m_window->mainMenu()->popup(pos);
}

int ShellController::popupChoice(QQuickItem *anchor, const QStringList &items,
                                 int current)
{
    hideToolTip();
    QMenu menu;
    menu.setObjectName("qmlChoiceMenu");
    QAction *selected = 0l;
    for (int i = 0; i < items.size(); i++) {
        QAction *act = menu.addAction(items.at(i));
        act->setData(i);
        act->setCheckable(true);
        act->setChecked(i == current);
        if (i == current)
            selected = act;
    }
    if (anchor)
        menu.setMinimumWidth(int(anchor->width()));
    QPoint pos =
        QmlUi::self()->globalPos(anchor, 0, anchor ? anchor->height() + 4 : 0);
    QAction *act = menu.exec(pos, selected);
    return act ? act->data().toInt() : -1;
}

void ShellController::showToolTip(QQuickItem *anchor, const QString &text)
{
    if (!anchor || text.isEmpty())
        return;
    QPoint pos = QmlUi::self()->globalPos(anchor, anchor->width() / 2,
                                          anchor->height() + 6);
    QToolTip::showText(pos, text);
}

void ShellController::hideToolTip()
{
    QToolTip::hideText();
}

void ShellController::toggleTheme()
{
    m_window->toggleTheme();
}

#include "moc_qmlcontrollers.cpp"
