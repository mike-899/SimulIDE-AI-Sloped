/***************************************************************************
 *   Floating property inspector backend for QML                           *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#ifndef INSPECTOR_H
#define INSPECTOR_H

#include <QAbstractListModel>
#include <QMetaProperty>
#include <QPointer>
#include <QStringList>

class QTimer;
class Component;

// The editable (USER) Q_PROPERTYs of one object, in the same order as the
// classic QPropertyEditor (base classes first).
// Roles: name, label, value (raw), display (text), kind ("bool", "int",
// "double", "quantity" (value + unit in one text, eg. "2 kΩ"), "string",
// "enum", "color", "other"), options (enum keys),
// readOnly.
class PropertyListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

  public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        LabelRole,
        ValueRole,
        DisplayRole,
        KindRole,
        OptionsRole,
        ReadOnlyRole
    };

    explicit PropertyListModel(QObject *parent = 0l);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const
    {
        return m_props.size();
    }

    void setObject(QObject *obj);
    QObject *object() const
    {
        return m_object.data();
    }
    // Re-read values; only rows whose value changed are signalled, so an
    // editor being typed into is not reset.
    void refreshValues();

    Q_INVOKABLE bool setValue(int row, const QVariant &value);

  signals:
    void countChanged();

  private:
    struct Prop {
        QMetaProperty meta;
        QString label;
        QString kind;
        QStringList options;
        QVariant value;
        // "quantity": a double merged with the following "Unit" property
        QMetaProperty unitMeta;
        QString unit;
    };
    QVariant readValue(const Prop &p) const;
    QString readUnit(const Prop &p) const;

    QPointer<QObject> m_object;
    QList<Prop> m_props;
};

// "Inspector": the currently selected component (single selection) and its
// properties, for the floating Tinkercad-like inspector over the canvas.
class Inspector : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString itemId READ itemId NOTIFY changed)
    Q_PROPERTY(QString itemType READ itemType NOTIFY changed)
    Q_PROPERTY(QString help READ help NOTIFY changed)
    Q_PROPERTY(bool collapsed READ collapsed WRITE setCollapsed NOTIFY
                   collapsedChanged)
    Q_PROPERTY(QObject *model READ model CONSTANT)

  public:
    explicit Inspector(QObject *parent = 0l);

    bool active() const
    {
        return !m_comp.isNull();
    }
    QString itemId() const;
    QString itemType() const;
    QString help() const;
    bool collapsed() const
    {
        return m_collapsed;
    }
    void setCollapsed(bool c);
    QObject *model() const
    {
        return m_model;
    }

    // Keep polling the scene selection (same rule as the Widgets inspector)
    void update();

  signals:
    void changed();
    void collapsedChanged();

  private:
    PropertyListModel *m_model;
    QPointer<Component> m_comp;
    QTimer *m_timer;
    bool m_collapsed;
    int m_tick;
    QString m_lastId;
};

#endif
