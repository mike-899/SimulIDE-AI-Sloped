/***************************************************************************
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

#include "netlabel.h"

#include "circuit.h"
#include "connector.h"
#include "e-node.h"
#include "itemlibrary.h"
#include "simulator.h"
#include "thememanager.h"

QList<NetLabel *> NetLabel::s_labels;
bool NetLabel::s_dirty       = false;
bool NetLabel::s_noPropagate = false;

static const double kOpenImp = 1e14;  // Not ground: effectively open
static const double kGndImp  = 1e-14; // Same as Ground component

static QFont labelFont()
{
    QFont font("Helvetica [Cronyx]", 7);
    font.setPixelSize(8);
    font.setBold(true);
    return font;
}

Component *NetLabel::construct(QObject *parent, QString type, QString id)
{
    return new NetLabel(parent, type, id);
}

LibraryItem *NetLabel::libraryItem()
{
    return new LibraryItem(tr("Net Label"), tr("Connectors"), "netlabel.png",
                           "NetLabel", NetLabel::construct);
}

LibraryItem *NetLabel::powerLibraryItem()
{
    return new LibraryItem(tr("VCC Label"), tr("Connectors"), "powerlabel.png",
                           "PowerLabel", NetLabel::construct);
}

LibraryItem *NetLabel::groundLibraryItem()
{
    return new LibraryItem(tr("GND Label"), tr("Connectors"), "ground.png",
                           "GroundLabel", NetLabel::construct);
}

NetLabel::NetLabel(QObject *parent, QString type, QString id) :
    Component(parent, type, id)
{
    if (type == "PowerLabel")
        m_kind = 1;
    else if (type == "GroundLabel")
        m_kind = 2;
    else
        m_kind = 0;

    QString pinId = id + "-pin";
    if (m_kind == 0)
        m_netPin = new Pin(180, QPoint(-8, 0), pinId, 0, this); // Left
    else if (m_kind == 1)
        m_netPin = new Pin(270, QPoint(0, 8), pinId, 0, this); // Bottom
    else
        m_netPin = new Pin(90, QPoint(0, -8), pinId, 0, this); // Top

    m_pin.resize(1);
    m_pin[0] = m_netPin;

    // Source used only when the label means ground (GND/VSS/0)
    m_out = new eSource((id + "-eSource").toStdString(), m_netPin);
    m_out->setVoltHigh(0);
    m_out->setOut(false);
    m_out->setImp(kOpenImp);

    m_textW = 0;
    setLabelPos(-16, -24, 0);
    setShowId(false);

    s_labels.append(this);
    s_dirty   = true;
    m_netName = (m_kind == 1) ? "VCC" : ((m_kind == 2) ? "GND" : "NET1");
    m_out->setImp(isGround() ? kGndImp : kOpenImp);
    updateArea();
}

NetLabel::~NetLabel()
{
    s_labels.removeAll(this);
    s_dirty = true;
    delete m_out;
}

void NetLabel::remove()
{
    s_labels.removeAll(this);
    s_dirty = true;
    Component::remove();
}

bool NetLabel::isGround() const
{
    QString n = m_netName.trimmed().toUpper();
    return (n == "GND") || (n == "VSS") || (n == "0");
}

void NetLabel::setNetName(QString name)
{
    name = name.trimmed();
    if (name.isEmpty())
        name = "NET1";
    if (name == m_netName)
        return;

    m_netName = name;
    m_out->setImp(isGround() ? kGndImp : kOpenImp);
    updateArea();
    s_dirty = true;

    // Re-merge nets now if the simulation is running
    if (Simulator::self()->isRunning()) {
        Simulator::self()->pauseSim();
        Simulator::self()->runContinuous();
    }
}

void NetLabel::updateArea()
{
    QFontMetrics fm(labelFont());
    m_textW = fm.horizontalAdvance(m_netName);

    prepareGeometryChange();
    if (m_kind == 0)
        m_area = QRectF(-1, -7, m_textW + 12, 14);
    else if (m_kind == 1)
        m_area = QRectF(-m_textW / 2 - 4, -12, m_textW + 8, 14);
    else
        m_area =
            QRectF(-qMax(8, m_textW / 2 + 2), -1, qMax(16, m_textW + 4), 20);
    update();
}

void NetLabel::registerPins(eNode *enode)
{
    if (s_noPropagate)
        return;

    for (NetLabel *label : s_labels) {
        if (label == this || label->m_netName != m_netName)
            continue;
        Pin *pin = label->m_netPin;
        if (!pin->connector())
            continue; // Unwired label: nothing to join
        if (pin->getEnode() == enode)
            continue; // Already on this net
        pin->registerPinsW(enode);
    }
}

void NetLabel::updateNets(bool force)
{
    if (!force && !s_dirty)
        return;
    s_dirty = false;
    if (s_labels.isEmpty())
        return;

    // 1. Give every wired label's local net its own fresh eNode, so stale
    //    merges (renamed / deleted labels) are split again.
    s_noPropagate = true;
    for (NetLabel *label : s_labels) {
        Pin *pin = label->m_netPin;
        if (!pin->connector())
            continue;
        eNode *enode = new eNode("netlabel-" + label->itemID());
        pin->registerPinsW(enode);
    }
    s_noPropagate = false;

    // 2. Join all labels sharing a name into one eNode.
    QMap<QString, QList<NetLabel *>> groups;
    for (NetLabel *label : s_labels)
        if (label->m_netPin->connector())
            groups[label->m_netName].append(label);

    for (auto it = groups.begin(); it != groups.end(); ++it) {
        QList<NetLabel *> &group = it.value();
        if (group.size() < 2)
            continue;
        eNode *enode = new eNode("netlabel-" + it.key());
        for (NetLabel *label : group)
            if (label->m_netPin->getEnode() != enode)
                label->m_netPin->registerPinsW(enode);
    }
}

void NetLabel::paint(QPainter *p, const QStyleOptionGraphicsItem *option,
                     QWidget *widget)
{
    Component::paint(p, option, widget);

    QPen pen         = p->pen();
    bool dark        = ThemeManager::self()->isDark();
    QColor textColor = dark ? QColor(20, 20, 24) : QColor(Qt::black);
    QColor lineColor = dark ? QColor(225, 225, 230) : QColor(Qt::black);
    QColor powerCol  = dark ? QColor(255, 130, 120) : QColor(170, 30, 30);
    if (m_kind != 0) {
        pen.setColor(lineColor);
        p->setPen(pen);
        textColor = lineColor;
    }
    if (!isSelected()) // Tag fill: readable in both themes
        p->setBrush(dark ? QColor(235, 215, 120) : QColor(255, 240, 170));
    p->setRenderHint(QPainter::Antialiasing);

    p->setFont(labelFont());
    int rot = int(rotation()) % 360;
    if (rot < 0)
        rot += 360;

    if (m_kind == 0) // Tag / flag shape pointing to the pin
    {
        QPointF pts[5] = { QPointF(0, 0), QPointF(6, -6),
                           QPointF(m_textW + 10, -6), QPointF(m_textW + 10, 6),
                           QPointF(6, 6) };
        p->drawPolygon(pts, 5);
        p->setPen(textColor);
        QRectF textRect(6, -6, m_textW + 3, 12);
        if (rot == 180) // Keep text readable
        {
            p->save();
            p->translate(textRect.center());
            p->rotate(180);
            p->drawText(QRectF(-textRect.width() / 2, -6, textRect.width(), 12),
                        Qt::AlignCenter, m_netName);
            p->restore();
        } else
            p->drawText(textRect, Qt::AlignCenter, m_netName);
    } else if (m_kind == 1) // Power bar, name above
    {
        pen.setWidthF(2.5);
        p->setPen(pen);
        p->drawLine(-7, 0, 7, 0);
        p->setPen(powerCol);
        QRectF textRect(-m_textW / 2 - 4, -12, m_textW + 8, 11);
        if (rot == 180) {
            p->save();
            p->translate(textRect.center());
            p->rotate(180);
            p->drawText(
                QRectF(-textRect.width() / 2, -5.5, textRect.width(), 11),
                Qt::AlignCenter, m_netName);
            p->restore();
        } else
            p->drawText(textRect, Qt::AlignCenter, m_netName);
    } else // Ground symbol, name below
    {
        pen.setWidthF(2.5);
        p->setPen(pen);
        p->drawLine(-7, 0, 7, 0);
        p->drawLine(-4, 3, 4, 3);
        p->drawLine(-1, 6, 1, 6);
        if (m_netName != "GND") {
            p->setPen(textColor);
            QRectF textRect(-qMax(8, m_textW / 2 + 2), 8, qMax(16, m_textW + 4),
                            11);
            p->drawText(textRect, Qt::AlignCenter, m_netName);
        }
    }
}

#include "moc_netlabel.cpp"
