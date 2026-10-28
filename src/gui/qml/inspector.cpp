/***************************************************************************
 *   Floating property inspector backend for QML                           *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#include "inspector.h"

#include "QPropertyEditorWidget.h"
#include "circuit.h"
#include "component.h"
#include "propertieswidget.h"

#include <QApplication>
#include <QColor>
#include <QMetaEnum>
#include <QRegularExpression>
#include <QTimer>

PropertyListModel::PropertyListModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

int PropertyListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_props.size();
}

QHash<int, QByteArray> PropertyListModel::roleNames() const
{
    return { { NameRole, "name" },        { LabelRole, "label" },
             { ValueRole, "value" },      { DisplayRole, "display" },
             { KindRole, "kind" },        { OptionsRole, "options" },
             { ReadOnlyRole, "readOnly" } };
}

QVariant PropertyListModel::readValue(const Prop &p) const
{
    if (!m_object)
        return QVariant();
    return p.meta.read(m_object.data());
}

QString PropertyListModel::readUnit(const Prop &p) const
{
    if (!m_object || !p.unitMeta.isValid())
        return QString();
    return p.unitMeta.read(m_object.data()).toString();
}

static const QString s_prefixes = "TGMkmunp";

// Unit property text (" kΩ") -> prefix char (or ' ') and base unit ("Ω")
static void splitUnit(const QString &unit, QChar &prefix, QString &base)
{
    QString u = unit.trimmed();
    prefix    = ' ';
    base      = u;
    if (u.size() > 1 && s_prefixes.contains(u.at(0))) {
        prefix = u.at(0);
        base   = u.mid(1);
    }
}

static QString quantityText(const QVariant &v, const QString &unit)
{
    return (QString::number(v.toDouble(), 'g', 8) + " " + unit.trimmed())
        .trimmed();
}

static QString displayText(const QMetaProperty &meta, const QString &kind,
                           const QVariant &v, const QString &unit = QString())
{
    if (kind == "quantity")
        return quantityText(v, unit);
    if (kind == "enum")
        return QString(meta.enumerator().valueToKey(v.toInt()));
    if (kind == "double")
        return QString::number(v.toDouble(), 'g', 8);
    if (kind == "color")
        return v.value<QColor>().name();
    if (kind == "bool")
        return v.toBool() ? "true" : "false";
    return v.toString();
}

QVariant PropertyListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_props.size())
        return QVariant();
    const Prop &p = m_props.at(index.row());
    switch (role) {
        case NameRole:
            return QString(p.meta.name());
        case LabelRole:
            return p.label;
        case ValueRole:
            if (p.kind == "enum")
                return displayText(p.meta, p.kind, p.value);
            if (p.kind == "color")
                return p.value.value<QColor>();
            return p.value;
        case DisplayRole:
            return displayText(p.meta, p.kind, p.value, p.unit);
        case KindRole:
            return p.kind;
        case OptionsRole:
            return p.options;
        case ReadOnlyRole:
            return !p.meta.isWritable();
    }
    return QVariant();
}

void PropertyListModel::setObject(QObject *obj)
{
    beginResetModel();
    m_props.clear();
    m_object = obj;
    if (obj) {
        // Same selection and order as QPropertyModel::addItem
        struct Pair {
            QMetaProperty prop;
            const QMetaObject *owner;
        };
        QList<Pair> props;
        QList<const QMetaObject *> classes;
        const QMetaObject *mo = obj->metaObject();
        do {
            for (int i = 0; i < mo->propertyCount(); ++i) {
                QMetaProperty prop = mo->property(i);
                if (!prop.isUser() || QString(prop.name()) == "plainText" ||
                    QString(prop.name()) == "itemtype")
                    continue;
                int found = -1;
                for (int j = 0; j < props.size(); ++j)
                    if (QString(props[j].prop.name()) == prop.name())
                        found = j;
                if (found >= 0)
                    props[found] = { prop, mo };
                else
                    props.append({ prop, mo });
            }
            classes.prepend(mo);
        } while ((mo = mo->superClass()) != 0l);

        for (const QMetaObject *cls : classes)
            for (const Pair &pair : props) {
                if (pair.owner != cls)
                    continue;
                Prop p;
                p.meta       = pair.prop;
                QString name = QString(pair.prop.name()).replace('_', ' ');
                p.label =
                    QApplication::translate("App::Property", name.toLatin1());
                int type = pair.prop.metaType().id();
                if (pair.prop.isEnumType()) {
                    p.kind          = "enum";
                    QMetaEnum qenum = pair.prop.enumerator();
                    for (int k = 0; k < qenum.keyCount(); ++k)
                        p.options << qenum.key(k);
                } else if (type == QMetaType::Bool)
                    p.kind = "bool";
                else if (type == QMetaType::Int || type == QMetaType::UInt ||
                         type == QMetaType::LongLong)
                    p.kind = "int";
                else if (type == QMetaType::Double || type == QMetaType::Float)
                    p.kind = "double";
                else if (type == QMetaType::QString)
                    p.kind = "string";
                else if (type == QMetaType::QColor)
                    p.kind = "color";
                else
                    p.kind = "other";
                p.value = readValue(p);
                if (QString(pair.prop.name()) == "Unit" &&
                    pair.prop.metaType().id() == QMetaType::QString &&
                    !m_props.isEmpty() && m_props.last().kind == "double" &&
                    !m_props.last().unitMeta.isValid()) {
                    // value + unit in a single editor
                    Prop &q    = m_props.last();
                    q.kind     = "quantity";
                    q.unitMeta = pair.prop;
                    q.unit     = p.value.toString();
                    continue;
                }
                m_props.append(p);
            }
    }
    endResetModel();
    emit countChanged();
}

void PropertyListModel::refreshValues()
{
    if (!m_object) {
        if (!m_props.isEmpty())
            setObject(0l);
        return;
    }
    for (int i = 0; i < m_props.size(); ++i) {
        QVariant v   = readValue(m_props[i]);
        QString unit = readUnit(m_props[i]);
        if (v != m_props[i].value || unit != m_props[i].unit) {
            m_props[i].value = v;
            m_props[i].unit  = unit;
            QModelIndex idx  = index(i);
            emit dataChanged(idx, idx);
        }
    }
}

bool PropertyListModel::setValue(int row, const QVariant &value)
{
    if (!m_object || row < 0 || row >= m_props.size())
        return false;
    Prop &p = m_props[row];
    if (!p.meta.isWritable())
        return false;

    QVariant v = value;
    if (p.kind == "quantity") {
        // "2 kΩ", "4.7k", "10 m", "100": number, then optional prefix/unit
        static const QRegularExpression re(
            "^\\s*([-+]?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][-+]?\\d+)?)\\s*(.*)$");
        QRegularExpressionMatch m = re.match(value.toString());
        if (!m.hasMatch())
            return false;
        double d  = m.captured(1).toDouble();
        QString t = m.captured(2);
        t.remove(' ');
        t.replace(QChar(0x00B5), 'u').replace(QChar(0x03BC), 'u');
        if (t.startsWith('K'))
            t[0] = 'k';
        if (!t.isEmpty()) {
            QChar curPrefix;
            QString base;
            splitUnit(p.unit, curPrefix, base);
            QChar prefix = ' ';
            if (t.size() == 1 && s_prefixes.contains(t.at(0)) && t != base)
                prefix = t.at(0); // only a prefix: keep the base unit
            else if (t.size() > 1 && s_prefixes.contains(t.at(0)) &&
                     t.mid(1) == base)
                prefix = t.at(0);
            else if (t != base)
                return false; // unknown unit text
            m_object->setProperty(p.unitMeta.name(), QString(prefix) + base);
        }
        bool ok = m_object->setProperty(p.meta.name(), d);
        refreshValues();
        return ok;
    }
    if (p.kind == "double") {
        bool ok;
        double d = value.toString().trimmed().toDouble(&ok);
        if (!ok)
            return false;
        v = d;
    } else if (p.kind == "int") {
        bool ok;
        int n = int(value.toString().trimmed().toDouble(&ok));
        if (!ok)
            return false;
        v = n;
    } else if (p.kind == "color")
        v = QColor(value.toString());
    else if (p.kind == "enum")
        v = value.toString(); // key name, QMetaProperty converts it
    else if (p.kind == "bool")
        v = value.toBool();

    bool ok = m_object->setProperty(p.meta.name(), v);
    refreshValues(); // A property can change others (eg. pin count)
    return ok;
}

//------------------------------------------------------------------------
// Inspector

Inspector::Inspector(QObject *parent) : QObject(parent)
{
    m_model     = new PropertyListModel(this);
    m_collapsed = false;
    m_active    = false;
    m_tick      = 0;
    m_timer     = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &Inspector::update);
    m_timer->start(150);
}

QString Inspector::itemId() const
{
    return m_comp ? m_comp->itemID() : QString();
}

QString Inspector::itemType() const
{
    return m_comp ? m_comp->itemType() : QString();
}

QString Inspector::help() const
{
    if (!m_comp || !m_comp->helpText())
        return QString();
    return *m_comp->helpText();
}

void Inspector::setCollapsed(bool c)
{
    if (c == m_collapsed)
        return;
    m_collapsed = c;
    emit collapsedChanged();
}

void Inspector::update()
{
    if (m_active && m_comp.isNull()) { // selected component was deleted
        m_active = false;
        m_model->setObject(0l);
        m_lastId.clear();
        emit changed();
    }
    Circuit *circ  = Circuit::self();
    Component *sel = 0l;
    if (circ) {
        int count = 0;
        for (QGraphicsItem *item : circ->selectedItems()) {
            Component *comp = dynamic_cast<Component *>(item);
            if (!comp)
                continue;
            sel = comp;
            if (++count > 1)
                break;
        }
        if (count != 1)
            sel = 0l;
    }
    if (sel != m_comp.data()) {
        m_comp   = sel;
        m_active = sel != 0l;
        m_model->setObject(sel);
        if (sel) {
            // Keep the classic (hidden) editor in sync: other code talks to it
            QPropertyEditorWidget *editor = QPropertyEditorWidget::self();
            if (editor && editor->object() != sel)
                editor->setObject(sel);
            if (PropertiesWidget::self())
                PropertiesWidget::self()->setHelpText(sel->helpText());
        }
        m_lastId = itemId();
        emit changed();
        return;
    }
    if (sel && ++m_tick % 3 == 0) {
        m_model->refreshValues();
        if (itemId() != m_lastId) {
            m_lastId = itemId();
            emit changed();
        }
    }
}

#include "moc_inspector.cpp"
