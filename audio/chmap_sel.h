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

#ifndef MP_CHMAP_SEL_H
#define MP_CHMAP_SEL_H

#include <stdbool.h>

#include "chmap.h"

struct mp_chmap_sel {
    // should be considered opaque
    bool allow_any, allow_waveext;
    bool speakers[MP_SPEAKER_ID_COUNT];
    struct mp_chmap *chmaps;
    int num_chmaps;

    struct mp_chmap chmaps_storage[20];

    void *tmp; // set to any talloc context to allow more chmaps entries
};

void mp_chmap_sel_add_any(struct mp_chmap_sel *s);
void mp_chmap_sel_add_waveext(struct mp_chmap_sel *s);
void mp_chmap_sel_add_waveext_def(struct mp_chmap_sel *s);
void mp_chmap_sel_add_alsa_def(struct mp_chmap_sel *s);
void mp_chmap_sel_add_map(struct mp_chmap_sel *s, const struct mp_chmap *map);
void mp_chmap_sel_add_speaker(struct mp_chmap_sel *s, int id);
bool mp_chmap_sel_adjust(const struct mp_chmap_sel *s, struct mp_chmap *map);
bool mp_chmap_sel_fallback(const struct mp_chmap_sel *s, struct mp_chmap *map);
bool mp_chmap_sel_get_def(const struct mp_chmap_sel *s, struct mp_chmap *map,
                          int num);

#endif
