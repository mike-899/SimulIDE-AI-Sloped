/***************************************************************************
 *   Copyright (C) 2026 SimulIDE contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// Exports the current circuit (simulator element list) as a SPICE netlist
// for ngspice. Element level export: every eElement is translated, so
// internal elements of components (potentiometer halves, switch contacts,
// meter outputs...) are covered automatically.

#ifndef SPICENETLIST_H
#define SPICENETLIST_H

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class eNode;
class ePin;
class McuComponentPin;

class MAINMODULE_EXPORT SpiceNetlist
{
  public:
    struct Ammeter {
        eNode *lNode;
        eNode *rNode;
        QString vector; // Branch current vector name (lower case)
        double res;     // Resistance of the native ammeter element
    };

    // Build from the simulator elements. Returns false (and the reasons in
    // *error) when the circuit contains parts that cannot run in SPICE.
    // withMcu: export microcontroller pins as EXTERNAL driven sources for
    // the lockstep co-simulation (SpiceCosim); otherwise MCUs are rejected.
    bool build(QString *error, bool withMcu = false);

    // Full netlist text: tran analysis tstop with max step, optional
    // initial node voltages (continuation of a previous run). saveOnly:
    // store only node voltages and ammeter currents (less memory).
    QStringList lines(double tstop, double maxStep,
                      const QHash<eNode *, double> *ic = nullptr,
                      bool saveOnly                    = false) const;

    // MCU pins in co-simulation slot order: slot k is driven by the
    // EXTERNAL sources "vmv<k>" (Thevenin voltage) and "vmg<k>" (output
    // conductance) through the B source "bmp<k>".
    const QList<McuComponentPin *> &mcuPins() const
    {
        return m_mcuPins;
    }

    const QHash<QString, eNode *> &vectorNodes() const
    {
        return m_vecNodes;
    }
    const QList<Ammeter> &ammeters() const
    {
        return m_ammeters;
    }
    QList<eNode *> nodes() const
    {
        return m_nodeNames.keys();
    }
    QString nodeName(eNode *node) const
    {
        return m_nodeNames.value(node);
    }

  private:
    QString node(ePin *pin);
    QString model(const QString &text, const QString &prefix);

    QHash<eNode *, QString> m_nodeNames;
    QHash<QString, eNode *> m_vecNodes;
    QHash<QString, QString> m_modelNames; // Model text -> name
    QList<Ammeter> m_ammeters;
    QList<McuComponentPin *> m_mcuPins;
    QStringList m_elements;
    QStringList m_models;
    int m_nc = 0;
};

#endif
