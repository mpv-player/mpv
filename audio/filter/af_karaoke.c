/*
 * simple voice removal filter
 *
 * copyright (c) 2006 Reynaldo H. Verdejo Pinochet
 * Based on code by Alex Beregszaszi for his 'center' filter.
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "af.h"

// Data for specific instances of this filter

// Initialization and runtime control
static int control(struct af_instance* af, int cmd, void* arg)
{
        switch(cmd){
                case AF_CONTROL_REINIT:
                mp_audio_copy_config(af->data, (struct mp_audio*)arg);
                mp_audio_set_format(af->data, AF_FORMAT_FLOAT);
                return af_test_output(af,(struct mp_audio*)arg);
        }
        return AF_UNKNOWN;
}

static int filter_frame(struct af_instance *af, struct mp_audio *c)
{
        if (!c)
                return 0;
        if (af_make_writeable(af, c) < 0) {
                talloc_free(c);
                return 0;
        }

        float*          a       = c->planes[0];  // Audio data
        int                     nch     = c->nch;        // Number of channels
        int                     len     = c->samples*nch;        // Number of samples in current audio block
        register int  i;

        /*
                FIXME1 add a low band pass filter to avoid suppressing
                centered bass/drums
                FIXME2 better calculated* attenuation factor
        */

        for(i=0;i<len;i+=nch)
        {
                a[i] = (a[i] - a[i+1]) * 0.7;
                a[i+1]=a[i];
        }

        af_add_output_frame(af, c);
        return 0;
}

// Allocate memory and set function pointers
static int af_open(struct af_instance* af){
        af->control     = control;
        af->filter_frame = filter_frame;
        return AF_OK;
}

// Description of this filter
const struct af_info af_info_karaoke = {
        .info = "Simple karaoke/voice-removal audio filter",
        .name = "karaoke",
        .flags = AF_FLAGS_NOT_REENTRANT,
        .open = af_open,
};
