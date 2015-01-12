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
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libavutil/common.h>

#include "osdep/io.h"

#include "talloc.h"

#include "player/core.h"
#include "common/msg.h"
#include "demux/demux.h"
#include "options/path.h"
#include "misc/bstr.h"
#include "common/common.h"
#include "common/playlist.h"
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

static bool test_matroska_ext(const char *filename)
{
    static const char *const exts[] = {".mkv", ".mka", ".mks", ".mk3d", NULL};
    for (int n = 0; exts[n]; n++) {
        const char *suffix = exts[n];
        int offset = strlen(filename) - strlen(suffix);
        // name must end with suffix
        if (offset > 0 && strcasecmp(filename + offset, suffix) == 0)
            return true;
    }
    return false;
}

static char **find_files(const char *original_file)
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
        if (!test_matroska_ext(ep->d_name))
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

    if (!stream_wants_cache(*stream, &opts->stream_cache))
        return 0;

    char *filename = talloc_strdup(NULL, (*demuxer)->filename);
    free_demuxer(*demuxer);
    free_stream(*stream);

    *stream = stream_open(filename, mpctx->global);
    if (!*stream) {
        talloc_free(filename);
        return -1;
    }

    stream_enable_cache(stream, &opts->stream_cache);

    *demuxer = demux_open(*stream, "mkv", params, mpctx->global);
    if (!*demuxer) {
        talloc_free(filename);
        free_stream(*stream);
        return -1;
    }

    talloc_free(filename);
    return 1;
}

static bool has_source_request(struct matroska_segment_uid *uids,
                               int num_sources,
                               struct matroska_segment_uid *new_uid)
{
    for (int i = 0; i < num_sources; ++i) {
        if (demux_matroska_uid_cmp(uids + i, new_uid))
            return true;
    }

    return false;
}

// segment = get Nth segment of a multi-segment file
static bool check_file_seg(struct MPContext *mpctx, struct demuxer ***sources,
                           int *num_sources, struct matroska_segment_uid **uids,
                           char *filename, int segment)
{
    bool was_valid = false;
    struct demuxer_params params = {
        .matroska_num_wanted_uids = *num_sources,
        .matroska_wanted_uids = *uids,
        .matroska_wanted_segment = segment,
        .matroska_was_valid = &was_valid,
    };
    struct stream *s = stream_open(filename, mpctx->global);
    if (!s)
        return false;
    struct demuxer *d = demux_open(s, "mkv", &params, mpctx->global);

    if (!d) {
        free_stream(s);
        return was_valid;
    }
    if (d->type == DEMUXER_TYPE_MATROSKA) {
        struct matroska_data *m = &d->matroska_data;

        for (int i = 1; i < *num_sources; i++) {
            struct matroska_segment_uid *uid = *uids + i;
            if ((*sources)[i])
                continue;
            /* Accept the source if the segment uid matches and the edition
             * either matches or isn't specified. */
            if (!memcmp(uid->segment, m->uid.segment, 16) &&
                (!uid->edition || uid->edition == m->uid.edition))
            {
                MP_INFO(mpctx, "Match for source %d: %s\n", i, d->filename);

                for (int j = 0; j < m->num_ordered_chapters; j++) {
                    struct matroska_chapter *c = m->ordered_chapters + j;

                    if (!c->has_segment_uid)
                        continue;

                    if (has_source_request(*uids, *num_sources, &c->uid))
                        continue;

                    /* Set the requested segment. */
                    MP_TARRAY_GROW(NULL, *uids, *num_sources);
                    (*uids)[*num_sources] = c->uid;

                    /* Add a new source slot. */
                    MP_TARRAY_APPEND(NULL, *sources, *num_sources, NULL);
                }

                if (enable_cache(mpctx, &s, &d, &params) < 0)
                    continue;

                (*sources)[i] = d;
                return true;
            }
        }
    }
    free_demuxer(d);
    free_stream(s);
    return was_valid;
}

static void check_file(struct MPContext *mpctx, struct demuxer ***sources,
                       int *num_sources, struct matroska_segment_uid **uids,
                       char *filename, int first)
{
    for (int segment = first; ; segment++) {
        if (!check_file_seg(mpctx, sources, num_sources, uids, filename, segment))
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
                                        struct demuxer ***sources,
                                        int *num_sources,
                                        struct matroska_segment_uid **uids)
{
    struct MPOpts *opts = mpctx->opts;
    void *tmp = talloc_new(NULL);
    int num_filenames = 0;
    char **filenames = NULL;
    if (*num_sources > 1) {
        char *main_filename = mpctx->demuxer->filename;
        MP_INFO(mpctx, "This file references data from other sources.\n");
        if (opts->ordered_chapters_files && opts->ordered_chapters_files[0]) {
            MP_INFO(mpctx, "Loading references from '%s'.\n",
                    opts->ordered_chapters_files);
            struct playlist *pl =
                playlist_parse_file(opts->ordered_chapters_files, mpctx->global);
            talloc_steal(tmp, pl);
            for (struct playlist_entry *e = pl->first; e; e = e->next)
                MP_TARRAY_APPEND(tmp, filenames, num_filenames, e->filename);
        } else if (mpctx->demuxer->stream->uncached_type != STREAMTYPE_FILE) {
            MP_WARN(mpctx, "Playback source is not a "
                    "normal disk file. Will not search for related files.\n");
        } else {
            MP_INFO(mpctx, "Will scan other files in the "
                    "same directory to find referenced sources.\n");
            filenames = find_files(main_filename);
            num_filenames = MP_TALLOC_ELEMS(filenames);
            talloc_steal(tmp, filenames);
        }
        // Possibly get further segments appended to the first segment
        check_file(mpctx, sources, num_sources, uids, main_filename, 1);
    }

    int old_source_count;
    do {
        old_source_count = *num_sources;
        for (int i = 0; i < num_filenames; i++) {
            if (!missing(*sources, *num_sources))
                break;
            MP_INFO(mpctx, "Checking file %s\n", filenames[i]);
            check_file(mpctx, sources, num_sources, uids, filenames[i], 0);
        }
    } while (old_source_count != *num_sources);

    if (missing(*sources, *num_sources)) {
        MP_ERR(mpctx, "Failed to find ordered chapter part!\n");
        int j = 1;
        for (int i = 1; i < *num_sources; i++) {
            if ((*sources)[i]) {
                struct matroska_segment_uid *source_uid = *uids + i;
                struct matroska_segment_uid *target_uid = *uids + j;
                (*sources)[j] = (*sources)[i];
                memmove(target_uid, source_uid, sizeof(*source_uid));
                j++;
            }
        }
        *num_sources = j;
    }

    talloc_free(tmp);
    return *num_sources;
}

static int64_t add_timeline_part(struct MPContext *mpctx,
                                 struct demuxer *source,
                                 struct timeline_part **timeline,
                                 int *part_count,
                                 uint64_t start,
                                 uint64_t *last_end_time,
                                 uint64_t *starttime)
{
    /* Merge directly adjacent parts. We allow for a configurable fudge factor
     * because of files which specify chapter end times that are one frame too
     * early; we don't want to try seeking over a one frame gap. */
    int64_t join_diff = start - *last_end_time;
    if (*part_count == 0
        || FFABS(join_diff) > mpctx->opts->chapter_merge_threshold * 1e6
        || source != (*timeline)[*part_count - 1].source)
    {
        struct timeline_part new = {
            .start = *starttime / 1e9,
            .source_start = start / 1e9,
            .source = source,
        };
        MP_TARRAY_APPEND(NULL, *timeline, *part_count, new);
    } else if (*part_count > 0 && join_diff) {
        // Chapter was merged at an inexact boundary; adjust timestamps to match.
        MP_VERBOSE(mpctx, "Merging timeline part %d with offset %g ms.\n",
                   *part_count, join_diff / 1e6);
        *starttime += join_diff;
        return join_diff;
    }

    return 0;
}

static void build_timeline_loop(struct MPContext *mpctx,
                                struct demuxer **sources,
                                int num_sources,
                                int current_source,
                                uint64_t *starttime,
                                uint64_t *missing_time,
                                uint64_t *last_end_time,
                                struct timeline_part **timeline,
                                struct demux_chapter *chapters,
                                int num_chapters,
                                int *part_count,
                                uint64_t skip,
                                uint64_t limit)
{
    uint64_t local_starttime = 0;
    struct demuxer *source = sources[current_source];
    struct matroska_data *m = &source->matroska_data;

    for (int i = 0; i < m->num_ordered_chapters; i++) {
        struct matroska_chapter *c = m->ordered_chapters + i;
        uint64_t chapter_length = c->end - c->start;

        if (!c->has_segment_uid)
            c->uid = m->uid;

        local_starttime += chapter_length;

        // If we're before the start time for the chapter, skip to the next one.
        if (local_starttime <= skip)
            continue;

        /* Look for the source for this chapter. */
        for (int j = 0; j < num_sources; j++) {
            struct demuxer *linked_source = sources[j];
            struct matroska_data *linked_m = &linked_source->matroska_data;

            if (!demux_matroska_uid_cmp(&c->uid, &linked_m->uid))
                continue;

            if (i >= num_chapters)
                break; // probably needed only for broken sources

            if (!limit) {
                chapters[i].pts = *starttime / 1e9;
                chapters[i].name = talloc_strdup(chapters, c->name);
            }

            /* If we're the source or it's a non-ordered edition reference,
             * just add a timeline part from the source. */
            if (current_source == j || !linked_m->num_ordered_chapters) {
                uint64_t source_full_length =
                    demuxer_get_time_length(linked_source) * 1e9;
                uint64_t source_length = source_full_length - c->start;
                int64_t join_diff = 0;

                /* If the chapter starts after the end of a source, there's
                 * nothing we can get from it. Instead, mark the entire chapter
                 * as missing and make the chapter length 0. */
                if (source_full_length <= c->start) {
                    *missing_time += chapter_length;
                    chapter_length = 0;
                    goto found;
                }

                /* If the source length starting at the chapter start is
                 * shorter than the chapter it is supposed to fill, add the gap
                 * to missing_time. Also, modify the chapter length to be what
                 * we actually have to avoid playing off the end of the file
                 * and not switching to the next source. */
                if (source_length < chapter_length) {
                    *missing_time += chapter_length - source_length;
                    chapter_length = source_length;
                }

                join_diff = add_timeline_part(mpctx, linked_source, timeline, part_count,
                                              c->start, last_end_time, starttime);

                /* If we merged two chapters into a single part due to them
                 * being off by a few frames, we need to change the limit to
                 * avoid chopping the end of the intended chapter (the adding
                 * frames case) or showing extra content (the removing frames
                 * case). Also update chapter_length to incorporate the extra
                 * time. */
                if (limit) {
                    limit += join_diff;
                    chapter_length += join_diff;
                }
            } else {
                /* We have an ordered edition as the source. Since this
                 * can jump around all over the place, we need to build up the
                 * timeline parts for each of its chapters, but not add them as
                 * chapters. */
                build_timeline_loop(mpctx, sources, num_sources, j, starttime,
                                    missing_time, last_end_time, timeline,
                                    chapters, num_chapters, part_count,
                                    c->start, c->end);
                // Already handled by the loop call.
                chapter_length = 0;
            }
            *last_end_time = c->end;
            goto found;
        }

        *missing_time += chapter_length;
        chapter_length = 0;
    found:;
        *starttime += chapter_length;
        /* If we're after the limit on this chapter, stop here. */
        if (limit && local_starttime >= limit) {
            /* Back up the global start time by the overflow. */
            *starttime -= local_starttime - limit;
            break;
        }
    }

    /* If we stopped before the limit, add up the missing time. */
    if (local_starttime < limit)
        *missing_time += limit - local_starttime;
}

void build_ordered_chapter_timeline(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    if (!opts->ordered_chapters) {
        MP_INFO(mpctx, "File uses ordered chapters, but "
               "you have disabled support for them. Ignoring.\n");
        return;
    }

    MP_INFO(mpctx, "File uses ordered chapters, will build "
           "edit timeline.\n");

    struct demuxer *demuxer = mpctx->demuxer;
    struct matroska_data *m = &demuxer->matroska_data;

    // +1 because sources/uid_map[0] is original file even if all chapters
    // actually use other sources and need separate entries
    struct demuxer **sources = talloc_zero_array(NULL, struct demuxer *,
                                                    m->num_ordered_chapters+1);
    sources[0] = mpctx->demuxer;
    struct matroska_segment_uid *uids =
        talloc_zero_array(NULL, struct matroska_segment_uid,
                          m->num_ordered_chapters + 1);
    int num_sources = 1;
    memcpy(uids[0].segment, m->uid.segment, 16);
    uids[0].edition = 0;

    for (int i = 0; i < m->num_ordered_chapters; i++) {
        struct matroska_chapter *c = m->ordered_chapters + i;
        /* If there isn't a segment uid, we are the source. If the segment uid
         * is our segment uid and the edition matches. We can't accept the
         * "don't care" edition value of 0 since the user may have requested a
         * non-default edition. */
        if (!c->has_segment_uid || demux_matroska_uid_cmp(&c->uid, &m->uid))
            continue;

        if (has_source_request(uids, num_sources, &c->uid))
            continue;

        memcpy(uids + num_sources, &c->uid, sizeof(c->uid));
        sources[num_sources] = NULL;
        num_sources++;
    }

    num_sources = find_ordered_chapter_sources(mpctx, &sources, &num_sources,
                                               &uids);
    talloc_free(uids);

    struct timeline_part *timeline = talloc_array_ptrtype(NULL, timeline, 0);
    struct demux_chapter *chapters =
        talloc_zero_array(NULL, struct demux_chapter, m->num_ordered_chapters);
    // Stupid hack, because fuck everything.
    for (int n = 0; n < m->num_ordered_chapters; n++)
        chapters[n].pts = -1;
    uint64_t starttime = 0;
    uint64_t missing_time = 0;
    uint64_t last_end_time = 0;
    int part_count = 0;
    build_timeline_loop(mpctx, sources, num_sources, 0, &starttime,
                        &missing_time, &last_end_time, &timeline,
                        chapters, m->num_ordered_chapters, &part_count, 0, 0);

    // Fuck everything (2): filter out all "unset" chapters.
    for (int n = m->num_ordered_chapters - 1; n >= 0; n--) {
        if (chapters[n].pts == -1)
            MP_TARRAY_REMOVE_AT(chapters, m->num_ordered_chapters, n);
    }

    if (!part_count) {
        // None of  the parts come from the file itself???
        // Broken file, but we need at least 1 valid timeline part - add a dummy.
        MP_WARN(mpctx, "Ordered chapters file with no parts?\n");
        struct timeline_part new = {
            .source = demuxer,
        };
        MP_TARRAY_APPEND(NULL, timeline, part_count, new);
    }

    struct timeline_part new = {
        .start = starttime / 1e9,
    };
    MP_TARRAY_APPEND(NULL, timeline, part_count, new);

    /* Ignore anything less than a millisecond when reporting missing time. If
     * users really notice less than a millisecond missing, maybe this can be
     * revisited. */
    if (missing_time >= 1e6) {
        MP_ERR(mpctx, "There are %.3f seconds missing from the timeline!\n",
               missing_time / 1e9);
    }
    talloc_free(mpctx->sources);
    mpctx->sources = sources;
    mpctx->num_sources = num_sources;
    mpctx->timeline = timeline;
    mpctx->num_timeline_parts = part_count - 1;
    mpctx->num_chapters = m->num_ordered_chapters;
    mpctx->chapters = chapters;
}
