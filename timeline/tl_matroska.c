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

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <libavutil/common.h>

#include "talloc.h"

#include "mp_core.h"
#include "mp_msg.h"
#include "libmpdemux/demuxer.h"
#include "osdep/findfiles.h"

static int find_ordered_chapter_sources(struct MPContext *mpctx,
                                        struct content_source *sources,
                                        int num_sources,
                                        unsigned char uid_map[][16])
{
    int num_filenames = 0;
    char **filenames = NULL;
    if (num_sources > 1) {
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "This file references data from "
               "other sources.\n");
        if (mpctx->stream->type != STREAMTYPE_FILE) {
            mp_msg(MSGT_CPLAYER, MSGL_WARN, "Playback source is not a "
                   "normal disk file. Will not search for related files.\n");
        } else {
            mp_msg(MSGT_CPLAYER, MSGL_INFO, "Will scan other files in the "
                   "same directory to find referenced sources.\n");
            filenames = find_files(mpctx->demuxer->filename, ".mkv",
                                   &num_filenames);
        }
    }

    int num_left = num_sources - 1;
    for (int i = 0; i < num_filenames && num_left > 0; i++) {
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "Checking file %s\n",
               filename_recode(filenames[i]));
        int format = 0;
        struct stream *s = open_stream(filenames[i], &mpctx->opts, &format);
        if (!s)
            continue;
        struct demuxer *d = demux_open(&mpctx->opts, s, DEMUXER_TYPE_MATROSKA,
                                       mpctx->opts.audio_id,
                                       mpctx->opts.video_id,
                                       mpctx->opts.sub_id, filenames[i]);
        if (!d) {
            free_stream(s);
            continue;
        }
        if (d->file_format == DEMUXER_TYPE_MATROSKA) {
            for (int i = 1; i < num_sources; i++) {
                if (sources[i].demuxer)
                    continue;
                if (!memcmp(uid_map[i], d->matroska_data.segment_uid, 16)) {
                    mp_msg(MSGT_CPLAYER, MSGL_INFO,"Match for source %d: %s\n",
                           i, filename_recode(d->filename));
                    sources[i].stream = s;
                    sources[i].demuxer = d;
                    num_left--;
                    goto match;
                }
            }
        }
        free_demuxer(d);
        free_stream(s);
        continue;
    match:
        ;
    }
    talloc_free(filenames);
    if (num_left) {
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "Failed to find ordered chapter part!\n"
               "There will be parts MISSING from the video!\n");
        for (int i = 1, j = 1; i < num_sources; i++)
            if (sources[i].demuxer) {
                sources[j] = sources[i];
                memcpy(uid_map[j], uid_map[i], 16);
                j++;
            }
    }
    return num_sources - num_left;
}

void build_ordered_chapter_timeline(struct MPContext *mpctx)
{
    struct MPOpts *opts = &mpctx->opts;

    if (!opts->ordered_chapters) {
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "File uses ordered chapters, but "
               "you have disabled support for them. Ignoring.\n");
        return;
    }

    mp_msg(MSGT_CPLAYER, MSGL_INFO, "File uses ordered chapters, will build "
           "edit timeline.\n");

    struct demuxer *demuxer = mpctx->demuxer;
    struct matroska_data *m = &demuxer->matroska_data;

    // +1 because sources/uid_map[0] is original file even if all chapters
    // actually use other sources and need separate entries
    struct content_source *sources = talloc_array_ptrtype(NULL, sources,
                                                   m->num_ordered_chapters+1);
    sources[0].stream = mpctx->stream;
    sources[0].demuxer = mpctx->demuxer;
    unsigned char uid_map[m->num_ordered_chapters+1][16];
    int num_sources = 1;
    memcpy(uid_map[0], m->segment_uid, 16);

    for (int i = 0; i < m->num_ordered_chapters; i++) {
        struct matroska_chapter *c = m->ordered_chapters + i;
        if (!c->has_segment_uid)
            memcpy(c->segment_uid, m->segment_uid, 16);

        for (int j = 0; j < num_sources; j++)
            if (!memcmp(c->segment_uid, uid_map[j], 16))
                goto found1;
        memcpy(uid_map[num_sources], c->segment_uid, 16);
        sources[num_sources] = (struct content_source){};
        num_sources++;
    found1:
        ;
    }

    num_sources = find_ordered_chapter_sources(mpctx, sources, num_sources,
                                               uid_map);


    // +1 for terminating chapter with start time marking end of last real one
    struct timeline_part *timeline = talloc_array_ptrtype(NULL, timeline,
                                                  m->num_ordered_chapters + 1);
    struct chapter *chapters = talloc_array_ptrtype(NULL, chapters,
                                                    m->num_ordered_chapters);
    uint64_t starttime = 0;
    uint64_t missing_time = 0;
    int part_count = 0;
    int num_chapters = 0;
    uint64_t prev_part_offset = 0;
    for (int i = 0; i < m->num_ordered_chapters; i++) {
        struct matroska_chapter *c = m->ordered_chapters + i;

        int j;
        for (j = 0; j < num_sources; j++) {
            if (!memcmp(c->segment_uid, uid_map[j], 16))
                goto found2;
        }
        missing_time += c->end - c->start;
        continue;
    found2:;
        /* Only add a separate part if the time or file actually changes.
         * Matroska files have chapter divisions that are redundant from
         * timeline point of view because the same chapter structure is used
         * both to specify the timeline and for normal chapter information.
         * Removing a missing inserted external chapter can also cause this.
         * We allow for a configurable fudge factor because of files which
         * specify chapter end times that are one frame too early;
         * we don't want to try seeking over a one frame gap. */
        int64_t join_diff = c->start - starttime - prev_part_offset;
        if (part_count == 0
            || FFABS(join_diff) > opts->chapter_merge_threshold * 1000000
            || sources + j != timeline[part_count - 1].source) {
            timeline[part_count].source = sources + j;
            timeline[part_count].start = starttime / 1e9;
            timeline[part_count].source_start = c->start / 1e9;
            prev_part_offset = c->start - starttime;
            part_count++;
        } else if (part_count > 0 && join_diff) {
            /* Chapter was merged at an inexact boundary;
             * adjust timestamps to match. */
            mp_msg(MSGT_CPLAYER, MSGL_V, "Merging timeline part %d with "
                   "offset %d ms.\n", i, (int) join_diff);
            starttime += join_diff;
        }
        chapters[num_chapters].start = starttime / 1e9;
        chapters[num_chapters].name = talloc_strdup(chapters, c->name);
        starttime += c->end - c->start;
        num_chapters++;
    }
    timeline[part_count].start = starttime / 1e9;

    if (!part_count) {
        // None of the parts come from the file itself???
        talloc_free(sources);
        talloc_free(timeline);
        talloc_free(chapters);
        return;
    }

    mp_msg(MSGT_CPLAYER, MSGL_V, "Timeline contains %d parts from %d "
           "sources. Total length %.3f seconds.\n", part_count, num_sources,
           timeline[part_count].start);
    if (missing_time)
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "There are %.3f seconds missing "
               "from the timeline!\n", missing_time / 1e9);
    mp_msg(MSGT_CPLAYER, MSGL_V, "Source files:\n");
    for (int i = 0; i < num_sources; i++)
        mp_msg(MSGT_CPLAYER, MSGL_V, "%d: %s\n", i,
               filename_recode(sources[i].demuxer->filename));
    mp_msg(MSGT_CPLAYER, MSGL_V, "Timeline parts: (number, start, "
           "source_start, source):\n");
    for (int i = 0; i < part_count; i++) {
        struct timeline_part *p = timeline + i;
        mp_msg(MSGT_CPLAYER, MSGL_V, "%3d %9.3f %9.3f %3td\n", i, p->start,
               p->source_start, p->source - sources);
    }
    mp_msg(MSGT_CPLAYER, MSGL_V, "END %9.3f\n", timeline[part_count].start);
    mpctx->sources = sources;
    mpctx->num_sources = num_sources;
    mpctx->timeline = timeline;
    mpctx->num_timeline_parts = part_count;
    mpctx->num_chapters = num_chapters;
    mpctx->chapters = chapters;

    mpctx->timeline_part = 0;
    mpctx->demuxer = timeline[0].source->demuxer;
}
