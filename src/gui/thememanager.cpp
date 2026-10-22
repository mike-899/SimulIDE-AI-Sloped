/***************************************************************************
 *   Material Design theme manager (light / dark)                          *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#include "thememanager.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>

ThemeManager* ThemeManager::self()
{
    static ThemeManager* inst = 0l;
    if( !inst ) inst = new ThemeManager( qApp );
    return inst;
}

ThemeManager::ThemeManager( QObject* parent )
            : QObject( parent )
{
    m_theme    = Light;
    m_settings = 0l;
    m_applied  = false;
}

void ThemeManager::init( QSettings* settings )
{
    m_settings = settings;
    if( m_settings && m_settings->contains( "theme" ) )
        m_theme = ( m_settings->value( "theme" ).toString() == "dark" ) ? Dark : Light;

    QStyle* fusion = QStyleFactory::create( "Fusion" );
    if( fusion ) qApp->setStyle( fusion );

    QFontDatabase db;
    QStringList families = db.families();
    QFont font = qApp->font();
    const QStringList prefs = { "Inter", "Roboto", "Noto Sans", "Segoe UI", "Cantarell", "DejaVu Sans" };
    for( const QString& f : prefs )
    {
        if( families.contains( f ) ) { font.setFamily( f ); break; }
    }
    qApp->setFont( font );

    apply();
}

void ThemeManager::setTheme( Theme theme )
{
    if( m_applied && theme == m_theme ) return;
    m_theme = theme;
    if( m_settings ) m_settings->setValue( "theme", themeName() );
    apply();
}

void ThemeManager::apply()
{
    bool dark = isDark();
    QPalette p;
    if( dark )
    {
        p.setColor( QPalette::Window,          QColor("#1E1F22") );
        p.setColor( QPalette::WindowText,      QColor("#E3E3E8") );
        p.setColor( QPalette::Base,            QColor("#2B2D31") );
        p.setColor( QPalette::AlternateBase,   QColor("#313338") );
        p.setColor( QPalette::Text,            QColor("#E3E3E8") );
        p.setColor( QPalette::Button,          QColor("#2B2D31") );
        p.setColor( QPalette::ButtonText,      QColor("#E3E3E8") );
        p.setColor( QPalette::ToolTipBase,     QColor("#3A3C42") );
        p.setColor( QPalette::ToolTipText,     QColor("#F2F2F5") );
        p.setColor( QPalette::Highlight,       QColor("#A8C7FA") );
        p.setColor( QPalette::HighlightedText, QColor("#062E6F") );
        p.setColor( QPalette::Link,            QColor("#A8C7FA") );
        p.setColor( QPalette::PlaceholderText, QColor("#8E9099") );
        p.setColor( QPalette::Disabled, QPalette::Text,       QColor("#6B6E76") );
        p.setColor( QPalette::Disabled, QPalette::ButtonText, QColor("#6B6E76") );
        p.setColor( QPalette::Disabled, QPalette::WindowText, QColor("#6B6E76") );
    }else{
        p.setColor( QPalette::Window,          QColor("#FFFFFF") );
        p.setColor( QPalette::WindowText,      QColor("#1B1B1F") );
        p.setColor( QPalette::Base,            QColor("#FFFFFF") );
        p.setColor( QPalette::AlternateBase,   QColor("#F5F6F8") );
        p.setColor( QPalette::Text,            QColor("#1B1B1F") );
        p.setColor( QPalette::Button,          QColor("#F3F4F6") );
        p.setColor( QPalette::ButtonText,      QColor("#1B1B1F") );
        p.setColor( QPalette::ToolTipBase,     QColor("#313338") );
        p.setColor( QPalette::ToolTipText,     QColor("#F2F2F5") );
        p.setColor( QPalette::Highlight,       QColor("#1A73E8") );
        p.setColor( QPalette::HighlightedText, QColor("#FFFFFF") );
        p.setColor( QPalette::Link,            QColor("#1A73E8") );
        p.setColor( QPalette::PlaceholderText, QColor("#80868B") );
        p.setColor( QPalette::Disabled, QPalette::Text,       QColor("#A0A4AA") );
        p.setColor( QPalette::Disabled, QPalette::ButtonText, QColor("#A0A4AA") );
        p.setColor( QPalette::Disabled, QPalette::WindowText, QColor("#A0A4AA") );
    }
    qApp->setPalette( p );

    QFile file( dark ? ":/themes/material-dark.qss" : ":/themes/material-light.qss" );
    if( file.open( QFile::ReadOnly ) )
        qApp->setStyleSheet( QString::fromUtf8( file.readAll() ) );

    m_applied = true;
    emit themeChanged( dark );
}

QColor ThemeManager::canvasBackground() const { return isDark() ? QColor("#1E1F22") : QColor("#F5F6F8"); }
QColor ThemeManager::gridDot()          const { return isDark() ? QColor("#3A3D44") : QColor("#C9CDD3"); }
QColor ThemeManager::wireColor()        const { return isDark() ? QColor("#C4C7CF") : QColor( 40, 40, 60 ); }
QColor ThemeManager::wireSelected()     const { return isDark() ? QColor("#A8C7FA") : QColor("#1A73E8"); }
QColor ThemeManager::textColor()        const { return isDark() ? QColor("#E3E3E8") : QColor("#1B1B1F"); }
QColor ThemeManager::selectionColor()   const { return isDark() ? QColor("#A8C7FA") : QColor("#1A73E8"); }
QColor ThemeManager::accentColor()      const { return isDark() ? QColor("#A8C7FA") : QColor("#1A73E8"); }
