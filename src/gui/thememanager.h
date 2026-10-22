/***************************************************************************
 *   Material Design theme manager (light / dark)                          *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QColor>
#include <QString>

class QSettings;

class ThemeManager : public QObject
{
    Q_OBJECT
    public:
        enum Theme { Light = 0, Dark = 1 };

        static ThemeManager* self();

        // Load persisted choice from settings (may be null) and apply it.
        void init( QSettings* settings );

        Theme   theme() const { return m_theme; }
        QString themeName() const { return m_theme==Dark ? "dark" : "light"; }
        bool    isDark() const { return m_theme == Dark; }

        // Theme-aware colours for manually painted items
        QColor canvasBackground() const;
        QColor gridDot() const;
        QColor wireColor() const;
        QColor wireSelected() const;
        QColor textColor() const;
        QColor selectionColor() const;
        QColor accentColor() const;

    public slots:
        void setTheme( Theme theme );
        void setDark( bool dark ) { setTheme( dark ? Dark : Light ); }
        void toggle() { setDark( !isDark() ); }

    signals:
        void themeChanged( bool dark );

    private:
        explicit ThemeManager( QObject* parent = 0l );
        void apply();

        Theme      m_theme;
        QSettings* m_settings;
        bool       m_applied;
};

#endif
