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

#ifndef NETLABEL_H
#define NETLABEL_H

#include "component.h"
#include "e-source.h"
#include "pin.h"

class LibraryItem;

// Wireless connector ("global label"): every NetLabel with the same Name is
// electrically the same net. Three flavours share this class:
//   NetLabel    - tag shape, pin on the left
//   PowerLabel  - power bar, pin below (default name VCC)
//   GroundLabel - ground symbol, pin above (default name GND)
// Labels named GND / VSS / 0 also tie their net to 0 V (like Ground).
class MAINMODULE_EXPORT NetLabel : public Component
{
    Q_OBJECT
    Q_PROPERTY(
        QString Name READ netName WRITE setNetName DESIGNABLE true USER true)

  public:
    NetLabel(QObject *parent, QString type, QString id);
    ~NetLabel();

    static Component *construct(QObject *parent, QString type, QString id);
    static LibraryItem *libraryItem();
    static LibraryItem *powerLibraryItem();
    static LibraryItem *groundLibraryItem();

    QString netName() const
    {
        return m_netName;
    }
    void setNetName(QString name);

    bool isGround() const;

    Pin *pin()
    {
        return m_netPin;
    }

    // Called by Pin::registerPins when an eNode reaches this label's pin:
    // propagate it to every other label with the same name.
    void registerPins(eNode *enode);

    // Re-derive label nets (split stale merges, join same-name labels).
    // Called by the simulator before (re)starting; does nothing unless
    // labels changed or force is set.
    static void updateNets(bool force);

    virtual void paint(QPainter *p, const QStyleOptionGraphicsItem *option,
                       QWidget *widget);

  public slots:
    virtual void remove();

  private:
    void updateArea();

    static QList<NetLabel *> s_labels;
    static bool s_dirty;
    static bool s_noPropagate;

    QString m_netName;
    int m_kind; // 0 net, 1 power, 2 ground
    int m_textW;

    Pin *m_netPin;
    eSource *m_out;
};

#endif
