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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets>

class PropertiesWidget;
class ComponentSelector;
class CircuitWidget;
class EditorWindow;
class FileWidget;
class SerialMonitor;
class QQuickWidget;

class MAINMODULE_EXPORT MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    MainWindow();
    ~MainWindow();

    static MainWindow *self()
    {
        return m_pSelf;
    }

    QSettings *settings();

    void loadPlugins();
    void unLoadPugin(QString pluginName);

    void readSettings();

    void setTitle(QString title);

    double fontScale()
    {
        return m_fontScale;
    }
    void setFontScale(double scale)
    {
        m_fontScale = scale;
    }

    int autoBck();
    void setAutoBck(int secs);

    QString loc();
    void setLoc(QString loc);

    QString *circHelp();
    void loadCircHelp();

    // Bring the Properties tab of the right side panel to front
    void showProperties();

    // Open the code panel with the integrated serial monitor expanded
    void openSerialMonitor(int uart = 0);

    // Top bar theme toggle (placeholder, see toggleTheme())
    QAction *themeToggleAction()
    {
        return m_themeToggleAct;
    }

    // ---- Used by the QML shell (src/gui/qml) ----------------------------
    bool qmlUi() const
    {
        return m_qml;
    }
    QMenu *mainMenu()
    {
        return m_mainMenu;
    }
    QAction *realisticPartsAction()
    {
        return m_realAct;
    }
    QAction *spiceAction()
    {
        return m_spiceAct;
    }
    bool isComponentPanelVisible() const;
    bool isCodePanelVisible() const;
    int sidePage() const;
    void setSidePage(int page);
    void selectMcu(const QString &id);

    QTabWidget *m_sidepanel;
    QWidget *m_ramTabWidget;
    QGridLayout *m_ramTabWidgetLayout;

  public slots:
    void setCodePanelVisible(bool visible);
    void setComponentPanelVisible(bool visible);
    void setSerialMonitorExpanded(bool expanded);

    // PLACEHOLDER for the theme toggle button in the top bar.
    // The Material theme (QSS in resources/themes) hooks in here.
    void toggleTheme();

  signals:
    // Emitted by the top bar theme toggle button.
    void themeToggleRequested();
    // Component / code panel visibility or side panel page changed
    void panelsChanged();

  private slots:
    void updateMcuSelector();
    void mcuSelected(int index);
    void updateSimButton();
    void updateProjectLabel(const QString &windowTitle);
    void showRamTable();
    void showFileExplorer();

  protected:
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);

  private:
    static MainWindow *m_pSelf;

    void loadPluginsAt(QDir pluginsDir);

    bool m_blocked;

    void createWidgets();
    void createTopBar();
    void createEditToolBar();
    void createCodePanel();
    void createMenus();
    void createInspector();
    void createQmlChrome(QVBoxLayout *baseLayout);
    void placeOverlays();
    void placeInspector();
    void updateInspector();
    void writeSettings();
    void applyStyle();

    float m_fontScale;
    int m_autoBck;

    QSettings m_settings;

    QString m_version;
    QString m_styleSheet;
    QString m_circHelp;

    QHash<QString, QPluginLoader *> m_plugins;

    CircuitWidget *m_circuit;
    ComponentSelector *m_components;
    PropertiesWidget *m_itemprop;
    EditorWindow *m_editor;

    QSplitter *m_Centralsplitter; // Main splitter: canvas | code | side panel
    FileWidget *m_fileSystemTree;

    // Tinkercad-like chrome
    QWidget *m_topBar;
    QLabel *m_projectLabel;
    QToolButton *m_menuButton;
    QToolButton *m_themeButton;
    QPushButton *m_componentsButton;
    QPushButton *m_codeButton;
    QPushButton *m_simButton;
    QToolBar *m_editToolBar;
    QMenu *m_mainMenu;

    QAction *m_themeToggleAct;
    QAction *m_codePanelAct;
    QAction *m_componentPanelAct;

    QWidget *m_codePanel;
    QSplitter *m_codeSplitter;
    QWidget *m_serialPanel;
    QToolButton *m_serialToggle;
    SerialMonitor *m_serialMonitor;
    QComboBox *m_mcuSelector;

    QFrame *m_inspector;
    QLabel *m_inspectorTitle;
    QToolButton *m_inspectorCollapse;
    QTimer *m_inspectorTimer;
    QPointer<QObject> m_inspectedComp;

    bool m_sideWasVisible; // Side panel state before the code panel opened

    bool m_qml;          // QML (Qt Quick) shell instead of Widgets chrome
    QWidget *m_sideHost; // Side panel (+ QML header in QML mode)
    QQuickWidget *m_qmlInspector;
    QQuickWidget *m_qmlToolbar;
    QAction *m_realAct;
    QAction *m_spiceAct;
};

#endif
