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

#include "common/common.h"
#include "stream/stream.h"
#include "demux.h"

#include "timeline.h"

struct timeline *timeline_load(struct mpv_global *global, struct mp_log *log,
                               struct demuxer *demuxer)
{
    if (!demuxer->desc->load_timeline)
        return NULL;

    struct timeline *tl = talloc_ptrtype(NULL, tl);
    *tl = (struct timeline){
        .global = global,
        .log = log,
        .cancel = demuxer->stream->cancel,
        .demuxer = demuxer,
        .track_layout = demuxer,
    };

    demuxer->desc->load_timeline(tl);

    if (tl->num_parts)
        return tl;
    timeline_destroy(tl);
    return NULL;
}

void timeline_destroy(struct timeline *tl)
{
    if (!tl)
        return;
    for (int n = 0; n < tl->num_sources; n++) {
        struct demuxer *d = tl->sources[n];
        if (d != tl->demuxer)
            free_demuxer_and_stream(d);
    }
    talloc_free(tl);
}
