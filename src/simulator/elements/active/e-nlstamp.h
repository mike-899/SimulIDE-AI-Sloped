/***************************************************************************
 *   Copyright (C) 2026 SimulIDE contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// Newton-Raphson companion stamp for an N-terminal non-linear device.
//
// The device is linearized at the terminal voltages v0:
//     I_k(v) ~ I_k(v0) + sum_j J[k][j] * (v_j - v0_j)
// where I_k is the current flowing INTO the device at terminal k.
// This adds J to the MNA matrix (rows/cols = terminal nodes) and the
// equivalent current I_k(v0) - J*v0 to the right hand side.

#ifndef ENLSTAMP_H
#define ENLSTAMP_H

#include "e-pin.h"

// Newton iterations per simulation step before a device hands the rest of
// its solve over to the next step (time continuation), and iteration after
// which the Newton steps are damped
static const int NEWTON_MAX  = 30;
static const int NEWTON_DAMP = 12;

template<int N> struct NLStamp {
    double J[N][N];
    double I[N];  // Terminal currents into the device at v0
    double v0[N]; // Linearization point

    void clear()
    {
        for (int k = 0; k < N; k++) {
            I[k] = 0;
            for (int j = 0; j < N; j++)
                J[k][j] = 0;
        }
    }
    // Linear conductance g between terminals a and b
    void addG(int a, int b, double g)
    {
        J[a][a] += g;
        J[a][b] -= g;
        J[b][b] += g;
        J[b][a] -= g;
        double i = g * (v0[a] - v0[b]);
        I[a] += i;
        I[b] -= i;
    }
    void stamp(ePin *const *pins) const
    {
        for (int k = 0; k < N; k++) {
            double ieq = I[k];
            for (int j = 0; j < N; j++) {
                ieq -= J[k][j] * v0[j];
                pins[k]->stampEntry(pins[j], J[k][j]);
            }
            pins[k]->stampCurrent(-ieq);
        }
    }
};

#endif
