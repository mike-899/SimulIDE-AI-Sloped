/***************************************************************************
 *   Minimal declarations of the ngspice shared library API              *
 *   (sharedspice.h, ngspice project, BSD-3-Clause license).              *
 *                                                                         *
 *   ngspice: Copyright (c) the ngspice team, http://ngspice.sourceforge.net
 *   Only the types used by SimulIDE are reproduced here so that SimulIDE  *
 *   builds without the ngspice headers; libngspice is loaded at runtime. *
 ***************************************************************************/

#ifndef NGSPICE_API_H
#define NGSPICE_API_H

extern "C" {

typedef bool NG_BOOL;

struct ngcomplex {
    double cx_real;
    double cx_imag;
};
typedef struct ngcomplex ngcomplex_t;

typedef struct vector_info {
    char *v_name;
    int v_type;
    short v_flags;
    double *v_realdata;
    ngcomplex_t *v_compdata;
    int v_length;
} vector_info, *pvector_info;

typedef struct vecvalues {
    char *name;
    double creal;
    double cimag;
    NG_BOOL is_scale;
    NG_BOOL is_complex;
} vecvalues, *pvecvalues;

typedef struct vecvaluesall {
    int veccount;
    int vecindex;
    pvecvalues *vecsa;
} vecvaluesall, *pvecvaluesall;

typedef struct vecinfo {
    int number;
    char *vecname;
    NG_BOOL is_real;
    void *pdvec;
    void *pdvecscale;
} vecinfo, *pvecinfo;

typedef struct vecinfoall {
    char *name;
    char *title;
    char *date;
    char *type;
    int veccount;
    pvecinfo *vecs;
} vecinfoall, *pvecinfoall;

typedef int(SendChar)(char *, int, void *);
typedef int(SendStat)(char *, int, void *);
typedef int(ControlledExit)(int, NG_BOOL, NG_BOOL, int, void *);
typedef int(SendData)(pvecvaluesall, int, int, void *);
typedef int(SendInitData)(pvecinfoall, int, void *);
typedef int(BGThreadRunning)(NG_BOOL, int, void *);

// ngSpice_Init_Sync callbacks (co-simulation)
typedef int(GetVSRCData)(double *, double, char *, int, void *);
typedef int(GetISRCData)(double *, double, char *, int, void *);
typedef int(GetSyncData)(double, double *, double, int, int, int, void *);

typedef int (*ngSpice_Init_Sync_t)(GetVSRCData *, GetISRCData *, GetSyncData *,
                                   int *, void *);
typedef int (*ngSpice_Init_t)(SendChar *, SendStat *, ControlledExit *,
                              SendData *, SendInitData *, BGThreadRunning *,
                              void *);
typedef int (*ngSpice_Circ_t)(char **);
typedef int (*ngSpice_Command_t)(char *);
typedef bool (*ngSpice_running_t)(void);
typedef pvector_info (*ngGet_Vec_Info_t)(char *);
}

#endif
