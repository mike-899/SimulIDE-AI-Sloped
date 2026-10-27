/***************************************************************************
 *   Copyright (C) 2026 SimulIDE contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "e-semimodel.h"

#include <algorithm>
#include <cmath>

namespace Semi
{

static const double EXP_LIM = 80.0;

double safeExp(double x, double *dexp)
{
    if (x > EXP_LIM) {
        static const double e80 = std::exp(EXP_LIM);
        if (dexp)
            *dexp = e80;
        return e80 * (1.0 + x - EXP_LIM);
    }
    double e = std::exp(x);
    if (dexp)
        *dexp = e;
    return e;
}

double vcrit(double nvt, double is)
{
    if (is <= 0)
        return 0.6;
    return nvt * std::log(nvt / (M_SQRT2 * is));
}

double pnjlim(double vnew, double vold, double vt, double vcrit)
{
    if ((vnew > vcrit) && (std::fabs(vnew - vold) > (vt + vt))) {
        if (vold > 0) {
            double arg = 1 + (vnew - vold) / vt;
            if (arg > 0)
                vnew = vold + vt * std::log(arg);
            else
                vnew = vcrit;
        } else
            vnew = vt * std::log(vnew / vt);
    }
    return vnew;
}

double fetlim(double vnew, double vold, double vto)
{
    double vtsthi = std::fabs(2 * (vold - vto)) + 2;
    double vtstlo = vtsthi / 2 + 2;
    double vtox   = vto + 3.5;
    double delv   = vnew - vold;

    if (vold >= vto) {
        if (vold >= vtox) {
            if (delv <= 0) {
                if (vnew >= vtox) {
                    if (-delv > vtstlo)
                        vnew = vold - vtstlo;
                } else
                    vnew = std::max(vnew, vto + 2);
            } else if (delv >= vtsthi)
                vnew = vold + vtsthi;
        } else {
            if (delv <= 0)
                vnew = std::max(vnew, vto - 0.5);
            else
                vnew = std::min(vnew, vto + 4);
        }
    } else {
        if (delv <= 0) {
            if (-delv > vtsthi)
                vnew = vold - vtsthi;
        } else {
            double vtemp = vto + 0.5;
            if (vnew <= vtemp) {
                if (delv > vtstlo)
                    vnew = vold + vtstlo;
            } else
                vnew = vtemp;
        }
    }
    return vnew;
}

double limvds(double vnew, double vold)
{
    if (vold >= 3.5) {
        if (vnew > vold)
            vnew = std::min(vnew, 3 * vold + 2);
        else if (vnew < 3.5)
            vnew = std::max(vnew, 2.0);
    } else {
        if (vnew > vold)
            vnew = std::min(vnew, 4.0);
        else
            vnew = std::max(vnew, -0.5);
    }
    return vnew;
}

//------------------------------------------------------------------- Diode --

void diodeJunction(const DiodeParams &p, double vd, double &id, double &gd)
{
    double nvt = p.n * VT;
    double de;
    double e = safeExp(vd / nvt, &de);
    id       = p.is * (e - 1);
    gd       = p.is * de / nvt;

    double nbvt = p.nbv * VT;
    if ((p.bv > 0) && (vd + p.bv < 60 * nbvt)) // Reverse breakdown
    {
        double eb = safeExp(-(vd + p.bv) / nbvt, &de);
        id -= p.ibv * eb;
        gd += p.ibv * de / nbvt;
    }
    id += GMIN * vd;
    gd += GMIN;
}

double diodeSolveRs(const DiodeParams &p, double vt, double vd0)
{
    if (p.rs <= 0)
        return vt;

    // h(vd) = vd + rs*Id(vd) - vt is monotonic increasing, the root lies in
    // [min(0,vt), max(0,vt)]. h is convex for forward bias and concave in
    // breakdown, so Newton started on the right side (h >= 0 forward,
    // h <= 0 reverse) converges monotonically without overshoot.
    double lo = std::min(0.0, vt);
    double hi = std::max(0.0, vt);
    double id, gd;
    double vd = std::min(std::max(vd0, lo), hi);

    diodeJunction(p, vd, id, gd);
    double h = vd + p.rs * id - vt;
    if ((vt > 0) && (h < 0)) // Warm start on the wrong side
    {
        // Voltage where Id = vt/rs (more current is impossible): h >= 0
        double vup = p.n * VT * std::log(vt / p.rs / p.is + 1);
        vd         = std::min(vup, hi);
        diodeJunction(p, vd, id, gd);
        h = vd + p.rs * id - vt;
    } else if ((vt <= 0) && (h > 0)) {
        vd = lo;
        diodeJunction(p, vd, id, gd);
        h = vd + p.rs * id - vt;
    }
    for (int i = 0; i < 100; i++) {
        double dv = -h / (1 + p.rs * gd);
        vd += dv;
        if (vd < lo)
            vd = lo;
        else if (vd > hi)
            vd = hi;
        if (std::fabs(dv) < 1e-11)
            break;
        diodeJunction(p, vd, id, gd);
        h = vd + p.rs * id - vt;
    }
    return vd;
}

double diodeCurrent(const DiodeParams &p, double vt, double *vdGuess)
{
    double vd = diodeSolveRs(p, vt, vdGuess ? *vdGuess : 0.6);
    if (vdGuess)
        *vdGuess = vd;
    double id, gd;
    diodeJunction(p, vd, id, gd);
    return id;
}

double diodeVoltage(const DiodeParams &p, double current)
{
    double nvt = p.n * VT;
    return nvt * std::log(current / p.is + 1) + current * p.rs;
}

static QList<DiodeParams> buildDiodes()
{
    QList<DiodeParams> list;
    auto add = [&](const char *name, double is, double n, double rs, double bv,
                   double ibv, double nbv, double cjo, double m, double tt) {
        DiodeParams p;
        p.name = name;
        p.is   = is;
        p.n    = n;
        p.rs   = rs;
        p.bv   = bv;
        p.ibv  = ibv;
        p.nbv  = nbv;
        p.cjo  = cjo;
        p.m    = m;
        p.tt   = tt;
        list.append(p);
    };
    // Small signal / rectifier / Schottky (common vendor SPICE models)
    add("1N4148", 2.52e-9, 1.752, 0.568, 100, 100e-6, 1, 4e-12, 0.4, 20e-9);
    add("1N914", 2.52e-9, 1.752, 0.568, 100, 100e-6, 1, 4e-12, 0.4, 20e-9);
    add("1N4001", 7.03e-9, 1.81, 0.0342, 50, 5e-6, 1, 10e-12, 0.5, 4.2e-6);
    add("1N4007", 7.03e-9, 1.81, 0.0342, 1000, 5e-6, 1, 10e-12, 0.5, 4.2e-6);
    add("1N5819", 31.7e-6, 1.373, 0.051, 40, 1e-3, 1, 110e-12, 0.35, 0);
    // Zeners from datasheet Vz @ Izt and Zzt: NBV = 2 (knee) and the rest
    // of Zzt as series resistance, BV so that Vz is met at Izt
    auto zener = [&](const char *name, double vz, double izt, double zzt,
                     double cjo) {
        double nbv = 2;
        double rs  = std::max(zzt - nbv * VT / izt, 0.5);
        add(name, 1e-12, 1.2, rs, vz - izt * rs, izt, nbv, cjo, 0.5, 0);
    };
    zener("1N4728A", 3.3, 0.076, 10, 450e-12);
    zener("1N4733A", 5.1, 0.049, 7, 300e-12);
    zener("1N4742A", 12, 0.021, 9, 100e-12);
    return list;
}

static const QList<DiodeParams> &diodes()
{
    static const QList<DiodeParams> list = buildDiodes();
    return list;
}

const QStringList &diodePresetNames()
{
    static QStringList names;
    if (names.isEmpty())
        for (const DiodeParams &p : diodes())
            names.append(p.name);
    return names;
}

bool diodePreset(const QString &name, DiodeParams &p)
{
    for (const DiodeParams &d : diodes()) {
        if (d.name.compare(name, Qt::CaseInsensitive) == 0) {
            p = d;
            return true;
        }
    }
    return false;
}

DiodeParams ledModel(int color)
{
    // Vf at 20 mA, emission coefficient and series resistance per colour
    double vf = 2.0, n = 2.0, rs = 5.0;
    QString name;
    switch (color) {
        case yellow:
            vf = 2.10, name = "LED_YELLOW";
            break;
        case red:
            vf = 2.00, name = "LED_RED";
            break;
        case green:
            vf = 2.20, name = "LED_GREEN";
            break;
        case orange:
            vf = 2.05, name = "LED_ORANGE";
            break;
        case blue:
            vf = 3.10, n = 2.5, rs = 10, name = "LED_BLUE";
            break;
        case purple:
            vf = 3.30, n = 2.5, rs = 10, name = "LED_PURPLE";
            break;
        case white:
        default:
            vf = 3.10, n = 2.5, rs = 10, name = "LED_WHITE";
            break;
    }
    DiodeParams p;
    p.name = name;
    p.n    = n;
    p.rs   = rs;
    p.is   = 0.02 / (std::exp((vf - 0.02 * rs) / (n * VT)) - 1);
    p.bv   = 5;
    p.ibv  = 10e-6;
    p.cjo  = 30e-12;
    return p;
}

DiodeParams customDiode(double vf10mA, double rs, double zenerV, double n)
{
    DiodeParams p;
    p.name    = "Custom";
    p.n       = n;
    p.rs      = std::max(rs, 0.0);
    double vj = vf10mA - 0.01 * p.rs;
    if (vj < 0.05)
        vj = 0.05;
    p.is = 0.01 / (std::exp(vj / (n * VT)) - 1);
    if (zenerV > 0) {
        p.bv  = zenerV;
        p.ibv = 5e-3;
        p.nbv = 1.5;
    }
    return p;
}

//--------------------------------------------------------------------- BJT --

void bjtEval(const BjtParams &p, double vbe, double vbc, BjtOp &op)
{
    double nfvt = p.nf * VT;
    double nrvt = p.nr * VT;
    double de;

    double cbe = p.is * (safeExp(vbe / nfvt, &de) - 1);
    double gbe = p.is * de / nfvt;
    double cbc = p.is * (safeExp(vbc / nrvt, &de) - 1);
    double gbc = p.is * de / nrvt;

    double ile = 0, gle = 0, ilc = 0, glc = 0;
    if (p.ise > 0) {
        double nevt = p.ne * VT;
        ile         = p.ise * (safeExp(vbe / nevt, &de) - 1);
        gle         = p.ise * de / nevt;
    }
    if (p.isc > 0) {
        double ncvt = p.nc * VT;
        ilc         = p.isc * (safeExp(vbc / ncvt, &de) - 1);
        glc         = p.isc * de / ncvt;
    }
    // Base charge (Early effect and high injection)
    double invVaf = (p.vaf > 0) ? 1 / p.vaf : 0;
    double invVar = (p.var > 0) ? 1 / p.var : 0;
    double invIkf = (p.ikf > 0) ? 1 / p.ikf : 0;
    double invIkr = (p.ikr > 0) ? 1 / p.ikr : 0;

    double d     = 1 - vbc * invVaf - vbe * invVar;
    double dq1be = 0, dq1bc = 0;
    double q1;
    if (d < 1e-2) {
        q1 = 100; // Punch-through clamp
    } else {
        q1    = 1 / d;
        dq1be = q1 * q1 * invVar;
        dq1bc = q1 * q1 * invVaf;
    }
    double q2 = cbe * invIkf + cbc * invIkr;
    double qb, dqbbe, dqbbc;
    if (q2 > 0) {
        double s = std::sqrt(1 + 4 * q2);
        qb       = q1 * (1 + s) / 2;
        dqbbe    = dq1be * (1 + s) / 2 + q1 * (gbe * invIkf) / s;
        dqbbc    = dq1bc * (1 + s) / 2 + q1 * (gbc * invIkr) / s;
    } else {
        qb    = q1;
        dqbbe = dq1be;
        dqbbc = dq1bc;
    }
    double it    = (cbe - cbc) / qb;
    double ditbe = (gbe - it * dqbbe) / qb;
    double ditbc = (-gbc - it * dqbbc) / qb;

    op.ic     = it - cbc / p.br - ilc - GMIN * vbc;
    op.ib     = cbe / p.bf + ile + cbc / p.br + ilc + GMIN * (vbe + vbc);
    op.dicVbe = ditbe;
    op.dicVbc = ditbc - gbc / p.br - glc - GMIN;
    op.dibVbe = gbe / p.bf + gle + GMIN;
    op.dibVbc = gbc / p.br + glc + GMIN;
}

static QList<BjtParams> buildBjts()
{
    QList<BjtParams> list;
    BjtParams p;

    p      = BjtParams();
    p.name = "2N2222", p.is = 14.34e-15, p.bf = 255.9, p.br = 6.092;
    p.vaf = 74.03, p.ikf = 0.2847, p.ise = 14.34e-15, p.ne = 1.307;
    p.rb = 10, p.rc = 1, p.cje = 22.01e-12, p.cjc = 7.306e-12;
    p.tf = 411.1e-12, p.tr = 46.91e-9, p.complement = "2N2907";
    list.append(p);

    p      = BjtParams();
    p.name = "2N2907", p.pnp = true, p.is = 650.6e-18, p.bf = 231.7;
    p.br = 3.563, p.vaf = 115.7, p.ikf = 1.079, p.ise = 54.81e-15;
    p.ne = 1.829, p.rb = 10, p.rc = 0.715, p.cje = 19.82e-12;
    p.cjc = 14.76e-12, p.tf = 603.7e-12, p.tr = 111.3e-9;
    p.complement = "2N2222";
    list.append(p);

    p      = BjtParams();
    p.name = "2N3904", p.is = 6.734e-15, p.bf = 416.4, p.br = 0.7371;
    p.vaf = 74.03, p.ikf = 66.78e-3, p.ise = 6.734e-15, p.ne = 1.259;
    p.rb = 10, p.rc = 1, p.cje = 4.493e-12, p.cjc = 3.638e-12;
    p.tf = 301.2e-12, p.tr = 239.5e-9, p.complement = "2N3906";
    list.append(p);

    p      = BjtParams();
    p.name = "2N3906", p.pnp = true, p.is = 1.41e-15, p.bf = 180.7;
    p.br = 4.977, p.vaf = 18.7, p.ikf = 80e-3, p.rb = 10, p.rc = 2.5;
    p.cje = 8.063e-12, p.cjc = 9.728e-12, p.tf = 179.3e-12;
    p.tr = 33.42e-9, p.complement = "2N3904";
    list.append(p);

    p      = BjtParams();
    p.name = "BC547", p.is = 2.39e-14, p.nf = 1.008, p.ise = 3.545e-15;
    p.ne = 1.541, p.bf = 294.3, p.ikf = 0.1357, p.vaf = 63.2, p.nr = 1.004;
    p.isc = 6.272e-14, p.nc = 1.243, p.br = 7.946, p.ikr = 0.1144;
    p.var = 25.9, p.rb = 1, p.re = 0.4683, p.rc = 0.85, p.cje = 13.58e-12;
    p.cjc = 3.728e-12, p.tf = 439.1e-12, p.complement = "BC557";
    list.append(p);

    p      = BjtParams();
    p.name = "BC557", p.pnp = true, p.is = 3.83e-14, p.nf = 1.008;
    p.ise = 1.22e-14, p.ne = 1.528, p.bf = 344.4, p.ikf = 0.08039;
    p.vaf = 21.11, p.nr = 1.005, p.isc = 2.85e-13, p.nc = 1.28;
    p.br = 14.84, p.ikr = 0.047, p.var = 32.02, p.rb = 1, p.re = 0.6202;
    p.rc = 0.5713, p.cje = 12.3e-12, p.cjc = 10.8e-12, p.tf = 600e-12;
    p.complement = "BC547";
    list.append(p);

    // Darlingtons approximated by a single transistor: NF=2 doubles VBE,
    // BF ~ 1000 (Vce(sat) of a real darlington is not reproduced)
    p      = BjtParams();
    p.name = "TIP120", p.is = 1.75e-12, p.nf = 2, p.nr = 2, p.bf = 1000;
    p.br = 5, p.vaf = 100, p.ikf = 8, p.rc = 0.1, p.cje = 300e-12;
    p.cjc = 200e-12, p.tf = 20e-9, p.complement = "TIP125";
    list.append(p);

    p      = BjtParams();
    p.name = "TIP125", p.pnp = true, p.is = 1.75e-12, p.nf = 2, p.nr = 2;
    p.bf = 1000, p.br = 5, p.vaf = 100, p.ikf = 8, p.rc = 0.1;
    p.cje = 300e-12, p.cjc = 200e-12, p.tf = 20e-9, p.complement = "TIP120";
    list.append(p);

    return list;
}

static const QList<BjtParams> &bjts()
{
    static const QList<BjtParams> list = buildBjts();
    return list;
}

const QStringList &bjtPresetNames()
{
    static QStringList names;
    if (names.isEmpty())
        for (const BjtParams &p : bjts())
            names.append(p.name);
    return names;
}

bool bjtPreset(const QString &name, BjtParams &p)
{
    for (const BjtParams &b : bjts()) {
        if (b.name.compare(name, Qt::CaseInsensitive) == 0) {
            p = b;
            return true;
        }
    }
    return false;
}

BjtParams customBjt(double gain, double vbe10mA, bool pnp)
{
    BjtParams p;
    p.name = "Custom";
    p.pnp  = pnp;
    p.bf   = std::max(gain, 1.0);
    p.br   = 1;
    p.vaf  = 75; // Same Early voltage as the legacy model
    if (vbe10mA < 0.1)
        vbe10mA = 0.1;
    p.is = 0.01 / std::exp(vbe10mA / VT);
    return p;
}

//------------------------------------------------------------------ MOSFET --

static void mosChannel(const MosParams &p, double vgs, double vds, double &id,
                       double &gm, double &gds)
{
    // Smooth overdrive: vov = a*ln(1+exp((vgs-vto)/a)) gives an exponential
    // sub-threshold tail and continuous derivatives (helps convergence).
    const double a = 2 * VT;
    double x       = (vgs - p.vto) / a;
    double vov, dvov;
    if (x > 40) {
        vov  = vgs - p.vto;
        dvov = 1;
    } else if (x < -40) {
        vov  = a * std::exp(x);
        dvov = std::exp(x);
    } else {
        double e = std::exp(x);
        vov      = a * std::log1p(e);
        dvov     = e / (1 + e);
    }
    if (vds < 0)
        vds = 0;
    double cl = 1 + p.lambda * vds;

    if (vds < vov) // Linear region
    {
        double f = (vov - vds / 2) * vds;
        id       = p.kp * f * cl;
        gm       = p.kp * vds * cl * dvov;
        gds      = p.kp * ((vov - vds) * cl + f * p.lambda);
    } else // Saturation
    {
        double f = vov * vov / 2;
        id       = p.kp * f * cl;
        gm       = p.kp * vov * cl * dvov;
        gds      = p.kp * f * p.lambda;
    }
}

void mosEval(const MosParams &p, double vgs, double vds, double idGuess,
             MosOp &op)
{
    double id, gm, gds;
    double rsd = p.rs + p.rd;

    if (rsd <= 0) {
        mosChannel(p, vgs, vds, id, gm, gds);
        op.id  = id;
        op.gm  = gm;
        op.gds = gds + GMIN;
        return;
    }
    // Series resistances: solve F(I) = I - f(vgs-I*rs, vds-I*(rs+rd)) = 0
    mosChannel(p, vgs, vds, id, gm, gds);
    double lo = 0, hi = id;
    double i = std::min(std::max(idGuess, lo), hi);

    for (int k = 0; k < 60; k++) {
        mosChannel(p, vgs - i * p.rs, vds - i * rsd, id, gm, gds);
        double F = i - id;
        if (F > 0)
            hi = i;
        else
            lo = i;
        double dF = 1 + gm * p.rs + gds * rsd;
        double in = i - F / dF;
        if ((in <= lo) || (in >= hi))
            in = 0.5 * (lo + hi);
        if (std::fabs(in - i) < 1e-13 + 1e-10 * std::fabs(i)) {
            i = in;
            break;
        }
        i = in;
    }
    mosChannel(p, vgs - i * p.rs, vds - i * rsd, id, gm, gds);
    double D = 1 + gm * p.rs + gds * rsd;
    op.id    = i;
    op.gm    = gm / D;
    op.gds   = gds / D + GMIN;
}

static QList<MosParams> buildMos()
{
    QList<MosParams> list;
    auto add = [&](const char *name, bool pch, double vto, double kp,
                   double lambda, double rd, double rs, double isb, double cgs,
                   double cgd, const char *comp) {
        MosParams p;
        p.name       = name;
        p.pch        = pch;
        p.vto        = vto;
        p.kp         = kp;
        p.lambda     = lambda;
        p.rd         = rd;
        p.rs         = rs;
        p.isb        = isb;
        p.cgs        = cgs;
        p.cgd        = cgd;
        p.complement = comp;
        list.append(p);
    };
    // Level-1 fits: VTO ~ typical Vgs(th), KP/RD/RS fitted to Rds(on) at
    // Vgs = 10 V and 4.5 V (logic level parts), C from Ciss/Crss.
    add("2N7000", false, 2.1, 0.58, 0.02, 0.5, 0.5, 1e-14, 15e-12, 5e-12,
        "BS250");
    add("BS170", false, 2.1, 0.6, 0.02, 0.5, 0.4, 1e-14, 20e-12, 4e-12,
        "BS250");
    add("IRF520", false, 3.2, 2.0, 0.01, 0.1, 0.09, 2.5e-13, 330e-12, 37e-12,
        "IRF9540");
    add("IRF540N", false, 3.2, 20, 0.01, 0.02, 0.016, 1e-12, 1.7e-9, 250e-12,
        "IRF9540");
    add("IRLZ44N", false, 1.8, 25.6, 0.01, 0.009, 0.008, 1e-12, 1.5e-9, 200e-12,
        "IRF9540");
    add("IRF9540", true, 3.2, 2.0, 0.01, 0.07, 0.056, 1e-12, 1.3e-9, 300e-12,
        "IRF540N");
    add("BS250", true, 2.4, 0.12, 0.02, 8, 4, 1e-14, 60e-12, 10e-12, "2N7000");
    return list;
}

static const QList<MosParams> &mosfets()
{
    static const QList<MosParams> list = buildMos();
    return list;
}

const QStringList &mosPresetNames()
{
    static QStringList names;
    if (names.isEmpty())
        for (const MosParams &p : mosfets())
            names.append(p.name);
    return names;
}

bool mosPreset(const QString &name, MosParams &p)
{
    for (const MosParams &m : mosfets()) {
        if (m.name.compare(name, Qt::CaseInsensitive) == 0) {
            p = m;
            return true;
        }
    }
    return false;
}

MosParams customMos(double vth, double rdsOn, bool pch, bool depletion)
{
    MosParams p;
    p.name = "Custom";
    p.pch  = pch;
    vth    = std::fabs(vth);
    p.vto  = depletion ? -vth : vth;
    if (rdsOn < 1e-3)
        rdsOn = 1e-3;
    double vov = 10 - p.vto; // Rds(on) defined at Vgs = 10 V
    if (vov < 1)
        vov = 1;
    p.kp     = 1 / (rdsOn * vov);
    p.lambda = 0.01;
    return p;
}

//-------------------------------------------------------------- SPICE text --

static QString num(double v)
{
    return QString::number(v, 'g', 7);
}

QString spiceModel(const DiodeParams &p, const QString &name)
{
    QString s = ".model " + name + " D(IS=" + num(p.is) + " N=" + num(p.n);
    if (p.rs > 0)
        s += " RS=" + num(p.rs);
    if (p.bv > 0)
        s += " BV=" + num(p.bv) + " IBV=" + num(p.ibv) + " NBV=" + num(p.nbv);
    if (p.cjo > 0)
        s += " CJO=" + num(p.cjo) + " M=" + num(p.m);
    if (p.tt > 0)
        s += " TT=" + num(p.tt);
    return s + ")";
}

QString spiceModel(const BjtParams &p, const QString &name)
{
    QString s = ".model " + name + (p.pnp ? " PNP(" : " NPN(");
    s += "IS=" + num(p.is) + " BF=" + num(p.bf) + " BR=" + num(p.br);
    s += " NF=" + num(p.nf) + " NR=" + num(p.nr);
    if (p.vaf > 0)
        s += " VAF=" + num(p.vaf);
    if (p.var > 0)
        s += " VAR=" + num(p.var);
    if (p.ikf > 0)
        s += " IKF=" + num(p.ikf);
    if (p.ikr > 0)
        s += " IKR=" + num(p.ikr);
    if (p.ise > 0)
        s += " ISE=" + num(p.ise) + " NE=" + num(p.ne);
    if (p.isc > 0)
        s += " ISC=" + num(p.isc) + " NC=" + num(p.nc);
    if (p.rb > 0)
        s += " RB=" + num(p.rb);
    if (p.rc > 0)
        s += " RC=" + num(p.rc);
    if (p.re > 0)
        s += " RE=" + num(p.re);
    if (p.cje > 0)
        s += " CJE=" + num(p.cje);
    if (p.cjc > 0)
        s += " CJC=" + num(p.cjc);
    if (p.tf > 0)
        s += " TF=" + num(p.tf);
    if (p.tr > 0)
        s += " TR=" + num(p.tr);
    return s + ")";
}

QString spiceModel(const MosParams &p, const QString &name)
{
    // Instances are written with W=L=100u, so KP*W/L = KP and
    // CGSO = Cgs/W
    QString s = ".model " + name + (p.pch ? " PMOS(" : " NMOS(");
    s += "LEVEL=1 VTO=" + num(p.pch ? -p.vto : p.vto) + " KP=" + num(p.kp);
    s += " LAMBDA=" + num(p.lambda);
    if (p.rd > 0)
        s += " RD=" + num(p.rd);
    if (p.rs > 0)
        s += " RS=" + num(p.rs);
    s += " IS=" + num(p.isb);
    if (p.cgs > 0)
        s += " CGSO=" + num(p.cgs / 100e-6);
    if (p.cgd > 0)
        s += " CGDO=" + num(p.cgd / 100e-6);
    return s + ")";
}

} // namespace Semi
