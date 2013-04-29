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

#ifndef MPLAYER_MIXER_H
#define MPLAYER_MIXER_H

#include <stdbool.h>

enum {
    SOFTVOL_NO = 0,
    SOFTVOL_YES = 1,
    SOFTVOL_AUTO = 2,
};

typedef struct mixer {
    struct ao *ao;
    struct af_stream *afilter;
    int volstep;
    int softvol;
    float softvol_max;
    bool muted;
    bool muted_by_us;
    bool muted_using_volume;
    float vol_l, vol_r;
    /* Contains ao driver name or "softvol" if volume is not persistent
     * and needs to be restored after the driver is reinitialized. */
    const char *restore_volume;
    float balance;
    bool user_set_mute;
    bool user_set_volume;
} mixer_t;

void mixer_reinit(struct mixer *mixer, struct ao *ao);
void mixer_uninit(struct mixer *mixer);
void mixer_getvolume(mixer_t *mixer, float *l, float *r);
void mixer_setvolume(mixer_t *mixer, float l, float r);
void mixer_incvolume(mixer_t *mixer);
void mixer_decvolume(mixer_t *mixer);
void mixer_getbothvolume(mixer_t *mixer, float *b);
void mixer_setmute(mixer_t *mixer, bool mute);
bool mixer_getmute(mixer_t *mixer);
void mixer_getbalance(mixer_t *mixer, float *bal);
void mixer_setbalance(mixer_t *mixer, float bal);

#endif /* MPLAYER_MIXER_H */
