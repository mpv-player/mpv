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

#include <libavutil/common.h>

#include "config.h"
#include "audio/out/ao.h"
#include "audio/filter/af.h"
#include "core/mp_msg.h"
#include "mixer.h"


static void checkvolume(struct mixer *mixer)
{
    if (!mixer->ao)
        return;

    if (mixer->softvol == SOFTVOL_AUTO) {
        mixer->softvol = mixer->ao->per_application_mixer
                         || mixer->ao->no_persistent_volume
                         ? SOFTVOL_NO : SOFTVOL_YES;
    }

    ao_control_vol_t vol;
    if (mixer->softvol || CONTROL_OK != ao_control(mixer->ao,
                                                AOCONTROL_GET_VOLUME, &vol)) {
        mixer->softvol = SOFTVOL_YES;
        if (!mixer->afilter)
            return;
        float db_vals[AF_NCH];
        if (!af_control_any_rev(mixer->afilter,
                        AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_GET, db_vals))
            db_vals[0] = db_vals[1] = 1.0;
        else
            af_from_dB(2, db_vals, db_vals, 20.0, -200.0, 60.0);
        vol.left = (db_vals[0] / (mixer->softvol_max / 100.0)) * 100.0;
        vol.right = (db_vals[1] / (mixer->softvol_max / 100.0)) * 100.0;
    }
    float l = mixer->vol_l;
    float r = mixer->vol_r;
    if (mixer->muted_using_volume)
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
        if (mixer->muted_using_volume)
            mixer->muted = false;
    }
    if (!mixer->softvol)
        // Rely on the value not changing if the query is not supported
        ao_control(mixer->ao, AOCONTROL_GET_MUTE, &mixer->muted);
    mixer->muted_by_us &= mixer->muted;
    mixer->muted_using_volume &= mixer->muted;
}

void mixer_getvolume(mixer_t *mixer, float *l, float *r)
{
    checkvolume(mixer);
    *l = mixer->vol_l;
    *r = mixer->vol_r;
}

static void setvolume_internal(mixer_t *mixer, float l, float r)
{
    struct ao_control_vol vol = {.left = l, .right = r};
    if (!mixer->softvol) {
        // relies on the driver data being permanent (so ptr stays valid)
        mixer->restore_volume = mixer->ao->no_persistent_volume ?
            mixer->ao->driver->info->short_name : NULL;
        if (ao_control(mixer->ao, AOCONTROL_SET_VOLUME, &vol) != CONTROL_OK)
            mp_tmsg(MSGT_GLOBAL, MSGL_ERR,
                    "[Mixer] Failed to change audio output volume.\n");
        return;
    }
    mixer->restore_volume = "softvol";
    if (!mixer->afilter)
        return;
    // af_volume uses values in dB
    float db_vals[AF_NCH];
    int i;
    db_vals[0] = (l / 100.0) * (mixer->softvol_max / 100.0);
    db_vals[1] = (r / 100.0) * (mixer->softvol_max / 100.0);
    for (i = 2; i < AF_NCH; i++)
        db_vals[i] = ((l + r) / 100.0) * (mixer->softvol_max / 100.0) / 2.0;
    af_to_dB(AF_NCH, db_vals, db_vals, 20.0);
    if (!af_control_any_rev(mixer->afilter,
                            AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET,
                            db_vals))
    {
        mp_tmsg(MSGT_GLOBAL, mixer->softvol ? MSGL_V : MSGL_WARN,
                "[Mixer] No hardware mixing, inserting volume filter.\n");
        if (!(af_add(mixer->afilter, "volume")
              && af_control_any_rev(mixer->afilter,
                                    AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET,
                                    db_vals)))
            mp_tmsg(MSGT_GLOBAL, MSGL_ERR,
                    "[Mixer] No volume control available.\n");
    }
}

void mixer_setvolume(mixer_t *mixer, float l, float r)
{
    checkvolume(mixer);  // to check mute status and AO support for volume
    mixer->vol_l = av_clipf(l, 0, 100);
    mixer->vol_r = av_clipf(r, 0, 100);
    if (!mixer->ao || mixer->muted_using_volume)
        return;
    setvolume_internal(mixer, mixer->vol_l, mixer->vol_r);
    mixer->user_set_volume = true;
}

void mixer_getbothvolume(mixer_t *mixer, float *b)
{
    float mixer_l, mixer_r;
    mixer_getvolume(mixer, &mixer_l, &mixer_r);
    *b = (mixer_l + mixer_r) / 2;
}

void mixer_setmute(struct mixer *mixer, bool mute)
{
    checkvolume(mixer);
    if (mute != mixer->muted) {
        if (!mixer->softvol && !mixer->muted_using_volume && ao_control(
                mixer->ao, AOCONTROL_SET_MUTE, &mute) == CONTROL_OK) {
            mixer->muted_using_volume = false;
        } else {
            setvolume_internal(mixer, mixer->vol_l*!mute, mixer->vol_r*!mute);
            mixer->muted_using_volume = mute;
        }
        mixer->muted = mute;
        mixer->muted_by_us = mute;
        mixer->user_set_mute = true;
    }
}

bool mixer_getmute(struct mixer *mixer)
{
    checkvolume(mixer);
    return mixer->muted;
}

static void addvolume(struct mixer *mixer, float d)
{
    checkvolume(mixer);
    mixer_setvolume(mixer, mixer->vol_l + d, mixer->vol_r + d);
    if (d > 0)
        mixer_setmute(mixer, false);
}

void mixer_incvolume(mixer_t *mixer)
{
    addvolume(mixer, mixer->volstep);
}

void mixer_decvolume(mixer_t *mixer)
{
    addvolume(mixer, -mixer->volstep);
}

void mixer_getbalance(mixer_t *mixer, float *val)
{
    if (mixer->afilter)
        af_control_any_rev(mixer->afilter,
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

void mixer_setbalance(mixer_t *mixer, float val)
{
    float level[AF_NCH];
    int i;
    af_control_ext_t arg_ext = { .arg = level };
    struct af_instance *af_pan_balance;

    mixer->balance = val;

    if (!mixer->afilter)
        return;

    if (af_control_any_rev(mixer->afilter,
                           AF_CONTROL_PAN_BALANCE | AF_CONTROL_SET, &val))
        return;

    if (val == 0 || mixer->ao->channels.num < 2)
        return;

    if (!(af_pan_balance = af_add(mixer->afilter, "pan"))) {
        mp_tmsg(MSGT_GLOBAL, MSGL_ERR,
                "[Mixer] No balance control available.\n");
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

// Called after the audio filter chain is built or rebuilt.
void mixer_reinit(struct mixer *mixer, struct ao *ao)
{
    mixer->ao = ao;
    /* Use checkvolume() to see if softvol needs to be enabled because of
     * lacking AO support, but first store values it could overwrite. */
    float left = mixer->vol_l, right = mixer->vol_r;
    bool muted = mixer->muted_by_us;
    checkvolume(mixer);
    /* Try to avoid restoring volume stored from one control method with
     * another. Especially, restoring softvol volume (typically high) on
     * system mixer could have very nasty effects. */
    const char *restore_reason = mixer->softvol ? "softvol" :
        mixer->ao->driver->info->short_name;
    if (mixer->restore_volume && !strcmp(mixer->restore_volume,
                                         restore_reason))
        mixer_setvolume(mixer, left, right);
    /* We turn mute off at AO uninit, so it has to be restored (unless
     * we're reinitializing filter chain while keeping AO); but we only
     * enable mute, not turn external mute off. */
    if (muted)
        mixer_setmute(mixer, true);
    if (mixer->balance != 0)
        mixer_setbalance(mixer, mixer->balance);
    mixer->user_set_mute = false;
    mixer->user_set_volume = false;
}

/* Called before uninitializing the audio output. The main purpose is to
 * turn off mute, in case it's a global/persistent setting which might
 * otherwise be left enabled even after this player instance exits.
 */
void mixer_uninit(struct mixer *mixer)
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
}
