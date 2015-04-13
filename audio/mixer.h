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

#ifndef MPLAYER_MIXER_H
#define MPLAYER_MIXER_H

#include <stdbool.h>

// Values of MPOpts.softvol
enum {
    SOFTVOL_NO = 0,
    SOFTVOL_YES = 1,
    SOFTVOL_AUTO = 2,
};

struct mpv_global;
struct ao;
struct af_stream;
struct mixer;

struct mixer *mixer_init(void *talloc_ctx, struct mpv_global *global);
void mixer_reinit_audio(struct mixer *mixer, struct ao *ao, struct af_stream *af);
void mixer_uninit_audio(struct mixer *mixer);
bool mixer_audio_initialized(struct mixer *mixer);
void mixer_getvolume(struct mixer *mixer, float *l, float *r);
void mixer_setvolume(struct mixer *mixer, float l, float r);
void mixer_addvolume(struct mixer *mixer, float step);
void mixer_getbothvolume(struct mixer *mixer, float *b);
void mixer_setmute(struct mixer *mixer, bool mute);
bool mixer_getmute(struct mixer *mixer);
void mixer_getbalance(struct mixer *mixer, float *bal);
void mixer_setbalance(struct mixer *mixer, float bal);
float mixer_getneutralvolume(struct mixer *mixer);
char *mixer_get_volume_restore_data(struct mixer *mixer);

#endif /* MPLAYER_MIXER_H */
