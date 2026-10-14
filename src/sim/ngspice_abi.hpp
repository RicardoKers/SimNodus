// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Binary interface of the ngspice shared library (sharedspice.h), declared
// here so that SimNodus never links against ngspice at build time. Layouts
// follow the pinned ngspice-47 header (tools/backend-baseline.json) and are
// unchanged since ngspice 26; only the functions SimNodus uses are listed.
#pragma once

namespace simnodus::sim::ngspice_abi {

using NgBool = bool;

struct VectorInfo {
    char* v_name;
    int v_type;
    short v_flags;
    double* v_realdata;
    void* v_compdata;
    int v_length;
};

struct VecValues {
    char* name;
    double creal;
    double cimag;
    NgBool is_scale;
    NgBool is_complex;
};

struct VecValuesAll {
    int veccount;
    int vecindex;
    VecValues** vecsa;
};

struct VecInfoAll; // Only passed through; SimNodus does not read it.

extern "C" {
using SendChar = int(char* text, int id, void* user);
using SendStat = int(char* text, int id, void* user);
using ControlledExit = int(int status, NgBool unload, NgBool quit, int id, void* user);
using SendData = int(VecValuesAll* values, int count, int id, void* user);
using SendInitData = int(VecInfoAll* info, int id, void* user);
using BGThreadRunning = int(NgBool not_running, int id, void* user);
using GetVSRCData = int(double* value, double time, char* name, int id, void* user);
using GetISRCData = int(double* value, double time, char* name, int id, void* user);
using GetSyncData = int(double time, double* delta, double old_delta, int redo, int id, int location, void* user);

using InitFn = int(SendChar*, SendStat*, ControlledExit*, SendData*, SendInitData*, BGThreadRunning*, void*);
using InitSyncFn = int(GetVSRCData*, GetISRCData*, GetSyncData*, int*, void*);
using CommandFn = int(char*);
using GetVecInfoFn = VectorInfo*(char*);
using CircFn = int(char**);
using CurPlotFn = char*();
using AllVecsFn = char**(char*);
using RunningFn = NgBool();
}

struct Api {
    InitFn* init = nullptr;
    InitSyncFn* init_sync = nullptr;
    CommandFn* command = nullptr;
    GetVecInfoFn* get_vec_info = nullptr;
    CircFn* circ = nullptr;
    CurPlotFn* cur_plot = nullptr;
    AllVecsFn* all_vecs = nullptr;
    RunningFn* running = nullptr;
};

} // namespace simnodus::sim::ngspice_abi
