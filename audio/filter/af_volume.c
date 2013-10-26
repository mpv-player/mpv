/*
 * Copyright (C)2002 Anders Johansson ajh@atri.curtin.edu.au
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
#include <string.h>

#include <inttypes.h>
#include <math.h>
#include <limits.h>

#include "af.h"

// Data for specific instances of this filter
typedef struct af_volume_s
{
    float level[AF_NCH];        // Gain level for each channel
    int soft;                   // Enable/disable soft clipping
    int fast;                   // Use fix-point volume control
} af_volume_t;

// Initialization and runtime control
static int control(struct af_instance *af, int cmd, void *arg)
{
    af_volume_t *s   = (af_volume_t *)af->setup;

    switch (cmd) {
    case AF_CONTROL_REINIT:
        // Sanity check
        if (!arg)
            return AF_ERROR;

        mp_audio_copy_config(af->data, (struct mp_audio *)arg);

        if (s->fast && (((struct mp_audio *)arg)->format != (AF_FORMAT_FLOAT_NE)))
            mp_audio_set_format(af->data, AF_FORMAT_S16_NE);
        else {
            // Cutoff set to 10Hz for forgetting factor
            mp_audio_set_format(af->data, AF_FORMAT_FLOAT_NE);
        }
        return af_test_output(af, (struct mp_audio *)arg);
    case AF_CONTROL_COMMAND_LINE: {
        float v = 1000.0;
        float vol[AF_NCH];
        int i;
        sscanf((char *)arg, "%f:%i:%i", &v, &s->soft, &s->fast);
        float dest = 0.0;
        if (v < 1000)
            af_from_dB(1, &v, &dest, 20.0, -200.0, 60.0);
        for (i = 0; i < AF_NCH; i++)
            vol[i] = dest;
        return control(af, AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET, vol);
    }
    case AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET:
        memcpy(s->level, arg, sizeof(float) * AF_NCH);
        return AF_OK;
    case AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_GET:
        memcpy(arg, s->level, sizeof(float) * AF_NCH);
        return AF_OK;
    }
    return AF_UNKNOWN;
}

// Deallocate memory
static void uninit(struct af_instance *af)
{
    free(af->data);
    free(af->setup);
}

// Filter data through filter
static struct mp_audio *play(struct af_instance *af, struct mp_audio *data)
{
    struct mp_audio *c   = data;                        // Current working data
    af_volume_t *s   = (af_volume_t *)af->setup; // Setup for this instance
    int nch = c->nch;                  // Number of channels

    if (af->data->format == AF_FORMAT_S16_NE) {
        int16_t *a = (int16_t *)c->audio;     // Audio data
        int len = c->len / 2;                 // Number of samples
        for (int ch = 0; ch < nch; ch++) {
            int vol = 256.0 * s->level[ch];
            if (vol != 256) {
                for (int i = ch; i < len; i += nch) {
                    int x = (a[i] * vol) >> 8;
                    a[i] = clamp(x, SHRT_MIN, SHRT_MAX);
                }
            }
        }
    } else if (af->data->format == AF_FORMAT_FLOAT_NE) {
        float *a = (float *)c->audio;     // Audio data
        int len = c->len / 4;             // Number of samples
        for (int ch = 0; ch < nch; ch++) {
            // Volume control (fader)
            if (s->level[ch] != 1.0) {
                for (int i = ch; i < len; i += nch) {
                    float x = a[i];
                    // Set volume
                    x *= s->level[ch];
                    /* Soft clipping, the sound of a dream, thanks to Jon Wattes
                       post to Musicdsp.org */
                    a[i] = s->soft ? af_softclip(x) : clamp(x, -1.0, 1.0);
                }
            }
        }
    }
    return c;
}

// Allocate memory and set function pointers
static int af_open(struct af_instance *af)
{
    af->control = control;
    af->uninit = uninit;
    af->play = play;
    af->mul = 1;
    af->data = calloc(1, sizeof(struct mp_audio));
    af->setup = calloc(1, sizeof(af_volume_t));
    if (af->data == NULL || af->setup == NULL)
        return AF_ERROR;
    // Enable volume control and set initial volume to 0dB.
    for (int i = 0; i < AF_NCH; i++)
        ((af_volume_t *)af->setup)->level[i]  = 1.0;
    return AF_OK;
}

// Description of this filter
struct af_info af_info_volume = {
    .info = "Volume control audio filter",
    .name = "volume",
    .flags = AF_FLAGS_NOT_REENTRANT,
    .open = af_open,
};
