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

#ifndef DIODE_H
#define DIODE_H

#include "e-diode.h"
#include "itemlibrary.h"

class MAINMODULE_EXPORT Diode : public Component, public eDiode
{
    Q_OBJECT
    Q_PROPERTY( double Threshold  READ threshold WRITE setThreshold DESIGNABLE true USER true )
    Q_PROPERTY( double Zener_Volt READ zenerV    WRITE setZenerV    DESIGNABLE true USER true )
    Q_PROPERTY(
        double Resistance READ res WRITE setRes DESIGNABLE true USER true)
    Q_PROPERTY(
        DiodeModel Model READ model WRITE setModel DESIGNABLE true USER true)

  public:
    Diode(QObject *parent, QString type, QString id);
    ~Diode();

    static Component *construct(QObject *parent, QString type, QString id);
    static LibraryItem *libraryItem();

    // Custom uses Threshold (Vf at 10 mA), Resistance and Zener_Volt.
    // Append new presets at the end (saved as an integer).
    enum DiodeModel {
        Custom = 0,
        D1N4148,
        D1N914,
        D1N4001,
        D1N4007,
        D1N5819,
        D1N4728A,
        D1N4733A,
        D1N4742A
    };
    Q_ENUMS(DiodeModel)

    DiodeModel model();
    void setModel(DiodeModel model);

    virtual void paint(QPainter *p, const QStyleOptionGraphicsItem *option,
                       QWidget *widget);
};

#endif
