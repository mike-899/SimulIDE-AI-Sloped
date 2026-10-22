/***************************************************************************
 *   Copyright (C) 2010 by santiago González                               *
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
 
#include "circuitwidget.h"
#include "mainwindow.h"
#include "circuit.h"
#include "filebrowser.h"

CircuitWidget*  CircuitWidget::m_pSelf = 0l;

CircuitWidget::CircuitWidget( QWidget *parent  )
             : QWidget( parent )
             , m_verticalLayout(this)
             , m_horizontLayout()
             , m_circView(this)
             , m_plotter(this)
{
    m_pSelf = this;

    m_circView.setObjectName( "circuitCanvas" );

    m_verticalLayout.setObjectName( "verticalLayout" );
    m_verticalLayout.setContentsMargins(0, 0, 0, 0);
    m_verticalLayout.setSpacing(0);

    m_verticalLayout.addWidget( &m_circView );

    m_verticalLayout.addLayout( &m_horizontLayout );
    m_horizontLayout.addWidget( &m_plotter );

    m_rateLabel = new QLabel( this );
    m_rateLabel->setObjectName( "simSpeedLabel" );
    QFont font( "Arial", 10, QFont::Normal );
    double fontScale = MainWindow::self()->fontScale();
    font.setPixelSize( int(10*fontScale) );
    m_rateLabel->setFont( font );

    createActions();

    QString appPath = QCoreApplication::applicationDirPath();

    m_lastCircDir = MainWindow::self()->settings()->value("lastCircDir").toByteArray();
    if( m_lastCircDir.isEmpty() )  m_lastCircDir = appPath + "..share/simulide/examples";

    newCircuit();
    setRate(0);
}
CircuitWidget::~CircuitWidget() { }

void CircuitWidget::clear()
{
    m_circView.clear();
    m_circView.setCircTime( 0 );
}

static QAction* makeAction( QObject* parent, const QString &icon, const QString &text,
                            const QString &tip, const QKeySequence &shortcut = QKeySequence() )
{
    QAction* act = new QAction( QIcon( ":/material/"+icon+".svg" ), text, parent );
    act->setStatusTip( tip );
    QString toolTip = tip;
    if( !shortcut.isEmpty() ) toolTip += "  ("+shortcut.toString( QKeySequence::NativeText )+")";
    act->setToolTip( toolTip );
    return act;
}

void CircuitWidget::createActions()
{
    // NOTE: Ctrl+N/O/S/C/V/Z/Y and Delete are handled by Circuit::keyPressEvent
    // (canvas focus) and EditorWindow (code focus), so actions only show the
    // shortcut text and do not register global shortcuts.

    newCircAct = makeAction( this, "note_add", tr("New C&ircuit\tCtrl+N"), tr("Create a new Circuit"), QKeySequence("Ctrl+N") );
    newCircAct->setObjectName( "newCircuitAction" );
    connect( newCircAct, SIGNAL( triggered()), this, SLOT( newCircuit()));

    openCircAct = makeAction( this, "folder_open", tr("&Open Circuit\tCtrl+O"), tr("Open an existing Circuit"), QKeySequence("Ctrl+O") );
    openCircAct->setObjectName( "openCircuitAction" );
    connect( openCircAct, SIGNAL( triggered()), this, SLOT(openCirc()));

    saveCircAct = makeAction( this, "save", tr("&Save Circuit\tCtrl+S"), tr("Save the Circuit to disk"), QKeySequence("Ctrl+S") );
    saveCircAct->setObjectName( "saveCircuitAction" );
    connect( saveCircAct, SIGNAL( triggered()), this, SLOT(saveCirc()));

    saveCircAsAct = makeAction( this, "save_as", tr("Save Circuit &As...\tCtrl+Shift+S"), tr("Save the Circuit under a new name"), QKeySequence("Ctrl+Shift+S") );
    saveCircAsAct->setObjectName( "saveCircuitAsAction" );
    connect( saveCircAsAct, SIGNAL( triggered()), this, SLOT(saveCircAs()));

    powerCircAct = makeAction( this, "play_arrow", tr("Start Simulation"), tr("Power the Circuit") );
    powerCircAct->setObjectName( "powerCircuitAction" );
    connect( powerCircAct, SIGNAL( triggered()), this, SLOT(powerCirc()));

    pauseSimAct = makeAction( this, "pause", tr("Pause Simulation"), tr("Pause / Resume Simulation") );
    pauseSimAct->setObjectName( "pauseSimAction" );
    connect( pauseSimAct, SIGNAL( triggered()), this, SLOT(pauseSim()));

    infoAct = makeAction( this, "help", tr("Online Help"), tr("Online Help") );
    connect( infoAct, SIGNAL( triggered()), this, SLOT(openInfo()));

    aboutAct = makeAction( this, "info", tr("About SimulIDE"), tr("About SimulIDE") );
    connect( aboutAct, SIGNAL( triggered()), this, SLOT(about()));

    aboutQtAct = makeAction( this, "info", tr("About Qt"), tr("About Qt") );
    connect( aboutQtAct, SIGNAL(triggered()), qApp, SLOT(aboutQt()) );

    // Edit toolbar
    copyAct = makeAction( this, "content_copy", tr("Copy"), tr("Copy selected components"), QKeySequence("Ctrl+C") );
    copyAct->setObjectName( "copyAction" );
    connect( copyAct, SIGNAL(triggered()), this, SLOT(copySelection()) );

    pasteAct = makeAction( this, "content_paste", tr("Paste"), tr("Paste components"), QKeySequence("Ctrl+V") );
    pasteAct->setObjectName( "pasteAction" );
    connect( pasteAct, SIGNAL(triggered()), this, SLOT(pasteClipboard()) );

    deleteAct = makeAction( this, "delete", tr("Delete"), tr("Delete selected components"), QKeySequence("Del") );
    deleteAct->setObjectName( "deleteAction" );
    connect( deleteAct, SIGNAL(triggered()), this, SLOT(deleteSelection()) );

    undoAct = makeAction( this, "undo", tr("Undo"), tr("Undo"), QKeySequence("Ctrl+Z") );
    undoAct->setObjectName( "undoAction" );
    connect( undoAct, SIGNAL(triggered()), this, SLOT(undo()) );

    redoAct = makeAction( this, "redo", tr("Redo"), tr("Redo"), QKeySequence("Ctrl+Y") );
    redoAct->setObjectName( "redoAction" );
    connect( redoAct, SIGNAL(triggered()), this, SLOT(redo()) );

    rotateCWAct = makeAction( this, "rotate_right", tr("Rotate"), tr("Rotate selected clockwise") );
    rotateCWAct->setObjectName( "rotateCWAction" );
    connect( rotateCWAct, SIGNAL(triggered()), this, SLOT(rotateSelectionCW()) );

    rotateCCWAct = makeAction( this, "rotate_left", tr("Rotate CCW"), tr("Rotate selected counter-clockwise") );
    rotateCCWAct->setObjectName( "rotateCCWAction" );
    connect( rotateCCWAct, SIGNAL(triggered()), this, SLOT(rotateSelectionCCW()) );

    hFlipAct = makeAction( this, "flip", tr("Flip Horizontal"), tr("Flip selected horizontally") );
    hFlipAct->setObjectName( "hFlipAction" );
    connect( hFlipAct, SIGNAL(triggered()), this, SLOT(hFlipSelection()) );

    vFlipAct = makeAction( this, "flip_vertical", tr("Flip Vertical"), tr("Flip selected vertically") );
    vFlipAct->setObjectName( "vFlipAction" );
    connect( vFlipAct, SIGNAL(triggered()), this, SLOT(vFlipSelection()) );

    zoomInAct = makeAction( this, "zoom_in", tr("Zoom In"), tr("Zoom In") );
    zoomInAct->setObjectName( "zoomInAction" );
    connect( zoomInAct, SIGNAL(triggered()), &m_circView, SLOT(zoomIn()) );

    zoomOutAct = makeAction( this, "zoom_out", tr("Zoom Out"), tr("Zoom Out") );
    zoomOutAct->setObjectName( "zoomOutAction" );
    connect( zoomOutAct, SIGNAL(triggered()), &m_circView, SLOT(zoomOut()) );

    zoomFitAct = makeAction( this, "fit_screen", tr("Zoom to Fit"), tr("Fit the whole circuit in view") );
    zoomFitAct->setObjectName( "zoomFitAction" );
    connect( zoomFitAct, SIGNAL(triggered()), &m_circView, SLOT(zoomToFit()) );

    zoomResetAct = makeAction( this, "search", tr("Zoom 100%"), tr("Reset zoom to 100%") );
    zoomResetAct->setObjectName( "zoomResetAction" );
    connect( zoomResetAct, SIGNAL(triggered()), &m_circView, SLOT(zoomReset()) );

    importCircAct = makeAction( this, "input", tr("Import Circuit"), tr("Import a circuit into the current one") );
    importCircAct->setObjectName( "importCircuitAction" );
    connect( importCircAct, SIGNAL(triggered()), this, SLOT(importCirc()) );

    saveImageAct = makeAction( this, "image", tr("Save Circuit as Image"), tr("Save Circuit as Image") );
    saveImageAct->setObjectName( "saveImageAction" );
    connect( saveImageAct, SIGNAL(triggered()), &m_circView, SLOT(saveImage()) );

    createSubcircAct = makeAction( this, "account_tree", tr("Create SubCircuit"), tr("Create SubCircuit") );
    createSubcircAct->setObjectName( "createSubcircuitAction" );
    connect( createSubcircAct, SIGNAL(triggered()), this, SLOT(createSubcircuit()) );

    bomAct = makeAction( this, "receipt_long", tr("Bill of Materials"), tr("Bill of Materials") );
    bomAct->setObjectName( "bomAction" );
    connect( bomAct, SIGNAL(triggered()), this, SLOT(bom()) );
}

QList<Component*> CircuitWidget::selectedComponents()
{
    QList<Component*> list;
    for( QGraphicsItem* item : Circuit::self()->selectedItems() )
    {
        Component* comp = qgraphicsitem_cast<Component*>( item );
        if( !comp ) continue;
        if( comp->itemType() == "Connector" ) continue;
        list.append( comp );
    }
    return list;
}

void CircuitWidget::copySelection()
{
    Circuit::self()->copy( m_circView.visibleCenter() );
}

void CircuitWidget::pasteClipboard()
{
    // Paste slightly offset so pasted items do not hide the originals
    Circuit::self()->paste( m_circView.visibleCenter()+QPointF( 16, 16 ) );
}

void CircuitWidget::deleteSelection()
{
    Circuit::self()->removeItems();
}

void CircuitWidget::undo() { Circuit::self()->undo(); }
void CircuitWidget::redo() { Circuit::self()->redo(); }

void CircuitWidget::rotateSelectionCW()
{
    for( Component* comp : selectedComponents() ) comp->rotateCW();
}

void CircuitWidget::rotateSelectionCCW()
{
    for( Component* comp : selectedComponents() ) comp->rotateCCW();
}

void CircuitWidget::hFlipSelection()
{
    for( Component* comp : selectedComponents() ) comp->H_flip();
}

void CircuitWidget::vFlipSelection()
{
    for( Component* comp : selectedComponents() ) comp->V_flip();
}

void CircuitWidget::importCirc()
{
    Circuit::self()->importCirc( m_circView.visibleCenter() );
}

void CircuitWidget::createSubcircuit() { Circuit::self()->createSubcircuit(); }
void CircuitWidget::bom() { Circuit::self()->bom(); }

bool CircuitWidget::newCircuit()
{
    powerCircOff();
    
    if( MainWindow::self()->windowTitle().endsWith('*') )
    {
        const QMessageBox::StandardButton ret
        = QMessageBox::warning(this, "MainWindow::closeEvent",
                               tr("\nCircuit has been modified.\n"
                                  "Do you want to save your changes?\n"),
          QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
                               
        if     ( ret == QMessageBox::Save )   saveCirc();
        else if( ret == QMessageBox::Cancel ) return false;
    }
    clear();
    Circuit::self()->setAutoBck( MainWindow::self()->autoBck() );
    m_curCirc = "";

    MainWindow::self()->setTitle( tr("New Circuit"));
    MainWindow::self()->settings()->setValue( "lastCircDir", m_lastCircDir );
    
    return true;
}

void CircuitWidget::openCirc()
{
    const QString dir = m_lastCircDir;
    QString fileName
    = QFileDialog::getOpenFileName( 0l, tr("Load Circuit"), dir,
                                        tr("Circuits (*.simu);;All files (*.*)"));

    loadCirc( fileName );
}

void CircuitWidget::loadCirc( QString path )
{
    if( !path.isEmpty() && path.endsWith(".simu") )
    {
        newCircuit();
        Circuit::self()->loadCircuit( path );
   
        m_curCirc = path;
        m_lastCircDir = path;
        MainWindow::self()->setTitle(path.split("/").last());
        MainWindow::self()->settings()->setValue( "lastCircDir", m_lastCircDir );
        //FileBrowser::self()->setPath(m_lastCircDir);
        m_circView.setCircTime( 0 );
    }
}

void CircuitWidget::saveCirc()
{
    bool saved = false;
    if( m_curCirc.isEmpty() ) saved =  saveCircAs();
    else                      saved =  Circuit::self()->saveCircuit( m_curCirc );
    
    if( saved ) 
    {
        QString fileName = m_curCirc;
        MainWindow::self()->setTitle(fileName.split("/").last());
    }
}

bool CircuitWidget::saveCircAs()
{
    const QString dir = m_lastCircDir;
    QString fileName = QFileDialog::getSaveFileName( this, tr("Save Circuit"), dir,
                                                     tr("Circuits (*.simu);;All files (*.*)"));
    if (fileName.isEmpty()) return false;

    m_curCirc = fileName;
    m_lastCircDir = fileName;
    
    bool saved = Circuit::self()->saveCircuit(fileName);
    if( saved ) 
    {
        QString fileName = m_curCirc;
        MainWindow::self()->setTitle(fileName.split("/").last());
        MainWindow::self()->settings()->setValue( "lastCircDir", m_lastCircDir );
        //FileBrowser::self()->setPath(m_lastCircDir);
    }
    return saved;
}

void CircuitWidget::powerCirc()
{
    if     ( powerCircAct->iconText() == "Off" ) powerCircOn();
    else if( powerCircAct->iconText() == "On" )  powerCircOff();
}

QString CircuitWidget::simState()
{
    if( Simulator::self()->isPaused() ) return "Paused";
    return powerCircAct->iconText();
}

void CircuitWidget::powerCircOn()
{
    powerCircAct->setIcon(QIcon(":/material/stop.svg"));
    powerCircAct->setText( tr("Stop Simulation") );
    powerCircAct->setEnabled( true );
    powerCircAct->setIconText("On");
    pauseSimAct->setEnabled( true );
    Simulator::self()->runContinuous();
    emit simStateChanged( simState() );
}

void CircuitWidget::powerCircOff()
{
    if( Simulator::self()->isPaused() ) Simulator::self()->resumeSim();
    Simulator::self()->stopSim();

    powerCircAct->setIcon(QIcon(":/material/play_arrow.svg"));
    powerCircAct->setText( tr("Start Simulation") );
    powerCircAct->setIconText("Off");
    powerCircAct->setEnabled( true );
    pauseSimAct->setEnabled( false );
    emit simStateChanged( simState() );
}

void CircuitWidget::powerCircDebug( bool run )
{
    powerCircAct->setIcon(QIcon(":/material/bug_report.svg"));
    powerCircAct->setText( tr("Debugging") );
    powerCircAct->setIconText("Debug");
    powerCircAct->setEnabled( true );

    Simulator::self()->debug( run );
    m_rateLabel->setText( tr("Real Speed: Debugger") );
    emit simStateChanged( simState() );
}

void CircuitWidget::pauseSim()
{
    if( Simulator::self()->isRunning() )
    {
        Simulator::self()->pauseSim();
        powerCircAct->setEnabled( false );
    }
    else if( Simulator::self()->isPaused() )
    {
        Simulator::self()->resumeSim();
        powerCircAct->setEnabled( true );
    }
    emit simStateChanged( simState() );

}

void CircuitWidget::openInfo()
{
    QDesktopServices::openUrl(QUrl("http://simulide.blogspot.com"));
}

void CircuitWidget::about()
{
    QString date = QDate::currentDate().toString( "dd-MM-yyyy" );
    date.append( " (dd-MM-yyyy)" );

    QString t ="&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;";
    QMessageBox::about( this, tr("About SimulIDE"),
                        "<br><br>"
    "<b>Version:</b>&nbsp; &nbsp; &nbsp;"+QString( APP_VERSION )+"<br><b>Compiled:</b> "+date+"<br><br><br>"
    "<b>Web site:</b> <a href=\"https://simulide.blogspot.com/\"> https://simulide.blogspot.com/ </a><br><br>"
    "<b>Become a Patron:</b> <a href=\"https://www.patreon.com/simulide\"> https://www.patreon.com/simulide </a><br><br>"
    "<br><br>"
               "<b>Creator:</b> Santiago Gonzalez. <br>"
               "<br>"
               "<b>Developers:</b> <br>"
               +t+"Santiago Gonzalez. <br>"
               +t+"Popov Alexey <br>"
               "<br>"
               "<b>Contributors:</b> <br>"
               +t+"Chris Roper <br>"
               +t+"Sergei Chiyanov <br>"
               +t+"Sergey Roenko <br>"
               +t+"Gabor Nagy <br>"
               "<br>"
               "<b>Translations:</b> <br>"
               +t+"Spanish: Santiago Gonzalez. <br>"
               +t+t+"&nbsp; Kike Gl. <br>"
               +t+"Russian: Sergei Chiyanov <br>"
               +t+"French:  Pascal Cotret <br>"
               );
}

void CircuitWidget::setRate( int rate )
{
    if( rate < 0 ) m_rateLabel->setText( tr("Circuit ERROR!!!") );
    else 
        m_rateLabel->setText( tr("Real Speed: ")+QString::number(rate) +" %" );
}

#include "moc_circuitwidget.cpp"
