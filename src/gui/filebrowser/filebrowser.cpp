/***************************************************************************
 *   Copyright (C) 2018 by Pavel Lamonov                                   *
 *   leamonpaul@yandex.ru                                                  *
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

#include "filebrowser.h"

#include "circuit.h"
#include "circuitwidget.h"
#include "editorwindow.h"
#include "filewidget.h"
#include "mainwindow.h"
#include "thememanager.h"

#include <QFileIconProvider>

// Material icons for the file tree (the platform provider often returns
// blank icons when no desktop icon theme is installed).
class MaterialFileIconProvider : public QFileIconProvider
{
  public:
    QIcon icon(IconType type) const override
    {
        if (type == Drive)
            return ThemeManager::icon("hard_drive");
        if (type == File)
            return ThemeManager::icon("draft");
        return ThemeManager::icon("folder");
    }
    QIcon icon(const QFileInfo &info) const override
    {
        if (info.isRoot())
            return ThemeManager::icon("hard_drive");
        if (info.isDir())
            return ThemeManager::icon("folder");
        QString suf = info.suffix().toLower();
        if (suf == "simu")
            return ThemeManager::icon("account_tree");
        if (suf == "png" || suf == "jpg" || suf == "svg" || suf == "bmp")
            return ThemeManager::icon("image");
        if (suf == "ino" || suf == "asm" || suf == "c" || suf == "cpp" ||
            suf == "h" || suf == "gcb" || suf == "hex" || suf == "py")
            return ThemeManager::icon("code");
        return ThemeManager::icon("draft");
    }
};

FileBrowser*  FileBrowser::m_pSelf = 0l;

FileBrowser::FileBrowser( QWidget *parent ) 
           : QTreeView( parent )
{
    m_pSelf = this;
    m_showHidden = false;
    
    m_fileSystemModel = new QFileSystemModel(this);
    static MaterialFileIconProvider iconProvider;
    m_fileSystemModel->setIconProvider(&iconProvider);
    m_fileSystemModel->setRootPath( QDir::rootPath() );
    
    m_currentPath = QDir::rootPath();
    
    setModel(m_fileSystemModel);
    setRootIndex( m_fileSystemModel->index( QDir::rootPath() ));
    
    setHeaderHidden( true );
    hideColumn( 1 );
    hideColumn( 2 );
    hideColumn( 3 );
    
    double fontScale = MainWindow::self()->fontScale();
    QString fontSize = QString::number( int(11*fontScale) );
    
    setStyleSheet("font-size:"+fontSize+"px; border: 0px solid red");
}

FileBrowser::~FileBrowser() { }

void FileBrowser::cdUp()
{
    QModelIndex currentDir = m_fileSystemModel->index( m_currentPath );
    
    setPath( m_fileSystemModel->filePath( currentDir.parent() ) );
}

void FileBrowser::openInEditor()
{
    QString path = m_fileSystemModel->filePath( currentIndex() );
    EditorWindow::self()->loadFile( path );
}

void FileBrowser::open()
{
    QString path = m_fileSystemModel->filePath( currentIndex() );
    
    if( path.isEmpty() ) return;
    
    if( m_fileSystemModel->isDir( currentIndex() ) )
    {
        setPath( path );
    }
    else  
    {
        if( path.endsWith(".simu") ) CircuitWidget::self()->loadCirc( path );
        else                         EditorWindow::self()->loadFile( path );
    }
}

void FileBrowser::setPath( QString path )
{
    m_currentPath = path;
    
    FileWidget::self()->setPath( path );
    
    QModelIndex index = m_fileSystemModel->index( path );
    
    if( path == QDir::rootPath() ) index = index.parent();
    setRootIndex( index );
}

void FileBrowser::addBookMark()
{
    
    QString fileName = m_fileSystemModel->fileName( currentIndex() );
    QString filePath = m_fileSystemModel->filePath( currentIndex() );
    
    FileWidget::self()->addBookMark( filePath );
}

void FileBrowser::showHidden()
{
    m_showHidden = !m_showHidden;
    if( m_showHidden ) m_fileSystemModel->setFilter( QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Hidden );
    else               m_fileSystemModel->setFilter( QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs );
}

void FileBrowser::contextMenuEvent( QContextMenuEvent* event )
{
    QTreeView::contextMenuEvent( event );

    if( !event->isAccepted() )
    {
        event->accept();
        QPoint eventPos = event->globalPos();

        QMenu menu;
        
        if( m_fileSystemModel->isDir( currentIndex()) )
        {
            QAction *addBookMarkAction = menu.addAction(
                ThemeManager::icon("bookmark"), tr("Add Bookmark"));
            connect( addBookMarkAction, SIGNAL( triggered()), 
                     this,              SLOT(   addBookMark() ) );
                     
            menu.addSeparator();
        }
        else
        {
            QAction *openWithEditor = menu.addAction(ThemeManager::icon("code"),
                                                     tr("Open in editor"));
            connect( openWithEditor, SIGNAL( triggered()), 
                     this,           SLOT(   openInEditor()) );
                     
            menu.addSeparator();
        }
        
        QAction* showHidden = menu.addAction( tr("Show Hidden"));
        showHidden->setCheckable( true );
        showHidden->setChecked( m_showHidden );
        connect( showHidden, SIGNAL( triggered()), 
                 this,       SLOT(   showHidden()) );
        menu.exec( eventPos );
    }
}

void FileBrowser::mouseDoubleClickEvent(QMouseEvent *event)
{
    open();
}

void FileBrowser::keyPressEvent( QKeyEvent *event )
{
    bool isEnter = ((event->key() == Qt::Key_Enter) || (event->key() == Qt::Key_Return));
    
    if( isEnter ) open();
}

#include  "moc_filebrowser.cpp"
