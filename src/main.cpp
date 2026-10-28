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

#include <QApplication>
#include <QDebug>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTranslator>

#ifdef _WIN32
#    include <windows.h>
#endif

QString langFile( QString locale )
{
    QString langF = "../share/simulide/translations/simulide_"+locale+".qm";

    QFile file( langF );
    if( !file.exists() ) langF = "";

    return langF;
}

// The QML shell renders with Qt Quick: use the software renderer when
// SIMULIDE_SOFTWARE_RENDER=1, when the user already picked a backend with
// QT_QUICK_BACKEND, or when no OpenGL context can be created (VMs, remote
// desktops, missing drivers) so the UI never fails to show.
static void chooseRenderBackend()
{
    if (qEnvironmentVariableIsSet("QT_QUICK_BACKEND") ||
        qEnvironmentVariableIsSet("QSG_RHI_BACKEND"))
        return; // User choice

    bool software = qEnvironmentVariable("SIMULIDE_SOFTWARE_RENDER") == "1";
    if (!software) {
        QOpenGLContext ctx;
        QOffscreenSurface surface;
        surface.create();
        if (!ctx.create() || !surface.isValid() || !ctx.makeCurrent(&surface)) {
            qWarning() << "OpenGL not available: using the software renderer";
            software = true;
        } else
            ctx.doneCurrent();
    }
    if (software)
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
}

int main(int argc, char *argv[])
{

#ifdef _WIN32
    QStringList paths = QCoreApplication::libraryPaths();
    paths.append("plugins");
    paths.append("plugins/platforms");
    paths.append("plugins/imageformats");
    paths.append("plugins/sqldrivers");
    paths.append("plugins/bearer");
    paths.append("plugins/generic");
    paths.append("plugins/iconengines");
    paths.append("plugins/qmltooling");
    paths.append("plugins/printsupport");
    QCoreApplication::setLibraryPaths(paths);
    
    if (AttachConsole(ATTACH_PARENT_PROCESS)) 
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif

    //QApplication::setGraphicsSystem( "raster" );//native, raster, opengl
    QApplication app( argc, argv );
    chooseRenderBackend();

    QSettings settings(
        QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation)
                .first() +
            "/simulide.ini",
        QSettings::IniFormat, 0l);

    QString locale = QLocale::system().name();
    if( settings.contains( "language" ) ) locale = settings.value( "language" ).toString();

    QString langF = langFile( locale );
    if( langF == "" )
    {
        locale = QLocale::system().name().split("_").first();
        langF = langFile( locale );
    }
    if( langF == "" ) langF = "../share/simulide/translations/simulide_en.qm";
    
    QTranslator translator;
    if (!translator.load(langF))
        qWarning() << "Cannot load translation" << langF;
    app.installTranslator( &translator );

    MainWindow window;
    window.setLoc( locale );
    
    /*QRect screenGeometry = QApplication::desktop()->screenGeometry();
    int x = ( screenGeometry.width()-window.width() ) / 2;
    int y = ( screenGeometry.height()-window.height() ) / 2;
    window.move( x, y );*/
    window.scroll( 0, 50 );

    window.show();
    app.setApplicationVersion( APP_VERSION );
    return app.exec();
}

