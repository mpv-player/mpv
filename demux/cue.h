/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MP_CUE_H_
#define MP_CUE_H_

#include <stdbool.h>

#include "misc/bstr.h"

struct cue_file {
    struct cue_track *tracks;
    int num_tracks;
    struct mp_tags *tags;
};

struct cue_track {
    double pregap_start;        // corresponds to INDEX 00
    double start;               // corresponds to INDEX 01
    char *filename;
    int source;
    struct mp_tags *tags;
};

bool mp_probe_cue(struct bstr data);
struct cue_file *mp_parse_cue(struct bstr data);
int mp_check_embedded_cue(struct cue_file *f);

#endif
