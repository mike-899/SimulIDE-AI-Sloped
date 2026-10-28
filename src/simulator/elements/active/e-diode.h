/***************************************************************************
 *   Copyright (C) 2012 by santiago González                               *
 *   santigoro@gmail.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
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

#ifndef EDIODE_H
#define EDIODE_H

#include "e-resistor.h"
#include "e-semimodel.h"

#include <QString>

// Shockley diode with series resistance and reverse breakdown, solved with
// Newton-Raphson (junction voltage limiting) inside the simulator's
// non-linear loop. See docs/ANALOG_ENGINE.md
class MAINMODULE_EXPORT eDiode : public eResistor
{
  public:
    eDiode(std::string id);
    ~eDiode();

    // Classic properties, used by the "Custom" model
    virtual double threshold() // Vf at 10 mA
    {
        return m_threshold;
    }
    virtual void setThreshold(double threshold);

    virtual double zenerV()
    {
        return m_zenerV;
    }
    virtual void setZenerV(double zenerV);

    virtual void setRes(double resist); // Series resistance
    virtual double res();

    // Model: "Custom", "Auto" (subclass default) or a preset name
    QString modelName()
    {
        return m_model;
    }
    void setModelName(const QString &model);

    const Semi::DiodeParams &params()
    {
        return m_p;
    }
    // Zener voltage of the active model (0 = not a zener)
    double effectiveZenerV();

    virtual void stamp();
    virtual void resetState();
    virtual void setVChanged();

  protected:
    virtual void updateVI();
    virtual Semi::DiodeParams autoParams();
    void refreshModel(); // Safe while running
    void loadModel();

    Semi::DiodeParams m_p;
    QString m_model;

    double m_voltPN;
    double m_threshold;
    double m_imped;
    double m_zenerV;
    double m_emission; // Emission coefficient of the custom model

    double m_vd;      // Junction voltage (last limited iterate)
    double m_vdStamp; // Junction voltage of the last stamp
    double m_vtEval;  // Terminal voltage of the last evaluation
    double m_vlinStamp, m_iStamp, m_gStamp; // Last stamp: V, I, G
    bool m_limited;                         // Last Newton step was limited
    double m_vcrit;
    double m_vcritBv;
};
#endif
