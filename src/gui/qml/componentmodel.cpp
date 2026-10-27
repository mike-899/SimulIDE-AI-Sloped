/***************************************************************************
 *   Component library model for the QML component panel                  *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#include "componentmodel.h"

#include "componentselector.h"
#include "thememanager.h"

#include <QUrl>

ComponentModel::ComponentModel(ComponentSelector *selector, QObject *parent) :
    QAbstractListModel(parent), m_selector(selector)
{
    m_cardCount = 0;
    m_revision  = 0;
    connect(m_selector, &ComponentSelector::contentChanged, this,
            &ComponentModel::rebuild);
    // Card icons are painted with theme colours (chips, category icons)
    connect(ThemeManager::self(), &ThemeManager::themeChanged, this,
            &ComponentModel::rebuild);
    rebuild();
}

int ComponentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_sections.size();
}

QVariant ComponentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_sections.size())
        return QVariant();
    const Section &s = m_sections.at(index.row());
    if (role == TitleRole)
        return s.title;
    if (role == CardsRole)
        return s.cards;
    return QVariant();
}

QHash<int, QByteArray> ComponentModel::roleNames() const
{
    return { { TitleRole, "title" }, { CardsRole, "cards" } };
}

QString ComponentModel::category() const
{
    return m_selector->currentCategory();
}

int ComponentModel::categoryIndex() const
{
    QString cur = category();
    for (int i = 0; i < m_categories.size(); i++)
        if (m_categories.at(i).toMap().value("key").toString() == cur)
            return i;
    return 0;
}

void ComponentModel::setCategory(const QString &key)
{
    m_selector->setCurrentCategory(key); // refresh -> contentChanged
}

QString ComponentModel::search() const
{
    return m_selector->searchText();
}

void ComponentModel::setSearch(const QString &text)
{
    if (text == search())
        return;
    m_selector->setSearchText(text);
    emit searchChanged();
}

void ComponentModel::rebuild()
{
    // Categories
    QVariantList cats;
    for (auto &pair : m_selector->categories()) {
        QVariantMap m;
        m["key"]   = pair.first;
        m["label"] = pair.second;
        cats.append(m);
    }
    if (cats != m_categories) {
        m_categories = cats;
        emit categoriesChanged();
    }
    if (m_lastCategory != category()) {
        m_lastCategory = category();
        emit categoryChanged();
    }

    // Sections
    m_revision++;
    QString rev = QString::number(m_revision);
    beginResetModel();
    m_sections.clear();
    m_cardCount = 0;
    for (const ComponentSelector::CardSection &cs : m_selector->sections()) {
        Section s;
        s.title = cs.title;
        for (QTreeWidgetItem *item : cs.items) {
            QString name =
                item->data(0, ComponentSelector::NameRole).toString();
            QVariantMap card;
            card["name"]   = name;
            card["info"]   = item->data(0, ComponentSelector::InfoRole);
            card["type"]   = item->data(0, ComponentSelector::TypeRole);
            card["dragId"] = item->data(0, Qt::WhatsThisRole);
            card["icon"]   = "image://parts/" +
                           QString::fromLatin1(QUrl::toPercentEncoding(name)) +
                           "?r=" + rev;
            s.cards.append(card);
            m_cardCount++;
        }
        m_sections.append(s);
    }
    endResetModel();
    emit contentChanged();
}

void ComponentModel::startDrag(const QString &name)
{
    QTreeWidgetItem *item = m_selector->itemByName(name);
    if (!item)
        return;
    QString type   = item->data(0, ComponentSelector::TypeRole).toString();
    QString dragId = item->data(0, Qt::WhatsThisRole).toString();
    QPixmap icon   = item->icon(0).pixmap(QSize(48, 36));
    ComponentSelector::execDrag(this, dragId, type, icon);
}

void ComponentModel::manage()
{
    m_selector->slotManageComponents();
}

//------------------------------------------------------------------------

PartImageProvider::PartImageProvider(ComponentSelector *selector) :
    QQuickImageProvider(QQuickImageProvider::Pixmap), m_selector(selector)
{
}

QPixmap PartImageProvider::requestPixmap(const QString &id, QSize *size,
                                         const QSize &requestedSize)
{
    QString name = QUrl::fromPercentEncoding(id.section('?', 0, 0).toUtf8());
    QSize want   = requestedSize.isValid() ? requestedSize : QSize(80, 60);
    QPixmap pm;
    if (QTreeWidgetItem *item = m_selector->itemByName(name))
        pm = item->icon(0).pixmap(want);
    if (pm.isNull()) {
        pm = QPixmap(want);
        pm.fill(Qt::transparent);
    }
    if (size)
        *size = pm.size();
    return pm;
}

//------------------------------------------------------------------------

IconImageProvider::IconImageProvider() :
    QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap IconImageProvider::requestPixmap(const QString &id, QSize *size,
                                         const QSize &requestedSize)
{
    QString name = id.section('/', 0, 0);
    QString hex  = id.section('/', 1, 1).section('?', 0, 0);
    QColor color =
        hex.isEmpty() ? ThemeManager::self()->iconColor() : QColor("#" + hex);
    QSize want = requestedSize.isValid() ? requestedSize : QSize(24, 24);
    QPixmap pm = ThemeManager::icon(name, color).pixmap(want);
    pm.setDevicePixelRatio(1.0);
    if (size)
        *size = pm.size();
    return pm;
}

#include "moc_componentmodel.cpp"
