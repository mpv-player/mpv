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
#include <assert.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libavutil/common.h>

#include "osdep/io.h"

#include "talloc.h"

#include "mpvcore/mp_core.h"
#include "mpvcore/mp_msg.h"
#include "demux/demux.h"
#include "mpvcore/path.h"
#include "mpvcore/bstr.h"
#include "mpvcore/mp_common.h"
#include "stream/stream.h"

struct find_entry {
    char *name;
    int matchlen;
    off_t size;
};

static int cmp_entry(const void *pa, const void *pb)
{
    const struct find_entry *a = pa, *b = pb;
    // check "similar" filenames first
    int matchdiff = b->matchlen - a->matchlen;
    if (matchdiff)
        return FFSIGN(matchdiff);
    // check small files first
    off_t sizediff = a->size - b->size;
    if (sizediff)
        return FFSIGN(sizediff);
    return 0;
}

static char **find_files(const char *original_file, const char *suffix)
{
    void *tmpmem = talloc_new(NULL);
    char *basename = mp_basename(original_file);
    struct bstr directory = mp_dirname(original_file);
    char **results = talloc_size(NULL, 0);
    char *dir_zero = bstrdup0(tmpmem, directory);
    DIR *dp = opendir(dir_zero);
    if (!dp) {
        talloc_free(tmpmem);
        return results;
    }
    struct find_entry *entries = NULL;
    struct dirent *ep;
    int num_results = 0;
    while ((ep = readdir(dp))) {
        int suffix_offset = strlen(ep->d_name) - strlen(suffix);
        // name must end with suffix
        if (suffix_offset < 0 || strcmp(ep->d_name + suffix_offset, suffix))
            continue;
        // don't list the original name
        if (!strcmp(ep->d_name, basename))
            continue;

        char *name = mp_path_join(results, directory, bstr0(ep->d_name));
        char *s1 = ep->d_name;
        char *s2 = basename;
        int matchlen = 0;
        while (*s1 && *s1++ == *s2++)
            matchlen++;
        // be a bit more fuzzy about matching the filename
        matchlen = (matchlen + 3) / 5;

        struct stat statbuf;
        if (stat(name, &statbuf) != 0)
            continue;
        off_t size = statbuf.st_size;

        entries = talloc_realloc(tmpmem, entries, struct find_entry,
                                 num_results + 1);
        entries[num_results] = (struct find_entry) { name, matchlen, size };
        num_results++;
    }
    closedir(dp);
    // NOTE: maybe should make it compare pointers instead
    if (entries)
        qsort(entries, num_results, sizeof(struct find_entry), cmp_entry);
    results = talloc_realloc(NULL, results, char *, num_results);
    for (int i = 0; i < num_results; i++) {
        results[i] = entries[i].name;
    }
    talloc_free(tmpmem);
    return results;
}

static int enable_cache(struct MPContext *mpctx, struct stream **stream,
                        struct demuxer **demuxer, struct demuxer_params *params)
{
    struct MPOpts *opts = mpctx->opts;

    if (opts->stream_cache_size <= 0)
        return 0;

    char *filename = talloc_strdup(NULL, (*demuxer)->filename);
    free_demuxer(*demuxer);
    free_stream(*stream);

    *stream = stream_open(filename, opts);
    if (!*stream) {
        talloc_free(filename);
        return -1;
    }

    stream_enable_cache_percent(stream,
                                opts->stream_cache_size,
                                opts->stream_cache_def_size,
                                opts->stream_cache_min_percent,
                                opts->stream_cache_seek_min_percent);

    *demuxer = demux_open(*stream, "mkv", params, opts);
    if (!*demuxer) {
        talloc_free(filename);
        free_stream(*stream);
        return -1;
    }

    talloc_free(filename);
    return 1;
}

// segment = get Nth segment of a multi-segment file
static bool check_file_seg(struct MPContext *mpctx, struct demuxer **sources,
                           int num_sources, struct matroska_segment_uid *uids,
                           char *filename, int segment)
{
    bool was_valid = false;
    struct demuxer_params params = {
        .matroska_num_wanted_uids = num_sources,
        .matroska_wanted_uids = uids,
        .matroska_wanted_segment = segment,
        .matroska_was_valid = &was_valid,
    };
    struct stream *s = stream_open(filename, mpctx->opts);
    if (!s)
        return false;
    struct demuxer *d = demux_open(s, "mkv", &params, mpctx->opts);

    if (!d) {
        free_stream(s);
        return was_valid;
    }
    if (d->type == DEMUXER_TYPE_MATROSKA) {
        struct matroska_data *m = &d->matroska_data;
        for (int i = 1; i < num_sources; i++) {
            struct matroska_segment_uid *uid = uids + i;
            if (sources[i])
                continue;
            /* Accept the source if the segment uid matches and the edition
             * either matches or isn't specified. */
            if (!memcmp(uid->segment, m->uid.segment, 16) &&
                (!uid->edition || uid->edition == m->uid.edition)) {
                mp_msg(MSGT_CPLAYER, MSGL_INFO, "Match for source %d: %s\n",
                       i, d->filename);

                if (enable_cache(mpctx, &s, &d, &params) < 0)
                    continue;

                sources[i] = d;
                return true;
            }
        }
    }
    free_demuxer(d);
    free_stream(s);
    return was_valid;
}

static void check_file(struct MPContext *mpctx, struct demuxer **sources,
                       int num_sources, struct matroska_segment_uid *uids,
                       char *filename, int first)
{
    for (int segment = first; ; segment++) {
        if (!check_file_seg(mpctx, sources, num_sources,
                            uids, filename, segment))
            break;
    }
}

static bool missing(struct demuxer **sources, int num_sources)
{
    for (int i = 0; i < num_sources; i++) {
        if (!sources[i])
            return true;
    }
    return false;
}

static int find_ordered_chapter_sources(struct MPContext *mpctx,
                                        struct demuxer **sources,
                                        int num_sources,
                                        struct matroska_segment_uid *uids)
{
    int num_filenames = 0;
    char **filenames = NULL;
    if (num_sources > 1) {
        char *main_filename = mpctx->demuxer->filename;
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "This file references data from "
               "other sources.\n");
        if (mpctx->demuxer->stream->uncached_type != STREAMTYPE_FILE) {
            mp_msg(MSGT_CPLAYER, MSGL_WARN, "Playback source is not a "
                   "normal disk file. Will not search for related files.\n");
        } else {
            mp_msg(MSGT_CPLAYER, MSGL_INFO, "Will scan other files in the "
                   "same directory to find referenced sources.\n");
            filenames = find_files(main_filename, ".mkv");
            num_filenames = MP_TALLOC_ELEMS(filenames);
        }
        // Possibly get further segments appended to the first segment
        check_file(mpctx, sources, num_sources, uids, main_filename, 1);
    }

    for (int i = 0; i < num_filenames; i++) {
        if (!missing(sources, num_sources))
            break;
        mp_msg(MSGT_CPLAYER, MSGL_INFO, "Checking file %s\n", filenames[i]);
        check_file(mpctx, sources, num_sources, uids, filenames[i], 0);
    }

    talloc_free(filenames);
    if (missing(sources, num_sources)) {
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "Failed to find ordered chapter part!\n"
               "There will be parts MISSING from the video!\n");
        int j = 1;
        for (int i = 1; i < num_sources; i++)
            if (sources[i]) {
                struct matroska_segment_uid *source_uid = uids + i;
                struct matroska_segment_uid *target_uid = uids + j;
                sources[j] = sources[i];
                memcpy(target_uid, source_uid, sizeof(*source_uid));
                j++;
            }
        num_sources = j;
    }
    return num_sources;
}

void build_ordered_chapter_timeline(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

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
    struct demuxer **sources = talloc_array_ptrtype(NULL, sources,
                                                    m->num_ordered_chapters+1);
    sources[0] = mpctx->demuxer;
    struct matroska_segment_uid *uids = talloc_array_ptrtype(NULL, uids,
                                                    m->num_ordered_chapters + 1);
    int num_sources = 1;
    memcpy(uids[0].segment, m->uid.segment, 16);
    uids[0].edition = 0;

    for (int i = 0; i < m->num_ordered_chapters; i++) {
        struct matroska_chapter *c = m->ordered_chapters + i;
        if (!c->has_segment_uid)
            memcpy(c->uid.segment, m->uid.segment, 16);

        for (int j = 0; j < num_sources; j++)
            /* If there isn't a segment uid, we are the source. If the segment
             * uid is our segment uid and the edition matches. We can't accept
             * the "don't care" edition value of 0 since the user may have
             * requested a non-default edition. */
            if (demux_matroska_uid_cmp(&c->uid, uids + j))
                goto found1;
        memcpy(uids + num_sources, &c->uid, sizeof(c->uid));
        sources[num_sources] = NULL;
        num_sources++;
    found1:
        ;
    }

    num_sources = find_ordered_chapter_sources(mpctx, sources, num_sources,
                                               uids);

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
            if (demux_matroska_uid_cmp(&c->uid, uids + j))
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
            || sources[j] != timeline[part_count - 1].source) {
            timeline[part_count].source = sources[j];
            timeline[part_count].start = starttime / 1e9;
            timeline[part_count].source_start = c->start / 1e9;
            prev_part_offset = c->start - starttime;
            part_count++;
        } else if (part_count > 0 && join_diff) {
            /* Chapter was merged at an inexact boundary;
             * adjust timestamps to match. */
            mp_msg(MSGT_CPLAYER, MSGL_V, "Merging timeline part %d with "
                   "offset %g ms.\n", i, join_diff / 1e6);
            starttime += join_diff;
        }
        chapters[num_chapters].start = starttime / 1e9;
        chapters[num_chapters].name = talloc_strdup(chapters, c->name);
        starttime += c->end - c->start;
        num_chapters++;
    }
    timeline[part_count].start = starttime / 1e9;
    talloc_free(uids);

    if (!part_count) {
        // None of the parts come from the file itself???
        talloc_free(sources);
        talloc_free(timeline);
        talloc_free(chapters);
        return;
    }

    if (missing_time)
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "There are %.3f seconds missing "
               "from the timeline!\n", missing_time / 1e9);
    talloc_free(mpctx->sources);
    mpctx->sources = sources;
    mpctx->num_sources = num_sources;
    mpctx->timeline = timeline;
    mpctx->num_timeline_parts = part_count;
    mpctx->num_chapters = num_chapters;
    mpctx->chapters = chapters;
}
