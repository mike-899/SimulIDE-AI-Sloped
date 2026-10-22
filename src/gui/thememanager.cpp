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
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QHash>

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

QColor ThemeManager::iconColor()         const { return isDark() ? QColor("#E3E3E3") : QColor("#1F1F1F"); }
QColor ThemeManager::iconDisabledColor() const { QColor c = iconColor(); c.setAlpha( 97 ); return c; }

// ---- Theme aware SVG icon engine ---------------------------------------
class MaterialIconEngine : public QIconEngine
{
    public:
        MaterialIconEngine( const QString& name, const QColor& fixed )
            : m_name( name ), m_fixed( fixed ) {}

        QIconEngine* clone() const override { return new MaterialIconEngine( m_name, m_fixed ); }

        QPixmap pixmap( const QSize& size, QIcon::Mode mode, QIcon::State ) override
        {
            return render( size, 1.0, mode );
        }

        void paint( QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State ) override
        {
            qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
            painter->drawPixmap( rect, render( rect.size()*dpr, dpr, mode ) );
        }

        QSize actualSize( const QSize& size, QIcon::Mode, QIcon::State ) override { return size; }

    private:
        QPixmap render( const QSize& size, qreal scale, QIcon::Mode mode )
        {
            if( size.isEmpty() ) return QPixmap();
            ThemeManager* tm = ThemeManager::self();
            QColor c = m_fixed.isValid() ? m_fixed : tm->iconColor();
            if( mode == QIcon::Disabled )
                c = m_fixed.isValid() ? QColor( c.red(), c.green(), c.blue(), 97 ) : tm->iconDisabledColor();

            QString key = m_name+"|"+c.name( QColor::HexArgb )+"|"+QString::number( size.width() )
                         +"x"+QString::number( size.height() )+"@"+QString::number( scale );
            static QHash<QString, QPixmap> cache;
            auto it = cache.constFind( key );
            if( it != cache.constEnd() ) return *it;

            QSvgRenderer svg( QString(":/material/%1.svg").arg( m_name ) );
            QSize px = size;
            QImage img( px, QImage::Format_ARGB32_Premultiplied );
            img.fill( Qt::transparent );
            if( svg.isValid() )
            {
                QPainter p( &img );
                QSizeF ds = svg.defaultSize();
                if( ds.isEmpty() ) ds = QSizeF( 1, 1 );
                ds.scale( QSizeF( px ), Qt::KeepAspectRatio );
                svg.render( &p, QRectF( (px.width()-ds.width())/2, (px.height()-ds.height())/2, ds.width(), ds.height() ) );
                p.setCompositionMode( QPainter::CompositionMode_SourceIn );
                p.fillRect( img.rect(), c );
            }
            QPixmap pm = QPixmap::fromImage( img );
            pm.setDevicePixelRatio( scale );
            cache.insert( key, pm );
            return pm;
        }

        QString m_name;
        QColor  m_fixed;
};

QIcon ThemeManager::icon( const QString& name, const QColor& fixed )
{
    return QIcon( new MaterialIconEngine( name, fixed ) );
}
