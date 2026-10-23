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

#include "componentselector.h"

#include "mainwindow.h"
#include "partart.h"
#include "simuapi_apppath.h"
#include "thememanager.h"

#include <QDomDocument>
#include <QGraphicsView>
#include <QTimer>

// Item data roles used in the (hidden) model tree
static const int TypeRole = Qt::UserRole; // component type ("" for categories)
static const int NameRole = Qt::UserRole + 1; // display name (without info)
static const int InfoRole = Qt::UserRole + 2; // extra description

static const int CardIconW = 40;

// Combo box entries that are not real categories
static const QString BasicCat = "__basic__";
static const QString AllCat   = "__all__";

// Tinkercad-like "Basic" set: most used components, by type
static const QStringList BasicTypes = {
    "Resistor", "Potentiometer", "Capacitor",   "Led",    "Push",
    "Switch",   "Fixed Voltage", "Rail",        "Ground", "Voltage Source",
    "Clock",    "Diode",         "BJT",         "Mosfet", "Seven Segment",
    "Servo",    "Voltimeter",    "Amperimeter", "Probe",  "Oscope",
    "Arduino"
};
static const int CardIconH = 30;

//------------------------------------------------------------------------
// ComponentCard

ComponentCard::ComponentCard(QWidget *parent, const QString &name,
                             const QString &info, const QIcon &icon,
                             const QString &type, const QString &dragId) :
    QFrame(parent)
{
    m_name   = name;
    m_type   = type;
    m_dragId = dragId;

    setObjectName("componentCard");
    setProperty("componentType", type);
    setFrameShape(QFrame::StyledPanel);
    setCursor(Qt::OpenHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(72);

    QString tip = name;
    if (!info.trimmed().isEmpty())
        tip += "\n" + info.trimmed();
    tip += "\n" + tr("Drag into the circuit to add it");
    setToolTip(tip);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 6, 4, 4);
    layout->setSpacing(2);

    QLabel *iconLabel = new QLabel(this);
    iconLabel->setObjectName("componentCardIcon");
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(icon.pixmap(CardIconW, CardIconH));
    iconLabel->setMinimumHeight(CardIconH + 4);
    layout->addWidget(iconLabel);

    QLabel *nameLabel = new QLabel(this);
    nameLabel->setObjectName("componentCardLabel");
    nameLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    nameLabel->setWordWrap(true);
    QFont font = nameLabel->font();
    font.setPixelSize(int(10 * MainWindow::self()->fontScale()));
    nameLabel->setFont(font);
    QFontMetrics fm(font);
    nameLabel->setFixedHeight(fm.lineSpacing() * 2 + 2); // max 2 lines
    // Break very long single words (eg. "7SegmentBCD") so they fit
    QString text = name;
    if (!text.contains(' ') && text.size() > 11)
        text.insert(11, QChar(0x200B));
    nameLabel->setText(text);
    layout->addWidget(nameLabel);

    setFixedHeight(CardIconH + fm.lineSpacing() * 2 + 22);
}

void ComponentCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_pressPos = event->pos();
    setCursor(Qt::ClosedHandCursor);
    QFrame::mousePressEvent(event);
}

void ComponentCard::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;
    if ((event->pos() - m_pressPos).manhattanLength() <
        QApplication::startDragDistance())
        return;
    if (m_type.isEmpty())
        return;

    QMimeData *mimeData = new QMimeData;
    mimeData->setText(m_dragId);
    mimeData->setHtml(m_type);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);

    // The drag pixmap itself is transparent: over the canvas the circuit
    // shows the real component preview, so a second "ghost" would look
    // duplicated. Outside the canvas a small floating card follows the
    // cursor instead.
    QPixmap empty(1, 1);
    empty.fill(Qt::transparent);
    drag->setPixmap(empty);
    drag->setHotSpot(QPoint(0, 0));

    QLabel *ghost   = 0l;
    QTimer *tracker = 0l;
    QLabel *icon    = findChild<QLabel *>("componentCardIcon");
    if (icon && icon->pixmap()) {
        ghost = new QLabel(0l, Qt::ToolTip | Qt::FramelessWindowHint |
                                   Qt::WindowStaysOnTopHint);
        ghost->setAttribute(Qt::WA_TransparentForMouseEvents);
        ghost->setAttribute(Qt::WA_ShowWithoutActivating);
        ghost->setAttribute(Qt::WA_TranslucentBackground);
        ghost->setPixmap(*icon->pixmap());
        ghost->setStyleSheet("background: transparent;");
        ghost->adjustSize();

        tracker     = new QTimer(this);
        auto follow = [ghost]() {
            QPoint pos  = QCursor::pos();
            QWidget *w  = QApplication::widgetAt(pos);
            bool canvas = false;
            for (QWidget *p = w; p; p = p->parentWidget())
                if (qobject_cast<QGraphicsView *>(p)) {
                    canvas = true;
                    break;
                }
            ghost->move(pos + QPoint(14, 14));
            ghost->setVisible(!canvas);
        };
        connect(tracker, &QTimer::timeout, ghost, follow);
        follow();
        tracker->start(16);
    }

    drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);

    if (tracker)
        tracker->deleteLater();
    if (ghost)
        ghost->deleteLater();
    setCursor(Qt::OpenHandCursor);
}

void ComponentCard::mouseReleaseEvent(QMouseEvent *event)
{
    setCursor(Qt::OpenHandCursor);
    QFrame::mouseReleaseEvent(event);
}

//------------------------------------------------------------------------
// ComponentSelector

ComponentSelector* ComponentSelector::m_pSelf = 0l;

ComponentSelector::ComponentSelector(QWidget *parent) :
    QWidget(parent), m_pluginsdDialog(this), m_itemLibrary()
{
    m_pSelf = this;
    m_columns        = 3;
    m_row            = 0;
    m_col            = 0;
    m_refreshPending = false;

    setObjectName("componentPanel");

    m_pluginsdDialog.setVisible(false);

    createWidgets();
    LoadLibraryItems();

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this,
            SLOT(slotContextMenu(const QPoint &)));
}
ComponentSelector::~ComponentSelector()
{
}

void ComponentSelector::createWidgets()
{
    // Hidden model
    m_tree = new QTreeWidget(this);
    m_tree->setObjectName("componentModel");
    m_tree->headerItem()->setHidden(true);
    m_tree->setVisible(false);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 0);
    layout->setSpacing(6);

    // Header: category dropdown + manage button
    QWidget *header = new QWidget(this);
    header->setObjectName("componentPanelHeader");
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);

    QLabel *title = new QLabel(tr("Components"), header);
    title->setObjectName("componentPanelTitle");
    QFont tf = title->font();
    tf.setBold(true);
    title->setFont(tf);
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    m_manageButton = new QToolButton(header);
    m_manageButton->setObjectName("manageComponentsButton");
    m_manageButton->setIcon(ThemeManager::icon("tune"));
    m_manageButton->setToolTip(tr("Manage Components (show/hide)"));
    m_manageButton->setAutoRaise(true);
    connect(m_manageButton, SIGNAL(clicked()), this,
            SLOT(slotManageComponents()));
    headerLayout->addWidget(m_manageButton);
    layout->addWidget(header);

    m_categoryBox = new QComboBox(this);
    m_categoryBox->setObjectName("componentCategoryBox");
    m_categoryBox->setToolTip(tr("Component category"));
    m_categoryBox->setSizeAdjustPolicy(
        QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_categoryBox->setMinimumContentsLength(10);
    layout->addWidget(m_categoryBox);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName("componentSearch");
    m_searchEdit->setPlaceholderText(tr("Search components"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->addAction(ThemeManager::icon("search"),
                            QLineEdit::LeadingPosition);
    layout->addWidget(m_searchEdit);

    // Card grid
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("componentScroll");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_gridHost = new QWidget(m_scrollArea);
    m_gridHost->setObjectName("componentGrid");
    m_gridLayout = new QGridLayout(m_gridHost);
    m_gridLayout->setContentsMargins(0, 0, 4, 8);
    m_gridLayout->setHorizontalSpacing(6);
    m_gridLayout->setVerticalSpacing(6);
    m_scrollArea->setWidget(m_gridHost);
    layout->addWidget(m_scrollArea, 1);

    connect(m_categoryBox, SIGNAL(currentIndexChanged(int)), this,
            SLOT(slotCategoryChanged(int)));

    connect(m_searchEdit, SIGNAL(textChanged(const QString &)), this,
            SLOT(slotSearchChanged(const QString &)));
}

void ComponentSelector::LoadLibraryItems()
{
    QList<LibraryItem*> itemList = m_itemLibrary.items();

    for( LibraryItem* libItem : itemList )
    {
        this->addItem( libItem );
    }
}

void ComponentSelector::LoadCompSetAt( QDir compSetDir )
{
    compSetDir.setNameFilters( QStringList( "*.xml" ) );

    QStringList xmlList = compSetDir.entryList( QDir::Files );

    if( xmlList.isEmpty() ) return;                  // No comp sets to load

    qDebug() << "\n" << tr("    Loading Component sets at:")<< "\n" << compSetDir.absolutePath()<<"\n";

    for( QString compSetName : xmlList )
    {
        QString compSetFilePath = compSetDir.absoluteFilePath( compSetName );

        if( !compSetFilePath.isEmpty( ))  loadXml( compSetFilePath );
    }
    qDebug() << "\n";
}

void ComponentSelector::loadXml( const QString &setFile )
{
    QFile file( setFile );
    if( !file.open(QFile::ReadOnly | QFile::Text) )
    {
          QMessageBox::warning(0, "ComponentSelector::loadXml", tr("Cannot read file %1:\n%2.").arg(setFile).arg(file.errorString()));
          return;
    }
    QDomDocument domDoc;
    if( !domDoc.setContent(&file) )
    {
         QMessageBox::warning(0, "ComponentSelector::loadXml", tr("Cannot set file %1\nto DomDocument").arg(setFile));
         file.close();
         return;
    }
    file.close();

    QDomElement root  = domDoc.documentElement();
    QDomNode    rNode = root.firstChild();

    while( !rNode.isNull() )
    {
        QDomElement element = rNode.toElement();
        QDomNode    node    = element.firstChild();

        QString category = element.attribute( "category" );
        //const char* charCat = category.toUtf8().data();
        std::string stdCat = category.toStdString();
        const char* charCat = &(stdCat[0]);
        category = QApplication::translate( "xmlfile", charCat );
        //qDebug()<<"category = " <<category;
        
        QString type = element.attribute( "type");
        LibraryItem* parent = m_itemLibrary.libraryItem( type );

        if( parent )
        {
            while( !node.isNull() )
            {
                element = node.toElement();
                QString icon = "";

                if( element.hasAttribute("icon") )
                {
                    QDir compSetDir( qApp->applicationDirPath() );
                    compSetDir.cd( "../share/simulide/data/images" );
                    icon = compSetDir.absoluteFilePath( element.attribute("icon") );
                }
                QString name = element.attribute( "name" );
                
                m_xmlFileList[ name ] = setFile;   // Save xml File used to create this item
                
                if( element.hasAttribute("info") ) name += "???"+element.attribute( "info" );
                
                addItem( name, category, icon, type );
                
                node = node.nextSibling();
            }
        }
        rNode = rNode.nextSibling();
    }
    QString compSetName = setFile.split( "/").last();

    qDebug() << tr("        Loaded Component set:           ") << compSetName;

    m_compSetUnique.append( compSetName );
}

QString ComponentSelector::getXmlFile( QString compName )
{
    return m_xmlFileList[ compName ];
}

QList<QTreeWidgetItem *>
ComponentSelector::findItems(const QString &text, Qt::MatchFlags flags) const
{
    return m_tree->findItems(text, flags);
}

void ComponentSelector::addLibraryItem( LibraryItem* libItem ) // Used By Plugins
{
    m_itemLibrary.addItem( libItem );
    this->addItem( libItem );
}

void ComponentSelector::addItem( LibraryItem* libItem )
{
    QString category = libItem->category();
    if( category != "")
    {
        QString icon = ":/"+libItem->iconfile();
        addItem( libItem->name(), category, icon, libItem->type() );
    }
}

void ComponentSelector::addItem( const QString &caption, const QString &_category, const QString &icon, const QString &type )
{
    QStringList nameFull = caption.split( "???" );
    QString         name = nameFull.first();
    QString info = "";
    if( nameFull.size() > 1 ) info = nameFull.last();

    QSettings *settings = MainWindow::self()->settings();

    bool hidden = settings->value(name + "/hidden").toBool();

    QTreeWidgetItem* catItem = 0l;

    QStringList catPath = _category.split( "/" );
    bool      isRootCat = (catPath.size() == 1);
    QString    category = catPath.takeLast();
    if( category.isEmpty() ) return;

    if( !m_categories.contains( category, Qt::CaseSensitive ))  // Create new Category
    {
        bool c_hidden = false;

        if (isRootCat)
            catItem = new QTreeWidgetItem(m_tree); // Is Main Category
        else
            catItem = new QTreeWidgetItem(0);

        catItem->setFlags(QFlag(32));
        catItem->setText( 0, category );
        catItem->setData(0, NameRole, category);
        catItem->setChildIndicatorPolicy( QTreeWidgetItem::ShowIndicator );
        m_categories.append( category );

        if( !catPath.isEmpty() )
        {
            QString topCat = catPath.takeLast();

            QList<QTreeWidgetItem *> list = m_tree->findItems(
                topCat, Qt::MatchExactly | Qt::MatchRecursive);

            if( !list.isEmpty() )
            {
                QTreeWidgetItem* topItem = list.first();
                topItem->addChild( catItem );
            }
        }

        if (settings->contains(category + "/hidden"))
            c_hidden = settings->value(category + "/hidden").toBool();

        catItem->setExpanded(true);
        catItem->setHidden( c_hidden );
    }
    else                                                                // Find Category
    {
        QList<QTreeWidgetItem *> list =
            m_tree->findItems(category, Qt::MatchExactly | Qt::MatchRecursive);

        if( !list.isEmpty() ) catItem = list.first();
    }
    if( !catItem ) return;

    if( !m_categories.contains( name, Qt::CaseSensitive ) )
        m_categories.append( name );

    QTreeWidgetItem *item = new QTreeWidgetItem(0);
    item->setFlags( QFlag(32) );
    item->setIcon(0, PartArt::cardIcon(type, name, _category, icon));
    item->setText( 0, name+info );
    item->setData(0, TypeRole, type);
    item->setData(0, NameRole, name);
    item->setData(0, InfoRole, info);

    if( ( type == "Subcircuit" )
      ||( type == "PIC" )
      ||( type == "AVR" )
      ||( type == "Arduino" ) )
    {
         item->setData( 0, Qt::WhatsThisRole, name );
    }
    else item->setData( 0, Qt::WhatsThisRole, type );

    catItem->addChild( item );
    item->setHidden( hidden );

    refresh();
}

//------------------------------------------------------------------------
// Grid view

void ComponentSelector::refresh()
{
    if (m_refreshPending)
        return;
    m_refreshPending = true;
    QTimer::singleShot(0, this, SLOT(doRefresh()));
}

bool ComponentSelector::isEffectivelyHidden(QTreeWidgetItem *item) const
{
    while (item) {
        if (item->isHidden())
            return true;
        item = item->parent();
    }
    return false;
}

void ComponentSelector::updateCategoryBox()
{
    QStringList cats;
    for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
        QTreeWidgetItem *cat = m_tree->topLevelItem(i);
        if (cat->isHidden())
            continue;
        cats.append(cat->text(0));
    }
    QStringList current;
    for (int i = 2; i < m_categoryBox->count(); i++)
        current.append(m_categoryBox->itemData(i).toString());

    if (current == cats && m_categoryBox->count() > 0)
        return;

    QString selected = m_categoryBox->currentData().toString();
    if (m_categoryBox->count() == 0) // First time: restore from settings
        selected = MainWindow::self()
                       ->settings()
                       ->value("componentPanel/category", BasicCat)
                       .toString();

    m_categoryBox->blockSignals(true);
    m_categoryBox->clear();
    m_categoryBox->addItem(ThemeManager::icon("widgets"), tr("Basic"),
                           BasicCat);
    m_categoryBox->addItem(ThemeManager::icon("widgets"), tr("All Components"),
                           AllCat);
    for (QString cat : cats)
        m_categoryBox->addItem(cat, cat);

    int index = m_categoryBox->findData(selected);
    if (index < 0)
        index = 0;
    m_categoryBox->setCurrentIndex(index);
    m_categoryBox->blockSignals(false);
}

void ComponentSelector::clearGrid()
{
    QLayoutItem *child;
    while ((child = m_gridLayout->takeAt(0)) != 0l) {
        if (child->widget())
            delete child->widget();
        delete child;
    }
    for (int r = 0; r < m_gridLayout->rowCount(); r++)
        m_gridLayout->setRowStretch(r, 0);
    for (int c = 0; c < m_columns; c++)
        m_gridLayout->setColumnStretch(c, 1);
    m_row = 0;
    m_col = 0;
}

void ComponentSelector::addSection(const QString &title)
{
    if (m_col != 0) {
        m_row++;
        m_col = 0;
    }

    QLabel *label = new QLabel(title, m_gridHost);
    label->setObjectName("componentSection");
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    label->setContentsMargins(2, (m_row > 0) ? 8 : 0, 0, 0);
    m_gridLayout->addWidget(label, m_row, 0, 1, m_columns);
    m_row++;
}

void ComponentSelector::addCard(QTreeWidgetItem *item)
{
    ComponentCard *card =
        new ComponentCard(m_gridHost, item->data(0, NameRole).toString(),
                          item->data(0, InfoRole).toString(), item->icon(0),
                          item->data(0, TypeRole).toString(),
                          item->data(0, Qt::WhatsThisRole).toString());
    m_gridLayout->addWidget(card, m_row, m_col);
    m_col++;
    if (m_col >= m_columns) {
        m_col = 0;
        m_row++;
    }
}

void ComponentSelector::addCategoryContent(QTreeWidgetItem *cat,
                                           const QString &title, bool showTitle)
{
    if (cat->isHidden())
        return;

    QList<QTreeWidgetItem *> cards;
    QList<QTreeWidgetItem *> subCats;

    for (int i = 0; i < cat->childCount(); i++) {
        QTreeWidgetItem *child = cat->child(i);
        if (child->isHidden())
            continue;

        if (!child->data(0, TypeRole).toString().isEmpty())
            cards.append(child);
        if (child->childCount() > 0)
            subCats.append(child);
    }
    if (!cards.isEmpty()) {
        if (showTitle)
            addSection(title);
        for (QTreeWidgetItem *item : cards)
            addCard(item);
    }
    for (QTreeWidgetItem *sub : subCats)
        addCategoryContent(sub, sub->data(0, NameRole).toString(), true);
}

static void collectMatches(QTreeWidgetItem *parent, const QString &text,
                           QList<QTreeWidgetItem *> *list)
{
    for (int i = 0; i < parent->childCount(); i++) {
        QTreeWidgetItem *child = parent->child(i);
        if (child->isHidden())
            continue;

        QString type = child->data(0, TypeRole).toString();
        if (!type.isEmpty()) {
            if (child->text(0).contains(text, Qt::CaseInsensitive) ||
                type.contains(text, Qt::CaseInsensitive))
                list->append(child);
        }
        if (child->childCount() > 0)
            collectMatches(child, text, list);
    }
}

void ComponentSelector::addSearchResults(QTreeWidgetItem *parent,
                                         const QString &text,
                                         const QString &title)
{
    if (parent->isHidden())
        return;

    QList<QTreeWidgetItem *> matches;
    collectMatches(parent, text, &matches);
    if (matches.isEmpty())
        return;

    addSection(title);
    for (QTreeWidgetItem *item : matches)
        addCard(item);
}

void ComponentSelector::doRefresh()
{
    m_refreshPending = false;

    updateCategoryBox();

    m_gridHost->setUpdatesEnabled(false);
    clearGrid();

    QString search = m_searchEdit->text().trimmed();

    if (!search.isEmpty()) // Search all categories
    {
        for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
            QTreeWidgetItem *cat = m_tree->topLevelItem(i);
            addSearchResults(cat, search, cat->text(0));
        }
    } else if (m_categoryBox->currentData().toString() == BasicCat) // Most used
    {
        QList<QTreeWidgetItem *> all =
            m_tree->findItems("", Qt::MatchStartsWith | Qt::MatchRecursive);
        for (const QString &type : BasicTypes) {
            for (QTreeWidgetItem *item : all) {
                if (item->data(0, TypeRole).toString() != type)
                    continue;
                if (isEffectivelyHidden(item))
                    continue;
                addCard(item);
            }
        }
    } else if (m_categoryBox->currentData().toString() ==
               AllCat) // All categories
    {
        for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
            QTreeWidgetItem *cat = m_tree->topLevelItem(i);
            addCategoryContent(cat, cat->text(0), true);
        }
    } else // One category
    {
        QString catName = m_categoryBox->currentData().toString();
        for (int i = 0; i < m_tree->topLevelItemCount(); i++) {
            QTreeWidgetItem *cat = m_tree->topLevelItem(i);
            if (cat->text(0) == catName) {
                addCategoryContent(cat, catName, false);
                break;
            }
        }
    }
    if (m_row == 0 && m_col == 0) {
        QLabel *empty = new QLabel(tr("No components found"), m_gridHost);
        empty->setObjectName("componentEmpty");
        empty->setAlignment(Qt::AlignCenter);
        m_gridLayout->addWidget(empty, 0, 0, 1, m_columns);
        m_row = 1;
    }
    if (m_col != 0)
        m_row++;
    m_gridLayout->setRowStretch(m_row, 1);

    m_gridHost->setUpdatesEnabled(true);
}

void ComponentSelector::slotCategoryChanged(int)
{
    m_scrollArea->verticalScrollBar()->setValue(0);
    refresh();
}

void ComponentSelector::slotSearchChanged(const QString &)
{
    m_scrollArea->verticalScrollBar()->setValue(0);
    refresh();
}

void ComponentSelector::writeSettings()
{
    MainWindow::self()->settings()->setValue(
        "componentPanel/category", m_categoryBox->currentData().toString());
}

void ComponentSelector::slotContextMenu(const QPoint &point)
{
    QMenu menu;

    QAction *managePluginAction =
        menu.addAction(ThemeManager::icon("tune"), tr("Manage Components"));
    connect(managePluginAction, SIGNAL(triggered()), this,
            SLOT(slotManageComponents()));

    menu.exec(mapToGlobal(point));
}

void ComponentSelector::slotManageComponents()
{
    m_pluginsdDialog.setPluginList();
    m_pluginsdDialog.setVisible(true);
}

#include "moc_componentselector.cpp"
