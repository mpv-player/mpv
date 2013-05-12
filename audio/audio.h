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

#ifndef MP_AUDIO_H
#define MP_AUDIO_H

#include "format.h"
#include "chmap.h"

// Audio data chunk
struct mp_audio {
    void *audio; // data buffer
    int len;    // buffer length (in bytes)
    int rate;   // sample rate
    struct mp_chmap channels; // channel layout, use mp_audio_set_*() to set
    int format; // format (AF_FORMAT_...), use mp_audio_set_format() to set
    // Redundant fields, for convenience
    int nch;    // number of channels (redundant with chmap)
    int bps;    // bytes per sample (redundant with format)
};

void mp_audio_set_format(struct mp_audio *mpa, int format);
void mp_audio_set_num_channels(struct mp_audio *mpa, int num_channels);
void mp_audio_set_channels_old(struct mp_audio *mpa, int num_channels);
void mp_audio_set_channels(struct mp_audio *mpa, const struct mp_chmap *chmap);
void mp_audio_copy_config(struct mp_audio *dst, const struct mp_audio *src);
bool mp_audio_config_equals(const struct mp_audio *a, const struct mp_audio *b);

char *mp_audio_fmt_to_str(int srate, const struct mp_chmap *chmap, int format);
char *mp_audio_config_to_str(struct mp_audio *mpa);

#endif
