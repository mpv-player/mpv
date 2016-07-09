/*
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

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include <libavutil/common.h>

#include "config.h"
#include "audio/out/ao.h"
#include "audio/filter/af.h"
#include "common/global.h"
#include "common/msg.h"
#include "mpv_talloc.h"
#include "mixer.h"

struct mixer {
    struct mp_log *log;
    struct MPOpts *opts;
    struct ao *ao;
    struct af_stream *af;
    // Other stuff
    float balance;
};

struct mixer *mixer_init(void *talloc_ctx, struct mpv_global *global)
{
    struct mixer *mixer = talloc_ptrtype(talloc_ctx, mixer);
    *mixer = (struct mixer) {
        .log = mp_log_new(mixer, global->log, "mixer"),
        .opts = global->opts,
    };
    return mixer;
}

bool mixer_audio_initialized(struct mixer *mixer)
{
    return !!mixer->af;
}

// Called when opts->softvol_volume or opts->softvol_mute were changed.
void mixer_update_volume(struct mixer *mixer)
{
    float gain = MPMAX(mixer->opts->softvol_volume / 100.0, 0);
    if (mixer->opts->softvol_mute == 1)
        gain = 0.0;

    if (!af_control_any_rev(mixer->af, AF_CONTROL_SET_VOLUME, &gain)) {
        if (gain == 1.0)
            return;
        MP_VERBOSE(mixer, "Inserting volume filter.\n");
        if (!(af_add(mixer->af, "volume", "softvol", NULL)
              && af_control_any_rev(mixer->af, AF_CONTROL_SET_VOLUME, &gain)))
            MP_ERR(mixer, "No volume control available.\n");
    }
}

void mixer_getbalance(struct mixer *mixer, float *val)
{
    if (mixer->af)
        af_control_any_rev(mixer->af, AF_CONTROL_GET_PAN_BALANCE, &mixer->balance);
    *val = mixer->balance;
}

/* NOTE: Currently the balance code is seriously buggy: it always changes
 * the af_pan mapping between the first two input channels and first two
 * output channels to particular values. These values make sense for an
 * af_pan instance that was automatically inserted for balance control
 * only and is otherwise an identity transform, but if the filter was
 * there for another reason, then ignoring and overriding the original
 * values is completely wrong.
 */

void mixer_setbalance(struct mixer *mixer, float val)
{
    struct af_instance *af_pan_balance;

    mixer->balance = val;

    if (!mixer->af)
        return;

    if (af_control_any_rev(mixer->af, AF_CONTROL_SET_PAN_BALANCE, &val))
        return;

    if (val == 0)
        return;

    if (!(af_pan_balance = af_add(mixer->af, "pan", "autopan", NULL))) {
        MP_ERR(mixer, "No balance control available.\n");
        return;
    }

    /* make all other channels pass through since by default pan blocks all */
    for (int i = 2; i < AF_NCH; i++) {
        float level[AF_NCH] = {0};
        level[i] = 1.f;
        af_control_ext_t arg_ext = { .ch = i, .arg = level };
        af_pan_balance->control(af_pan_balance, AF_CONTROL_SET_PAN_LEVEL,
                                &arg_ext);
    }

    af_pan_balance->control(af_pan_balance, AF_CONTROL_SET_PAN_BALANCE, &val);
}

// Called after the audio filter chain is built or rebuilt.
// (Can be called multiple times, even without mixer_uninit() in-between.)
void mixer_reinit_audio(struct mixer *mixer, struct af_stream *af)
{
    mixer->af = af;
    if (!af)
        return;

    if (mixer->opts->softvol == SOFTVOL_NO)
        MP_ERR(mixer, "--softvol=no is not supported anymore.\n");

    mixer_update_volume(mixer);

    if (mixer->balance != 0)
        mixer_setbalance(mixer, mixer->balance);
}

/* Called before uninitializing the audio filter chain. The main purpose is to
 * turn off mute, in case it's a global/persistent setting which might
 * otherwise be left enabled even after this player instance exits.
 */
void mixer_uninit_audio(struct mixer *mixer)
{
    if (!mixer->ao)
        return;

    mixer->af = NULL;
}
