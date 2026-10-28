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

#ifndef COMPONENTSELECTOR_H
#define COMPONENTSELECTOR_H

#include <QtWidgets>

#include "compplugindialog.h"
#include "itemlibrary.h"

// One tile in the component grid: icon on top, name below.
// Dragging it into the circuit creates the component (same mime
// format the old tree list used: text = name/id, html = type).
class MAINMODULE_EXPORT ComponentCard : public QFrame
{
    Q_OBJECT

  public:
    ComponentCard(QWidget *parent, const QString &name, const QString &info,
                  const QIcon &icon, const QString &type,
                  const QString &dragId);

    QString name() const
    {
        return m_name;
    }

  protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

  private:
    QString m_name;
    QString m_type;
    QString m_dragId;
    QPoint m_pressPos;
};

// Right-side component panel (Tinkercad style):
//   [ category combo  ][manage]
//   [ search field             ]
//   [ grid of ComponentCards   ]
//
// The category/item hierarchy is still kept in a (hidden) QTreeWidget,
// which acts as the data model: CompPluginDialog and settings code keep
// working on QTreeWidgetItems exactly as before.
class MAINMODULE_EXPORT ComponentSelector : public QWidget
{
    Q_OBJECT

  public:
    // One titled group of cards, as shown in the grid (title may be empty)
    struct CardSection {
        QString title;
        QList<QTreeWidgetItem *> items;
    };
    static const int TypeRole = Qt::UserRole;     // component type
    static const int NameRole = Qt::UserRole + 1; // display name
    static const int InfoRole = Qt::UserRole + 2; // extra description

    ComponentSelector(QWidget *parent);
    ~ComponentSelector();

    static ComponentSelector *self()
    {
        return m_pSelf;
    }

    void addLibraryItem(LibraryItem *libItem);

    void LoadCompSetAt(QDir compSetDir);
    void loadXml(const QString &setFile);

    QString getXmlFile(QString compName);

    // Access to the underlying item model (used by CompPluginDialog)
    QList<QTreeWidgetItem *> findItems(const QString &text,
                                       Qt::MatchFlags flags) const;
    QTreeWidget *model()
    {
        return m_tree;
    }

    void writeSettings();

    // ---- Data API shared by the Widgets grid and the QML panel --------
    // Category combo entries as (key, label); first two are Basic / All.
    QList<QPair<QString, QString>> categories() const;
    QString currentCategory() const;
    void setCurrentCategory(const QString &key);
    QString searchText() const;
    void setSearchText(const QString &text);
    // Sections shown for the current category / search text
    QList<CardSection> sections() const;
    QTreeWidgetItem *itemByName(const QString &name) const;

    // When false, the widget grid is not built (QML panel in use)
    void setGridEnabled(bool enabled)
    {
        m_gridEnabled = enabled;
    }

    // Drag a component into the circuit (mime text = dragId, html = type).
    // The drag pixmap is empty: over the canvas the circuit shows the real
    // component, elsewhere a small ghost with icon follows the cursor.
    static void execDrag(QObject *source, const QString &dragId,
                         const QString &type, const QPixmap &icon);

  signals:
    // Category list or visible cards changed
    void contentChanged();

  public slots:
    void refresh();
    void slotManageComponents();

  private slots:
    void slotCategoryChanged(int index);
    void slotSearchChanged(const QString &text);
    void slotContextMenu(const QPoint &);
    void doRefresh();

  private:
    static ComponentSelector *m_pSelf;

    void createWidgets();
    void addItem(LibraryItem *libItem);
    void addItem(const QString &caption, const QString &category,
                 const QString &icon, const QString &type);

    void LoadLibraryItems();

    void updateCategoryBox();
    void clearGrid();
    void addSection(const QString &title);
    void addCard(QTreeWidgetItem *item);
    void collectCategory(QTreeWidgetItem *cat, const QString &title,
                         QList<CardSection> *out) const;
    bool isEffectivelyHidden(QTreeWidgetItem *item) const;

    QStringList m_compSetUnique;
    QStringList m_categories;
    QHash<QString, QString> m_xmlFileList;

    QTreeWidget *m_tree; // Hidden data model
    QComboBox *m_categoryBox;
    QLineEdit *m_searchEdit;
    QToolButton *m_manageButton;
    QScrollArea *m_scrollArea;
    QWidget *m_gridHost;
    QGridLayout *m_gridLayout;

    int m_columns;
    int m_row;
    int m_col;
    bool m_refreshPending;
    bool m_gridEnabled;

    CompPluginDialog m_pluginsdDialog;

    ItemLibrary m_itemLibrary;
};

#endif
