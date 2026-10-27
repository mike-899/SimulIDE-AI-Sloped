/***************************************************************************
 *   Copyright (C) 2016 by santiago González                               *
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

#ifndef EMOSFET_H
#define EMOSFET_H

#include "e-resistor.h"
#include "e-semimodel.h"

#include <QString>

// Level-1 MOSFET with series RD/RS and body diode (see Semi::mosEval),
// stamped as a 3x3 Newton-Raphson companion model.
class MAINMODULE_EXPORT eMosfet : public eResistor
{
  public:
    eMosfet(std::string id);
    ~eMosfet();

    virtual void stamp();
    virtual void resetState();
    virtual void setVChanged();

    virtual bool pChannel();
    virtual void setPchannel(bool pc);

    virtual bool depletion();
    virtual void setDepletion(bool dep);

    // Classic properties, used by the "Custom" model
    virtual double RDSon(); // At Vgs = 10 V
    virtual void setRDSon(double rdson);

    virtual double threshold();
    virtual void setThreshold(double th);

    QString modelName()
    {
        return m_model;
    }
    void setModelName(const QString &model);

    const Semi::MosParams &params()
    {
        return m_p;
    }
    double drainCurrent()
    {
        return m_drainCurr;
    }

    virtual ePin *getEpin(QString pinName);
    virtual void initEpins();

  protected:
    void refreshModel(); // Safe while running
    void loadModel();

    Semi::MosParams m_p;
    Semi::MosParams m_pRev; // RD and RS swapped (reverse mode)
    QString m_model;

    double m_threshold;
    double m_RDSon;
    double m_drainCurr;
    double m_gateV; // Gate overdrive, used to paint

    double m_vgs, m_vds;           // Last limited iterate (device frame)
    double m_vgsStamp, m_vdsStamp; // Voltages of the last stamp
    double m_idGuess;
    double m_vcritB;

    bool m_Pchannel;
    bool m_depletion;
};

#endif
