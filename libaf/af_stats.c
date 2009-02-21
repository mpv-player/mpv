/*
 * Copyright (C) 2009 Nicolas George <nicolas.george@normalesup.org>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>

#include "af.h"

#define MAX_DB  80
#define MIN_VAL 1E-8

struct af_stats {
    long long n_samples;
    double tsquare;
    int max;
    long long histogram[65536];
};

static inline int logdb(double v)
{
    if (v > 1)
        return 0;
    if (v <= MIN_VAL)
        return MAX_DB - 1;
    return log(v) / -0.23025850929940456840179914546843642076;
}

static int stats_init(af_instance_t *af, struct af_stats *s, af_data_t *data)
{
    int i;

    if (!data)
        return AF_ERROR;
    *(af->data) = *data;
    af->data->format = AF_FORMAT_S16_NE;
    af->data->bps    = 2;
    s->n_samples = 0;
    s->tsquare   = 0;
    s->max       = 0;
    for (i = 0; i < 65536; i++)
        s->histogram[i] = 0;
    return af_test_output(af, data);
}

static void stats_print(struct af_stats *s)
{
    int i;
    long long sum;
    float v;
    long long h[MAX_DB];

    s->tsquare /= 32768 * 32768;
    mp_msg(MSGT_AFILTER, MSGL_INFO, "stats: n_samples: %lld\n", s->n_samples);
    if (s->n_samples == 0)
        return;
    mp_msg(MSGT_AFILTER, MSGL_INFO, "stats: mean_volume: -%d dB\n",
           logdb(s->tsquare / s->n_samples));
    mp_msg(MSGT_AFILTER, MSGL_INFO, "stats: max_volume: -%d dB\n",
           logdb(s->max / (32768.0 * 32768.0)));
    for (i = 0; i < MAX_DB; i++)
        h[i] = 0;
    for (i = 0; i < 65536; i++) {
        v = (i - 32768) / 32768.0;
        h[logdb(v * v)] += s->histogram[i];
    }
    for (i = 0; i < MAX_DB; i++)
        if (h[i] != 0)
            break;
    sum = 0;
    for (; i < MAX_DB; i++) {
        sum += h[i];
        mp_msg(MSGT_AFILTER, MSGL_INFO, "stats: histogram_%ddb: %lld\n",
               i, h[i]);
        if (sum > s->n_samples / 1000)
            break;
    }
}

static int control(struct af_instance_s *af, int cmd, void *arg)
{
    struct af_stats *s = af->setup;

    switch(cmd) {
        case AF_CONTROL_REINIT:
            return stats_init(af, s, arg);

        case AF_CONTROL_PRE_DESTROY:
            stats_print(s);
            return AF_OK;
    }
    return AF_UNKNOWN;
}

static void uninit(struct af_instance_s *af)
{
    free(af->data);
    free(af->setup);
}

static af_data_t *play(struct af_instance_s *af, af_data_t *data)
{
    struct af_stats *s = af->setup;
    int16_t *a, *aend;
    int v, v2;

    a = data->audio;
    aend = (int16_t *)((char *)data->audio + data->len);
    s->n_samples += aend - a;
    for (; a < aend; a++) {
        v = *a;
        v2 = v * v;
        s->tsquare += v2;
        s->histogram[v + 32768]++;
        if (v2 > s->max)
            s->max = v2;
    }
    return data;
}

static int af_open(af_instance_t *af)
{
    af->control = control;
    af->uninit  = uninit;
    af->play    = play;
    af->mul     = 1;
    af->data    = malloc(sizeof(af_data_t));
    af->setup   = malloc(sizeof(struct af_stats));
    if (af->data == NULL || af->setup == NULL)
        return AF_ERROR;
    return AF_OK;
}

af_info_t af_info_stats = {
    "Statistics audio filter",
    "stats",
    "Nicolas George",
    "",
    0,
    af_open
};
