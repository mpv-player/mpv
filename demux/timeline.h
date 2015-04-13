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

#ifndef MP_TIMELINE_H_
#define MP_TIMELINE_H_

struct timeline_part {
    double start;
    double source_start;
    struct demuxer *source;
};

struct timeline {
    struct mpv_global *global;
    struct mp_log *log;
    struct mp_cancel *cancel;

    // main source
    struct demuxer *demuxer;

    // All referenced files. The source file must be at sources[0].
    struct demuxer **sources;
    int num_sources;

    // Segments to play, ordered by time. parts[num_parts] must be valid; its
    // start field sets the duration, and source must be NULL.
    struct timeline_part *parts;
    int num_parts;

    struct demux_chapter *chapters;
    int num_chapters;

    // Which source defines the overall track list (over the full timeline).
    struct demuxer *track_layout;
};

struct timeline *timeline_load(struct mpv_global *global, struct mp_log *log,
                               struct demuxer *demuxer);
void timeline_destroy(struct timeline *tl);

#endif
