/*
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

#include <string.h>
#include <stdio.h>

#include <libavutil/common.h>

#include "config.h"
#include "audio/out/ao.h"
#include "audio/filter/af.h"
#include "mpvcore/mp_msg.h"
#include "talloc.h"
#include "mixer.h"

struct mixer {
    struct MPOpts *opts;
    struct ao *ao;
    struct af_stream *af;
    // Static, dependent on ao/softvol settings
    bool softvol;                       // use AO (true) or af_volume (false)
    bool emulate_mute;                  // if true, emulate mute with volume=0
    // Last known values (possibly out of sync with reality)
    float vol_l, vol_r;
    bool muted;
    // Used to decide whether we should unmute on uninit
    bool muted_by_us;
    /* Contains ao driver name or "softvol" if volume is not persistent
     * and needs to be restored after the driver is reinitialized. */
    const char *driver;
    // Other stuff
    float balance;
};

struct mixer *mixer_init(void *talloc_ctx, struct MPOpts *opts)
{
    struct mixer *mixer = talloc_ptrtype(talloc_ctx, mixer);
    *mixer = (struct mixer) {
        .opts = opts,
        .vol_l = 100,
        .vol_r = 100,
        .driver = "",
    };
    return mixer;
}

static void checkvolume(struct mixer *mixer)
{
    if (!mixer->ao)
        return;

    ao_control_vol_t vol = {mixer->vol_l, mixer->vol_r};
    if (mixer->softvol) {
        float vals[AF_NCH];
        if (!af_control_any_rev(mixer->af,
                                AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_GET, vals))
            vals[0] = vals[1] = 1.0;
        vol.left = (vals[0] / (mixer->opts->softvol_max / 100.0)) * 100.0;
        vol.right = (vals[1] / (mixer->opts->softvol_max / 100.0)) * 100.0;
    } else {
        // Rely on the values not changing if the query is not supported
        ao_control(mixer->ao, AOCONTROL_GET_VOLUME, &vol);
        ao_control(mixer->ao, AOCONTROL_GET_MUTE, &mixer->muted);
    }
    float l = mixer->vol_l;
    float r = mixer->vol_r;
    if (mixer->emulate_mute && mixer->muted)
        l = r = 0;
    /* Try to detect cases where the volume has been changed by some external
     * action (such as something else changing a shared system-wide volume).
     * We don't test for exact equality, as some AOs may round the value
     * we last set to some nearby supported value. 3 has been the default
     * volume step for increase/decrease keys, and is apparently big enough
     * to step to the next possible value in most setups.
     */
    if (FFABS(vol.left - l) >= 3 || FFABS(vol.right - r) >= 3) {
        mixer->vol_l = vol.left;
        mixer->vol_r = vol.right;
        if (mixer->emulate_mute)
            mixer->muted = false;
    }
    mixer->muted_by_us &= mixer->muted;
}

void mixer_getvolume(struct mixer *mixer, float *l, float *r)
{
    checkvolume(mixer);
    *l = mixer->vol_l;
    *r = mixer->vol_r;
}

static void setvolume_internal(struct mixer *mixer, float l, float r)
{
    struct ao_control_vol vol = {.left = l, .right = r};
    if (!mixer->softvol) {
        if (ao_control(mixer->ao, AOCONTROL_SET_VOLUME, &vol) != CONTROL_OK)
            mp_tmsg(MSGT_GLOBAL, MSGL_ERR,
                    "[Mixer] Failed to change audio output volume.\n");
        return;
    }
    float vals[AF_NCH];
    vals[0] = l / 100.0 * mixer->opts->softvol_max / 100.0;
    vals[1] = r / 100.0 * mixer->opts->softvol_max / 100.0;
    for (int i = 2; i < AF_NCH; i++)
        vals[i] = (vals[0] + vals[1]) / 2.0;
    if (!af_control_any_rev(mixer->af,
                            AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET,
                            vals))
    {
        mp_tmsg(MSGT_GLOBAL, MSGL_V, "[Mixer] Inserting volume filter.\n");
        if (!(af_add(mixer->af, "volume", NULL)
              && af_control_any_rev(mixer->af,
                                    AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET,
                                    vals)))
            mp_tmsg(MSGT_GLOBAL, MSGL_ERR,
                    "[Mixer] No volume control available.\n");
    }
}

void mixer_setvolume(struct mixer *mixer, float l, float r)
{
    checkvolume(mixer);  // to check mute status
    if (mixer->vol_l == l && mixer->vol_r == r)
        return; // just prevent af_volume insertion when not needed
    mixer->vol_l = av_clipf(l, 0, 100);
    mixer->vol_r = av_clipf(r, 0, 100);
    if (mixer->ao && !(mixer->emulate_mute && mixer->muted))
        setvolume_internal(mixer, mixer->vol_l, mixer->vol_r);
}

void mixer_getbothvolume(struct mixer *mixer, float *b)
{
    float mixer_l, mixer_r;
    mixer_getvolume(mixer, &mixer_l, &mixer_r);
    *b = (mixer_l + mixer_r) / 2;
}

void mixer_setmute(struct mixer *mixer, bool mute)
{
    checkvolume(mixer);
    if (mute == mixer->muted)
        return;
    if (mixer->ao) {
        mixer->muted = mute;
        mixer->muted_by_us = mute;
        if (mixer->emulate_mute) {
            setvolume_internal(mixer, mixer->vol_l*!mute, mixer->vol_r*!mute);
        } else {
            ao_control(mixer->ao, AOCONTROL_SET_MUTE, &mute);
        }
        checkvolume(mixer);
    } else {
        mixer->muted = mute;
        mixer->muted_by_us = mute;
    }
}

bool mixer_getmute(struct mixer *mixer)
{
    checkvolume(mixer);
    return mixer->muted;
}

static void addvolume(struct mixer *mixer, float d)
{
    float vol_l, vol_r;
    mixer_getvolume(mixer, &vol_l, &vol_r);
    mixer_setvolume(mixer, vol_l + d, vol_r + d);
}

void mixer_incvolume(struct mixer *mixer)
{
    addvolume(mixer, mixer->opts->volstep);
}

void mixer_decvolume(struct mixer *mixer)
{
    addvolume(mixer, -mixer->opts->volstep);
}

void mixer_getbalance(struct mixer *mixer, float *val)
{
    if (mixer->af)
        af_control_any_rev(mixer->af,
                           AF_CONTROL_PAN_BALANCE | AF_CONTROL_GET,
                           &mixer->balance);
    *val = mixer->balance;
}

/* NOTE: Currently the balance code is seriously buggy: it always changes
 * the af_pan mapping between the first two input channels and first two
 * output channels to particular values. These values make sense for an
 * af_pan instance that was automatically inserted for balance control
 * only and is otherwise an identity transform, but if the filter was
 * there for another reason, then ignoring and overriding the original
 * values is completely wrong. In particular, this will break
 * automatically inserted downmix filters; the original coefficients that
 * are significantly below 1 will be overwritten with much higher values.
 */

void mixer_setbalance(struct mixer *mixer, float val)
{
    float level[AF_NCH];
    int i;
    af_control_ext_t arg_ext = { .arg = level };
    struct af_instance *af_pan_balance;

    mixer->balance = val;

    if (!mixer->af)
        return;

    if (af_control_any_rev(mixer->af,
                           AF_CONTROL_PAN_BALANCE | AF_CONTROL_SET, &val))
        return;

    if (val == 0 || mixer->ao->channels.num < 2)
        return;

    if (!(af_pan_balance = af_add(mixer->af, "pan", NULL))) {
        mp_tmsg(MSGT_GLOBAL, MSGL_ERR,
                "[Mixer] No balance control available.\n");
        return;
    }

    /* make all other channels pass thru since by default pan blocks all */
    memset(level, 0, sizeof(level));
    for (i = 2; i < AF_NCH; i++) {
        arg_ext.ch = i;
        level[i] = 1.f;
        af_pan_balance->control(af_pan_balance,
                                AF_CONTROL_PAN_LEVEL | AF_CONTROL_SET,
                                &arg_ext);
        level[i] = 0.f;
    }

    af_pan_balance->control(af_pan_balance,
                            AF_CONTROL_PAN_BALANCE | AF_CONTROL_SET, &val);
}

char *mixer_get_volume_restore_data(struct mixer *mixer)
{
    if (!mixer->driver[0])
        return NULL;
    return talloc_asprintf(NULL, "%s:%f:%f:%d", mixer->driver, mixer->vol_l,
                           mixer->vol_r, mixer->muted_by_us);
}

static void probe_softvol(struct mixer *mixer)
{
    if (mixer->opts->softvol == SOFTVOL_AUTO) {
        // No system-wide volume => fine with AO volume control.
        mixer->softvol = !(mixer->ao->per_application_mixer ||
                           mixer->ao->no_persistent_volume);
    } else {
        mixer->softvol = mixer->opts->softvol == SOFTVOL_YES;
    }

    // If we can't use real volume control => force softvol.
    if (!mixer->softvol) {
        ao_control_vol_t vol;
        if (ao_control(mixer->ao, AOCONTROL_GET_VOLUME, &vol) != CONTROL_OK) {
            mixer->softvol = true;
            mp_tmsg(MSGT_GLOBAL, MSGL_WARN,
                    "[mixer] Hardware volume control unavailable.\n");
        }
    }

    // Probe native mute support.
    mixer->emulate_mute = true;
    if (!mixer->softvol) {
        if (ao_control(mixer->ao, AOCONTROL_GET_MUTE, &(bool){0}) == CONTROL_OK)
            mixer->emulate_mute = false;
    }
}

static void restore_volume(struct mixer *mixer)
{
    struct MPOpts *opts = mixer->opts;
    struct ao *ao = mixer->ao;

    float force_vol_l = -1, force_vol_r = -1;
    int force_mute = -1;

    const char *prev_driver = mixer->driver;
    mixer->driver = mixer->softvol ? "softvol" : ao->driver->info->short_name;

    bool restore = mixer->softvol || ao->no_persistent_volume;

    // Restore old parameters if volume won't survive reinitialization.
    // But not if volume scale is possibly different.
    if (restore && strcmp(mixer->driver, prev_driver) == 0) {
        force_vol_l = mixer->vol_l;
        force_vol_r = mixer->vol_r;
    }

    // Set mute if we disabled it on uninit last time.
    if (mixer->muted_by_us)
        force_mute = 1;

    // Set parameters from command line.
    if (opts->mixer_init_volume >= 0)
        force_vol_l = force_vol_r = opts->mixer_init_volume;
    if (opts->mixer_init_mute >= 0)
        force_mute = opts->mixer_init_mute;

    // Set parameters from playback resume.
    char *restore_data = mixer->opts->mixer_restore_volume_data;
    if (restore && restore_data && restore_data[0]) {
        char drv[40];
        float v_l, v_r;
        int m;
        if (sscanf(restore_data, "%39[^:]:%f:%f:%d", drv, &v_l, &v_r, &m) == 4) {
            if (strcmp(mixer->driver, drv) == 0) {
                force_vol_l = v_l;
                force_vol_r = v_r;
                force_mute = !!m;
            }
        }
        talloc_free(restore_data);
        mixer->opts->mixer_restore_volume_data = NULL;
    }

    // Using --volume should not reset the volume on every file (i.e. reinit),
    // OTOH mpv --{ --volume 10 f1.mkv --} --{ --volume 20 f2.mkv --} must work.
    // Resetting the option volumes to "auto" (-1) is easiest. If file local
    // options (as shown above) are used, the option handler code will reset
    // them to other values, and force the volume to be reset as well.
    opts->mixer_init_volume = -1;
    opts->mixer_init_mute = -1;

    checkvolume(mixer);
    if (force_vol_l >= 0 && force_vol_r >= 0)
        mixer_setvolume(mixer, force_vol_l, force_vol_r);
    if (force_mute >= 0)
        mixer_setmute(mixer, force_mute);
}

// Called after the audio filter chain is built or rebuilt.
// (Can be called multiple times, even without mixer_uninit() in-between.)
void mixer_reinit_audio(struct mixer *mixer, struct ao *ao, struct af_stream *af)
{
    if (!ao || !af)
        return;
    mixer->ao = ao;
    mixer->af = af;

    probe_softvol(mixer);
    restore_volume(mixer);

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

    checkvolume(mixer);
    if (mixer->muted_by_us) {
        /* Current audio output API combines playing the remaining buffered
         * audio and uninitializing the AO into one operation, even though
         * ideally unmute would happen between those two steps. We can't do
         * volume changes after uninitialization, but we don't want the
         * remaining audio to play at full volume either. Thus this
         * workaround to drop remaining audio first. */
        ao_reset(mixer->ao);
        mixer_setmute(mixer, false);
        /* We remember mute status and re-enable it if we play more audio
         * in the same process. */
        mixer->muted_by_us = true;
    }
    mixer->ao = NULL;
    mixer->af = NULL;
}
