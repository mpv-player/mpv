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

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>
#include <time.h>

#include <libavutil/avutil.h>

#include "mpv_talloc.h"

#include "misc/thread_pool.h"
#include "misc/thread_tools.h"
#include "osdep/io.h"
#include "osdep/terminal.h"
#include "osdep/threads.h"
#include "osdep/timer.h"

#include "client.h"
#include "common/msg.h"
#include "common/msg_control.h"
#include "options/path.h"
#include "options/m_config.h"
#include "options/parse_configfile.h"
#include "common/playlist.h"
#include "options/options.h"
#include "options/m_property.h"
#include "common/common.h"
#include "common/encode.h"
#include "common/stats.h"
#include "input/input.h"
#include "misc/json.h"
#include "misc/language.h"

#include "audio/out/ao.h"
#include "filters/f_decoder_wrapper.h"
#include "filters/f_lavfi.h"
#include "filters/filter_internal.h"
#include "demux/demux.h"
#include "stream/stream.h"
#include "sub/dec_sub.h"
#include "external_files.h"
#include "video/out/vo.h"

#include "core.h"
#include "command.h"
#include "libmpv/client.h"

// Called from the demuxer thread if a new packet is available, or other changes.
static void wakeup_demux(void *pctx)
{
    struct MPContext *mpctx = pctx;
    mp_wakeup_core(mpctx);
}

// Called by foreign threads when playback should be stopped and such.
void mp_abort_playback_async(struct MPContext *mpctx)
{
    mp_cancel_trigger(mpctx->playback_abort);

    mp_mutex_lock(&mpctx->abort_lock);

    for (int n = 0; n < mpctx->num_abort_list; n++) {
        struct mp_abort_entry *abort = mpctx->abort_list[n];
        if (abort->coupled_to_playback)
            mp_abort_trigger_locked(mpctx, abort);
    }

    mp_mutex_unlock(&mpctx->abort_lock);
}

// Add it to the global list, and allocate required data structures.
void mp_abort_add(struct MPContext *mpctx, struct mp_abort_entry *abort)
{
    mp_mutex_lock(&mpctx->abort_lock);
    assert(!abort->cancel);
    abort->cancel = mp_cancel_new(NULL);
    MP_TARRAY_APPEND(NULL, mpctx->abort_list, mpctx->num_abort_list, abort);
    mp_abort_recheck_locked(mpctx, abort);
    mp_mutex_unlock(&mpctx->abort_lock);
}

// Remove Add it to the global list, and free/clear required data structures.
// Does not deallocate the abort value itself.
void mp_abort_remove(struct MPContext *mpctx, struct mp_abort_entry *abort)
{
    mp_mutex_lock(&mpctx->abort_lock);
    for (int n = 0; n < mpctx->num_abort_list; n++) {
        if (mpctx->abort_list[n] == abort) {
            MP_TARRAY_REMOVE_AT(mpctx->abort_list, mpctx->num_abort_list, n);
            TA_FREEP(&abort->cancel);
            abort = NULL; // it's not free'd, just clear for the assert below
            break;
        }
    }
    assert(!abort); // should have been in the list
    mp_mutex_unlock(&mpctx->abort_lock);
}

// Verify whether the abort needs to be signaled after changing certain fields
// in abort.
void mp_abort_recheck_locked(struct MPContext *mpctx,
                             struct mp_abort_entry *abort)
{
    if ((abort->coupled_to_playback && mp_cancel_test(mpctx->playback_abort)) ||
        mpctx->abort_all)
    {
        mp_abort_trigger_locked(mpctx, abort);
    }
}

void mp_abort_trigger_locked(struct MPContext *mpctx,
                             struct mp_abort_entry *abort)
{
    mp_cancel_trigger(abort->cancel);
}

static void kill_demuxers_reentrant(struct MPContext *mpctx,
                                    struct demuxer **demuxers, int num_demuxers)
{
    struct demux_free_async_state **items = NULL;
    int num_items = 0;

    for (int n = 0; n < num_demuxers; n++) {
        struct demuxer *d = demuxers[n];

        if (!demux_cancel_test(d)) {
            // Make sure it is set if it wasn't yet.
            demux_set_wakeup_cb(d, wakeup_demux, mpctx);

            struct demux_free_async_state *item = demux_free_async(d);
            if (item) {
                MP_TARRAY_APPEND(NULL, items, num_items, item);
                d = NULL;
            }
        }

        demux_cancel_and_free(d);
    }

    if (!num_items)
        return;

    MP_DBG(mpctx, "Terminating demuxers...\n");

    double end = mp_time_sec() + mpctx->opts->demux_termination_timeout;
    bool force = false;
    while (num_items) {
        double wait = end - mp_time_sec();

        for (int n = 0; n < num_items; n++) {
            struct demux_free_async_state *item = items[n];
            if (demux_free_async_finish(item)) {
                items[n] = items[num_items - 1];
                num_items -= 1;
                n--;
                goto repeat;
            } else if (wait < 0) {
                demux_free_async_force(item);
                if (!force)
                    MP_VERBOSE(mpctx, "Forcefully terminating demuxers...\n");
                force = true;
            }
        }

        if (wait >= 0)
            mp_set_timeout(mpctx, wait);
        mp_idle(mpctx);
    repeat:;
    }

    talloc_free(items);

    MP_DBG(mpctx, "Done terminating demuxers.\n");
}

static void uninit_demuxer(struct MPContext *mpctx)
{
    for (int t = 0; t < STREAM_TYPE_COUNT; t++) {
        for (int r = 0; r < num_ptracks[t]; r++)
            mpctx->current_track[r][t] = NULL;
    }

    talloc_free(mpctx->chapters);
    mpctx->chapters = NULL;
    mpctx->num_chapters = 0;

    mp_abort_cache_dumping(mpctx);

    struct demuxer **demuxers = NULL;
    int num_demuxers = 0;

    if (mpctx->demuxer)
        MP_TARRAY_APPEND(NULL, demuxers, num_demuxers, mpctx->demuxer);
    mpctx->demuxer = NULL;

    for (int i = 0; i < mpctx->num_tracks; i++) {
        struct track *track = mpctx->tracks[i];

        assert(!track->dec && !track->d_sub);
        assert(!track->vo_c && !track->ao_c);
        assert(!track->sink);

        // Demuxers can be added in any order (if they appear mid-stream), and
        // we can't know which tracks uses which, so here's some O(n^2) trash.
        for (int n = 0; n < num_demuxers; n++) {
            if (demuxers[n] == track->demuxer) {
                track->demuxer = NULL;
                break;
            }
        }
        if (track->demuxer)
            MP_TARRAY_APPEND(NULL, demuxers, num_demuxers, track->demuxer);

        talloc_free(track);
    }
    mpctx->num_tracks = 0;

    kill_demuxers_reentrant(mpctx, demuxers, num_demuxers);
    talloc_free(demuxers);
}

#define APPEND(s, ...) mp_snprintf_cat(s, sizeof(s), __VA_ARGS__)
#define FILL(s, n) mp_snprintf_cat(s, sizeof(s), "%*s", n, "")

static void print_stream(struct MPContext *mpctx, struct track *t, bool indent)
{
    const char *tname = "?";
    const char *selopt = "?";
    const char *langopt = "?";
    switch (t->type) {
    case STREAM_VIDEO:
        tname = t->image ? "Image" : "Video"; selopt = "vid"; langopt = "vlang";
        break;
    case STREAM_AUDIO:
        tname = "Audio"; selopt = "aid"; langopt = "alang";
        break;
    case STREAM_SUB:
        tname = "Subs"; selopt = "sid"; langopt = "slang";
        break;
    }
    char b[2048] = {0};

    int max_lang_length = 0;
    for (int n = 0; n < mpctx->num_tracks; n++) {
        if (mpctx->tracks[n]->lang)
            max_lang_length = MPMAX(strlen(mpctx->tracks[n]->lang), max_lang_length);
    }

    if (indent)
        APPEND(b, " ");
    APPEND(b, "%s %-5s  --%s=%-2d", t->selected ? BLACK_CIRCLE : WHITE_CIRCLE,
           tname, selopt, t->user_tid);
    if (t->lang) {
        APPEND(b, " --%s=%-*s ", langopt, max_lang_length, t->lang);
    } else if (max_lang_length) {
        FILL(b, (int) strlen(" --alang= ") + max_lang_length);
    }

    void *ctx = talloc_new(NULL);
    APPEND(b, " %s", mp_format_track_metadata(ctx, t, false));
    talloc_free(ctx);

    MP_INFO(mpctx, "%s\n", b);
}

void print_track_list(struct MPContext *mpctx, const char *msg)
{
    if (msg)
        MP_INFO(mpctx, "%s\n", msg);
    for (int t = 0; t < STREAM_TYPE_COUNT; t++) {
        for (int n = 0; n < mpctx->num_tracks; n++)
            if (mpctx->tracks[n]->type == t)
                // Indent tracks after messages like "Tracks switched" and
                // "Playing:".
                print_stream(mpctx, mpctx->tracks[n], msg ||
                             mpctx->playlist->num_entries > 1 ||
                             mpctx->playing->playlist_path);
    }
}

void update_demuxer_properties(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    if (!demuxer)
        return;
    demux_update(demuxer, get_current_time(mpctx));
    int events = demuxer->events;
    if ((events & DEMUX_EVENT_INIT) && demuxer->num_editions > 1) {
        for (int n = 0; n < demuxer->num_editions; n++) {
            struct demux_edition *edition = &demuxer->editions[n];
            char b[128] = {0};
            if (mpctx->playlist->num_entries > 1 || mpctx->playing->playlist_path)
                APPEND(b, " ");
            APPEND(b, "%s --edition=%d", n == demuxer->edition ?
                   BLACK_CIRCLE : WHITE_CIRCLE, n);
            char *name = mp_tags_get_str(edition->metadata, "title");
            if (name)
                APPEND(b, " '%s'", name);
            if (edition->default_edition)
                APPEND(b, " [default]");
            MP_INFO(mpctx, "%s\n", b);
        }
    }
    struct demuxer *tracks = mpctx->demuxer;
    if (tracks->events & DEMUX_EVENT_STREAMS) {
        add_demuxer_tracks(mpctx, tracks);
        print_track_list(mpctx, NULL);
        tracks->events &= ~DEMUX_EVENT_STREAMS;
    }
    if (events & DEMUX_EVENT_METADATA) {
        struct mp_tags *info =
            mp_tags_filtered(mpctx, demuxer->metadata, mpctx->opts->display_tags);
        // prev is used to attempt to print changed tags only (to some degree)
        struct mp_tags *prev = mpctx->filtered_tags;
        int n_prev = 0;
        bool had_output = false;
        for (int n = 0; n < info->num_keys; n++) {
            if (prev && n_prev < prev->num_keys) {
                if (strcmp(prev->keys[n_prev], info->keys[n]) == 0) {
                    n_prev++;
                    if (strcmp(prev->values[n_prev - 1], info->values[n]) == 0)
                        continue;
                }
            }
            struct mp_log *log = mp_log_new(NULL, mpctx->log, "!display-tags");
            if (!had_output)
                mp_info(log, "File tags:\n");
            mp_info(log, " %s: %s\n", info->keys[n], info->values[n]);
            had_output = true;
            talloc_free(log);
        }
        talloc_free(mpctx->filtered_tags);
        mpctx->filtered_tags = info;
        mp_notify(mpctx, MP_EVENT_METADATA_UPDATE, NULL);
    }
    if (events & DEMUX_EVENT_DURATION)
        mp_notify(mpctx, MP_EVENT_DURATION_UPDATE, NULL);
    demuxer->events = 0;
}

// Enables or disables the stream for the given track, according to
// track->selected.
// With refresh_only=true, refreshes the stream if it's enabled.
void reselect_demux_stream(struct MPContext *mpctx, struct track *track,
                           bool refresh_only)
{
    if (!track->stream)
        return;
    double pts = get_current_time(mpctx);
    if (pts != MP_NOPTS_VALUE) {
        pts += get_track_seek_offset(mpctx, track);
        if (track->type == STREAM_SUB)
            pts -= 10.0;
    }
    if (refresh_only)
        demuxer_refresh_track(track->demuxer, track->stream, pts);
    else
        demuxer_select_track(track->demuxer, track->stream, pts, track->selected);
}

static void enable_demux_thread(struct MPContext *mpctx, struct demuxer *demux)
{
    if (mpctx->opts->demuxer_thread && !demux->fully_read) {
        demux_set_wakeup_cb(demux, wakeup_demux, mpctx);
        demux_start_thread(demux);
    }
}

static int find_new_tid(struct MPContext *mpctx, enum stream_type t)
{
    int new_id = 0;
    for (int i = 0; i < mpctx->num_tracks; i++) {
        struct track *track = mpctx->tracks[i];
        if (track->type == t)
            new_id = MPMAX(new_id, track->user_tid);
    }
    return new_id + 1;
}

static struct track *add_stream_track(struct MPContext *mpctx,
                                      struct demuxer *demuxer,
                                      struct sh_stream *stream)
{
    for (int i = 0; i < mpctx->num_tracks; i++) {
        struct track *track = mpctx->tracks[i];
        if (track->stream == stream)
            return track;
    }

    struct track *track = talloc_ptrtype(NULL, track);
    *track = (struct track) {
        .type = stream->type,
        .user_tid = find_new_tid(mpctx, stream->type),
        .demuxer_id = stream->demuxer_id,
        .ff_index = stream->ff_index,
        .hls_bitrate = stream->hls_bitrate,
        .program_id = stream->program_id,
        .title = stream->title,
        .default_track = stream->default_track,
        .forced_track = stream->forced_track,
        .dependent_track = stream->dependent_track,
        .visual_impaired_track = stream->visual_impaired_track,
        .hearing_impaired_track = stream->hearing_impaired_track,
        .image = stream->image,
        .attached_picture = stream->attached_picture != NULL,
        .lang = stream->lang,
        .demuxer = demuxer,
        .stream = stream,
    };
    MP_TARRAY_APPEND(mpctx, mpctx->tracks, mpctx->num_tracks, track);

    mp_notify(mpctx, MP_EVENT_TRACKS_CHANGED, NULL);

    return track;
}

void add_demuxer_tracks(struct MPContext *mpctx, struct demuxer *demuxer)
{
    for (int n = 0; n < demux_get_num_stream(demuxer); n++)
        add_stream_track(mpctx, demuxer, demux_get_stream(demuxer, n));
}

/* Get the track wanted by the user.
 * tid is the track ID requested by the user (-2: deselect, -1: default)
 * lang is a string list, NULL is same as empty list
 * Sort tracks based on the following criteria, and pick the first:
  *0a) track matches tid (always wins)
 * 0b) track is not from --external-file
 * 1) track is external (no_default cancels this)
 * 1b) track was passed explicitly (is not an auto-loaded subtitle)
 * 1c) track matches the program ID of the video
 * 2) earlier match in lang list but not if we're using os_langs
 * 3a) track is marked forced and we're preferring forced tracks
 * 3b) track is marked non-forced and we're preferring non-forced tracks
 * 3c) track is marked default
 * 3d) match in lang list with os_langs
 * 4) attached picture, HLS bitrate
 * 5) lower track number
 * If select_fallback is not set, 5) is only used to determine whether a
 * matching track is preferred over another track. Otherwise, always pick a
 * track (if nothing else matches, return the track with lowest ID).
 * Forced tracks are preferred when the user prefers not to display subtitles
 */
// Return whether t1 is preferred over t2
static bool compare_track(struct track *t1, struct track *t2, char **langs, bool os_langs,
                          bool forced, struct MPOpts *opts, int preferred_program)
{
    bool sub = t2->type == STREAM_SUB;
    if (!opts->autoload_files && t1->is_external != t2->is_external)
        return !t1->is_external;
    bool ext1 = t1->is_external && !t1->no_default;
    bool ext2 = t2->is_external && !t2->no_default;
    if (ext1 != ext2) {
        if (t1->attached_picture && t2->attached_picture
            && opts->audio_display == 1)
            return !ext1;
        return ext1;
    }
    if (t1->auto_loaded != t2->auto_loaded)
        return !t1->auto_loaded;
    if (preferred_program != -1 && t1->program_id != -1 && t2->program_id != -1) {
        if ((t1->program_id == preferred_program) !=
            (t2->program_id == preferred_program))
            return t1->program_id == preferred_program;
    }
    int l1 = mp_match_lang(langs, t1->lang), l2 = mp_match_lang(langs, t2->lang);
    if (!os_langs && l1 != l2)
        return l1 > l2;
    if (forced)
        return t1->forced_track;
    if (t1->default_track != t2->default_track && !t2->forced_select)
        return t1->default_track;
    if (sub && !t2->forced_select && t2->forced_track)
        return !t1->forced_track;
    if (os_langs && l1 != l2)
        return l1 > l2;
    if (t1->attached_picture != t2->attached_picture)
        return !t1->attached_picture;
    if (t1->image != t2->image)
        return !t1->image;
    if (t1->dependent_track != t2->dependent_track)
        return !t1->dependent_track;
    if (t1->stream && t2->stream && opts->hls_bitrate >= 0 &&
        t1->stream->hls_bitrate != t2->stream->hls_bitrate)
    {
        bool t1_ok = t1->stream->hls_bitrate <= opts->hls_bitrate;
        bool t2_ok = t2->stream->hls_bitrate <= opts->hls_bitrate;
        if (t1_ok != t2_ok)
            return t1_ok;
        if (t1_ok && t2_ok)
            return t1->stream->hls_bitrate > t2->stream->hls_bitrate;
        return t1->stream->hls_bitrate < t2->stream->hls_bitrate;
    }
    return t1->user_tid <= t2->user_tid;
}

static bool duplicate_track(struct MPContext *mpctx, int order,
                            enum stream_type type, struct track *track)
{
    for (int i = 0; i < order; i++) {
        if (mpctx->current_track[i][type] == track)
            return true;
    }
    return false;
}

static bool append_lang(size_t *nb, char ***out, char *in)
{
    if (!in)
        return false;
    MP_TARRAY_GROW(NULL, *out, *nb + 1);
    (*out)[(*nb)++] = in;
    (*out)[*nb] = NULL;
    talloc_steal(*out, in);
    return true;
}

static char **add_os_langs(void)
{
    size_t nb = 0;
    char **out = NULL;
    char **autos = mp_get_user_langs();
    for (int i = 0; autos && autos[i]; i++) {
        if (!append_lang(&nb, &out, autos[i]))
            goto cleanup;
    }

cleanup:
    talloc_free(autos);
    return out;
}

static char **process_langs(char **in)
{
    size_t nb = 0;
    char **out = NULL;
    for (int i = 0; in && in[i]; i++) {
        if (!append_lang(&nb, &out, talloc_strdup(NULL, in[i])))
            break;
    }
    return out;
}

struct track *select_default_track(struct MPContext *mpctx, int order,
                                   enum stream_type type)
{
    struct MPOpts *opts = mpctx->opts;
    int tid = opts->stream_id[order][type];
    int preferred_program = (type != STREAM_VIDEO && mpctx->current_track[0][STREAM_VIDEO]) ?
                            mpctx->current_track[0][STREAM_VIDEO]->program_id : -1;
    if (tid == -2)
        return NULL;
    char **langs = process_langs(opts->stream_lang[type]);
    bool os_langs = false;
    // Try to add OS languages if enabled by the user and we don't already have a lang from slang.
    if (type == STREAM_SUB && (!langs || !strcmp(langs[0], "")) && opts->subs_match_os_language) {
        talloc_free(langs);
        langs = add_os_langs();
        os_langs = true;
    }
    const char *audio_lang = mpctx->current_track[0][STREAM_AUDIO] ?
                             mpctx->current_track[0][STREAM_AUDIO]->lang :
                             NULL;
    bool sub = type == STREAM_SUB;
    struct track *pick = NULL;
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];
        if (track->type != type)
            continue;
        if (track->user_tid == tid) {
            pick = track;
            goto cleanup;
        }
        if (tid >= 0)
            continue;
        if (track->no_auto_select)
            continue;
        if (duplicate_track(mpctx, order, type, track))
            continue;
        if (sub) {
            // Subtitle specific auto-selecting crap.
            bool audio_matches = audio_lang && track->lang && !strcasecmp(audio_lang, track->lang);
            bool forced = track->forced_track && (opts->subs_fallback_forced == 2 ||
                          (audio_matches && opts->subs_fallback_forced == 1));
            bool lang_match = !os_langs && mp_match_lang(langs, track->lang) > 0;
            bool subs_fallback = (track->is_external && !track->no_default) || opts->subs_fallback == 2 ||
                                 (opts->subs_fallback == 1 && track->default_track);
            bool subs_matching_audio = (!mp_match_lang(langs, audio_lang) || opts->subs_with_matching_audio == 2 ||
                                        (opts->subs_with_matching_audio == 1 && track->forced_track));
            if (subs_matching_audio && ((!pick && (forced || lang_match || subs_fallback)) ||
                (pick && compare_track(track, pick, langs, os_langs, forced, mpctx->opts, preferred_program))))
            {
                pick = track;
                pick->forced_select = forced;
            }
        } else if (!pick || compare_track(track, pick, langs, os_langs, false, mpctx->opts, preferred_program)) {
            pick = track;
        }
    }

    if (pick && pick->attached_picture && !mpctx->opts->audio_display)
        pick = NULL;
    if (pick && !opts->autoload_files && pick->is_external)
        pick = NULL;
cleanup:
    talloc_free(langs);
    return pick;
}

static char *track_layout_hash(struct MPContext *mpctx)
{
    char *h = talloc_strdup(NULL, "");
    for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
        for (int n = 0; n < mpctx->num_tracks; n++) {
            struct track *track = mpctx->tracks[n];
            if (track->type != type)
                continue;
            h = talloc_asprintf_append_buffer(h, "%d-%d-%d-%d-%s\n", type,
                    track->user_tid, track->default_track, track->is_external,
                    track->lang ? track->lang : "");
        }
    }
    return h;
}

// Normally, video/audio/sub track selection is persistent across files. This
// code resets track selection if the new file has a different track layout.
static void check_previous_track_selection(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    if (!mpctx->track_layout_hash)
        return;

    char *h = track_layout_hash(mpctx);
    if (strcmp(h, mpctx->track_layout_hash) != 0) {
        // Reset selection, but only if they're not "auto" or "off". The
        // defaults are -1 (default selection), or -2 (off) for secondary tracks.
        for (int t = 0; t < STREAM_TYPE_COUNT; t++) {
            for (int i = 0; i < num_ptracks[t]; i++) {
                if (opts->stream_id[i][t] >= 0)
                    mark_track_selection(mpctx, i, t, i == 0 ? -1 : -2);
            }
        }
        talloc_free(mpctx->track_layout_hash);
        mpctx->track_layout_hash = NULL;
    }
    talloc_free(h);
}

// Update the matching track selection user option to the given value.
void mark_track_selection(struct MPContext *mpctx, int order,
                          enum stream_type type, int value)
{
    assert(order >= 0 && order < num_ptracks[type]);
    mpctx->opts->stream_id[order][type] = value;
    m_config_notify_change_opt_ptr(mpctx->mconfig,
                                   &mpctx->opts->stream_id[order][type]);
}

void mp_switch_track_n(struct MPContext *mpctx, int order, enum stream_type type,
                       struct track *track, int flags)
{
    assert(!track || track->type == type);
    assert(type >= 0 && type < STREAM_TYPE_COUNT);
    assert(order >= 0 && order < num_ptracks[type]);

    // Mark the current track selection as explicitly user-requested. (This is
    // different from auto-selection or disabling a track due to errors.)
    if (flags & FLAG_MARK_SELECTION)
        mark_track_selection(mpctx, order, type, track ? track->user_tid : -2);

    // No decoder should be initialized yet.
    if (!mpctx->demuxer)
        return;

    struct track *current = mpctx->current_track[order][type];
    if (track == current)
        return;

    if (current && current->sink) {
        MP_ERR(mpctx, "Can't disable input to complex filter.\n");
        goto error;
    }
    if ((type == STREAM_VIDEO && mpctx->vo_chain && !mpctx->vo_chain->track) ||
        (type == STREAM_AUDIO && mpctx->ao_chain && !mpctx->ao_chain->track))
    {
        MP_ERR(mpctx, "Can't switch away from complex filter output.\n");
        goto error;
    }

    if (track && track->selected) {
        // Track has been selected in a different order parameter.
        MP_ERR(mpctx, "Track %d is already selected.\n", track->user_tid);
        goto error;
    }

    if (order == 0) {
        if (type == STREAM_VIDEO) {
            uninit_video_chain(mpctx);
            if (!track)
                handle_force_window(mpctx, true);
        } else if (type == STREAM_AUDIO) {
            clear_audio_output_buffers(mpctx);
            uninit_audio_chain(mpctx);
            if (!track)
                uninit_audio_out(mpctx);
        }
    }
    if (type == STREAM_SUB)
        uninit_sub(mpctx, current);

    if (current) {
        current->selected = false;
        reselect_demux_stream(mpctx, current, false);
    }

    mpctx->current_track[order][type] = track;

    if (track) {
        track->selected = true;
        reselect_demux_stream(mpctx, track, false);
    }

    if (type == STREAM_VIDEO && order == 0) {
        reinit_video_chain(mpctx);
    } else if (type == STREAM_AUDIO && order == 0) {
        reinit_audio_chain(mpctx);
    } else if (type == STREAM_SUB && order >= 0 && order <= 2) {
        reinit_sub(mpctx, track);
    }

    mp_notify(mpctx, MP_EVENT_TRACK_SWITCHED, NULL);
    mp_wakeup_core(mpctx);

    talloc_free(mpctx->track_layout_hash);
    mpctx->track_layout_hash = talloc_steal(mpctx, track_layout_hash(mpctx));

    return;
error:
    mark_track_selection(mpctx, order, type, -1);
}

void mp_switch_track(struct MPContext *mpctx, enum stream_type type,
                     struct track *track, int flags)
{
    mp_switch_track_n(mpctx, 0, type, track, flags);
}

void mp_deselect_track(struct MPContext *mpctx, struct track *track)
{
    if (track && track->selected) {
        for (int t = 0; t < num_ptracks[track->type]; t++) {
            if (mpctx->current_track[t][track->type] != track)
                continue;
            mp_switch_track_n(mpctx, t, track->type, NULL, 0);
            mark_track_selection(mpctx, t, track->type, -1); // default
        }
    }
}

struct track *mp_track_by_tid(struct MPContext *mpctx, enum stream_type type,
                              int tid)
{
    if (tid == -1)
        return mpctx->current_track[0][type];
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];
        if (track->type == type && track->user_tid == tid)
            return track;
    }
    return NULL;
}

bool mp_remove_track(struct MPContext *mpctx, struct track *track)
{
    if (!track->is_external)
        return false;

    mp_deselect_track(mpctx, track);
    if (track->selected)
        return false;

    struct demuxer *d = track->demuxer;

    int index = 0;
    while (index < mpctx->num_tracks && mpctx->tracks[index] != track)
        index++;
    MP_TARRAY_REMOVE_AT(mpctx->tracks, mpctx->num_tracks, index);
    talloc_free(track);

    // Close the demuxer, unless there is still a track using it. These are
    // all external tracks.
    bool in_use = false;
    for (int n = mpctx->num_tracks - 1; n >= 0 && !in_use; n--)
        in_use |= mpctx->tracks[n]->demuxer == d;

    if (!in_use)
        demux_cancel_and_free(d);

    mp_notify(mpctx, MP_EVENT_TRACKS_CHANGED, NULL);

    return true;
}

// Add the given file as additional track. The filter argument controls how or
// if tracks are auto-selected at any point.
// To be run on a worker thread, locked (temporarily unlocks core).
// cancel will generally be used to abort the loading process, but on success
// the demuxer is changed to be slaved to mpctx->playback_abort instead.
int mp_add_external_file(struct MPContext *mpctx, char *filename,
                         enum stream_type filter, struct mp_cancel *cancel,
                         bool cover_art)
{
    struct MPOpts *opts = mpctx->opts;
    if (!filename || mp_cancel_test(cancel))
        return -1;

    void *unescaped_url = NULL;
    char *disp_filename = filename;
    if (strncmp(disp_filename, "memory://", 9) == 0) {
        disp_filename = "memory://"; // avoid noise
    } else if (mp_is_url(bstr0(disp_filename))) {
        disp_filename = unescaped_url = mp_url_unescape(NULL, disp_filename);
    }

    struct demuxer_params params = {
        .is_top_level = true,
        .stream_flags = STREAM_ORIGIN_DIRECT,
        .allow_playlist_create = false,
    };

    switch (filter) {
    case STREAM_SUB:
        params.force_format = opts->sub_demuxer_name;
        break;
    case STREAM_AUDIO:
        params.force_format = opts->audio_demuxer_name;
        break;
    }

    mp_core_unlock(mpctx);

    char *path = mp_get_user_path(NULL, mpctx->global, filename);
    struct demuxer *demuxer =
        demux_open_url(path, &params, cancel, mpctx->global);
    talloc_free(path);

    if (demuxer)
        enable_demux_thread(mpctx, demuxer);

    mp_core_lock(mpctx);

    // The command could have overlapped with playback exiting. (We don't care
    // if playback has started again meanwhile - weird, but not a problem.)
    if (mpctx->stop_play)
        goto err_out;

    if (!demuxer)
        goto err_out;

    if (filter != STREAM_SUB && opts->rebase_start_time)
        demux_set_ts_offset(demuxer, -demuxer->start_time);

    bool has_any = false;
    for (int n = 0; n < demux_get_num_stream(demuxer); n++) {
        struct sh_stream *sh = demux_get_stream(demuxer, n);
        if (sh->type == filter || filter == STREAM_TYPE_COUNT) {
            has_any = true;
            break;
        }
    }

    if (!has_any) {
        char *tname = mp_tprintf(20, "%s ", stream_type_name(filter));
        if (filter == STREAM_TYPE_COUNT)
            tname = "";
        MP_ERR(mpctx, "No %sstreams in file %s.\n", tname, disp_filename);
        goto err_out;
    }

    int first_num = -1;
    for (int n = 0; n < demux_get_num_stream(demuxer); n++) {
        struct sh_stream *sh = demux_get_stream(demuxer, n);
        struct track *t = add_stream_track(mpctx, demuxer, sh);
        t->is_external = true;
        if (sh->title && sh->title[0]) {
            t->title = talloc_strdup(t, sh->title);
        } else {
            t->title = talloc_strdup(t, mp_basename(disp_filename));
        }
        t->external_filename = mp_normalize_user_path(t, mpctx->global, filename);
        t->no_default = sh->type != filter;
        t->no_auto_select = t->no_default;
        // if we found video, and we are loading cover art, flag as such.
        t->attached_picture = t->type == STREAM_VIDEO && cover_art;
        if (first_num < 0 && (filter == STREAM_TYPE_COUNT || sh->type == filter))
            first_num = mpctx->num_tracks - 1;
    }

    mp_cancel_set_parent(demuxer->cancel, mpctx->playback_abort);

    talloc_free(unescaped_url);
    return first_num;

err_out:
    demux_cancel_and_free(demuxer);
    if (!mp_cancel_test(cancel))
        MP_ERR(mpctx, "Can not open external file %s.\n", disp_filename);
    talloc_free(unescaped_url);
    return -1;
}

// to be run on a worker thread, locked (temporarily unlocks core)
static void open_external_files(struct MPContext *mpctx, char **files,
                                enum stream_type filter)
{
    // Need a copy, because the option value could be mutated during iteration.
    void *tmp = talloc_new(NULL);
    files = mp_dup_str_array(tmp, files);

    for (int n = 0; files && files[n]; n++)
        // when given filter is set to video, we are loading up cover art
        mp_add_external_file(mpctx, files[n], filter, mpctx->playback_abort,
                             filter == STREAM_VIDEO);

    talloc_free(tmp);
}

// See mp_add_external_file() for meaning of cancel parameter.
void autoload_external_files(struct MPContext *mpctx, struct mp_cancel *cancel)
{
    struct MPOpts *opts = mpctx->opts;

    if (opts->sub_auto < 0 && opts->audiofile_auto < 0 && opts->coverart_auto < 0)
        return;
    if (!opts->autoload_files || strcmp(mpctx->filename, "-") == 0)
        return;

    void *tmp = talloc_new(NULL);
    struct subfn *list = find_external_files(mpctx->global, mpctx->filename, opts);
    talloc_steal(tmp, list);

    int sc[STREAM_TYPE_COUNT] = {0};
    for (int n = 0; n < mpctx->num_tracks; n++) {
        if (!mpctx->tracks[n]->attached_picture)
            sc[mpctx->tracks[n]->type]++;
    }

    for (int i = 0; list && list[i].fname; i++) {
        struct subfn *e = &list[i];

        for (int n = 0; n < mpctx->num_tracks; n++) {
            struct track *t = mpctx->tracks[n];
            if (t->demuxer && strcmp(t->demuxer->filename, e->fname) == 0)
                goto skip;
        }
        if (e->type == STREAM_SUB && !sc[STREAM_VIDEO] && !sc[STREAM_AUDIO])
            goto skip;
        if (e->type == STREAM_AUDIO && !sc[STREAM_VIDEO])
            goto skip;
        if (e->type == STREAM_VIDEO && (sc[STREAM_VIDEO] || !sc[STREAM_AUDIO]))
            goto skip;

        // when given filter is set to video, we are loading up cover art
        int first = mp_add_external_file(mpctx, e->fname, e->type, cancel,
                                         e->type == STREAM_VIDEO);
        if (first < 0)
            goto skip;

        for (int n = first; n < mpctx->num_tracks; n++) {
            struct track *t = mpctx->tracks[n];
            t->auto_loaded = true;
            if (!t->lang)
                t->lang = talloc_strdup(t, e->lang);
        }
    skip:;
    }

    talloc_free(tmp);
}

// Do stuff to a newly loaded playlist. This includes any processing that may
// be required after loading a playlist.
void prepare_playlist(struct MPContext *mpctx, struct playlist *pl)
{
    struct MPOpts *opts = mpctx->opts;

    pl->current = NULL;
    pl->playlist_completed = false;
    pl->playlist_started = false;

    if (opts->playlist_pos >= 0)
        pl->current = playlist_entry_from_index(pl, opts->playlist_pos);

    if (pl->playlist_dir)
        playlist_set_current(pl);

    if (opts->shuffle)
        playlist_shuffle(pl);

    if (opts->merge_files)
        merge_playlist_files(pl);

    if (!pl->current)
        pl->current = mp_check_playlist_resume(mpctx, pl);

    if (!pl->current)
        pl->current = playlist_get_first(pl);
}

// Replace the current playlist entry with playlist contents. Moves the entries
// from the given playlist pl, so the entries don't actually need to be copied.
static void transfer_playlist(struct MPContext *mpctx, struct playlist *pl,
                              int64_t *start_id, int *num_new_entries)
{
    if (pl->num_entries) {
        prepare_playlist(mpctx, pl);
        struct playlist_entry *new = pl->current;
        *num_new_entries = pl->num_entries;
        *start_id = playlist_transfer_entries(mpctx->playlist, pl);
        // current entry is replaced
        if (mpctx->playlist->current)
            playlist_remove(mpctx->playlist, mpctx->playlist->current);
        if (new)
            mpctx->playlist->current = new;
        mpctx->playlist->playlist_dir = talloc_steal(mpctx->playlist, pl->playlist_dir);
    } else {
        MP_WARN(mpctx, "Empty playlist!\n");
    }
}

static void process_hooks(struct MPContext *mpctx, char *name)
{
    mp_hook_start(mpctx, name);

    while (!mp_hook_test_completion(mpctx, name)) {
        mp_idle(mpctx);

        // We have no idea what blocks a hook, so just do a full abort. This
        // does nothing for hooks that happen outside of playback.
        if (mpctx->stop_play)
            mp_abort_playback_async(mpctx);
    }
}

// to be run on a worker thread, locked (temporarily unlocks core)
static void load_chapters(struct MPContext *mpctx)
{
    struct demuxer *src = mpctx->demuxer;
    bool free_src = false;
    char *chapter_file = mpctx->opts->chapter_file;
    if (chapter_file && chapter_file[0]) {
        chapter_file = mp_get_user_path(NULL, mpctx->global, chapter_file);
        mp_core_unlock(mpctx);
        struct demuxer_params p = {.stream_flags = STREAM_ORIGIN_DIRECT};
        struct demuxer *demux = demux_open_url(chapter_file, &p,
                                               mpctx->playback_abort,
                                               mpctx->global);
        mp_core_lock(mpctx);
        if (demux) {
            src = demux;
            free_src = true;
        }
        talloc_free(mpctx->chapters);
        mpctx->chapters = NULL;
        talloc_free(chapter_file);
    }
    if (src && !mpctx->chapters) {
        talloc_free(mpctx->chapters);
        mpctx->num_chapters = src->num_chapters;
        mpctx->chapters = demux_copy_chapter_data(src->chapters, src->num_chapters);
        if (mpctx->opts->rebase_start_time) {
            for (int n = 0; n < mpctx->num_chapters; n++)
                mpctx->chapters[n].pts -= src->start_time;
        }
    }
    if (free_src)
        demux_cancel_and_free(src);
}

static void load_per_file_options(m_config_t *conf,
                                  struct playlist_param *params,
                                  int params_count)
{
    for (int n = 0; n < params_count; n++) {
        m_config_set_option_cli(conf, params[n].name, params[n].value,
                                M_SETOPT_BACKUP);
    }
}

static MP_THREAD_VOID open_demux_thread(void *ctx)
{
    struct MPContext *mpctx = ctx;

    mp_thread_set_name("opener");

    struct demuxer_params p = {
        .force_format = mpctx->open_format,
        .stream_flags = mpctx->open_url_flags,
        .stream_record = true,
        .is_top_level = true,
        .allow_playlist_create = mpctx->playlist->num_entries <= 1 &&
                                 !mpctx->playlist->playlist_dir,
    };
    struct demuxer *demux =
        demux_open_url(mpctx->open_url, &p, mpctx->open_cancel, mpctx->global);
    mpctx->open_res_demuxer = demux;

    if (demux) {
        MP_VERBOSE(mpctx, "Opening done: %s\n", mpctx->open_url);

        if (mpctx->open_for_prefetch && !demux->fully_read) {
            int num_streams = demux_get_num_stream(demux);
            for (int n = 0; n < num_streams; n++) {
                struct sh_stream *sh = demux_get_stream(demux, n);
                demuxer_select_track(demux, sh, MP_NOPTS_VALUE, true);
            }

            demux_set_wakeup_cb(demux, wakeup_demux, mpctx);
            demux_start_thread(demux);
            demux_start_prefetch(demux);
        }
    } else {
        MP_VERBOSE(mpctx, "Opening failed or was aborted: %s\n", mpctx->open_url);

        if (p.demuxer_failed) {
            mpctx->open_res_error = MPV_ERROR_UNKNOWN_FORMAT;
        } else {
            mpctx->open_res_error = MPV_ERROR_LOADING_FAILED;
        }
    }

    atomic_store(&mpctx->open_done, true);
    mp_wakeup_core(mpctx);
    MP_THREAD_RETURN();
}

static void cancel_open(struct MPContext *mpctx)
{
    if (mpctx->open_cancel)
        mp_cancel_trigger(mpctx->open_cancel);

    if (mpctx->open_active)
        mp_thread_join(mpctx->open_thread);
    mpctx->open_active = false;

    if (mpctx->open_res_demuxer)
        demux_cancel_and_free(mpctx->open_res_demuxer);
    mpctx->open_res_demuxer = NULL;

    TA_FREEP(&mpctx->open_cancel);
    TA_FREEP(&mpctx->open_url);
    TA_FREEP(&mpctx->open_format);

    atomic_store(&mpctx->open_done, false);
}

// Setup all the field to open this url, and make sure a thread is running.
static void start_open(struct MPContext *mpctx, char *url, int url_flags,
                       bool for_prefetch)
{
    cancel_open(mpctx);

    assert(!mpctx->open_active);
    assert(!mpctx->open_cancel);
    assert(!mpctx->open_res_demuxer);
    assert(!atomic_load(&mpctx->open_done));

    mpctx->open_cancel = mp_cancel_new(NULL);
    mpctx->open_url = talloc_strdup(NULL, url);
    mpctx->open_format = talloc_strdup(NULL, mpctx->opts->demuxer_name);
    mpctx->open_url_flags = url_flags;
    mpctx->open_for_prefetch = for_prefetch && mpctx->opts->demuxer_thread;
    mpctx->demuxer_changed = false;

    if (mp_thread_create(&mpctx->open_thread, open_demux_thread, mpctx)) {
        cancel_open(mpctx);
        return;
    }

    mpctx->open_active = true;
}

static void open_demux_reentrant(struct MPContext *mpctx)
{
    char *url = mpctx->stream_open_filename;

    if (mpctx->open_active) {
        bool done = atomic_load(&mpctx->open_done);
        bool failed = done && !mpctx->open_res_demuxer;
        bool correct_url = strcmp(mpctx->open_url, url) == 0;

        if (correct_url && !mpctx->demuxer_changed && !failed) {
            MP_VERBOSE(mpctx, "Using prefetched/prefetching URL.\n");
        } else {
            if (correct_url && failed) {
                MP_VERBOSE(mpctx, "Prefetched URL failed, retrying.\n");
            } else if (mpctx->demuxer_changed) {
                if (done) {
                    MP_VERBOSE(mpctx, "Dropping finished prefetch because demuxer options changed.\n");
                } else {
                    MP_VERBOSE(mpctx, "Aborting ongoing prefetch because demuxer options changed.\n");
                }
            } else {
                if (done) {
                    MP_VERBOSE(mpctx, "Dropping finished prefetch of wrong URL.\n");
                } else {
                    MP_VERBOSE(mpctx, "Aborting ongoing prefetch of wrong URL.\n");
                }
            }
            cancel_open(mpctx);
        }
    }

    if (!mpctx->open_active)
        start_open(mpctx, url, mpctx->playing->stream_flags, false);

    // If thread failed to start, cancel the playback
    if (!mpctx->open_active)
        goto cancel;

    // User abort should cancel the opener now.
    mp_cancel_set_parent(mpctx->open_cancel, mpctx->playback_abort);

    while (!atomic_load(&mpctx->open_done)) {
        mp_idle(mpctx);

        if (mpctx->stop_play)
            mp_abort_playback_async(mpctx);
    }

    if (mpctx->open_res_demuxer) {
        mpctx->demuxer = mpctx->open_res_demuxer;
        mpctx->open_res_demuxer = NULL;
        mp_cancel_set_parent(mpctx->demuxer->cancel, mpctx->playback_abort);
    } else {
        mpctx->error_playing = mpctx->open_res_error;
    }

cancel:
    cancel_open(mpctx); // cleanup
}

void prefetch_next(struct MPContext *mpctx)
{
    if (!mpctx->opts->prefetch_open || mpctx->open_active)
        return;

    struct playlist_entry *new_entry = mp_next_file(mpctx, +1, false, false);
    if (new_entry && new_entry->filename) {
        MP_VERBOSE(mpctx, "Prefetching: %s\n", new_entry->filename);
        start_open(mpctx, new_entry->filename, new_entry->stream_flags, true);
    }
}

static void clear_playlist_paths(struct MPContext *mpctx)
{
    TA_FREEP(&mpctx->playlist_paths);
    mpctx->playlist_paths_len = 0;
}

static bool infinite_playlist_loading_loop(struct MPContext *mpctx, struct playlist *pl)
{
    if (pl->num_entries) {
        struct playlist_entry *e = pl->entries[0];
        for (int n = 0; n < mpctx->playlist_paths_len; n++) {
            if (strcmp(mpctx->playlist_paths[n], e->filename) == 0) {
                clear_playlist_paths(mpctx);
                return true;
            }
        }
    }
    MP_TARRAY_APPEND(mpctx, mpctx->playlist_paths, mpctx->playlist_paths_len,
                     talloc_strdup(mpctx->playlist_paths, mpctx->filename));
    return false;
}

// Destroy the complex filter, and remove the references to the filter pads.
// (Call cleanup_deassociated_complex_filters() to close decoders/VO/AO
// that are not connected anymore due to this.)
static void deassociate_complex_filters(struct MPContext *mpctx)
{
    for (int n = 0; n < mpctx->num_tracks; n++)
        mpctx->tracks[n]->sink = NULL;
    if (mpctx->vo_chain)
        mpctx->vo_chain->filter_src = NULL;
    if (mpctx->ao_chain)
        mpctx->ao_chain->filter_src = NULL;
    TA_FREEP(&mpctx->lavfi);
    TA_FREEP(&mpctx->lavfi_graph);
}

// Close all decoders and sinks (AO/VO) that are not connected to either
// a track or a filter pad.
static void cleanup_deassociated_complex_filters(struct MPContext *mpctx)
{
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];
        if (!(track->sink || track->vo_c || track->ao_c)) {
            if (track->dec && !track->vo_c && !track->ao_c) {
                talloc_free(track->dec->f);
                track->dec = NULL;
            }
            track->selected = false;
        }
    }

    if (mpctx->vo_chain && !mpctx->vo_chain->dec_src &&
        !mpctx->vo_chain->filter_src)
    {
        uninit_video_chain(mpctx);
    }
    if (mpctx->ao_chain && !mpctx->ao_chain->dec_src &&
        !mpctx->ao_chain->filter_src)
    {
        uninit_audio_chain(mpctx);
    }
}

static void kill_outputs(struct MPContext *mpctx, struct track *track)
{
    if (track->vo_c || track->ao_c) {
        MP_VERBOSE(mpctx, "deselecting track %d for lavfi-complex option\n",
                   track->user_tid);
        mp_switch_track(mpctx, track->type, NULL, 0);
    }
    assert(!(track->vo_c || track->ao_c));
}

// >0: changed, 0: no change, -1: error
static int reinit_complex_filters(struct MPContext *mpctx, bool force_uninit)
{
    char *graph = mpctx->opts->lavfi_complex;
    bool have_graph = graph && graph[0] && !force_uninit;
    if (have_graph && mpctx->lavfi &&
        strcmp(graph, mpctx->lavfi_graph) == 0 &&
        !mp_filter_has_failed(mpctx->lavfi))
        return 0;
    if (!mpctx->lavfi && !have_graph)
        return 0;

    // Deassociate the old filter pads. We leave both sources (tracks) and
    // sinks (AO/VO) "dangling", connected to neither track or filter pad.
    // Later, we either reassociate them with new pads, or uninit them if
    // they are still dangling. This avoids too interruptive actions like
    // recreating the VO.
    deassociate_complex_filters(mpctx);

    bool success = false;
    if (!have_graph) {
        success = true; // normal full removal of graph
        goto done;
    }

    struct mp_lavfi *l =
        mp_lavfi_create_graph(mpctx->filter_root, 0, false, NULL, NULL, graph);
    if (!l)
        goto done;
    mpctx->lavfi = l->f;
    mpctx->lavfi_graph = talloc_strdup(NULL, graph);

    mp_filter_set_error_handler(mpctx->lavfi, mpctx->filter_root);

    for (int n = 0; n < mpctx->lavfi->num_pins; n++)
        mp_pin_disconnect(mpctx->lavfi->pins[n]);

    struct mp_pin *pad = mp_filter_get_named_pin(mpctx->lavfi, "vo");
    if (pad && mp_pin_get_dir(pad) == MP_PIN_OUT) {
        if (mpctx->vo_chain && mpctx->vo_chain->track)
            kill_outputs(mpctx, mpctx->vo_chain->track);
        if (!mpctx->vo_chain) {
            reinit_video_chain_src(mpctx, NULL);
            if (!mpctx->vo_chain)
                goto done;
        }
        struct vo_chain *vo_c = mpctx->vo_chain;
        assert(!vo_c->track);
        vo_c->filter_src = pad;
        mp_pin_connect(vo_c->filter->f->pins[0], vo_c->filter_src);
    }

    pad = mp_filter_get_named_pin(mpctx->lavfi, "ao");
    if (pad && mp_pin_get_dir(pad) == MP_PIN_OUT) {
        if (mpctx->ao_chain && mpctx->ao_chain->track)
            kill_outputs(mpctx, mpctx->ao_chain->track);
        if (!mpctx->ao_chain) {
            reinit_audio_chain_src(mpctx, NULL);
            if (!mpctx->ao_chain)
                goto done;
        }
        struct ao_chain *ao_c = mpctx->ao_chain;
        assert(!ao_c->track);
        ao_c->filter_src = pad;
        mp_pin_connect(ao_c->filter->f->pins[0], ao_c->filter_src);
    }

    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];

        char label[32];
        char prefix;
        switch (track->type) {
        case STREAM_VIDEO: prefix = 'v'; break;
        case STREAM_AUDIO: prefix = 'a'; break;
        default: continue;
        }
        snprintf(label, sizeof(label), "%cid%d", prefix, track->user_tid);

        pad = mp_filter_get_named_pin(mpctx->lavfi, label);
        if (!pad)
            continue;
        if (mp_pin_get_dir(pad) != MP_PIN_IN)
            continue;
        assert(!mp_pin_is_connected(pad));

        assert(!track->sink);

        kill_outputs(mpctx, track);

        track->sink = pad;
        track->selected = true;

        if (!track->dec) {
            if (track->type == STREAM_VIDEO && !init_video_decoder(mpctx, track))
                goto done;
            if (track->type == STREAM_AUDIO && !init_audio_decoder(mpctx, track))
                goto done;
        }

        mp_pin_connect(track->sink, track->dec->f->pins[0]);
    }

    // Don't allow unconnected pins. Libavfilter would make the data flow a
    // real pain anyway.
    for (int n = 0; n < mpctx->lavfi->num_pins; n++) {
        struct mp_pin *pin = mpctx->lavfi->pins[n];
        if (!mp_pin_is_connected(pin)) {
            MP_ERR(mpctx, "Pad %s is not connected to anything.\n",
                   mp_pin_get_name(pin));
            goto done;
        }
    }

    success = true;
done:

    if (!success)
        deassociate_complex_filters(mpctx);

    cleanup_deassociated_complex_filters(mpctx);

    if (mpctx->playback_initialized) {
        for (int n = 0; n < mpctx->num_tracks; n++)
            reselect_demux_stream(mpctx, mpctx->tracks[n], false);
    }

    mp_notify(mpctx, MP_EVENT_TRACKS_CHANGED, NULL);

    return success ? 1 : -1;
}

void update_lavfi_complex(struct MPContext *mpctx)
{
    if (mpctx->playback_initialized) {
        if (reinit_complex_filters(mpctx, false) != 0)
            issue_refresh_seek(mpctx, MPSEEK_EXACT);
    }
}


// Worker thread for loading external files and such. This is needed to avoid
// freezing the core when waiting for network while loading these.
static void load_external_opts_thread(void *p)
{
    void **a = p;
    struct MPContext *mpctx = a[0];
    struct mp_waiter *waiter = a[1];

    mp_core_lock(mpctx);

    load_chapters(mpctx);
    open_external_files(mpctx, mpctx->opts->audio_files, STREAM_AUDIO);
    open_external_files(mpctx, mpctx->opts->sub_name, STREAM_SUB);
    open_external_files(mpctx, mpctx->opts->coverart_files, STREAM_VIDEO);
    open_external_files(mpctx, mpctx->opts->external_files, STREAM_TYPE_COUNT);
    autoload_external_files(mpctx, mpctx->playback_abort);

    mp_waiter_wakeup(waiter, 0);
    mp_wakeup_core(mpctx);
    mp_core_unlock(mpctx);
}

static void load_external_opts(struct MPContext *mpctx)
{
    struct mp_waiter wait = MP_WAITER_INITIALIZER;

    void *a[] = {mpctx, &wait};
    if (!mp_thread_pool_queue(mpctx->thread_pool, load_external_opts_thread, a)) {
        mpctx->stop_play = PT_ERROR;
        return;
    }

    while (!mp_waiter_poll(&wait)) {
        mp_idle(mpctx);

        if (mpctx->stop_play)
            mp_abort_playback_async(mpctx);
    }

    mp_waiter_wait(&wait);
}

static void append_to_watch_history(struct MPContext *mpctx)
{
    if (!mpctx->opts->save_watch_history)
        return;

    void *ctx = talloc_new(NULL);
    char *history_path = mp_get_user_path(ctx, mpctx->global,
                                          mpctx->opts->watch_history_path);
    FILE *history_file = fopen(history_path, "ab");

    if (!history_file) {
        MP_ERR(mpctx, "Failed to open history file: %s\n",
               mp_strerror(errno));
        goto done;
    }

    char *title = (char *)mp_find_non_filename_media_title(mpctx);

    mpv_node_list *list = talloc_zero(ctx, mpv_node_list);
    mpv_node node = {
        .format = MPV_FORMAT_NODE_MAP,
        .u.list = list,
    };
    list->num = title ? 3 : 2;
    list->keys = talloc_array(ctx, char*, list->num);
    list->values = talloc_array(ctx, mpv_node, list->num);
    list->keys[0] = "time";
    list->values[0] = (struct mpv_node) {
        .format = MPV_FORMAT_INT64,
        .u.int64 = time(NULL),
    };
    list->keys[1] = "path";
    list->values[1] = (struct mpv_node) {
        .format = MPV_FORMAT_STRING,
        .u.string = mp_normalize_path(ctx, mpctx->filename),
    };
    if (title) {
        list->keys[2] = "title";
        list->values[2] = (struct mpv_node) {
            .format = MPV_FORMAT_STRING,
            .u.string = title,
        };
    }

    bstr dst = {0};
    json_append(&dst, &node, -1);
    talloc_steal(ctx, dst.start);
    if (!dst.len) {
        MP_ERR(mpctx, "Failed to serialize history entry\n");
        goto done;
    }
    bstr_xappend0(ctx, &dst, "\n");

    int seek = fseek(history_file, 0, SEEK_END);
    off_t history_size = ftell(history_file);
    if (seek != 0 || history_size == -1) {
        MP_ERR(mpctx, "Failed to get history file size: %s\n",
               mp_strerror(errno));
        fclose(history_file);
        goto done;
    }

    bool failed = fwrite(dst.start, dst.len, 1, history_file) != 1 ||
                  fflush(history_file) != 0;

    if (failed) {
        MP_ERR(mpctx, "Failed to write to history file: %s\n",
               mp_strerror(errno));

        int fd = fileno(history_file);
        if (fd == -1 || ftruncate(fd, history_size) == -1)
            MP_ERR(mpctx, "Failed to roll-back history file: %s\n",
                   mp_strerror(errno));
    }

    if (fclose(history_file) != 0)
        MP_ERR(mpctx, "Failed to close history file: %s\n",
               mp_strerror(errno));

done:
    talloc_free(ctx);
}

// Start playing the current playlist entry.
// Handle initialization and deinitialization.
static void play_current_file(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    assert(mpctx->stop_play);
    mpctx->stop_play = 0;

    process_hooks(mpctx, "on_before_start_file");
    if (mpctx->stop_play || !mpctx->playlist->current)
        return;

    mpv_event_start_file start_event = {
        .playlist_entry_id = mpctx->playlist->current->id,
    };
    mpv_event_end_file end_event = {
        .playlist_entry_id = start_event.playlist_entry_id,
    };

    mp_notify(mpctx, MPV_EVENT_START_FILE, &start_event);

    mp_cancel_reset(mpctx->playback_abort);

    mpctx->error_playing = MPV_ERROR_LOADING_FAILED;
    mpctx->filename = NULL;
    mpctx->shown_aframes = 0;
    mpctx->shown_vframes = 0;
    mpctx->last_chapter_seek = -2;
    mpctx->last_chapter_flag = false;
    mpctx->last_chapter = -2;
    mpctx->paused = false;
    mpctx->playing_msg_shown = false;
    mpctx->max_frames = -1;
    mpctx->video_speed = mpctx->audio_speed = opts->playback_speed;
    mpctx->speed_factor_a = mpctx->speed_factor_v = 1.0;
    mpctx->display_sync_error = 0.0;
    mpctx->display_sync_active = false;
    // let get_current_time() show 0 as start time (before playback_pts is set)
    mpctx->last_seek_pts = 0.0;
    mpctx->seek = (struct seek_params){ 0 };
    mpctx->filter_root = mp_filter_create_root(mpctx->global);
    mp_filter_graph_set_wakeup_cb(mpctx->filter_root, mp_wakeup_core_cb, mpctx);
    mp_filter_graph_set_max_run_time(mpctx->filter_root, 0.1);

    reset_playback_state(mpctx);

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    if (mpctx->playlist->num_entries > 10)
        goto terminate_playback;
#endif

    mpctx->playing = mpctx->playlist->current;
    assert(mpctx->playing);
    assert(mpctx->playing->filename);
    mpctx->playing->reserved += 1;

    mpctx->filename = talloc_strdup(NULL, mpctx->playing->filename);
    mpctx->stream_open_filename = mpctx->filename;

    mpctx->add_osd_seek_info &= OSD_SEEK_INFO_CURRENT_FILE;

    if (opts->reset_options) {
        for (int n = 0; opts->reset_options[n]; n++) {
            const char *opt = opts->reset_options[n];
            if (opt[0]) {
                if (strcmp(opt, "all") == 0) {
                    m_config_backup_all_opts(mpctx->mconfig);
                } else {
                    m_config_backup_opt(mpctx->mconfig, opt);
                }
            }
        }
    }

    mp_load_auto_profiles(mpctx);

    bool watch_later = mp_load_playback_resume(mpctx, mpctx->filename);

    load_per_file_options(mpctx->mconfig, mpctx->playing->params,
                          mpctx->playing->num_params);

    mpctx->remaining_file_loops = mpctx->opts->loop_file;
    mp_notify_property(mpctx, "remaining-file-loops");
    mpctx->remaining_ab_loops = mpctx->opts->ab_loop_count;
    mp_notify_property(mpctx, "remaining-ab-loops");

    mpctx->max_frames = opts->play_frames;

    handle_force_window(mpctx, false);

    if (mpctx->playlist->num_entries > 1 ||
        mpctx->playing->playlist_path)
        MP_INFO(mpctx, "Playing: %s\n", mpctx->filename);

    assert(mpctx->demuxer == NULL);

    process_hooks(mpctx, "on_load");
    if (mpctx->stop_play)
        goto terminate_playback;

    if (opts->stream_dump && opts->stream_dump[0]) {
        if (stream_dump(mpctx, mpctx->stream_open_filename) >= 0)
            mpctx->error_playing = 1;
        goto terminate_playback;
    }

    open_demux_reentrant(mpctx);
    if (!mpctx->stop_play && !mpctx->demuxer) {
        process_hooks(mpctx, "on_load_fail");
        if (strcmp(mpctx->stream_open_filename, mpctx->filename) != 0 &&
            !mpctx->stop_play)
        {
            mpctx->error_playing = MPV_ERROR_LOADING_FAILED;
            open_demux_reentrant(mpctx);
        }
    }
    if (!mpctx->demuxer || mpctx->stop_play)
        goto terminate_playback;

    struct playlist *pl = mpctx->demuxer->playlist;
    if (pl) {
        // pl->playlist_dir indicates that the playlist was auto-created from
        // the parent file. In this case, mpctx->filename points to real file.
        if (watch_later && !pl->playlist_dir)
            mp_delete_watch_later_conf(mpctx, mpctx->filename);
        playlist_populate_playlist_path(pl, mpctx->filename);
        if (infinite_playlist_loading_loop(mpctx, pl)) {
            mpctx->stop_play = PT_STOP;
            MP_ERR(mpctx, "Infinite playlist loading loop detected.\n");
            goto terminate_playback;
        }
        transfer_playlist(mpctx, pl, &end_event.playlist_insert_id,
                          &end_event.playlist_insert_num_entries);
        mp_notify_property(mpctx, "playlist");
        mpctx->error_playing = 2;
        goto terminate_playback;
    }

    if (mpctx->opts->rebase_start_time)
        demux_set_ts_offset(mpctx->demuxer, -mpctx->demuxer->start_time);
    enable_demux_thread(mpctx, mpctx->demuxer);

    add_demuxer_tracks(mpctx, mpctx->demuxer);

    load_external_opts(mpctx);
    if (mpctx->stop_play)
        goto terminate_playback;

    check_previous_track_selection(mpctx);

    process_hooks(mpctx, "on_preloaded");
    if (mpctx->stop_play)
        goto terminate_playback;

    if (reinit_complex_filters(mpctx, false) < 0)
        goto terminate_playback;

    for (int t = 0; t < STREAM_TYPE_COUNT; t++) {
        for (int i = 0; i < num_ptracks[t]; i++) {
            struct track *sel = NULL;
            bool taken = (t == STREAM_VIDEO && mpctx->vo_chain) ||
                         (t == STREAM_AUDIO && mpctx->ao_chain);
            if (!taken && opts->stream_auto_sel)
                sel = select_default_track(mpctx, i, t);
            mpctx->current_track[i][t] = sel;
        }
    }
    for (int t = 0; t < STREAM_TYPE_COUNT; t++) {
        for (int i = 0; i < num_ptracks[t]; i++) {
            // One track can strictly feed at most 1 decoder
            struct track *track = mpctx->current_track[i][t];
            if (track) {
                if (track->type != STREAM_SUB &&
                    mpctx->encode_lavc_ctx &&
                    !encode_lavc_stream_type_ok(mpctx->encode_lavc_ctx,
                                                track->type))
                {
                    MP_WARN(mpctx, "Disabling %s (not supported by target "
                            "format).\n", stream_type_name(track->type));
                    mpctx->current_track[i][t] = NULL;
                    mark_track_selection(mpctx, i, t, -2); // disable
                } else if (track->selected) {
                    MP_ERR(mpctx, "Track %d can't be selected twice.\n",
                           track->user_tid);
                    mpctx->current_track[i][t] = NULL;
                    mark_track_selection(mpctx, i, t, -2); // disable
                } else {
                    track->selected = true;
                }
            }

            // Revert selection of unselected tracks to default. This is needed
            // because track properties have inconsistent behavior.
            if (!track && opts->stream_id[i][t] >= 0)
                mark_track_selection(mpctx, i, t, -1); // default
        }
    }

    for (int t = 0; t < STREAM_TYPE_COUNT; t++)
        for (int n = 0; n < mpctx->num_tracks; n++)
            if (mpctx->tracks[n]->type == t)
                reselect_demux_stream(mpctx, mpctx->tracks[n], false);

    update_demuxer_properties(mpctx);

    update_playback_speed(mpctx);

    reinit_video_chain(mpctx);
    reinit_audio_chain(mpctx);
    reinit_sub_all(mpctx);

    if (mpctx->encode_lavc_ctx) {
        if (mpctx->vo_chain)
            encode_lavc_expect_stream(mpctx->encode_lavc_ctx, STREAM_VIDEO);
        if (mpctx->ao_chain)
            encode_lavc_expect_stream(mpctx->encode_lavc_ctx, STREAM_AUDIO);
        encode_lavc_set_metadata(mpctx->encode_lavc_ctx,
                                 mpctx->demuxer->metadata);
    }

    if (!mpctx->vo_chain && !mpctx->ao_chain && opts->stream_auto_sel) {
        MP_FATAL(mpctx, "No video or audio streams selected.\n");
        mpctx->error_playing = MPV_ERROR_NOTHING_TO_PLAY;
        goto terminate_playback;
    }

    if (mpctx->vo_chain && mpctx->vo_chain->is_coverart) {
        MP_INFO(mpctx,
            "Displaying cover art. Use --no-audio-display to prevent this.\n");
    }

    if (!mpctx->vo_chain)
        handle_force_window(mpctx, true);

    MP_VERBOSE(mpctx, "Starting playback...\n");

    mpctx->playback_initialized = true;
    mpctx->playing->playlist_prev_attempt = false;
    mpctx->playlist->playlist_completed = false;
    mpctx->playlist->playlist_started = true;
    mp_notify(mpctx, MPV_EVENT_FILE_LOADED, NULL);
    update_screensaver_state(mpctx);
    clear_playlist_paths(mpctx);

    // Clear out subs from the previous file if the video track is a still image.
    redraw_subs(mpctx);

    if (watch_later)
        mp_delete_watch_later_conf(mpctx, mpctx->filename);

    append_to_watch_history(mpctx);

    if (mpctx->max_frames == 0) {
        if (!mpctx->stop_play)
            mpctx->stop_play = PT_NEXT_ENTRY;
        mpctx->error_playing = 0;
        goto terminate_playback;
    }

    if (opts->demuxer_cache_wait) {
        demux_start_prefetch(mpctx->demuxer);

        while (!mpctx->stop_play) {
            struct demux_reader_state s;
            demux_get_reader_state(mpctx->demuxer, &s);
            if (s.idle)
                break;

            mp_idle(mpctx);
        }
    }

    // (Not get_play_start_pts(), which would always trigger a seek.)
    double play_start_pts = rel_time_to_abs(mpctx, opts->play_start);

    // Backward playback -> start from end by default.
    if (play_start_pts == MP_NOPTS_VALUE && opts->play_dir < 0)
        play_start_pts = get_start_time(mpctx, -1);

    if (play_start_pts != MP_NOPTS_VALUE) {
        queue_seek(mpctx, MPSEEK_ABSOLUTE, play_start_pts, MPSEEK_DEFAULT, 0);
        execute_queued_seek(mpctx);
    }

    update_internal_pause_state(mpctx);

    mpctx->error_playing = 0;
    mpctx->in_playloop = true;
    while (!mpctx->stop_play)
        run_playloop(mpctx);
    mpctx->in_playloop = false;

    MP_VERBOSE(mpctx, "EOF code: %d  \n", mpctx->stop_play);

terminate_playback:

    if (!mpctx->stop_play)
        mpctx->stop_play = PT_ERROR;

    if (mpctx->stop_play != AT_END_OF_FILE)
        clear_audio_output_buffers(mpctx);

    update_core_idle_state(mpctx);

    if (mpctx->step_frames) {
        opts->pause = true;
        m_config_notify_change_opt_ptr(mpctx->mconfig, &opts->pause);
    }

    process_hooks(mpctx, "on_unload");

    // time to uninit all, except global stuff:
    reinit_complex_filters(mpctx, true);
    uninit_audio_chain(mpctx);
    uninit_video_chain(mpctx);
    uninit_sub_all(mpctx);
    if (!opts->gapless_audio && !mpctx->encode_lavc_ctx)
        uninit_audio_out(mpctx);

    mpctx->playback_initialized = false;

    uninit_demuxer(mpctx);

    // Possibly stop ongoing async commands.
    mp_abort_playback_async(mpctx);

    m_config_restore_backups(mpctx->mconfig);

    TA_FREEP(&mpctx->filter_root);
    talloc_free(mpctx->filtered_tags);
    mpctx->filtered_tags = NULL;

    mp_notify(mpctx, MP_EVENT_TRACKS_CHANGED, NULL);

    if (encode_lavc_didfail(mpctx->encode_lavc_ctx))
        mpctx->stop_play = PT_ERROR;

    if (mpctx->stop_play == PT_ERROR && !mpctx->error_playing)
        mpctx->error_playing = MPV_ERROR_GENERIC;

    bool nothing_played = !mpctx->shown_aframes && !mpctx->shown_vframes &&
                          mpctx->error_playing <= 0;
    bool playlist_prev_continue = false;
    switch (mpctx->stop_play) {
    case PT_ERROR:
    case AT_END_OF_FILE:
    {
        if (mpctx->error_playing == 0 && nothing_played)
            mpctx->error_playing = MPV_ERROR_NOTHING_TO_PLAY;
        if (mpctx->error_playing < 0) {
            end_event.error = mpctx->error_playing;
            end_event.reason = MPV_END_FILE_REASON_ERROR;
        } else if (mpctx->error_playing == 2) {
            end_event.reason = MPV_END_FILE_REASON_REDIRECT;
        } else {
            end_event.reason = MPV_END_FILE_REASON_EOF;
        }
        if (mpctx->playing) {
            mpctx->playing->init_failed = nothing_played;
            playlist_prev_continue = mpctx->playing->playlist_prev_attempt &&
                                     nothing_played;
            mpctx->playing->playlist_prev_attempt = false;
        }
        break;
    }
    // Note that error_playing is meaningless in these cases.
    case PT_NEXT_ENTRY:
    case PT_CURRENT_ENTRY:
    case PT_STOP:           end_event.reason = MPV_END_FILE_REASON_STOP; break;
    case PT_QUIT:           end_event.reason = MPV_END_FILE_REASON_QUIT; break;
    };
    mp_notify(mpctx, MPV_EVENT_END_FILE, &end_event);

    MP_VERBOSE(mpctx, "finished playback, %s (reason %d)\n",
               mpv_error_string(end_event.error), end_event.reason);
    if (end_event.error == MPV_ERROR_UNKNOWN_FORMAT)
        MP_ERR(mpctx, "Failed to recognize file format.\n");

    if (mpctx->playing)
        playlist_entry_unref(mpctx->playing);
    mpctx->playing = NULL;
    talloc_free(mpctx->filename);
    mpctx->filename = NULL;
    mpctx->stream_open_filename = NULL;

    if (end_event.error < 0 && nothing_played) {
        mpctx->files_broken++;
    } else if (end_event.error < 0) {
        mpctx->files_errored++;
    } else {
        mpctx->files_played++;
    }

    assert(mpctx->stop_play);

    process_hooks(mpctx, "on_after_end_file");

    if (playlist_prev_continue) {
        struct playlist_entry *e = mp_next_file(mpctx, -1, false, true);
        if (e) {
            mp_set_playlist_entry(mpctx, e);
            play_current_file(mpctx);
        }
    }
}

// Determine the next file to play. Note that if this function returns non-NULL,
// it can have side-effects and mutate mpctx.
//  direction: -1 (previous) or +1 (next)
//  force: if true, don't skip playlist entries marked as failed
//  update_loop: whether to decrement --loop-playlist=N if it was specified
struct playlist_entry *mp_next_file(struct MPContext *mpctx, int direction,
                                    bool force, bool update_loop)
{
    struct playlist_entry *next = playlist_get_next(mpctx->playlist, direction);
    if (next && direction < 0 && !force)
        next->playlist_prev_attempt = true;
    if (!next && mpctx->opts->loop_times != 1) {
        if (direction > 0) {
            if (mpctx->opts->shuffle) {
                if (!update_loop)
                    return NULL;
                playlist_shuffle(mpctx->playlist);
            }
            next = playlist_get_first(mpctx->playlist);
            if (next && mpctx->opts->loop_times > 1 && update_loop) {
                mpctx->opts->loop_times--;
                m_config_notify_change_opt_ptr(mpctx->mconfig,
                                               &mpctx->opts->loop_times);
            }
        } else {
            next = playlist_get_last(mpctx->playlist);
        }
        bool ignore_failures = mpctx->opts->loop_times == -2;
        if (!force && next && next->init_failed && !ignore_failures) {
            // Don't endless loop if no file in playlist is playable
            bool all_failed = true;
            for (int n = 0; n < mpctx->playlist->num_entries; n++) {
                all_failed &= mpctx->playlist->entries[n]->init_failed;
                if (!all_failed)
                    break;
            }
            if (all_failed)
                next = NULL;
        }
    }
    return next;
}

// Play all entries on the playlist, starting from the current entry.
// Return if all done.
void mp_play_files(struct MPContext *mpctx)
{
    stats_register_thread_cputime(mpctx->stats, "thread");

    // Wait for all scripts to load before possibly starting playback.
    if (!mp_clients_all_initialized(mpctx)) {
        MP_VERBOSE(mpctx, "Waiting for scripts...\n");
        while (!mp_clients_all_initialized(mpctx))
            mp_idle(mpctx);
        mp_wakeup_core(mpctx); // avoid lost wakeups during waiting
        MP_VERBOSE(mpctx, "Done loading scripts.\n");
    }
    // After above is finished; but even if it's skipped.
    mp_msg_set_early_logging(mpctx->global, false);

    prepare_playlist(mpctx, mpctx->playlist);

    for (;;) {
        idle_loop(mpctx);

        if (mpctx->stop_play == PT_QUIT)
            break;

        if (mpctx->playlist->current)
            play_current_file(mpctx);

        if (mpctx->stop_play == PT_QUIT)
            break;

        struct playlist_entry *new_entry = NULL;
        if (mpctx->stop_play == PT_NEXT_ENTRY || mpctx->stop_play == PT_ERROR ||
            mpctx->stop_play == AT_END_OF_FILE)
        {
            new_entry = mp_next_file(mpctx, +1, false, true);
        } else if (mpctx->stop_play == PT_CURRENT_ENTRY) {
            new_entry = mpctx->playlist->current;
        }

        if (!new_entry)
            mpctx->playlist->playlist_completed = true;

        mpctx->playlist->current = new_entry;
        mpctx->playlist->current_was_replaced = false;
        mpctx->stop_play = new_entry ? PT_NEXT_ENTRY : PT_STOP;

        if (!mpctx->playlist->current && mpctx->opts->player_idle_mode < 2)
            break;
    }

    cancel_open(mpctx);

    if (mpctx->encode_lavc_ctx) {
        // Make sure all streams get finished.
        uninit_audio_out(mpctx);
        uninit_video_out(mpctx);

        if (!encode_lavc_free(mpctx->encode_lavc_ctx))
            mpctx->files_errored += 1;

        mpctx->encode_lavc_ctx = NULL;
    }
}

// Abort current playback and set the given entry to play next.
// e must be on the mpctx->playlist.
void mp_set_playlist_entry(struct MPContext *mpctx, struct playlist_entry *e)
{
    assert(!e || playlist_entry_to_index(mpctx->playlist, e) >= 0);
    mpctx->playlist->current = e;
    mpctx->playlist->current_was_replaced = false;
    mp_notify(mpctx, MP_EVENT_CHANGE_PLAYLIST, NULL);
    // Make it pick up the new entry.
    if (mpctx->stop_play != PT_QUIT)
        mpctx->stop_play = e ? PT_CURRENT_ENTRY : PT_STOP;
    mp_wakeup_core(mpctx);
}
