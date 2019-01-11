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

#include "mpv_talloc.h"

#include "common/msg.h"
#include "demux/demux.h"
#include "demux/timeline.h"
#include "demux/matroska.h"
#include "options/m_config.h"
#include "options/options.h"
#include "options/path.h"
#include "misc/bstr.h"
#include "misc/thread_tools.h"
#include "common/common.h"
#include "common/playlist.h"
#include "stream/stream.h"

struct tl_ctx {
    struct mp_log *log;
    struct mpv_global *global;
    struct MPOpts *opts;
    struct timeline *tl;

    struct demuxer *demuxer;

    struct demuxer **sources;
    int num_sources;

    struct timeline_part *timeline;
    int num_parts;

    struct matroska_segment_uid *uids;
    uint64_t start_time; // When the next part should start on the complete timeline.
    uint64_t missing_time; // Total missing time so far.
    uint64_t last_end_time; // When the last part ended on the complete timeline.
    int num_chapters; // Total number of expected chapters.
};

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

        char *name = mp_path_join_bstr(results, directory, bstr0(ep->d_name));
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

static bool has_source_request(struct tl_ctx *ctx,
                               struct matroska_segment_uid *new_uid)
{
    for (int i = 0; i < ctx->num_sources; ++i) {
        if (demux_matroska_uid_cmp(&ctx->uids[i], new_uid))
            return true;
    }
    return false;
}

// segment = get Nth segment of a multi-segment file
static bool check_file_seg(struct tl_ctx *ctx, char *filename, int segment)
{
    bool was_valid = false;
    struct demuxer_params params = {
        .force_format = "mkv",
        .matroska_num_wanted_uids = ctx->num_sources,
        .matroska_wanted_uids = ctx->uids,
        .matroska_wanted_segment = segment,
        .matroska_was_valid = &was_valid,
        .disable_timeline = true,
    };
    struct mp_cancel *cancel = ctx->tl->cancel;
    if (mp_cancel_test(cancel))
        return false;

    struct demuxer *d = demux_open_url(filename, &params, cancel, ctx->global);
    if (!d)
        return false;

    struct matroska_data *m = &d->matroska_data;

    for (int i = 1; i < ctx->num_sources; i++) {
        struct matroska_segment_uid *uid = &ctx->uids[i];
        if (ctx->sources[i])
            continue;
        /* Accept the source if the segment uid matches and the edition
         * either matches or isn't specified. */
        if (!memcmp(uid->segment, m->uid.segment, 16) &&
            (!uid->edition || uid->edition == m->uid.edition))
        {
            MP_INFO(ctx, "Match for source %d: %s\n", i, d->filename);

            if (!uid->edition) {
                m->uid.edition = 0;
            } else {
                for (int j = 0; j < m->num_ordered_chapters; j++) {
                    struct matroska_chapter *c = m->ordered_chapters + j;

                    if (!c->has_segment_uid)
                        continue;

                    if (has_source_request(ctx, &c->uid))
                        continue;

                    /* Set the requested segment. */
                    MP_TARRAY_GROW(NULL, ctx->uids, ctx->num_sources);
                    ctx->uids[ctx->num_sources] = c->uid;

                    /* Add a new source slot. */
                    MP_TARRAY_APPEND(NULL, ctx->sources, ctx->num_sources, NULL);
                }
            }

            ctx->sources[i] = d;
            return true;
        }
    }

    demux_free(d);
    return was_valid;
}

static void check_file(struct tl_ctx *ctx, char *filename, int first)
{
    for (int segment = first; ; segment++) {
        if (!check_file_seg(ctx, filename, segment))
            break;
    }
}

static bool missing(struct tl_ctx *ctx)
{
    for (int i = 0; i < ctx->num_sources; i++) {
        if (!ctx->sources[i])
            return true;
    }
    return false;
}

static void find_ordered_chapter_sources(struct tl_ctx *ctx)
{
    struct MPOpts *opts = ctx->opts;
    void *tmp = talloc_new(NULL);
    int num_filenames = 0;
    char **filenames = NULL;
    if (ctx->num_sources > 1) {
        char *main_filename = ctx->demuxer->filename;
        MP_INFO(ctx, "This file references data from other sources.\n");
        if (opts->ordered_chapters_files && opts->ordered_chapters_files[0]) {
            MP_INFO(ctx, "Loading references from '%s'.\n",
                    opts->ordered_chapters_files);
            struct playlist *pl =
                playlist_parse_file(opts->ordered_chapters_files,
                                    ctx->tl->cancel, ctx->global);
            talloc_steal(tmp, pl);
            for (struct playlist_entry *e = pl ? pl->first : NULL; e; e = e->next)
                MP_TARRAY_APPEND(tmp, filenames, num_filenames, e->filename);
        } else if (!ctx->demuxer->stream->is_local_file) {
            MP_WARN(ctx, "Playback source is not a "
                    "normal disk file. Will not search for related files.\n");
        } else {
            MP_INFO(ctx, "Will scan other files in the "
                    "same directory to find referenced sources.\n");
            filenames = find_files(main_filename);
            num_filenames = MP_TALLOC_AVAIL(filenames);
            talloc_steal(tmp, filenames);
        }
        // Possibly get further segments appended to the first segment
        check_file(ctx, main_filename, 1);
    }

    int old_source_count;
    do {
        old_source_count = ctx->num_sources;
        for (int i = 0; i < num_filenames; i++) {
            if (!missing(ctx))
                break;
            MP_VERBOSE(ctx, "Checking file %s\n", filenames[i]);
            check_file(ctx, filenames[i], 0);
        }
    } while (old_source_count != ctx->num_sources);

    if (missing(ctx)) {
        MP_ERR(ctx, "Failed to find ordered chapter part!\n");
        int j = 1;
        for (int i = 1; i < ctx->num_sources; i++) {
            if (ctx->sources[i]) {
                ctx->sources[j] = ctx->sources[i];
                ctx->uids[j] = ctx->uids[i];
                j++;
            }
        }
        ctx->num_sources = j;
    }

    // Copy attachments from referenced sources so fonts are loaded for sub
    // rendering.
    for (int i = 1; i < ctx->num_sources; i++) {
        for (int j = 0; j < ctx->sources[i]->num_attachments; j++) {
            struct demux_attachment *att = &ctx->sources[i]->attachments[j];
            demuxer_add_attachment(ctx->demuxer, att->name, att->type,
                                   att->data, att->data_size);
        }
    }

    talloc_free(tmp);
}

struct inner_timeline_info {
    uint64_t skip; // Amount of time to skip.
    uint64_t limit; // How much time is expected for the parent chapter.
};

static int64_t add_timeline_part(struct tl_ctx *ctx,
                                 struct demuxer *source,
                                 uint64_t start)
{
    /* Merge directly adjacent parts. We allow for a configurable fudge factor
     * because of files which specify chapter end times that are one frame too
     * early; we don't want to try seeking over a one frame gap. */
    int64_t join_diff = start - ctx->last_end_time;
    if (ctx->num_parts == 0
        || FFABS(join_diff) > ctx->opts->chapter_merge_threshold * 1e6
        || source != ctx->timeline[ctx->num_parts - 1].source)
    {
        struct timeline_part new = {
            .start = ctx->start_time / 1e9,
            .source_start = start / 1e9,
            .source = source,
        };
        MP_TARRAY_APPEND(NULL, ctx->timeline, ctx->num_parts, new);
    } else if (ctx->num_parts > 0 && join_diff) {
        // Chapter was merged at an inexact boundary; adjust timestamps to match.
        MP_VERBOSE(ctx, "Merging timeline part %d with offset %g ms.\n",
                   ctx->num_parts, join_diff / 1e6);
        ctx->start_time += join_diff;
        return join_diff;
    }

    return 0;
}

static void build_timeline_loop(struct tl_ctx *ctx,
                                struct demux_chapter *chapters,
                                struct inner_timeline_info *info,
                                int current_source)
{
    uint64_t local_starttime = 0;
    struct demuxer *source = ctx->sources[current_source];
    struct matroska_data *m = &source->matroska_data;

    for (int i = 0; i < m->num_ordered_chapters; i++) {
        struct matroska_chapter *c = m->ordered_chapters + i;
        uint64_t chapter_length = c->end - c->start;

        if (!c->has_segment_uid)
            c->uid = m->uid;

        local_starttime += chapter_length;

        // If we're before the start time for the chapter, skip to the next one.
        if (local_starttime <= info->skip)
            continue;

        /* Look for the source for this chapter. */
        for (int j = 0; j < ctx->num_sources; j++) {
            struct demuxer *linked_source = ctx->sources[j];
            struct matroska_data *linked_m = &linked_source->matroska_data;

            if (!demux_matroska_uid_cmp(&c->uid, &linked_m->uid))
                continue;

            if (!info->limit) {
                if (i >= ctx->num_chapters)
                    break; // malformed files can cause this to happen.

                chapters[i].pts = ctx->start_time / 1e9;
                chapters[i].metadata = talloc_zero(chapters, struct mp_tags);
                mp_tags_set_str(chapters[i].metadata, "title", c->name);
            }

            /* If we're the source or it's a non-ordered edition reference,
             * just add a timeline part from the source. */
            if (current_source == j || !linked_m->uid.edition) {
                uint64_t source_full_length = linked_source->duration * 1e9;
                uint64_t source_length = source_full_length - c->start;
                int64_t join_diff = 0;

                /* If the chapter starts after the end of a source, there's
                 * nothing we can get from it. Instead, mark the entire chapter
                 * as missing and make the chapter length 0. */
                if (source_full_length <= c->start) {
                    ctx->missing_time += chapter_length;
                    chapter_length = 0;
                    goto found;
                }

                /* If the source length starting at the chapter start is
                 * shorter than the chapter it is supposed to fill, add the gap
                 * to missing_time. Also, modify the chapter length to be what
                 * we actually have to avoid playing off the end of the file
                 * and not switching to the next source. */
                if (source_length < chapter_length) {
                    ctx->missing_time += chapter_length - source_length;
                    chapter_length = source_length;
                }

                join_diff = add_timeline_part(ctx, linked_source, c->start);

                /* If we merged two chapters into a single part due to them
                 * being off by a few frames, we need to change the limit to
                 * avoid chopping the end of the intended chapter (the adding
                 * frames case) or showing extra content (the removing frames
                 * case). Also update chapter_length to incorporate the extra
                 * time. */
                if (info->limit) {
                    info->limit += join_diff;
                    chapter_length += join_diff;
                }
            } else {
                /* We have an ordered edition as the source. Since this
                 * can jump around all over the place, we need to build up the
                 * timeline parts for each of its chapters, but not add them as
                 * chapters. */
                struct inner_timeline_info new_info = {
                    .skip = c->start,
                    .limit = c->end
                };
                build_timeline_loop(ctx, chapters, &new_info, j);
                // Already handled by the loop call.
                chapter_length = 0;
            }
            ctx->last_end_time = c->end;
            goto found;
        }

        ctx->missing_time += chapter_length;
        chapter_length = 0;
    found:;
        ctx->start_time += chapter_length;
        /* If we're after the limit on this chapter, stop here. */
        if (info->limit && local_starttime >= info->limit) {
            /* Back up the global start time by the overflow. */
            ctx->start_time -= local_starttime - info->limit;
            break;
        }
    }

    /* If we stopped before the limit, add up the missing time. */
    if (local_starttime < info->limit)
        ctx->missing_time += info->limit - local_starttime;
}

static void check_track_compatibility(struct tl_ctx *tl, struct demuxer *mainsrc)
{
    for (int n = 0; n < tl->num_parts; n++) {
        struct timeline_part *p = &tl->timeline[n];
        if (p->source == mainsrc)
            continue;

        int num_source_streams = demux_get_num_stream(p->source);
        for (int i = 0; i < num_source_streams; i++) {
            struct sh_stream *s = demux_get_stream(p->source, i);
            if (s->attached_picture)
                continue;

            if (!demuxer_stream_by_demuxer_id(mainsrc, s->type, s->demuxer_id)) {
                MP_WARN(tl, "Source %s has %s stream with TID=%d, which "
                            "is not present in the ordered chapters main "
                            "file. This is a broken file. "
                            "The additional stream is ignored.\n",
                            p->source->filename, stream_type_name(s->type),
                            s->demuxer_id);
            }
        }

        int num_main_streams = demux_get_num_stream(mainsrc);
        for (int i = 0; i < num_main_streams; i++) {
            struct sh_stream *m = demux_get_stream(mainsrc, i);
            if (m->attached_picture)
                continue;

            struct sh_stream *s =
                demuxer_stream_by_demuxer_id(p->source, m->type, m->demuxer_id);
            if (s) {
                // There are actually many more things that in theory have to
                // match (though mpv's implementation doesn't care).
                if (strcmp(s->codec->codec, m->codec->codec) != 0)
                    MP_WARN(tl, "Timeline segments have mismatching codec.\n");
            } else {
                MP_WARN(tl, "Source %s lacks %s stream with TID=%d, which "
                            "is present in the ordered chapters main "
                            "file. This is a broken file.\n",
                            p->source->filename, stream_type_name(m->type),
                            m->demuxer_id);
            }
        }
    }
}

void build_ordered_chapter_timeline(struct timeline *tl)
{
    struct demuxer *demuxer = tl->demuxer;

    if (!demuxer->matroska_data.ordered_chapters)
        return;

    struct tl_ctx *ctx = talloc_ptrtype(tl, ctx);
    *ctx = (struct tl_ctx){
        .log = tl->log,
        .global = tl->global,
        .tl = tl,
        .demuxer = demuxer,
        .opts = mp_get_config_group(ctx, tl->global, GLOBAL_CONFIG),
    };

    if (!ctx->opts->ordered_chapters || !demuxer->access_references) {
        MP_INFO(demuxer, "File uses ordered chapters, but "
                "you have disabled support for them. Ignoring.\n");
        talloc_free(ctx);
        return;
    }

    MP_INFO(ctx, "File uses ordered chapters, will build edit timeline.\n");

    struct matroska_data *m = &demuxer->matroska_data;

    // +1 because sources/uid_map[0] is original file even if all chapters
    // actually use other sources and need separate entries
    ctx->sources = talloc_zero_array(tl, struct demuxer *,
                                     m->num_ordered_chapters + 1);
    ctx->sources[0] = demuxer;
    ctx->num_sources = 1;

    ctx->uids = talloc_zero_array(NULL, struct matroska_segment_uid,
                                  m->num_ordered_chapters + 1);
    ctx->uids[0] = m->uid;
    ctx->uids[0].edition = 0;

    for (int i = 0; i < m->num_ordered_chapters; i++) {
        struct matroska_chapter *c = m->ordered_chapters + i;
        /* If there isn't a segment uid, we are the source. If the segment uid
         * is our segment uid and the edition matches. We can't accept the
         * "don't care" edition value of 0 since the user may have requested a
         * non-default edition. */
        if (!c->has_segment_uid || demux_matroska_uid_cmp(&c->uid, &m->uid))
            continue;

        if (has_source_request(ctx, &c->uid))
            continue;

        ctx->uids[ctx->num_sources] = c->uid;
        ctx->sources[ctx->num_sources] = NULL;
        ctx->num_sources++;
    }

    find_ordered_chapter_sources(ctx);

    talloc_free(ctx->uids);
    ctx->uids = NULL;

    struct demux_chapter *chapters =
        talloc_zero_array(tl, struct demux_chapter, m->num_ordered_chapters);

    ctx->timeline = talloc_array_ptrtype(tl, ctx->timeline, 0);
    ctx->num_chapters = m->num_ordered_chapters;

    struct inner_timeline_info info = {
        .skip = 0,
        .limit = 0
    };
    build_timeline_loop(ctx, chapters, &info, 0);

    // Fuck everything: filter out all "unset" chapters.
    for (int n = m->num_ordered_chapters - 1; n >= 0; n--) {
        if (!chapters[n].metadata)
            MP_TARRAY_REMOVE_AT(chapters, m->num_ordered_chapters, n);
    }

    if (!ctx->num_parts) {
        // None of  the parts come from the file itself???
        // Broken file, but we need at least 1 valid timeline part - add a dummy.
        MP_WARN(ctx, "Ordered chapters file with no parts?\n");
        struct timeline_part new = {
            .source = demuxer,
        };
        MP_TARRAY_APPEND(NULL, ctx->timeline, ctx->num_parts, new);
    }

    for (int n = 0; n < ctx->num_parts; n++) {
        ctx->timeline[n].end = n == ctx->num_parts - 1
            ? ctx->start_time / 1e9
            : ctx->timeline[n + 1].start;
    };

    /* Ignore anything less than a millisecond when reporting missing time. If
     * users really notice less than a millisecond missing, maybe this can be
     * revisited. */
    if (ctx->missing_time >= 1e6) {
        MP_ERR(ctx, "There are %.3f seconds missing from the timeline!\n",
               ctx->missing_time / 1e9);
    }

    // With Matroska, the "master" file usually dictates track layout etc.,
    // except maybe with playlist-like files.
    struct demuxer *track_layout = ctx->timeline[0].source;
    for (int n = 0; n < ctx->num_parts; n++) {
        if (ctx->timeline[n].source == ctx->demuxer) {
            track_layout = ctx->demuxer;
            break;
        }
    }

    check_track_compatibility(ctx, track_layout);

    tl->sources = ctx->sources;
    tl->num_sources = ctx->num_sources;

    struct timeline_par *par = talloc_ptrtype(tl, par);
    *par = (struct timeline_par){
        .parts = ctx->timeline,
        .num_parts = ctx->num_parts,
        .track_layout = track_layout,
    };
    MP_TARRAY_APPEND(tl, tl->pars, tl->num_pars, par);
    tl->chapters = chapters;
    tl->num_chapters = m->num_ordered_chapters;
    tl->meta = track_layout;
}
