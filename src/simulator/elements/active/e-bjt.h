/***************************************************************************
 *   Copyright (C) 2018 by santiago González                               *
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

#ifndef EBJT_H
#define EBJT_H

#include "e-resistor.h"
#include "e-semimodel.h"

#include <QString>

// Gummel-Poon "lite" bipolar transistor (see Semi::bjtEval) stamped as a
// full 3x3 Newton-Raphson companion model (gm, gpi, gmu, go).
class MAINMODULE_EXPORT eBJT : public eResistor
{
  public:
    eBJT(std::string id);
    virtual ~eBJT();

    virtual void resetState();
    virtual void stamp();

    virtual void setVChanged();

    // Classic properties, used by the "Custom" model
    virtual double gain()
    {
        return m_gain;
    }
    virtual void setGain(double gain);

    virtual double BEthr(); // Vbe at Ic = 10 mA
    virtual void setBEthr(double thr);

    virtual double pnp()
    {
        return m_PNP;
    }
    virtual void setPnp(double pnp);

    // Kept for compatibility: the B-C junction is always modelled now
    virtual bool BCd()
    {
        return m_BCdiodeOn;
    }
    virtual void setBCd(bool bcd)
    {
        m_BCdiodeOn = bcd;
    }

    QString modelName()
    {
        return m_model;
    }
    void setModelName(const QString &model);

    const Semi::BjtParams &params()
    {
        return m_p;
    }

    virtual ePin *getEpin(QString pinName);
    virtual void initEpins();

  protected:
    void refreshModel(); // Safe while running
    void loadModel();

    Semi::BjtParams m_p;
    QString m_model;

    double m_baseCurr;
    double m_collCurr;
    double m_BEthr;
    double m_gain;

    double m_vbe, m_vbc;           // Last limited iterate (NPN frame)
    double m_vbeStamp, m_vbcStamp; // Junction voltages of the last stamp
    double m_vcritBE, m_vcritBC;
    Semi::BjtOp m_opS; // Operating point of the last stamp

    bool m_PNP;
    bool m_BCdiodeOn;
};

#endif
