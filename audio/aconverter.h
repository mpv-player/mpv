#pragma once

#include <stdbool.h>

#include "chmap.h"

struct mp_aconverter;
struct mp_aframe;
struct mpv_global;
struct mp_log;

struct mp_resample_opts {
    int filter_size;
    int phase_shift;
    int linear;
    double cutoff;
    int normalize;
    int allow_passthrough;
    char **avopts;
};

#define MP_RESAMPLE_OPTS_DEF {  \
    .filter_size = 16,          \
    .cutoff      = 0.0,         \
    .phase_shift = 10,          \
    .normalize   = -1,          \
    }

struct mp_aconverter *mp_aconverter_create(struct mpv_global *global,
                                           struct mp_log *log,
                                           const struct mp_resample_opts *opts);
bool mp_aconverter_reconfig(struct mp_aconverter *p,
                    int in_rate, int in_format, struct mp_chmap in_channels,
                    int out_rate, int out_format, struct mp_chmap out_channels);
void mp_aconverter_flush(struct mp_aconverter *p);
void mp_aconverter_set_speed(struct mp_aconverter *p, double speed);
bool mp_aconverter_write_input(struct mp_aconverter *p, struct mp_aframe *in);
struct mp_aframe *mp_aconverter_read_output(struct mp_aconverter *p, bool *eof);
double mp_aconverter_get_latency(struct mp_aconverter *p);
