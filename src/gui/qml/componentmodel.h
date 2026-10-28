/***************************************************************************
 *   Component library model for the QML component panel                  *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#ifndef COMPONENTMODEL_H
#define COMPONENTMODEL_H

#include <QAbstractListModel>
#include <QQuickImageProvider>
#include <QVariantList>

class ComponentSelector;

// Exposed to QML as "Components". One row per section of the grid
// (category title + its cards); the data still lives in the hidden
// ComponentSelector tree, so the plugin dialog, hidden components and
// settings keep working unchanged.
//
// Roles: title (section title, may be empty), cards (list of
// { name, info, type, dragId, icon }) where icon is an
// "image://parts/<name>" url served by PartImageProvider.
class ComponentModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QVariantList categories READ categories NOTIFY categoriesChanged)
    Q_PROPERTY(
        QString category READ category WRITE setCategory NOTIFY categoryChanged)
    Q_PROPERTY(int categoryIndex READ categoryIndex NOTIFY categoryChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(int cardCount READ cardCount NOTIFY contentChanged)
    Q_PROPERTY(int revision READ revision NOTIFY contentChanged)

  public:
    enum Roles { TitleRole = Qt::UserRole + 1, CardsRole };

    explicit ComponentModel(ComponentSelector *selector, QObject *parent = 0l);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QVariantList categories() const
    {
        return m_categories;
    }
    QString category() const;
    void setCategory(const QString &key);
    int categoryIndex() const;
    QString search() const;
    void setSearch(const QString &text);
    int cardCount() const
    {
        return m_cardCount;
    }
    int revision() const
    {
        return m_revision;
    }

    // Starts a real drag (same mime data as the Widgets cards, so
    // CircuitView::dropEvent creates the component). Blocks until drop.
    Q_INVOKABLE void startDrag(const QString &name);
    Q_INVOKABLE void manage();

  signals:
    void categoriesChanged();
    void categoryChanged();
    void searchChanged();
    void contentChanged();

  private slots:
    void rebuild();

  private:
    struct Section {
        QString title;
        QVariantList cards;
    };
    ComponentSelector *m_selector;
    QList<Section> m_sections;
    QVariantList m_categories;
    QString m_lastCategory;
    int m_cardCount;
    int m_revision;
};

// image://parts/<component name>: selector card icon (PartArt::cardIcon)
class PartImageProvider : public QQuickImageProvider
{
  public:
    explicit PartImageProvider(ComponentSelector *selector);
    QPixmap requestPixmap(const QString &id, QSize *size,
                          const QSize &requestedSize) override;

  private:
    ComponentSelector *m_selector;
};

// image://icon/<material name>/<AARRGGBB or RRGGBB>: tinted Material Symbol
class IconImageProvider : public QQuickImageProvider
{
  public:
    IconImageProvider();
    QPixmap requestPixmap(const QString &id, QSize *size,
                          const QSize &requestedSize) override;
};

#endif
