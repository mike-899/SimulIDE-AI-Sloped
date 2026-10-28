/***************************************************************************
 *   Controllers exposed to the QML shell                                  *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#ifndef QMLCONTROLLERS_H
#define QMLCONTROLLERS_H

#include <QObject>
#include <QVariantList>

class CircuitWidget;
class MainWindow;
class QQuickItem;

// "Sim": simulation state for the Start Simulation FAB / toolbar
class SimController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY stateChanged)
    Q_PROPERTY(bool canToggle READ canToggle NOTIFY stateChanged)
    Q_PROPERTY(bool canPause READ canPause NOTIFY stateChanged)
    Q_PROPERTY(int speed READ speed NOTIFY speedChanged)
    Q_PROPERTY(bool error READ error NOTIFY speedChanged)

  public:
    explicit SimController(CircuitWidget *circuit, QObject *parent = 0l);

    QString state() const;
    bool running() const
    {
        return state() != "Off";
    }
    bool paused() const
    {
        return state() == "Paused";
    }
    bool canToggle() const;
    bool canPause() const;
    int speed() const
    {
        return m_speed;
    }
    bool error() const
    {
        return m_speed < 0;
    }

    Q_INVOKABLE void toggle();
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void pause();

  signals:
    void stateChanged();
    void speedChanged();

  private:
    CircuitWidget *m_circuit;
    int m_speed;
};

// "Files": project title and file actions
class FileController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY titleChanged)

  public:
    explicit FileController(MainWindow *window, CircuitWidget *circuit,
                            QObject *parent = 0l);

    QString title() const;
    bool modified() const;

    Q_INVOKABLE void newCircuit();
    Q_INVOKABLE void open();
    Q_INVOKABLE void save();
    Q_INVOKABLE void saveAs();
    // Code editor: open a source file / start a new one
    Q_INVOKABLE void openCode();
    Q_INVOKABLE void newCode();

  signals:
    void titleChanged();

  private:
    MainWindow *m_window;
    CircuitWidget *m_circuit;
};

// "Mcus": MCU target selector of the code panel
class McuModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList mcus READ mcus NOTIFY changed)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY changed)
    Q_PROPERTY(bool empty READ empty NOTIFY changed)

  public:
    explicit McuModel(QObject *parent = 0l);

    QVariantList mcus() const
    {
        return m_mcus;
    }
    int currentIndex() const
    {
        return m_current;
    }
    bool empty() const
    {
        return m_mcus.isEmpty();
    }

    Q_INVOKABLE void select(int index);

  signals:
    void changed();

  private slots:
    void update();

  private:
    QVariantList m_mcus;
    int m_current;
};

// "Shell": main window chrome (menus, panels, toggles)
class ShellController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool componentsVisible READ componentsVisible WRITE
                   setComponentsVisible NOTIFY panelsChanged)
    Q_PROPERTY(bool codeVisible READ codeVisible WRITE setCodeVisible NOTIFY
                   panelsChanged)
    Q_PROPERTY(
        int sidePage READ sidePage WRITE setSidePage NOTIFY panelsChanged)
    Q_PROPERTY(bool realisticParts READ realisticParts WRITE setRealisticParts
                   NOTIFY togglesChanged)
    Q_PROPERTY(bool spice READ spice WRITE setSpice NOTIFY togglesChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QObject *actions READ actions CONSTANT)

  public:
    explicit ShellController(MainWindow *window, CircuitWidget *circuit,
                             QObject *parent = 0l);

    bool componentsVisible() const;
    void setComponentsVisible(bool v);
    bool codeVisible() const;
    void setCodeVisible(bool v);
    int sidePage() const;
    void setSidePage(int page);
    bool realisticParts() const;
    void setRealisticParts(bool on);
    bool spice() const;
    void setSpice(bool on);
    QString version() const;
    QObject *actions() const
    {
        return m_actions;
    }

    // Pops the classic hamburger QMenu (all menu items) under anchor
    Q_INVOKABLE void popupMenu(QQuickItem *anchor);
    // Native menu of choices under anchor (not clipped by the hosting
    // QQuickWidget); returns the chosen index or -1. Blocks.
    Q_INVOKABLE int popupChoice(QQuickItem *anchor, const QStringList &items,
                                int current);
    Q_INVOKABLE void showToolTip(QQuickItem *anchor, const QString &text);
    Q_INVOKABLE void hideToolTip();
    Q_INVOKABLE void toggleTheme();

  signals:
    void panelsChanged();
    void togglesChanged();

  private:
    MainWindow *m_window;
    CircuitWidget *m_circuit;
    QObject *m_actions; // QQmlPropertyMap: name -> QAction
};

#endif
