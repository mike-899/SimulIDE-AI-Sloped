/***************************************************************************
 *   Copyright (C) 2026 SimulIDE contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// Semiconductor device models used by the native analog engine:
//   - Shockley diode with series resistance and reverse breakdown
//   - Gummel-Poon "lite" BJT (transport model + Early effect + high
//     injection knee + low current leakage), no internal series resistors
//   - Level-1 (Shichman-Hodges) MOSFET with smooth sub-threshold transition,
//     series RD/RS and body diode
//
// Parameters use the standard SPICE names and units so the same presets can
// be exported to a SPICE netlist (see src/simulator/spice).

#ifndef ESEMIMODEL_H
#define ESEMIMODEL_H

#include <QString>
#include <QStringList>

namespace Semi
{

constexpr double VT   = 0.025865; // Thermal voltage kT/q at 27 C (SPICE TNOM)
constexpr double GMIN = 1e-12;    // Minimum junction conductance (SPICE gmin)

// exp() that continues linearly above x = 80 to avoid overflow
double safeExp(double x, double *dexp = nullptr);

// SPICE3 junction voltage limiting (pnjlim), FET limiting (fetlim, limvds)
double pnjlim(double vnew, double vold, double vt, double vcrit);
double fetlim(double vnew, double vold, double vto);
double limvds(double vnew, double vold);
double vcrit(double nvt, double is);

//------------------------------------------------------------------- Diode --
struct DiodeParams {
    QString name;
    double is  = 1e-14; // Saturation current        [A]
    double n   = 1.0;   // Emission coefficient
    double rs  = 0.0;   // Series resistance         [Ohm]
    double bv  = 0.0;   // Reverse breakdown voltage [V] (0 = none)
    double ibv = 1e-3;  // Current at breakdown      [A]
    double nbv = 1.0;   // Breakdown emission coefficient
    double cjo = 0.0;   // Zero bias junction capacitance [F] (SPICE only)
    double m   = 0.5;   // Grading coefficient (SPICE only)
    double tt  = 0.0;   // Transit time [s] (SPICE only)
};

// Junction current and conductance at internal junction voltage vd
void diodeJunction(const DiodeParams &p, double vd, double &id, double &gd);
// Solve vd + rs*Id(vd) = vt (series resistance), warm started at vd0
double diodeSolveRs(const DiodeParams &p, double vt, double vd0);
// Terminal current for a terminal voltage vt (series resistance included)
double diodeCurrent(const DiodeParams &p, double vt, double *vdGuess = nullptr);
// Terminal voltage at a given forward current (inverse model)
double diodeVoltage(const DiodeParams &p, double current);

const QStringList &diodePresetNames(); // Diodes, rectifiers, zeners
bool diodePreset(const QString &name, DiodeParams &p);

// LED colours, same order as LedBase::LedColor
enum LedColor { yellow = 0, red, green, blue, orange, purple, white };
DiodeParams ledModel(int color);

// Custom diode from the legacy properties: Vf at 10 mA, series R, zener V
DiodeParams customDiode(double vf10mA, double rs, double zenerV, double n);

//--------------------------------------------------------------------- BJT --
struct BjtParams {
    QString name;
    bool pnp   = false;
    double is  = 1e-14;
    double bf  = 100;
    double br  = 1;
    double nf  = 1;
    double nr  = 1;
    double vaf = 0; // 0 = infinite
    double var = 0;
    double ikf = 0; // 0 = infinite
    double ikr = 0;
    double ise = 0;
    double ne  = 1.5;
    double isc = 0;
    double nc  = 2;
    double rb  = 0;     // SPICE only
    double rc  = 0;     // SPICE only
    double re  = 0;     // SPICE only
    double cje = 0;     // SPICE only
    double cjc = 0;     // SPICE only
    double tf  = 0;     // SPICE only
    double tr  = 0;     // SPICE only
    QString complement; // Preset of opposite polarity
};

struct BjtOp {
    double ic, ib;         // Terminal currents (NPN frame)
    double dicVbe, dicVbc; // Partial derivatives
    double dibVbe, dibVbc;
};
void bjtEval(const BjtParams &p, double vbe, double vbc, BjtOp &op);

const QStringList &bjtPresetNames();
bool bjtPreset(const QString &name, BjtParams &p);
BjtParams customBjt(double gain, double vbe10mA, bool pnp);

//------------------------------------------------------------------ MOSFET --
struct MosParams {
    QString name;
    bool pch      = false;
    double vto    = 2.0;   // Threshold in device frame (> 0 enhancement)
    double kp     = 0.1;   // Transconductance KP*W/L [A/V^2]
    double lambda = 0.01;  // Channel length modulation [1/V]
    double rd     = 0;     // Drain resistance [Ohm]
    double rs     = 0;     // Source resistance [Ohm]
    double isb    = 1e-14; // Body (bulk-drain) diode saturation current
    double cgs    = 0;     // Gate-source capacitance [F] (SPICE only)
    double cgd    = 0;     // Gate-drain capacitance  [F] (SPICE only)
    QString complement;
};

struct MosOp {
    double id;      // Drain current (device frame, D->S), series R included
    double gm, gds; // dId/dVgs, dId/dVds at the terminals
};
// vgs, vds in device frame with vds >= 0; idGuess warm start
void mosEval(const MosParams &p, double vgs, double vds, double idGuess,
             MosOp &op);

const QStringList &mosPresetNames();
bool mosPreset(const QString &name, MosParams &p);
MosParams customMos(double vth, double rdsOn, bool pch, bool depletion);

//-------------------------------------------------------------- SPICE text --
QString spiceModel(const DiodeParams &p, const QString &name);
QString spiceModel(const BjtParams &p, const QString &name);
QString spiceModel(const MosParams &p, const QString &name);

} // namespace Semi

#endif
