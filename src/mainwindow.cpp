/***************************************************************************
 *   Copyright (C) 2012 by santiago González                               *
 *   santigoro@gmail.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.  *
 *                                                                         *
 ***************************************************************************/

#include "mainwindow.h"

#include "QPropertyEditorWidget.h"
#include "appiface.h"
#include "circuit.h"
#include "circuitwidget.h"
#include "codeeditor.h"
#include "component.h"
#include "componentselector.h"
#include "editorwindow.h"
#include "filewidget.h"
#include "mcpserver.h"
#include "mcucomponent.h"
#include "partart.h"
#include "propertieswidget.h"
#include "serialmonitor.h"
#include "simuapi_apppath.h"
#include "thememanager.h"
#include "utils.h"

#include <QGraphicsDropShadowEffect>
#include <QMenuBar>
#include <QScreen>

MainWindow *MainWindow::m_pSelf = 0l;

MainWindow::MainWindow() :
    QMainWindow(), m_settings(QStandardPaths::standardLocations(
                                  QStandardPaths::AppLocalDataLocation)
                                      .first() +
                                  "/simulide.ini",
                              QSettings::IniFormat, this)
{
    setWindowIcon(QIcon(":/simulide.png"));
    m_pSelf   = this;
    m_circuit = 0l;
    m_simButton    = 0l;
    m_projectLabel = 0l;
    m_version = "SimulIDE-" + QString(APP_VERSION);

    this->setWindowTitle(m_version);

    QString userAddonPath =
        SIMUAPI_AppPath::self()->RWDataFolder().absoluteFilePath("addons");

    QDir pluginsDir(userAddonPath);

    if (!pluginsDir.exists())
        pluginsDir.mkpath(userAddonPath);

    m_fontScale = 1.0;
    if (m_settings.contains("fontScale")) {
        m_fontScale = m_settings.value("fontScale").toFloat();
        if (m_fontScale == 0)
            m_fontScale = 1;
    } else {
        double dpiX = qApp->primaryScreen()->logicalDotsPerInchX();
        m_fontScale = dpiX / 96.0;
    }
    // qDebug()<<dpiX;
    ThemeManager::self()->init(&m_settings);
    loadCircHelp();
    createWidgets();
    readSettings();

    loadPlugins();
    McpServer::startDefault(this); // MCP server (see docs/MCP.md)

    QString backPath = m_settings.value("backupPath").toString();
    if (!backPath.isEmpty()) {
        // qDebug() << "MainWindow::readSettings" << backPath;
        if (QFile::exists(backPath))
            CircuitWidget::self()->loadCirc(backPath);
    }
}
MainWindow::~MainWindow() {}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_editor->close()) {
        event->ignore();
        return;
    }
    if (!m_circuit->newCircuit()) {
        event->ignore();
        return;
    }

    writeSettings();

    event->accept();
}

void MainWindow::readSettings()
{
    restoreGeometry(m_settings.value("geometry").toByteArray());
    restoreState(m_settings.value("windowState").toByteArray());

    if (m_settings.contains("mainSplitter/state"))
        m_Centralsplitter->restoreState(
            m_settings.value("mainSplitter/state").toByteArray());
    if (m_settings.contains("codeSplitter/state"))
        m_codeSplitter->restoreState(
            m_settings.value("codeSplitter/state").toByteArray());

    setComponentPanelVisible(
        m_settings.value("componentPanel/visible", true).toBool());
    setCodePanelVisible(m_settings.value("codePanel/visible", false).toBool());
    setSerialMonitorExpanded(
        m_settings.value("serialMonitor/expanded", false).toBool());

    int autoBck = 15;
    if (m_settings.contains("autoBck"))
        autoBck = m_settings.value("autoBck").toInt();
    Circuit::self()->setAutoBck(autoBck);
}

void MainWindow::writeSettings()
{
    m_settings.setValue("autoBck", m_autoBck);
    m_settings.setValue("fontScale", m_fontScale);
    m_settings.setValue("geometry", saveGeometry());
    m_settings.setValue("windowState", saveState());
    m_settings.setValue("mainSplitter/state", m_Centralsplitter->saveState());
    m_settings.setValue("codeSplitter/state", m_codeSplitter->saveState());
    bool sideVisible =
        m_codePanel->isHidden() ? !m_sidepanel->isHidden() : m_sideWasVisible;
    m_settings.setValue("componentPanel/visible", sideVisible);
    m_settings.setValue("codePanel/visible", !m_codePanel->isHidden());
    m_settings.setValue("serialMonitor/expanded", m_serialToggle->isChecked());

    m_components->writeSettings();

    FileWidget::self()->writeSettings();
}

QString MainWindow::loc()
{
    return Circuit::self()->loc();
}

void MainWindow::setLoc(QString loc)
{
    Circuit::self()->setLoc(loc);
}

int MainWindow::autoBck()
{
    return m_autoBck;
}

void MainWindow::setAutoBck(int secs)
{
    m_autoBck = secs;
}

void MainWindow::setTitle(QString title)
{
    setWindowTitle(m_version + "  -  " + title);
}

void MainWindow::updateProjectLabel(const QString &windowTitle)
{
    if (!m_projectLabel)
        return;
    QString project = windowTitle.section("  -  ", 1);
    m_projectLabel->setText(project);
    m_projectLabel->setToolTip(project);
}

static QIcon materialIcon(const QString &name)
{
    return ThemeManager::icon(name);
}

void MainWindow::createWidgets()
{
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);

    QVBoxLayout *baseWidgetLayout = new QVBoxLayout(centralWidget);
    baseWidgetLayout->setSpacing(0);
    baseWidgetLayout->setContentsMargins(0, 0, 0, 0);
    baseWidgetLayout->setObjectName("baseLayout");

    // ---- Main area: canvas | code panel | side panel -------------------
    m_Centralsplitter = new QSplitter(this);
    m_Centralsplitter->setObjectName("mainSplitter");
    m_Centralsplitter->setOrientation(Qt::Horizontal);
    m_Centralsplitter->setChildrenCollapsible(false);

    // Right side panel: Components (grid), Properties, RAM table, Files
    m_sidepanel = new QTabWidget(this);
    m_sidepanel->setObjectName("sidePanel");
    m_sidepanel->setTabPosition(QTabWidget::North);
    m_sidepanel->setDocumentMode(true);
    m_sidepanel->setUsesScrollButtons(false);
    m_sidepanel->setMinimumWidth(250);
    m_sidepanel->tabBar()->setDrawBase(false);
    m_sidepanel->tabBar()->setExpanding(false);
    m_sidepanel->setIconSize(QSize(24, 24));
    m_sidepanel->tabBar()->setFocusPolicy(Qt::NoFocus);

    m_components = new ComponentSelector(m_sidepanel);
    m_sidepanel->addTab(m_components, tr("Components"));

    m_ramTabWidget = new QWidget(this);
    m_ramTabWidget->setObjectName("ramTabWidget");
    m_ramTabWidgetLayout = new QGridLayout(m_ramTabWidget);
    m_ramTabWidgetLayout->setSpacing(0);
    m_ramTabWidgetLayout->setContentsMargins(0, 0, 0, 0);
    m_ramTabWidgetLayout->setObjectName("ramTabWidgetLayout");
    m_sidepanel->addTab(m_ramTabWidget, materialIcon("memory"), "");
    m_sidepanel->setTabToolTip(1, tr("RamTable"));

    m_fileSystemTree = new FileWidget(this);
    m_fileSystemTree->setObjectName("fileExplorer");
    m_sidepanel->addTab(m_fileSystemTree, materialIcon("folder"), "");
    m_sidepanel->setTabToolTip(2, tr("File explorer"));

    m_circuit = new CircuitWidget(this);
    m_circuit->setObjectName("circuit");
    m_Centralsplitter->addWidget(m_circuit);

    createInspector();

    createCodePanel();
    m_Centralsplitter->addWidget(m_codePanel);

    m_Centralsplitter->addWidget(m_sidepanel);

    m_Centralsplitter->setStretchFactor(0, 1);
    m_Centralsplitter->setStretchFactor(1, 0);
    m_Centralsplitter->setStretchFactor(2, 0);

    // ---- Top bar + edit toolbar ----------------------------------------
    createMenus();
    createTopBar();
    createEditToolBar();

    baseWidgetLayout->addWidget(m_topBar);
    baseWidgetLayout->addWidget(m_editToolBar);
    baseWidgetLayout->addWidget(m_Centralsplitter, 1);

    QList<int> sizes;
    sizes << 900 << 450 << 300;
    m_Centralsplitter->setSizes(sizes);

    m_sideWasVisible = true;
    m_codePanel->setVisible(false);

    connect(this, SIGNAL(windowTitleChanged(const QString &)), this,
            SLOT(updateProjectLabel(const QString &)));
    updateProjectLabel(windowTitle());

    connect(m_circuit, SIGNAL(simStateChanged(const QString &)), this,
            SLOT(updateSimButton()));
    connect(m_circuit->powerCircAct, SIGNAL(changed()), this,
            SLOT(updateSimButton()));
    updateSimButton();

    this->showMaximized();
}

void MainWindow::createTopBar()
{
    m_topBar = new QWidget(this);
    m_topBar->setObjectName("topBar");
    m_topBar->setAttribute(Qt::WA_StyledBackground, true);

    QHBoxLayout *layout = new QHBoxLayout(m_topBar);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    // Hamburger menu with all the classic menus
    m_menuButton = new QToolButton(m_topBar);
    m_menuButton->setObjectName("menuButton");
    m_menuButton->setIcon(materialIcon("menu"));
    m_menuButton->setIconSize(QSize(24, 24));
    m_menuButton->setToolTip(tr("Menu"));
    m_menuButton->setPopupMode(QToolButton::InstantPopup);
    m_menuButton->setAutoRaise(true);
    m_menuButton->setMenu(m_mainMenu);
    layout->addWidget(m_menuButton);

    QLabel *logo = new QLabel(m_topBar);
    logo->setObjectName("appLogo");
    logo->setPixmap(QIcon(":/simulide.png").pixmap(24, 24));
    layout->addWidget(logo);

    QLabel *appName = new QLabel("SimulIDE", m_topBar);
    appName->setObjectName("appTitle");
    QFont nameFont = appName->font();
    nameFont.setBold(true);
    nameFont.setPixelSize(int(16 * m_fontScale));
    appName->setFont(nameFont);
    layout->addWidget(appName);

    QFrame *sep = new QFrame(m_topBar);
    sep->setObjectName("topBarSeparator");
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep);

    m_projectLabel = new QLabel(m_topBar);
    m_projectLabel->setObjectName("projectName");
    QFont projFont = m_projectLabel->font();
    projFont.setPixelSize(int(14 * m_fontScale));
    m_projectLabel->setFont(projFont);
    m_projectLabel->setMinimumWidth(80);
    m_projectLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_projectLabel, 1);

    // Theme toggle (placeholder)
    m_themeButton = new QToolButton(m_topBar);
    m_themeButton->setObjectName("themeToggleButton");
    m_themeButton->setDefaultAction(m_themeToggleAct);
    m_themeButton->setIconSize(QSize(22, 22));
    m_themeButton->setAutoRaise(true);
    layout->addWidget(m_themeButton);

    // File actions
    QList<QPair<QAction *, QString>> fileActs;
    fileActs << qMakePair(m_circuit->newCircAct, QString("newCircuitButton"))
             << qMakePair(m_circuit->openCircAct, QString("openCircuitButton"))
             << qMakePair(m_circuit->saveCircAct, QString("saveCircuitButton"));
    for (auto &pair : fileActs) {
        QToolButton *button = new QToolButton(m_topBar);
        button->setObjectName(pair.second);
        button->setProperty("topBarFileButton", true);
        button->setDefaultAction(pair.first);
        button->setIconSize(QSize(22, 22));
        button->setAutoRaise(true);
        layout->addWidget(button);
    }
    layout->addSpacing(8);

    // Big action buttons
    m_componentsButton =
        new QPushButton(materialIcon("widgets"), tr("Components"), m_topBar);
    m_componentsButton->setObjectName("componentsButton");
    m_componentsButton->setCheckable(true);
    m_componentsButton->setChecked(true);
    m_componentsButton->setToolTip(tr("Show / hide the component panel"));
    m_componentsButton->setIconSize(QSize(20, 20));
    connect(m_componentsButton, SIGNAL(toggled(bool)), this,
            SLOT(setComponentPanelVisible(bool)));
    QHBoxLayout *segment = new QHBoxLayout();
    segment->setSpacing(0);
    segment->addWidget(m_componentsButton);
    layout->addLayout(segment);

    m_codeButton = new QPushButton(materialIcon("code"), tr("Code"), m_topBar);
    m_codeButton->setObjectName("codeButton");
    m_codeButton->setCheckable(true);
    m_codeButton->setToolTip(tr("Show / hide the code editor"));
    m_codeButton->setIconSize(QSize(20, 20));
    connect(m_codeButton, SIGNAL(toggled(bool)), this,
            SLOT(setCodePanelVisible(bool)));
    segment->addWidget(m_codeButton);
    layout->addSpacing(8);

    m_simButton = new QPushButton(m_topBar);
    m_simButton->setObjectName("simButton");
    m_simButton->setIconSize(QSize(20, 20));
    m_simButton->setMinimumWidth(150);
    m_simButton->setDefault(false);
    m_simButton->setAutoDefault(false);
    // Fallback colour so the button stands out even without a theme;
    // the QSS theme can override it with #simButton[simState="..."].
    QPalette pal = m_simButton->palette();
    pal.setColor(QPalette::Button, QColor(0x2E, 0x7D, 0x32));
    pal.setColor(QPalette::ButtonText, Qt::white);
    m_simButton->setPalette(pal);
    connect(m_simButton, SIGNAL(clicked()), m_circuit, SLOT(powerCirc()));
    layout->addWidget(m_simButton);
}

void MainWindow::createEditToolBar()
{
    m_editToolBar = new QToolBar(this);
    m_editToolBar->setObjectName("editToolBar");
    m_editToolBar->setIconSize(QSize(20, 20));
    m_editToolBar->setMovable(false);
    m_editToolBar->setFloatable(false);

    m_editToolBar->addAction(m_circuit->copyAct);
    m_editToolBar->addAction(m_circuit->pasteAct);
    m_editToolBar->addAction(m_circuit->deleteAct);
    m_editToolBar->addSeparator();
    m_editToolBar->addAction(m_circuit->undoAct);
    m_editToolBar->addAction(m_circuit->redoAct);
    m_editToolBar->addSeparator();
    m_editToolBar->addAction(m_circuit->rotateCWAct);
    m_editToolBar->addAction(m_circuit->rotateCCWAct);
    m_editToolBar->addAction(m_circuit->hFlipAct);
    m_editToolBar->addAction(m_circuit->vFlipAct);
    m_editToolBar->addSeparator();
    m_editToolBar->addAction(m_circuit->zoomInAct);
    m_editToolBar->addAction(m_circuit->zoomOutAct);
    m_editToolBar->addAction(m_circuit->zoomFitAct);
    m_editToolBar->addSeparator();
    m_editToolBar->addAction(m_circuit->pauseSimAct);

    QWidget *spacer = new QWidget(m_editToolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_editToolBar->addWidget(spacer);
    m_editToolBar->addWidget(m_circuit->rateLabel());

    QWidget *pad = new QWidget(m_editToolBar);
    pad->setFixedWidth(8);
    m_editToolBar->addWidget(pad);
}

void MainWindow::updateMcuSelector()
{
    QSignalBlocker blocker(m_mcuSelector);
    m_mcuSelector->clear();
    QList<McuComponent *> mcus = McuComponent::mcuList();
    if (mcus.isEmpty()) {
        m_mcuSelector->addItem(tr("No MCU"));
        m_mcuSelector->setEnabled(false);
        return;
    }
    m_mcuSelector->setEnabled(true);
    for (McuComponent *mcu : mcus) {
        QString text = mcu->itemID() + " (" + mcu->device() + ")";
        m_mcuSelector->addItem(text, mcu->itemID());
        if (mcu == McuComponent::self())
            m_mcuSelector->setCurrentIndex(m_mcuSelector->count() - 1);
    }
}

void MainWindow::mcuSelected(int index)
{
    McuComponent *mcu =
        McuComponent::findMcu(m_mcuSelector->itemData(index).toString());
    if (!mcu)
        return;
    McuComponent::setSelf(mcu);
    // Bind the current editor tab to this MCU
    if (CodeEditor *ce = m_editor->getCodeEditor())
        ce->setTargetMcuId(mcu->itemID());
}

void MainWindow::createCodePanel()
{
    m_codePanel = new QWidget(this);
    m_codePanel->setObjectName("codePanel");
    m_codePanel->setMinimumWidth(300);

    QVBoxLayout *layout = new QVBoxLayout(m_codePanel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QWidget *header = new QWidget(m_codePanel);
    header->setObjectName("codePanelHeader");
    header->setAttribute(Qt::WA_StyledBackground, true);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 4, 4);
    QLabel *title = new QLabel(tr("Code"), header);
    title->setObjectName("codePanelTitle");
    QFont tf = title->font();
    tf.setBold(true);
    title->setFont(tf);
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    // Target MCU: upload/debug, RAM table and Serial Monitor follow it.
    QLabel *mcuLabel = new QLabel(tr("Target:"), header);
    headerLayout->addWidget(mcuLabel);
    m_mcuSelector = new QComboBox(header);
    m_mcuSelector->setObjectName("mcuSelector");
    m_mcuSelector->setToolTip(
        tr("MCU targeted by upload, debugger, RAM table and Serial Monitor"));
    m_mcuSelector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    headerLayout->addWidget(m_mcuSelector);
    connect(McuSignals::self(), SIGNAL(mcuListChanged()), this,
            SLOT(updateMcuSelector()));
    connect(McuSignals::self(), SIGNAL(activeMcuChanged(McuComponent *)), this,
            SLOT(updateMcuSelector()));
    connect(m_mcuSelector, SIGNAL(activated(int)), this,
            SLOT(mcuSelected(int)));
    updateMcuSelector();

    QToolButton *closeButton = new QToolButton(header);
    closeButton->setObjectName("codePanelClose");
    closeButton->setIcon(materialIcon("close"));
    closeButton->setToolTip(tr("Close code panel"));
    closeButton->setAutoRaise(true);
    headerLayout->addWidget(closeButton);
    layout->addWidget(header);

    m_codeSplitter = new QSplitter(Qt::Vertical, m_codePanel);
    m_codeSplitter->setObjectName("codeSplitter");
    m_codeSplitter->setChildrenCollapsible(false);
    layout->addWidget(m_codeSplitter, 1);

    m_editor = new EditorWindow(m_codeSplitter);
    m_editor->setObjectName("editor");
    m_codeSplitter->addWidget(m_editor);

    // Collapsible serial monitor at the bottom
    m_serialPanel = new QWidget(m_codeSplitter);
    m_serialPanel->setObjectName("serialMonitorPanel");
    m_serialPanel->setAttribute(Qt::WA_StyledBackground, true);
    QVBoxLayout *serialLayout = new QVBoxLayout(m_serialPanel);
    serialLayout->setContentsMargins(0, 0, 0, 0);
    serialLayout->setSpacing(0);

    m_serialToggle = new QToolButton(m_serialPanel);
    m_serialToggle->setObjectName("serialMonitorToggle");
    m_serialToggle->setText(tr("Serial Monitor"));
    m_serialToggle->setIcon(materialIcon("terminal"));
    m_serialToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_serialToggle->setCheckable(true);
    m_serialToggle->setAutoRaise(true);
    m_serialToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    serialLayout->addWidget(m_serialToggle);

    m_serialMonitor = new SerialMonitor(m_serialPanel);
    serialLayout->addWidget(m_serialMonitor, 1);
    m_codeSplitter->addWidget(m_serialPanel);
    m_codeSplitter->setStretchFactor(0, 1);
    m_codeSplitter->setStretchFactor(1, 0);

    connect(m_serialToggle, SIGNAL(toggled(bool)), this,
            SLOT(setSerialMonitorExpanded(bool)));
    connect(closeButton, &QToolButton::clicked,
            [this]() { setCodePanelVisible(false); });

    setSerialMonitorExpanded(false);
}

void MainWindow::createMenus()
{
    m_themeToggleAct =
        new QAction(materialIcon(ThemeManager::self()->isDark() ? "light_mode"
                                                                : "dark_mode"),
                    tr("Toggle Theme"), this);
    connect(ThemeManager::self(), &ThemeManager::themeChanged, this,
            [this](bool dark) {
                m_themeToggleAct->setIcon(
                    materialIcon(dark ? "light_mode" : "dark_mode"));
            });
    m_themeToggleAct->setObjectName("themeToggleAction");
    m_themeToggleAct->setToolTip(tr("Switch light / dark theme"));
    connect(m_themeToggleAct, SIGNAL(triggered()), this, SLOT(toggleTheme()));

    m_componentPanelAct =
        new QAction(materialIcon("widgets"), tr("Components Panel"), this);
    m_componentPanelAct->setCheckable(true);
    m_componentPanelAct->setChecked(true);
    connect(m_componentPanelAct, SIGNAL(toggled(bool)), this,
            SLOT(setComponentPanelVisible(bool)));

    m_codePanelAct = new QAction(materialIcon("code"), tr("Code Panel"), this);
    m_codePanelAct->setCheckable(true);
    connect(m_codePanelAct, SIGNAL(toggled(bool)), this,
            SLOT(setCodePanelVisible(bool)));

    m_mainMenu = new QMenu(this);
    m_mainMenu->setObjectName("mainMenu");

    QMenu *fileMenu = m_mainMenu->addMenu(tr("&File"));
    fileMenu->addAction(m_circuit->newCircAct);
    fileMenu->addAction(m_circuit->openCircAct);
    fileMenu->addAction(m_circuit->saveCircAct);
    fileMenu->addAction(m_circuit->saveCircAsAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_circuit->importCircAct);
    fileMenu->addAction(m_circuit->saveImageAct);
    fileMenu->addSeparator();
    QAction *quitAct = fileMenu->addAction(materialIcon("close"), tr("&Quit"));
    connect(quitAct, SIGNAL(triggered()), this, SLOT(close()));

    QMenu *editMenu = m_mainMenu->addMenu(tr("&Edit"));
    editMenu->addAction(m_circuit->undoAct);
    editMenu->addAction(m_circuit->redoAct);
    editMenu->addSeparator();
    editMenu->addAction(m_circuit->copyAct);
    editMenu->addAction(m_circuit->pasteAct);
    editMenu->addAction(m_circuit->deleteAct);
    editMenu->addSeparator();
    editMenu->addAction(m_circuit->rotateCWAct);
    editMenu->addAction(m_circuit->rotateCCWAct);
    editMenu->addAction(m_circuit->hFlipAct);
    editMenu->addAction(m_circuit->vFlipAct);

    QMenu *viewMenu = m_mainMenu->addMenu(tr("&View"));
    viewMenu->addAction(m_componentPanelAct);
    viewMenu->addAction(m_codePanelAct);
    viewMenu->addSeparator();
    QAction *propAct =
        viewMenu->addAction(materialIcon("tune"), tr("Properties"));
    connect(propAct, &QAction::triggered, [this]() { showProperties(); });
    QAction *ramAct =
        viewMenu->addAction(materialIcon("memory"), tr("RamTable"));
    connect(ramAct, SIGNAL(triggered()), this, SLOT(showRamTable()));
    QAction *filesAct =
        viewMenu->addAction(materialIcon("folder"), tr("File explorer"));
    connect(filesAct, SIGNAL(triggered()), this, SLOT(showFileExplorer()));
    viewMenu->addSeparator();
    viewMenu->addAction(m_circuit->zoomInAct);
    viewMenu->addAction(m_circuit->zoomOutAct);
    viewMenu->addAction(m_circuit->zoomFitAct);
    viewMenu->addAction(m_circuit->zoomResetAct);
    viewMenu->addSeparator();
    viewMenu->addAction(m_themeToggleAct);
    QAction *realAct =
        viewMenu->addAction(materialIcon("widgets"), tr("Realistic Parts"));
    realAct->setObjectName("realisticPartsAction");
    realAct->setToolTip(tr("Draw components as realistic parts instead of "
                           "schematic symbols"));
    realAct->setCheckable(true);
    realAct->setChecked(PartArt::enabled());
    connect(realAct, &QAction::toggled,
            [](bool on) { PartArt::setEnabled(on); });

    QMenu *simMenu = m_mainMenu->addMenu(tr("&Simulation"));
    simMenu->addAction(m_circuit->powerCircAct);
    simMenu->addAction(m_circuit->pauseSimAct);

    QMenu *toolsMenu = m_mainMenu->addMenu(tr("&Tools"));
    QAction *manageAct =
        toolsMenu->addAction(materialIcon("tune"), tr("Manage Components"));
    connect(manageAct, SIGNAL(triggered()), m_components,
            SLOT(slotManageComponents()));
    toolsMenu->addAction(m_circuit->createSubcircAct);
    toolsMenu->addAction(m_circuit->bomAct);

    QMenu *helpMenu = m_mainMenu->addMenu(tr("&Help"));
    helpMenu->addAction(m_circuit->infoAct);
    helpMenu->addAction(m_circuit->aboutAct);
    helpMenu->addAction(m_circuit->aboutQtAct);
}

void MainWindow::updateSimButton()
{
    if (!m_simButton)
        return;
    QAction *power = m_circuit->powerCircAct;
    QString state  = m_circuit->simState();

    QString text = tr("Start Simulation");
    QString icon = "play_arrow";
    if (state == "On") {
        text = tr("Stop Simulation");
        icon = "stop";
    } else if (state == "Paused") {
        text = tr("Paused");
        icon = "pause";
    } else if (state == "Debug") {
        text = tr("Debugging");
        icon = "bug_report";
    }
    m_simButton->setText(text);
    m_simButton->setIcon(ThemeManager::icon(icon, Qt::white));
    m_simButton->setToolTip(power->statusTip());
    m_simButton->setEnabled(power->isEnabled() && state != "Debug");

    // Let the QSS target #simButton[simState="On"] etc.
    if (m_simButton->property("simState").toString() != state) {
        m_simButton->setProperty("simState", state);
        m_simButton->style()->unpolish(m_simButton);
        m_simButton->style()->polish(m_simButton);
    }
    QPalette pal = m_simButton->palette();
    pal.setColor(QPalette::Button, (state == "Off") ? QColor(0x2E, 0x7D, 0x32)
                                                    : QColor(0xC6, 0x28, 0x28));
    m_simButton->setPalette(pal);
}

void MainWindow::setCodePanelVisible(bool visible)
{
    if (visible == !m_codePanel->isHidden() &&
        m_codeButton->isChecked() == visible)
        return;

    if (visible) {
        // Tinkercad-like: the code panel takes the place of the components
        m_sideWasVisible = !m_sidepanel->isHidden();
        m_codePanel->setVisible(true);
        setComponentPanelVisible(false);

        QList<int> sizes = m_Centralsplitter->sizes();
        int total        = sizes[0] + sizes[1] + sizes[2];
        int code         = qMax(sizes[1], qMax(380, total * 2 / 5));
        if (code > total - 200)
            code = total / 2;
        sizes[1] = code;
        sizes[0] = total - code - sizes[2];
        m_Centralsplitter->setSizes(sizes);
    } else {
        m_codePanel->setVisible(false);
        if (m_sideWasVisible)
            setComponentPanelVisible(true);
    }
    m_codeButton->blockSignals(true);
    m_codeButton->setChecked(visible);
    m_codeButton->blockSignals(false);
    m_codePanelAct->blockSignals(true);
    m_codePanelAct->setChecked(visible);
    m_codePanelAct->blockSignals(false);
}

void MainWindow::setComponentPanelVisible(bool visible)
{
    m_sidepanel->setVisible(visible);

    m_componentsButton->blockSignals(true);
    m_componentsButton->setChecked(visible);
    m_componentsButton->blockSignals(false);
    m_componentPanelAct->blockSignals(true);
    m_componentPanelAct->setChecked(visible);
    m_componentPanelAct->blockSignals(false);
}

void MainWindow::setSerialMonitorExpanded(bool expanded)
{
    m_serialMonitor->setVisible(expanded);
    m_serialToggle->blockSignals(true);
    m_serialToggle->setChecked(expanded);
    m_serialToggle->blockSignals(false);

    if (expanded) {
        m_serialPanel->setMaximumHeight(QWIDGETSIZE_MAX);
        QList<int> sizes = m_codeSplitter->sizes();
        int total        = sizes[0] + sizes[1];
        if (sizes[1] < 150 && total > 300) {
            sizes[1] = qMax(180, total / 3);
            sizes[0] = total - sizes[1];
            m_codeSplitter->setSizes(sizes);
        }
    } else {
        m_serialPanel->setMaximumHeight(m_serialToggle->sizeHint().height());
    }
}

void MainWindow::toggleTheme()
{
    ThemeManager::self()->toggle();
    emit themeToggleRequested();
}

void MainWindow::createInspector()
{
    // Floating, Tinkercad-like property inspector over the circuit canvas
    QWidget *view = m_circuit->view();
    m_inspector   = new QFrame(view);
    m_inspector->setObjectName("propertyInspector");
    m_inspector->setFrameShape(QFrame::StyledPanel);
    m_inspector->setAttribute(Qt::WA_StyledBackground, true);
    m_inspector->setAutoFillBackground(false);
    QGraphicsDropShadowEffect *shadow =
        new QGraphicsDropShadowEffect(m_inspector);
    shadow->setBlurRadius(16);
    shadow->setOffset(0, 2);
    shadow->setColor(QColor(0, 0, 0, 60));
    m_inspector->setGraphicsEffect(shadow);

    QVBoxLayout *layout = new QVBoxLayout(m_inspector);
    layout->setContentsMargins(12, 8, 12, 12);
    layout->setSpacing(6);

    QHBoxLayout *header = new QHBoxLayout();
    header->setSpacing(4);
    m_inspectorTitle = new QLabel(m_inspector);
    m_inspectorTitle->setObjectName("propertyInspectorTitle");
    QFont tf = m_inspectorTitle->font();
    tf.setBold(true);
    m_inspectorTitle->setFont(tf);
    m_inspectorTitle->setAttribute(Qt::WA_StyledBackground, true);
    header->addWidget(m_inspectorTitle, 1);

    m_inspectorCollapse = new QToolButton(m_inspector);
    m_inspectorCollapse->setObjectName("propertyInspectorCollapse");
    m_inspectorCollapse->setIcon(materialIcon("expand_less"));
    m_inspectorCollapse->setAutoRaise(true);
    m_inspectorCollapse->setCheckable(true);
    m_inspectorCollapse->setToolTip(tr("Collapse"));
    header->addWidget(m_inspectorCollapse);
    layout->addLayout(header);

    m_itemprop = new PropertiesWidget(m_inspector);
    m_itemprop->setObjectName("propertiesPanel");
    layout->addWidget(m_itemprop, 1);

    connect(m_inspectorCollapse, &QToolButton::toggled, [this](bool c) {
        m_itemprop->setVisible(!c);
        m_inspectorCollapse->setIcon(
            materialIcon(c ? "expand_more" : "expand_less"));
        m_inspectorCollapse->setToolTip(c ? tr("Expand") : tr("Collapse"));
        placeInspector();
    });

    view->installEventFilter(this);
    m_inspector->hide();

    m_inspectorTimer = new QTimer(this);
    connect(m_inspectorTimer, &QTimer::timeout, this,
            &MainWindow::updateInspector);
    m_inspectorTimer->start(150);
}

void MainWindow::placeInspector()
{
    QWidget *view = m_circuit->view();
    int margin    = 12;
    int w         = qMin(300, view->width() - 2 * margin);
    int h         = m_itemprop->isVisible()
                        ? qMin(420, view->height() - 2 * margin)
                        : m_inspector->layout()->sizeHint().height();
    if (w < 50 || h < 20)
        return;
    m_inspector->setGeometry(view->width() - w - margin, margin, w, h);
    m_inspector->raise();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (m_circuit && obj == m_circuit->view() &&
        event->type() == QEvent::Resize)
        placeInspector();
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::updateInspector()
{
    Circuit *circ  = Circuit::self();
    Component *sel = 0l;
    if (circ) {
        int count = 0;
        for (QGraphicsItem *item : circ->selectedItems()) {
            Component *comp = dynamic_cast<Component *>(item);
            if (!comp)
                continue;
            sel = comp;
            if (++count > 1)
                break;
        }
        if (count != 1)
            sel = 0l;
    }
    if (!sel) {
        if (m_inspector->isVisible())
            m_inspector->hide();
        m_inspectedComp = 0l;
        return;
    }
    // Circuit::mousePressEvent points the property editor at the circuit
    // on every click; if the click then lands on a child item of the
    // selected component (display, pin, label...) the component never
    // re-claims the editor. Always re-sync with the selection.
    QPropertyEditorWidget *editor = QPropertyEditorWidget::self();
    if (m_inspectedComp.data() != sel || editor->object() != sel) {
        m_inspectedComp = sel;
        editor->setObject(sel);
        PropertiesWidget::self()->setHelpText(sel->helpText());
        m_inspectorTitle->setText(sel->itemType() + "  " + sel->itemID());
    }
    if (!m_inspector->isVisible()) {
        placeInspector();
        m_inspector->show();
        m_inspector->raise();
    }
}

void MainWindow::showProperties()
{
    // Properties now live in the floating inspector over the canvas
    m_inspectorCollapse->setChecked(false);
    updateInspector();
    placeInspector();
}

void MainWindow::openSerialMonitor(int uart)
{
    setCodePanelVisible(true);
    if (uart > 0)
        m_serialMonitor->setUart(uart);
    setSerialMonitorExpanded(true);
}

void MainWindow::showRamTable()
{
    setComponentPanelVisible(true);
    m_sidepanel->setCurrentWidget(m_ramTabWidget);
}

void MainWindow::showFileExplorer()
{
    setComponentPanelVisible(true);
    m_sidepanel->setCurrentWidget(m_fileSystemTree);
}

void MainWindow::loadCircHelp()
{
    QString locale = "_" + QLocale::system().name().split("_").first();
    QString dfPath = SIMUAPI_AppPath::self()->availableDataFilePath(
        "help/" + locale + "/circuit" + locale + ".txt");

    if (dfPath == "")
        dfPath =
            SIMUAPI_AppPath::self()->availableDataFilePath("help/circuit.txt");

    if (dfPath != "") {
        QFile file(dfPath);

        if (file.open(QFile::ReadOnly | QFile::Text)) // Get Text from Help File
        {
            QTextStream s1(&file);
            s1.setEncoding(QStringConverter::Utf8);

            m_circHelp = "";
            m_circHelp.append(s1.readAll());

            file.close();
        }
    }
}

QString *MainWindow::circHelp()
{
    return &m_circHelp;
}

void MainWindow::loadPlugins()
{
    // Load main Plugins
    QDir pluginsDir(qApp->applicationDirPath());

    pluginsDir.cd("../lib/simulide/plugins");

    loadPluginsAt(pluginsDir);

    // Load main Component Sets
    QDir compSetDir = SIMUAPI_AppPath::self()->RODataFolder();

    if (compSetDir.exists())
        ComponentSelector::self()->LoadCompSetAt(compSetDir);

    // Load Addons
    QString userPluginsPath =
        SIMUAPI_AppPath::self()->RWDataFolder().absoluteFilePath("addons");

    pluginsDir.setPath(userPluginsPath);

    if (!pluginsDir.exists())
        return;

    for (QString pluginFolder : pluginsDir.entryList(QDir::Dirs)) {
        if (pluginFolder.contains("."))
            continue;
        // qDebug() << pluginFolder;
        pluginsDir.cd(pluginFolder);

        ComponentSelector::self()->LoadCompSetAt(pluginsDir);

        if (pluginsDir.entryList(QDir::Dirs).contains("lib")) {
            pluginsDir.cd("lib");
            loadPluginsAt(pluginsDir);
            pluginsDir.cd("../");
        }
        pluginsDir.cd("../");
    }
}

void MainWindow::loadPluginsAt(QDir pluginsDir)
{
    QString pluginName = "*plugin";

#ifndef Q_OS_UNIX
    pluginName += ".dll";
#else
    pluginName += ".so";
#endif

    pluginsDir.setNameFilters(QStringList(pluginName));

    QStringList fileList = pluginsDir.entryList(QDir::Files);

    if (fileList.isEmpty())
        return; // No plugins to load

    qDebug() << "\n    Loading Plugins at:\n"
             << pluginsDir.absolutePath() << "\n";

    for (QString libName : fileList) {
        pluginName =
            libName.split(".").first().remove("lib").remove("plugin").toUpper();

        if (m_plugins.contains(pluginName))
            continue;

        QPluginLoader *pluginLoader =
            new QPluginLoader(pluginsDir.absoluteFilePath(libName));
        QObject *plugin = pluginLoader->instance();

        if (plugin) {
            AppIface *item = qobject_cast<AppIface *>(plugin);

            if (item) {
                item->initialize();
                m_plugins[pluginName] = pluginLoader;
                qDebug() << "        Plugin Loaded Successfully:\t"
                         << pluginName;
            } else {
                pluginLoader->unload();
                delete pluginLoader;
            }
        } else {
            QString errorMsg = pluginLoader->errorString();
            qDebug() << "        " << pluginName
                     << "\tplugin FAILED: " << errorMsg;

            if (errorMsg.contains("libQt5SerialPort"))
                errorMsg = tr(" Qt5SerialPort is not installed in your "
                              "system\n\n    Mcu SerialPort will not work\n    "
                              "Just Install libQt5SerialPort package\n    To "
                              "have Mcu Serial Port Working");

            QMessageBox::warning(0, tr("Plugin Error:"), errorMsg);
        }
    }
    qDebug() << "\n";
}

void MainWindow::unLoadPugin(QString pluginName)
{
    if (m_plugins.contains(pluginName)) {
        QPluginLoader *pluginLoader = m_plugins[pluginName];
        QObject *plugin             = pluginLoader->instance();
        AppIface *item              = qobject_cast<AppIface *>(plugin);
        item->terminate();
        pluginLoader->unload();
        m_plugins.remove(pluginName);
        delete pluginLoader;
    }
}

void MainWindow::applyStyle()
{
    QFile file(":/simulide.qss");
    file.open(QFile::ReadOnly);

    m_styleSheet = QLatin1String(file.readAll());

    qApp->setStyleSheet(m_styleSheet);
}

QSettings *MainWindow::settings()
{
    return &m_settings;
}

#include "moc_mainwindow.cpp"
