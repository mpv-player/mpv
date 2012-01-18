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
#ifndef __MINGW32__
#include <sys/ioctl.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "libao2/audio_out.h"
#include "libaf/af.h"
#include "mixer.h"


char *mixer_device = NULL;
char *mixer_channel = NULL;
int soft_vol = 0;
float soft_vol_max = 110.0;

static void internal_setvolume(mixer_t *mixer, float l, float r);


// Called after the audio filter chain is built or rebuilt.
void mixer_reinit(mixer_t *mixer)
{
    if (!mixer->ao)
        return;
    // Some of this might be incorrect when the AO behavior changes (e.g.
    // different AO due to file specific -ao options), but we assume this
    // doesn't happen. We could attempt to handle this by trying to detect
    // whether a system mixer or mute is supported, but it would probably add
    // even more bugs to the code.
    if (mixer->restore_volume) {
        // restore previous volume (softvol, or no persistent AO volume)
        internal_setvolume(mixer, mixer->restore_vol_l, mixer->restore_vol_r);
    }
    if (mixer->muted &&
        (mixer->mute_emulation || mixer->ao->no_persistent_volume))
    {
        // undo mixer_uninit(), or restore mute state
        mixer_setmuted(mixer, true);
    }
    if (mixer->restore_balance) {
        // balance control always uses af_pan, it always needs to be restored
        mixer_setbalance(mixer, mixer->balance);
    }
}

// Called before the audio output is uninitialized.
// Note that this doesn't necessarily terminate the mixer_t instance, and it's
// possible that mixer_reinit() will be called later.
void mixer_uninit(mixer_t *mixer)
{
    if (!mixer->ao)
        return;
    // The player is supposed to restore the volume, when mute was enabled, and
    // the player terminates. No other attempts at restoring anything are done.
    // One complication is that the mute state should survive audio
    // reinitialization (e.g. when switching to a new file), so we have to be
    // sure mixer_reinit() will restore the mute state.
    // This is only needed when a global system mixer without mute control is
    // used, i.e. we emulate mute by setting the volume to 0.
    if (mixer->mute_emulation && mixer_getmuted(mixer)) {
        // avoid playing the rest of the audio buffer at restored volume
        ao_reset(mixer->ao);
        mixer_setmuted(mixer, false);
        mixer->muted = true;
    }
}

static void internal_getvolume(mixer_t *mixer, float *l, float *r)
{
    ao_control_vol_t vol;
    *l = 0;
    *r = 0;
    if (mixer->ao) {
        if (soft_vol ||
            CONTROL_OK != ao_control(mixer->ao, AOCONTROL_GET_VOLUME, &vol))
        {
            if (!mixer->afilter)
                return;
            float db_vals[AF_NCH];
            if (!af_control_any_rev(mixer->afilter,
                        AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_GET, db_vals))
            {
                db_vals[0] = db_vals[1] = 1.0;
            } else {
                af_from_dB(2, db_vals, db_vals, 20.0, -200.0, 60.0);
            }
            vol.left = (db_vals[0] / (soft_vol_max / 100.0)) * 100.0;
            vol.right = (db_vals[1] / (soft_vol_max / 100.0)) * 100.0;
        }
        *r = vol.right;
        *l = vol.left;
    }
}

static float clip_vol(float v)
{
    return v > 100 ? 100 : (v < 0 ? 0 : v);
}

static void internal_setvolume(mixer_t *mixer, float l, float r)
{
    l = clip_vol(l);
    r = clip_vol(r);
    ao_control_vol_t vol;
    vol.right = r;
    vol.left = l;
    if (mixer->ao) {
        bool use_softvol = soft_vol;
        if (!use_softvol) {
            if (CONTROL_OK != ao_control(mixer->ao, AOCONTROL_SET_VOLUME, &vol))
            {
                use_softvol = true;
            } else {
                mixer->restore_volume = mixer->ao->no_persistent_volume;
            }
        }
        if (use_softvol) {
            if (!mixer->afilter)
                return;
            // af_volume uses values in dB
            float db_vals[AF_NCH];
            int i;
            db_vals[0] = (l / 100.0) * (soft_vol_max / 100.0);
            db_vals[1] = (r / 100.0) * (soft_vol_max / 100.0);
            for (i = 2; i < AF_NCH; i++)
                db_vals[i] = ((l + r) / 100.0) * (soft_vol_max / 100.0) / 2.0;
            af_to_dB(AF_NCH, db_vals, db_vals, 20.0);
            if (!af_control_any_rev(mixer->afilter,
                            AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET, db_vals))
            {
                mp_tmsg(MSGT_GLOBAL, MSGL_INFO,
                    "[Mixer] No hardware mixing, inserting volume filter.\n");
                if (!(af_add(mixer->afilter, "volume")
                      && af_control_any_rev(mixer->afilter,
                            AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET, db_vals)))
                {
                    mp_tmsg(MSGT_GLOBAL, MSGL_ERR,
                            "[Mixer] No volume control available.\n");
                    return;
                }
            }
            mixer->restore_volume = true;
        }
        if (mixer->restore_volume) {
            mixer->restore_vol_l = l;
            mixer->restore_vol_r = r;
        }
    }
}

void mixer_setvolume(mixer_t *mixer, float l, float r)
{
    internal_setvolume(mixer, l, r);
    // Changing the volume clears mute; these are mplayer semantics. (If this
    // is not desired, this would be removed, and code for restoring the softvol
    // volume had to be added.)
    mixer_setmuted(mixer, false);
}

void mixer_getvolume(mixer_t *mixer, float *l, float *r)
{
    *l = 0;
    *r = 0;
    if (mixer->ao) {
        float real_l, real_r;
        internal_getvolume(mixer, &real_l, &real_r);
        // consider the case when the system mixer volumes change independently
        if (real_l != 0 || real_r != 0)
            mixer->muted = false;
        if (mixer->muted) {
            *l = mixer->last_l;
            *r = mixer->last_r;
        } else {
            *l = real_l;
            *r = real_r;
        }
    }
}

static void mixer_addvolume(mixer_t *mixer, float d)
{
    float mixer_l, mixer_r;
    mixer_getvolume(mixer, &mixer_l, &mixer_r);
    mixer_setvolume(mixer, mixer_l + d, mixer_r + d);
}

void mixer_incvolume(mixer_t *mixer)
{
    mixer_addvolume(mixer, +mixer->volstep);
}

void mixer_decvolume(mixer_t *mixer)
{
    mixer_addvolume(mixer, -mixer->volstep);
}

void mixer_getbothvolume(mixer_t *mixer, float *b)
{
    float mixer_l, mixer_r;
    mixer_getvolume(mixer, &mixer_l, &mixer_r);
    *b = (mixer_l + mixer_r) / 2;
}

void mixer_mute(mixer_t *mixer)
{
    mixer_setmuted(mixer, !mixer_getmuted(mixer));
}

bool mixer_getmuted(mixer_t *mixer)
{
    ao_control_vol_t vol = {0};
    if (!soft_vol &&
        CONTROL_OK == ao_control(mixer->ao, AOCONTROL_GET_MUTE, &vol))
    {
        mixer->muted = vol.left == 0.0f || vol.right == 0.0f;
    } else {
        float l, r;
        mixer_getvolume(mixer, &l, &r);     // updates mixer->muted
    }
    return mixer->muted;
}

void mixer_setmuted(mixer_t *mixer, bool mute)
{
    bool muted = mixer_getmuted(mixer);
    if (mute == muted)
        return;
    ao_control_vol_t vol;
    vol.left = vol.right = mute ? 0.0f : 1.0f;
    mixer->mute_emulation = soft_vol ||
        CONTROL_OK != ao_control(mixer->ao, AOCONTROL_SET_MUTE, &vol);
    if (mixer->mute_emulation) {
        // mute is emulated by setting volume to 0
        if (!mute) {
            internal_setvolume(mixer, mixer->last_l, mixer->last_r);
        } else {
            mixer_getvolume(mixer, &mixer->last_l, &mixer->last_r);
            internal_setvolume(mixer, 0, 0);
        }
    }
    mixer->muted = mute;
}

void mixer_getbalance(mixer_t *mixer, float *val)
{
    *val = 0.f;
    if (!mixer->afilter)
        return;
    af_control_any_rev(mixer->afilter, AF_CONTROL_PAN_BALANCE | AF_CONTROL_GET,
                       val);
}

void mixer_setbalance(mixer_t *mixer, float val)
{
    float level[AF_NCH];
    int i;
    af_control_ext_t arg_ext = { .arg = level };
    af_instance_t *af_pan_balance;

    if (!mixer->afilter)
        return;

    mixer->balance = val;
    mixer->restore_balance = true;

    if (af_control_any_rev(mixer->afilter,
                           AF_CONTROL_PAN_BALANCE | AF_CONTROL_SET, &val))
        return;

    if (!(af_pan_balance = af_add(mixer->afilter, "pan"))) {
        mp_tmsg(MSGT_GLOBAL, MSGL_ERR,
                "[Mixer] No balance control available.\n");
        mixer->restore_balance = false;
        return;
    }

    af_init(mixer->afilter);
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
