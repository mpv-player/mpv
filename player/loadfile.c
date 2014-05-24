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

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>

#include <libavutil/avutil.h>

#include "config.h"
#include "talloc.h"

#include "osdep/io.h"
#include "osdep/terminal.h"
#include "osdep/timer.h"

#include "common/msg.h"
#include "options/path.h"
#include "options/m_config.h"
#include "options/parse_configfile.h"
#include "common/playlist.h"
#include "options/options.h"
#include "options/m_property.h"
#include "common/common.h"
#include "common/encode.h"
#include "input/input.h"

#include "audio/mixer.h"
#include "audio/audio.h"
#include "audio/audio_buffer.h"
#include "audio/decode/dec_audio.h"
#include "audio/out/ao.h"
#include "demux/demux.h"
#include "stream/stream.h"
#include "stream/resolve/resolve.h"
#include "sub/ass_mp.h"
#include "sub/dec_sub.h"
#include "sub/find_subfiles.h"
#include "video/decode/dec_video.h"
#include "video/out/vo.h"

#include "core.h"
#include "command.h"
#include "libmpv/client.h"

#if HAVE_DVBIN
#include "stream/dvbin.h"
#endif

static void uninit_sub(struct MPContext *mpctx, int order)
{
    if (mpctx->d_sub[order])
        sub_reset(mpctx->d_sub[order]);
    mpctx->d_sub[order] = NULL; // Note: not free'd.
    int obj = order ? OSDTYPE_SUB2 : OSDTYPE_SUB;
    osd_set_sub(mpctx->osd, obj, NULL);
    reset_subtitles(mpctx, order);
    reselect_demux_streams(mpctx);
}

void uninit_player(struct MPContext *mpctx, unsigned int mask)
{
    struct MPOpts *opts = mpctx->opts;

    mask &= mpctx->initialized_flags;

    MP_DBG(mpctx, "\n*** uninit(0x%X)\n", mask);

    if (mask & INITIALIZED_ACODEC) {
        mpctx->initialized_flags &= ~INITIALIZED_ACODEC;
        mixer_uninit_audio(mpctx->mixer);
        audio_uninit(mpctx->d_audio);
        mpctx->d_audio = NULL;
        reselect_demux_streams(mpctx);
    }

    if (mask & INITIALIZED_SUB) {
        mpctx->initialized_flags &= ~INITIALIZED_SUB;
        uninit_sub(mpctx, 0);
    }
    if (mask & INITIALIZED_SUB2) {
        mpctx->initialized_flags &= ~INITIALIZED_SUB2;
        uninit_sub(mpctx, 1);
    }

    if (mask & INITIALIZED_LIBASS) {
        mpctx->initialized_flags &= ~INITIALIZED_LIBASS;
#if HAVE_LIBASS
        if (mpctx->ass_renderer)
            ass_renderer_done(mpctx->ass_renderer);
        mpctx->ass_renderer = NULL;
        ass_clear_fonts(mpctx->ass_library);
#endif
    }

    if (mask & INITIALIZED_VCODEC) {
        mpctx->initialized_flags &= ~INITIALIZED_VCODEC;
        if (mpctx->d_video)
            video_uninit(mpctx->d_video);
        mpctx->d_video = NULL;
        mpctx->sync_audio_to_video = false;
        reselect_demux_streams(mpctx);
    }

    if (mask & INITIALIZED_DEMUXER) {
        mpctx->initialized_flags &= ~INITIALIZED_DEMUXER;
        assert(!(mpctx->initialized_flags &
                 (INITIALIZED_VCODEC | INITIALIZED_ACODEC |
                  INITIALIZED_SUB2 | INITIALIZED_SUB)));
        for (int i = 0; i < mpctx->num_tracks; i++) {
            talloc_free(mpctx->tracks[i]);
        }
        mpctx->num_tracks = 0;
        for (int r = 0; r < NUM_PTRACKS; r++) {
            for (int t = 0; t < STREAM_TYPE_COUNT; t++)
                mpctx->current_track[r][t] = NULL;
        }
        assert(!mpctx->d_video && !mpctx->d_audio &&
               !mpctx->d_sub[0] && !mpctx->d_sub[1]);
        mpctx->master_demuxer = NULL;
        for (int i = 0; i < mpctx->num_sources; i++) {
            uninit_subs(mpctx->sources[i]);
            struct demuxer *demuxer = mpctx->sources[i];
            struct stream *stream = demuxer->stream;
            free_demuxer(demuxer);
            if (stream != mpctx->stream)
                free_stream(stream);
        }
        talloc_free(mpctx->sources);
        mpctx->sources = NULL;
        mpctx->demuxer = NULL;
        mpctx->num_sources = 0;
        talloc_free(mpctx->timeline);
        mpctx->timeline = NULL;
        mpctx->num_timeline_parts = 0;
        talloc_free(mpctx->chapters);
        mpctx->chapters = NULL;
        mpctx->num_chapters = 0;
        mpctx->video_offset = 0;
    }

    // kill the cache process:
    if (mask & INITIALIZED_STREAM) {
        mpctx->initialized_flags &= ~INITIALIZED_STREAM;
        if (mpctx->stream)
            free_stream(mpctx->stream);
        mpctx->stream = NULL;
    }

    if (mask & INITIALIZED_VO) {
        mpctx->initialized_flags &= ~INITIALIZED_VO;
        vo_destroy(mpctx->video_out);
        mpctx->video_out = NULL;
    }

    if (mask & INITIALIZED_AO) {
        struct ao *ao = mpctx->ao;
        mpctx->initialized_flags &= ~INITIALIZED_AO;
        if (ao) {
            // Note: with gapless_audio, stop_play is not correctly set
            if (opts->gapless_audio || mpctx->stop_play == AT_END_OF_FILE)
                ao_drain(ao);
            ao_uninit(ao);
        }
        mpctx->ao = NULL;
    }

    if (mask & INITIALIZED_PLAYBACK)
        mpctx->initialized_flags &= ~INITIALIZED_PLAYBACK;
}

static void print_stream(struct MPContext *mpctx, struct track *t)
{
    struct sh_stream *s = t->stream;
    const char *tname = "?";
    const char *selopt = "?";
    const char *langopt = "?";
    const char *iid = NULL;
    switch (t->type) {
    case STREAM_VIDEO:
        tname = "Video"; selopt = "vid"; langopt = NULL; iid = "VID";
        break;
    case STREAM_AUDIO:
        tname = "Audio"; selopt = "aid"; langopt = "alang"; iid = "AID";
        break;
    case STREAM_SUB:
        tname = "Subs"; selopt = "sid"; langopt = "slang"; iid = "SID";
        break;
    }
    MP_INFO(mpctx, "[stream] %-5s %3s", tname, t->selected ? "(+)" : "");
    MP_INFO(mpctx, " --%s=%d", selopt, t->user_tid);
    if (t->lang && langopt)
        MP_INFO(mpctx, " --%s=%s", langopt, t->lang);
    if (t->default_track)
        MP_INFO(mpctx, " (*)");
    if (t->attached_picture)
        MP_INFO(mpctx, " [P]");
    if (t->title)
        MP_INFO(mpctx, " '%s'", t->title);
    const char *codec = s ? s->codec : NULL;
    MP_INFO(mpctx, " (%s)", codec ? codec : "<unknown>");
    if (t->is_external)
        MP_INFO(mpctx, " (external)");
    MP_INFO(mpctx, "\n");
    // legacy compatibility
    if (!iid)
        return;
}

static void print_file_properties(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->master_demuxer;
    if (demuxer->num_editions > 1) {
        for (int n = 0; n < demuxer->num_editions; n++) {
            struct demux_edition *edition = &demuxer->editions[n];
            MP_INFO(mpctx, "[edition] %3s --edition=%d",
                    n == demuxer->edition ? "(+)" : "", n);
            char *name = mp_tags_get_str(edition->metadata, "title");
            if (name)
                MP_INFO(mpctx, " '%s'", name);
            if (edition->default_edition)
                MP_INFO(mpctx, " (*)");
            MP_INFO(mpctx, "\n");
        }
    }
    for (int t = 0; t < STREAM_TYPE_COUNT; t++) {
        for (int n = 0; n < mpctx->num_tracks; n++)
            if (mpctx->tracks[n]->type == t)
                print_stream(mpctx, mpctx->tracks[n]);
    }
}

// Enable needed streams, disable others.
// Note that switching all tracks at once (instead when initializing something)
// can be important, because reading from a demuxer stream (e.g. during init)
// will implicitly discard interleaved packets from unselected streams.
void reselect_demux_streams(struct MPContext *mpctx)
{
    // Note: we assume that all demuxer streams are covered by the track list.
    for (int t = 0; t < mpctx->num_tracks; t++) {
        struct track *track = mpctx->tracks[t];
        if (track->demuxer && track->stream)
            demuxer_select_track(track->demuxer, track->stream, track->selected);
    }
}

// External demuxers might need a seek to the current playback position.
static void external_track_seek(struct MPContext *mpctx, struct track *track)
{
    if (track && track->demuxer && track->selected && track->is_external) {
        for (int t = 0; t < mpctx->num_tracks; t++) {
            struct track *other = mpctx->tracks[t];
            if (other->demuxer == track->demuxer &&
                demuxer_stream_is_selected(other->demuxer, other->stream))
                return;
        }
        double pts = get_main_demux_pts(mpctx);
        demux_seek(track->demuxer, pts, SEEK_ABSOLUTE);
    }
}

struct sh_stream *init_demux_stream(struct MPContext *mpctx, struct track *track)
{
    external_track_seek(mpctx, track);
    return track ? track->stream : NULL;
}

static struct sh_stream *select_fallback_stream(struct demuxer *d,
                                                enum stream_type type,
                                                int index)
{
    struct sh_stream *best_stream = NULL;
    for (int n = 0; n < d->num_streams; n++) {
        struct sh_stream *s = d->streams[n];
        if (s->type == type) {
            best_stream = s;
            if (index == 0)
                break;
            index -= 1;
        }
    }
    return best_stream;
}

bool timeline_set_part(struct MPContext *mpctx, int i, bool force)
{
    struct timeline_part *p = mpctx->timeline + mpctx->timeline_part;
    struct timeline_part *n = mpctx->timeline + i;
    mpctx->timeline_part = i;
    mpctx->video_offset = n->start - n->source_start;
    if (n->source == p->source && !force)
        return false;
    enum stop_play_reason orig_stop_play = mpctx->stop_play;
    if (!mpctx->d_video && mpctx->stop_play == KEEP_PLAYING)
        mpctx->stop_play = AT_END_OF_FILE;  // let audio uninit drain data
    uninit_player(mpctx, INITIALIZED_VCODEC | (mpctx->opts->fixed_vo ? 0 : INITIALIZED_VO) | (mpctx->opts->gapless_audio ? 0 : INITIALIZED_AO) | INITIALIZED_ACODEC | INITIALIZED_SUB | INITIALIZED_SUB2);
    mpctx->stop_play = orig_stop_play;

    mpctx->demuxer = n->source;
    mpctx->stream = mpctx->demuxer->stream;

    // While another timeline was active, the selection of active tracks might
    // have been changed - possibly we need to update this source.
    for (int x = 0; x < mpctx->num_tracks; x++) {
        struct track *track = mpctx->tracks[x];
        if (track->under_timeline) {
            track->demuxer = mpctx->demuxer;
            track->stream = demuxer_stream_by_demuxer_id(track->demuxer,
                                                         track->type,
                                                         track->demuxer_id);
            // EDL can have mismatched files in the same timeline
            if (!track->stream) {
                track->stream = select_fallback_stream(track->demuxer,
                                                       track->type,
                                                       track->user_tid - 1);
            }
        }
    }
    reselect_demux_streams(mpctx);

    return true;
}

// Given pts, switch playback to the corresponding part.
// Return offset within that part.
double timeline_set_from_time(struct MPContext *mpctx, double pts, bool *need_reset)
{
    if (pts < 0)
        pts = 0;
    for (int i = 0; i < mpctx->num_timeline_parts; i++) {
        struct timeline_part *p = mpctx->timeline + i;
        if (pts < (p + 1)->start) {
            *need_reset = timeline_set_part(mpctx, i, false);
            return pts - p->start + p->source_start;
        }
    }
    return -1;
}

// Map stream number (as used by libdvdread) to MPEG IDs (as used by demuxer).
static int map_id_from_demuxer(struct demuxer *d, enum stream_type type, int id)
{
    if (d->stream->uncached_type == STREAMTYPE_DVD && type == STREAM_SUB)
        id = id & 0x1F;
    return id;
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
                                      struct sh_stream *stream,
                                      bool under_timeline)
{
    for (int i = 0; i < mpctx->num_tracks; i++) {
        struct track *track = mpctx->tracks[i];
        if (track->stream == stream)
            return track;
        // DVD subtitle track that was added later
        if (stream->type == STREAM_SUB && track->type == STREAM_SUB &&
            map_id_from_demuxer(stream->demuxer, stream->type,
                                stream->demuxer_id) == track->demuxer_id
            && !track->stream)
        {
            track->stream = stream;
            track->demuxer_id = stream->demuxer_id;
            // Initialize lazily selected track
            demuxer_select_track(track->demuxer, stream, track->selected);
            if (mpctx->current_track[0][STREAM_SUB] == track)
                reinit_subs(mpctx, 0);
            if (mpctx->current_track[1][STREAM_SUB] == track)
                reinit_subs(mpctx, 1);
            return track;
        }
    }

    struct track *track = talloc_ptrtype(NULL, track);
    *track = (struct track) {
        .type = stream->type,
        .user_tid = find_new_tid(mpctx, stream->type),
        .demuxer_id = stream->demuxer_id,
        .title = stream->title,
        .default_track = stream->default_track,
        .attached_picture = stream->attached_picture != NULL,
        .lang = stream->lang,
        .under_timeline = under_timeline,
        .demuxer = stream->demuxer,
        .stream = stream,
    };
    MP_TARRAY_APPEND(mpctx, mpctx->tracks, mpctx->num_tracks, track);

    // Needed for DVD and Blu-ray.
    struct stream *st = track->demuxer->stream;
    if (!track->lang && (st->uncached_type == STREAMTYPE_BLURAY ||
                         st->uncached_type == STREAMTYPE_DVD))
    {
        struct stream_lang_req req = {
            .type = track->type,
            .id = map_id_from_demuxer(track->demuxer, track->type,
                                      track->demuxer_id)
        };
        stream_control(st, STREAM_CTRL_GET_LANG, &req);
        if (req.name[0])
            track->lang = talloc_strdup(track, req.name);
    }

    demuxer_select_track(track->demuxer, stream, false);

    mp_notify(mpctx, MPV_EVENT_TRACKS_CHANGED, NULL);

    return track;
}

void add_demuxer_tracks(struct MPContext *mpctx, struct demuxer *demuxer)
{
    for (int n = 0; n < demuxer->num_streams; n++)
        add_stream_track(mpctx, demuxer->streams[n], !!mpctx->timeline);
}

static void add_dvd_tracks(struct MPContext *mpctx)
{
    struct demuxer *demuxer = mpctx->demuxer;
    struct stream *stream = demuxer->stream;
    struct stream_dvd_info_req info;
    if (stream->uncached_type != STREAMTYPE_DVD)
        return;
    if (stream_control(stream, STREAM_CTRL_GET_DVD_INFO, &info) > 0) {
        for (int n = 0; n < info.num_subs; n++) {
            struct track *track = talloc_ptrtype(NULL, track);
            *track = (struct track) {
                .type = STREAM_SUB,
                .user_tid = find_new_tid(mpctx, STREAM_SUB),
                .demuxer_id = n,
                .demuxer = mpctx->demuxer,
            };
            MP_TARRAY_APPEND(mpctx, mpctx->tracks, mpctx->num_tracks, track);

            struct stream_lang_req req = {.type = STREAM_SUB, .id = n};
            stream_control(stream, STREAM_CTRL_GET_LANG, &req);
            track->lang = talloc_strdup(track, req.name);

            mp_notify(mpctx, MPV_EVENT_TRACKS_CHANGED, NULL);
        }
    }
    demuxer_enable_autoselect(demuxer);
}

// Result numerically higher => better match. 0 == no match.
static int match_lang(char **langs, char *lang)
{
    for (int idx = 0; langs && langs[idx]; idx++) {
        if (lang && strcmp(langs[idx], lang) == 0)
            return INT_MAX - idx;
    }
    return 0;
}

/* Get the track wanted by the user.
 * tid is the track ID requested by the user (-2: deselect, -1: default)
 * lang is a string list, NULL is same as empty list
 * Sort tracks based on the following criteria, and pick the first:
 * 0) track matches tid (always wins)
 * 1) track is external (no_default cancels this)
 * 1b) track was passed explicitly (is not an auto-loaded subtitle)
 * 2) earlier match in lang list
 * 3) track is marked default
 * 4) lower track number
 * If select_fallback is not set, 4) is only used to determine whether a
 * matching track is preferred over another track. Otherwise, always pick a
 * track (if nothing else matches, return the track with lowest ID).
 */
// Return whether t1 is preferred over t2
static bool compare_track(struct track *t1, struct track *t2, char **langs)
{
    bool ext1 = t1->is_external && !t1->no_default;
    bool ext2 = t2->is_external && !t2->no_default;
    if (ext1 != ext2)
        return ext1;
    if (t1->auto_loaded != t2->auto_loaded)
        return !t1->auto_loaded;
    int l1 = match_lang(langs, t1->lang), l2 = match_lang(langs, t2->lang);
    if (l1 != l2)
        return l1 > l2;
    if (t1->default_track != t2->default_track)
        return t1->default_track;
    if (t1->attached_picture != t2->attached_picture)
        return !t1->attached_picture;
    return t1->user_tid <= t2->user_tid;
}
static struct track *select_track(struct MPContext *mpctx,
                                  enum stream_type type, int tid, char **langs)
{
    if (tid == -2)
        return NULL;
    bool select_fallback = type == STREAM_VIDEO || type == STREAM_AUDIO;
    struct track *pick = NULL;
    for (int n = 0; n < mpctx->num_tracks; n++) {
        struct track *track = mpctx->tracks[n];
        if (track->type != type)
            continue;
        if (track->user_tid == tid)
            return track;
        if (!pick || compare_track(track, pick, langs))
            pick = track;
    }
    if (pick && !select_fallback && !(pick->is_external && !pick->no_default)
        && !match_lang(langs, pick->lang) && !pick->default_track)
        pick = NULL;
    if (pick && pick->attached_picture && !mpctx->opts->audio_display)
        pick = NULL;
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
        // Reset selection, but only if they're not "auto" or "off".
        if (opts->video_id >= 0)
            mpctx->opts->video_id = -1;
        if (opts->audio_id >= 0)
            mpctx->opts->audio_id = -1;
        if (opts->sub_id >= 0)
            mpctx->opts->sub_id = -1;
        if (opts->sub2_id >= 0)
            mpctx->opts->sub2_id = -2;
        talloc_free(mpctx->track_layout_hash);
        mpctx->track_layout_hash = NULL;
    }
    talloc_free(h);
}

void mp_switch_track_n(struct MPContext *mpctx, int order, enum stream_type type,
                       struct track *track)
{
    assert(!track || track->type == type);
    assert(order >= 0 && order < NUM_PTRACKS);

    struct track *current = mpctx->current_track[order][type];
    if (track == current)
        return;

    if (track && track->selected) {
        // Track has been selected in a different order parameter.
        MP_ERR(mpctx, "Track %d is already selected.\n", track->user_tid);
        return;
    }

    if (order == 0) {
        if (type == STREAM_VIDEO) {
            int uninit = INITIALIZED_VCODEC;
            if (!mpctx->opts->force_vo)
                uninit |= mpctx->opts->fixed_vo && track ? 0 : INITIALIZED_VO;
            uninit_player(mpctx, uninit);
        } else if (type == STREAM_AUDIO) {
            uninit_player(mpctx, INITIALIZED_AO | INITIALIZED_ACODEC);
        } else if (type == STREAM_SUB) {
            uninit_player(mpctx, INITIALIZED_SUB);
        }
    } else if (order == 1) {
        if (type == STREAM_SUB)
            uninit_player(mpctx, INITIALIZED_SUB2);
    }

    if (current)
        current->selected = false;

    reselect_demux_streams(mpctx);

    mpctx->current_track[order][type] = track;

    if (track)
        track->selected = true;

    reselect_demux_streams(mpctx);

    if (type == STREAM_VIDEO && order == 0) {
        reinit_video_chain(mpctx);
    } else if (type == STREAM_AUDIO && order == 0) {
        reinit_audio_chain(mpctx);
    } else if (type == STREAM_SUB && order >= 0 && order <= 2) {
        reinit_subs(mpctx, order);
    }

    mp_notify(mpctx, MPV_EVENT_TRACK_SWITCHED, NULL);
    osd_changed_all(mpctx->osd);

    talloc_free(mpctx->track_layout_hash);
    mpctx->track_layout_hash = talloc_steal(mpctx, track_layout_hash(mpctx));
}

void mp_switch_track(struct MPContext *mpctx, enum stream_type type,
                     struct track *track)
{
    mp_switch_track_n(mpctx, 0, type, track);
}

void mp_deselect_track(struct MPContext *mpctx, struct track *track)
{
    if (track && track->selected) {
        for (int t = 0; t < NUM_PTRACKS; t++)
            mp_switch_track_n(mpctx, t, track->type, NULL);
    }
}

// Mark the current track selection as explicitly user-requested. (This is
// different from auto-selection or disabling a track due to errors.)
void mp_mark_user_track_selection(struct MPContext *mpctx, int order,
                                  enum stream_type type)
{
    struct track *track = mpctx->current_track[order][type];
    int user_tid = track ? track->user_tid : -2;
    if (type == STREAM_VIDEO && order == 0) {
        mpctx->opts->video_id = user_tid;
    } else if (type == STREAM_AUDIO && order == 0) {
        mpctx->opts->audio_id = user_tid;
    } else if (type == STREAM_SUB && order == 0) {
        mpctx->opts->sub_id = user_tid;
    } else if (type == STREAM_SUB && order == 1) {
        mpctx->opts->sub2_id = user_tid;
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
    if (track->under_timeline)
        return false;
    if (!track->is_external)
        return false;

    mp_deselect_track(mpctx, track);
    if (track->selected)
        return false;

    int index = 0;
    while (index < mpctx->num_tracks && mpctx->tracks[index] != track)
        index++;
    assert(index < mpctx->num_tracks);
    while (index + 1 < mpctx->num_tracks) {
        mpctx->tracks[index] = mpctx->tracks[index + 1];
        index++;
    }
    mpctx->num_tracks--;
    talloc_free(track);

    mp_notify(mpctx, MPV_EVENT_TRACKS_CHANGED, NULL);

    return true;
}

static void open_subtitles_from_options(struct MPContext *mpctx)
{
    if (mpctx->opts->sub_name) {
        for (int i = 0; mpctx->opts->sub_name[i] != NULL; ++i)
            mp_add_subtitles(mpctx, mpctx->opts->sub_name[i]);
    }
    if (mpctx->opts->sub_auto >= 0) { // auto load sub file ...
        void *tmp = talloc_new(NULL);
        char *base_filename = mpctx->filename;
        char *stream_filename = NULL;
        if (stream_control(mpctx->stream, STREAM_CTRL_GET_BASE_FILENAME,
                           &stream_filename) > 0)
            base_filename = talloc_steal(tmp, stream_filename);
        struct subfn *list = find_text_subtitles(mpctx->global, base_filename);
        talloc_steal(tmp, list);
        for (int i = 0; list && list[i].fname; i++) {
            char *filename = list[i].fname;
            char *lang = list[i].lang;
            for (int n = 0; n < mpctx->num_sources; n++) {
                if (strcmp(mpctx->sources[n]->stream->url, filename) == 0)
                    goto skip;
            }
            struct track *track = mp_add_subtitles(mpctx, filename);
            if (track) {
                track->auto_loaded = true;
                if (!track->lang)
                    track->lang = talloc_strdup(track, lang);
            }
        skip:;
        }
        talloc_free(tmp);
    }
}

static struct track *open_external_file(struct MPContext *mpctx, char *filename,
                                        char *demuxer_name,
                                        enum stream_type filter)
{
    struct MPOpts *opts = mpctx->opts;
    if (!filename)
        return NULL;
    char *disp_filename = filename;
    if (strncmp(disp_filename, "memory://", 9) == 0)
        disp_filename = "memory://"; // avoid noise
    struct stream *stream = stream_open(filename, mpctx->global);
    if (!stream)
        goto err_out;
    if (filter != STREAM_SUB)
        stream_enable_cache(&stream, &opts->stream_cache);
    struct demuxer_params params = {
        .expect_subtitle = filter == STREAM_SUB,
    };
    struct demuxer *demuxer =
        demux_open(stream, demuxer_name, &params, mpctx->global);
    if (!demuxer) {
        free_stream(stream);
        goto err_out;
    }
    struct track *first = NULL;
    for (int n = 0; n < demuxer->num_streams; n++) {
        struct sh_stream *sh = demuxer->streams[n];
        if (sh->type == filter) {
            struct track *t = add_stream_track(mpctx, sh, false);
            t->is_external = true;
            t->title = talloc_strdup(t, disp_filename);
            t->external_filename = talloc_strdup(t, filename);
            first = t;
        }
    }
    if (!first) {
        free_demuxer(demuxer);
        free_stream(stream);
        MP_WARN(mpctx, "No streams added from file %s.\n",
                disp_filename);
        goto err_out;
    }
    MP_TARRAY_APPEND(NULL, mpctx->sources, mpctx->num_sources, demuxer);
    return first;

err_out:
    MP_ERR(mpctx, "Can not open external file %s.\n",
           disp_filename);
    return false;
}

static void open_audiofiles_from_options(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    open_external_file(mpctx, opts->audio_stream, opts->audio_demuxer_name,
                       STREAM_AUDIO);
}

struct track *mp_add_subtitles(struct MPContext *mpctx, char *filename)
{
    struct MPOpts *opts = mpctx->opts;
    return open_external_file(mpctx, filename, opts->sub_demuxer_name,
                              STREAM_SUB);
}

static void open_subtitles_from_resolve(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct mp_resolve_result *res = mpctx->resolve_result;
    if (!res)
        return;
    for (int n = 0; n < res->num_subs; n++) {
        struct mp_resolve_sub *sub = res->subs[n];
        char *s = talloc_strdup(NULL, sub->url);
        if (!s)
            s = talloc_asprintf(NULL, "memory://%s", sub->data);
        struct track *t =
            open_external_file(mpctx, s, opts->sub_demuxer_name, STREAM_SUB);
        talloc_free(s);
        if (t) {
            t->lang = talloc_strdup(t, sub->lang);
            t->no_default = true;
        }
    }
}

static const char *font_mimetypes[] = {
    "application/x-truetype-font",
    "application/vnd.ms-opentype",
    "application/x-font-ttf",
    "application/x-font", // probably incorrect
    NULL
};

static const char *font_exts[] = {".ttf", ".ttc", ".otf", NULL};

static bool attachment_is_font(struct mp_log *log, struct demux_attachment *att)
{
    if (!att->name || !att->type || !att->data || !att->data_size)
        return false;
    for (int n = 0; font_mimetypes[n]; n++) {
        if (strcmp(font_mimetypes[n], att->type) == 0)
            return true;
    }
    // fallback: match against file extension
    char *ext = strlen(att->name) > 4 ? att->name + strlen(att->name) - 4 : "";
    for (int n = 0; font_exts[n]; n++) {
        if (strcasecmp(ext, font_exts[n]) == 0) {
            mp_warn(log, "Loading font attachment '%s' with MIME type %s. "
                    "Assuming this is a broken Matroska file, which was "
                    "muxed without setting a correct font MIME type.\n",
                    att->name, att->type);
            return true;
        }
    }
    return false;
}

static void add_subtitle_fonts_from_sources(struct MPContext *mpctx)
{
#if HAVE_LIBASS
    if (mpctx->opts->ass_enabled) {
        for (int j = 0; j < mpctx->num_sources; j++) {
            struct demuxer *d = mpctx->sources[j];
            for (int i = 0; i < d->num_attachments; i++) {
                struct demux_attachment *att = d->attachments + i;
                if (mpctx->opts->use_embedded_fonts &&
                    attachment_is_font(mpctx->log, att))
                {
                    ass_add_font(mpctx->ass_library, att->name, att->data,
                                 att->data_size);
                }
            }
        }
    }
#endif
}

static void init_sub_renderer(struct MPContext *mpctx)
{
#if HAVE_LIBASS
    assert(!(mpctx->initialized_flags & INITIALIZED_LIBASS));
    assert(!mpctx->ass_renderer);

    mpctx->ass_renderer = ass_renderer_init(mpctx->ass_library);
    if (mpctx->ass_renderer) {
        mp_ass_configure_fonts(mpctx->ass_renderer, mpctx->opts->sub_text_style,
                               mpctx->global, mpctx->ass_log);
    }
    mpctx->initialized_flags |= INITIALIZED_LIBASS;
#endif
}

static struct mp_resolve_result *resolve_url(const char *filename,
                                             struct mpv_global *global)
{
    if (!mp_is_url(bstr0(filename)))
        return NULL;
#if HAVE_LIBQUVI
    return mp_resolve_quvi(filename, global);
#else
    return NULL;
#endif
}

static void print_resolve_contents(struct mp_log *log,
                                   struct mp_resolve_result *res)
{
    mp_msg(log, MSGL_V, "Resolve:\n");
    mp_msg(log, MSGL_V, "  title: %s\n", res->title);
    mp_msg(log, MSGL_V, "  url: %s\n", res->url);
    for (int n = 0; n < res->num_srcs; n++) {
        mp_msg(log, MSGL_V, "  source %d:\n", n);
        if (res->srcs[n]->url)
            mp_msg(log, MSGL_V, "    url: %s\n", res->srcs[n]->url);
        if (res->srcs[n]->encid)
            mp_msg(log, MSGL_V, "    encid: %s\n", res->srcs[n]->encid);
    }
    for (int n = 0; n < res->num_subs; n++) {
        mp_msg(log, MSGL_V, "  subtitle %d:\n", n);
        if (res->subs[n]->url)
            mp_msg(log, MSGL_V, "    url: %s\n", res->subs[n]->url);
        if (res->subs[n]->lang)
            mp_msg(log, MSGL_V, "    lang: %s\n", res->subs[n]->lang);
        if (res->subs[n]->data) {
            mp_msg(log, MSGL_V, "    data: %zd bytes\n",
                       strlen(res->subs[n]->data));
        }
    }
    if (res->playlist) {
        mp_msg(log, MSGL_V, "  playlist with %d entries\n",
                   playlist_entry_count(res->playlist));
    }
}

// Replace the current playlist entry with playlist contents. Moves the entries
// from the given playlist pl, so the entries don't actually need to be copied.
static void transfer_playlist(struct MPContext *mpctx, struct playlist *pl)
{
    if (pl->first) {
        playlist_transfer_entries(mpctx->playlist, pl);
        // current entry is replaced
        if (mpctx->playlist->current)
            playlist_remove(mpctx->playlist, mpctx->playlist->current);
    } else {
        MP_WARN(mpctx, "Empty playlist!\n");
    }
}

static void print_timeline(struct MPContext *mpctx)
{
    if (mpctx->timeline) {
        int part_count = mpctx->num_timeline_parts;
        MP_VERBOSE(mpctx, "Timeline contains %d parts from %d "
                   "sources. Total length %.3f seconds.\n", part_count,
                   mpctx->num_sources, mpctx->timeline[part_count].start);
        MP_VERBOSE(mpctx, "Source files:\n");
        for (int i = 0; i < mpctx->num_sources; i++)
            MP_VERBOSE(mpctx, "%d: %s\n", i,
                       mpctx->sources[i]->filename);
        MP_VERBOSE(mpctx, "Timeline parts: (number, start, "
               "source_start, source):\n");
        for (int i = 0; i < part_count; i++) {
            struct timeline_part *p = mpctx->timeline + i;
            MP_VERBOSE(mpctx, "%3d %9.3f %9.3f %p/%s\n", i, p->start,
                       p->source_start, p->source, p->source->filename);
        }
        MP_VERBOSE(mpctx, "END %9.3f\n",
                   mpctx->timeline[part_count].start);
    }
}

static void load_chapters(struct MPContext *mpctx)
{
    if (!mpctx->chapters && mpctx->master_demuxer &&
        mpctx->master_demuxer->num_chapters)
    {
        int count = mpctx->master_demuxer->num_chapters;
        mpctx->chapters = talloc_array(NULL, struct chapter, count);
        mpctx->num_chapters = count;
        for (int n = 0; n < count; n++) {
            struct demux_chapter *dchapter = &mpctx->master_demuxer->chapters[n];
            mpctx->chapters[n] = (struct chapter){
                .start = dchapter->start / 1e9,
                .name = talloc_strdup(mpctx->chapters, dchapter->name),
            };
        }
    }
}

/* When demux performs a blocking operation (network connection or
 * cache filling) if the operation fails we use this function to check
 * if it was interrupted by the user.
 * The function returns whether it was interrupted. */
static bool demux_was_interrupted(struct MPContext *mpctx)
{
    for (;;) {
        if (mpctx->stop_play != KEEP_PLAYING
            && mpctx->stop_play != AT_END_OF_FILE)
            return true;
        mp_cmd_t *cmd = mp_input_get_cmd(mpctx->input, 0, 0);
        if (!cmd)
            break;
        if (mp_input_is_abort_cmd(cmd))
            run_command(mpctx, cmd);
        mp_cmd_free(cmd);
    }
    return false;
}

static void load_per_file_options(m_config_t *conf,
                                  struct playlist_param *params,
                                  int params_count)
{
    for (int n = 0; n < params_count; n++) {
        m_config_set_option_ext(conf, params[n].name, params[n].value,
                                M_SETOPT_BACKUP);
    }
}

// Start playing the current playlist entry.
// Handle initialization and deinitialization.
static void play_current_file(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    void *tmp = talloc_new(NULL);
    double playback_start = -1e100;

    mpctx->initialized_flags |= INITIALIZED_PLAYBACK;

    mp_notify(mpctx, MPV_EVENT_START_FILE, NULL);

    mpctx->stop_play = 0;
    mpctx->filename = NULL;
    mpctx->shown_aframes = 0;
    mpctx->shown_vframes = 0;

    if (mpctx->playlist->current)
        mpctx->filename = mpctx->playlist->current->filename;

    if (!mpctx->filename)
        goto terminate_playback;

    char *local_filename = mp_file_url_to_filename(tmp, bstr0(mpctx->filename));
    if (local_filename)
        mpctx->filename = local_filename;

#if HAVE_ENCODING
    encode_lavc_discontinuity(mpctx->encode_lavc_ctx);
#endif

    mpctx->add_osd_seek_info &= OSD_SEEK_INFO_EDITION;

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

    if (opts->position_resume)
        mp_load_playback_resume(mpctx, mpctx->filename);

    load_per_file_options(mpctx->mconfig, mpctx->playlist->current->params,
                          mpctx->playlist->current->num_params);

#if HAVE_LIBASS
    if (opts->ass_style_override)
        ass_set_style_overrides(mpctx->ass_library, opts->ass_force_style_list);
#endif

    MP_INFO(mpctx, "Playing: %s\n", mpctx->filename);

    //============ Open & Sync STREAM --- fork cache2 ====================

    assert(mpctx->stream == NULL);
    assert(mpctx->demuxer == NULL);
    assert(mpctx->d_audio == NULL);
    assert(mpctx->d_video == NULL);
    assert(mpctx->d_sub[0] == NULL);
    assert(mpctx->d_sub[1] == NULL);

    char *stream_filename = mpctx->filename;
    mpctx->resolve_result = resolve_url(stream_filename, mpctx->global);
    if (mpctx->resolve_result) {
        talloc_steal(tmp, mpctx->resolve_result);
        print_resolve_contents(mpctx->log, mpctx->resolve_result);
        if (mpctx->resolve_result->playlist) {
            transfer_playlist(mpctx, mpctx->resolve_result->playlist);
            goto terminate_playback;
        }
        stream_filename = mpctx->resolve_result->url;
    }
    mpctx->stream = stream_open(stream_filename, mpctx->global);
    if (!mpctx->stream) { // error...
        demux_was_interrupted(mpctx);
        goto terminate_playback;
    }
    mpctx->initialized_flags |= INITIALIZED_STREAM;

    if (opts->stream_dump && opts->stream_dump[0]) {
        stream_dump(mpctx);
        goto terminate_playback;
    }

    // Must be called before enabling cache.
    mp_nav_init(mpctx);

    int res = stream_enable_cache(&mpctx->stream, &opts->stream_cache);
    if (res == 0)
        if (demux_was_interrupted(mpctx))
            goto terminate_playback;

    stream_set_capture_file(mpctx->stream, opts->stream_capture);

goto_reopen_demuxer: ;

    mp_nav_reset(mpctx);

    //============ Open DEMUXERS --- DETECT file type =======================

    mpctx->audio_delay = opts->audio_delay;

    mpctx->demuxer = demux_open(mpctx->stream, opts->demuxer_name, NULL,
                                mpctx->global);
    mpctx->master_demuxer = mpctx->demuxer;
    if (!mpctx->demuxer) {
        MP_ERR(mpctx, "Failed to recognize file format.\n");
        goto terminate_playback;
    }

    MP_TARRAY_APPEND(NULL, mpctx->sources, mpctx->num_sources, mpctx->demuxer);

    mpctx->initialized_flags |= INITIALIZED_DEMUXER;

    if (mpctx->demuxer->playlist) {
        if (mpctx->demuxer->stream->safe_origin || opts->load_unsafe_playlists) {
            transfer_playlist(mpctx, mpctx->demuxer->playlist);
        } else {
            MP_ERR(mpctx, "\nThis looks like a playlist, but playlist support "
                   "will not be used automatically.\nThe main problem with "
                   "playlist safety is that playlist entries can be arbitrary,\n"
                   "and an attacker could make mpv poke around in your local "
                   "filesystem or network.\nUse --playlist=file or the "
                   "--load-unsafe-playlists option to load them anyway.\n");
        }
        goto terminate_playback;
    }

    if (mpctx->demuxer->matroska_data.ordered_chapters)
        build_ordered_chapter_timeline(mpctx);

    if (mpctx->demuxer->type == DEMUXER_TYPE_EDL)
        build_mpv_edl_timeline(mpctx);

    if (mpctx->demuxer->type == DEMUXER_TYPE_CUE)
        build_cue_timeline(mpctx);

    print_timeline(mpctx);
    load_chapters(mpctx);

    if (mpctx->timeline) {
        // With Matroska, the "master" file usually dictates track layout etc.
        // On the contrary, the EDL and CUE demuxers are empty wrappers, as
        // well as Matroska ordered chapter playlist-like files.
        for (int n = 0; n < mpctx->num_timeline_parts; n++) {
            if (mpctx->timeline[n].source == mpctx->demuxer)
                goto main_is_ok;
        }
        mpctx->demuxer = mpctx->timeline[0].source;
    main_is_ok: ;
    }
    add_dvd_tracks(mpctx);
    add_demuxer_tracks(mpctx, mpctx->demuxer);

    mpctx->timeline_part = 0;
    if (mpctx->timeline)
        timeline_set_part(mpctx, mpctx->timeline_part, true);

    add_subtitle_fonts_from_sources(mpctx);
    // libass seems to misbehave if fonts are changed while a renderer
    // exists, so we (re)create the renderer after fonts are set.
    init_sub_renderer(mpctx);

    open_subtitles_from_options(mpctx);
    open_subtitles_from_resolve(mpctx);
    open_audiofiles_from_options(mpctx);

    check_previous_track_selection(mpctx);

    mpctx->current_track[0][STREAM_VIDEO] =
        select_track(mpctx, STREAM_VIDEO, mpctx->opts->video_id, NULL);
    mpctx->current_track[0][STREAM_AUDIO] =
        select_track(mpctx, STREAM_AUDIO, mpctx->opts->audio_id,
                     mpctx->opts->audio_lang);
    mpctx->current_track[0][STREAM_SUB] =
        select_track(mpctx, STREAM_SUB, mpctx->opts->sub_id,
                     mpctx->opts->sub_lang);
    mpctx->current_track[1][STREAM_SUB] =
        select_track(mpctx, STREAM_SUB, mpctx->opts->sub2_id, NULL);
    for (int t = 0; t < STREAM_TYPE_COUNT; t++) {
        for (int i = 0; i < NUM_PTRACKS; i++) {
            struct track *track = mpctx->current_track[i][t];
            if (track) {
                if (track->selected) {
                    MP_ERR(mpctx, "Track %d can't be selected twice.\n",
                           track->user_tid);
                    mpctx->current_track[i][t] = NULL;
                } else {
                    track->selected = true;
                }
            }
        }
    }
    reselect_demux_streams(mpctx);

    if (mpctx->current_track[0][STREAM_VIDEO] &&
        mpctx->current_track[0][STREAM_VIDEO]->attached_picture)
    {
        MP_INFO(mpctx,
            "Displaying attached picture. Use --no-audio-display to prevent this.\n");
    }

    demux_info_update(mpctx->master_demuxer);
    print_file_properties(mpctx);

#if HAVE_ENCODING
    if (mpctx->encode_lavc_ctx && mpctx->current_track[0][STREAM_VIDEO])
        encode_lavc_expect_stream(mpctx->encode_lavc_ctx, AVMEDIA_TYPE_VIDEO);
    if (mpctx->encode_lavc_ctx && mpctx->current_track[0][STREAM_AUDIO])
        encode_lavc_expect_stream(mpctx->encode_lavc_ctx, AVMEDIA_TYPE_AUDIO);
    if (mpctx->encode_lavc_ctx) {
        encode_lavc_set_metadata(mpctx->encode_lavc_ctx,
                                 mpctx->demuxer->metadata);
    }
#endif

    reinit_video_chain(mpctx);
    reinit_audio_chain(mpctx);
    reinit_subs(mpctx, 0);
    reinit_subs(mpctx, 1);

    //==================== START PLAYING =======================

    if (!mpctx->d_video && !mpctx->d_audio) {
        MP_FATAL(mpctx, "No video or audio streams selected.\n");
#if HAVE_DVBIN
        if (mpctx->stream->type == STREAMTYPE_DVB) {
            int dir;
            int v = mpctx->last_dvb_step;
            if (v > 0)
                dir = DVB_CHANNEL_HIGHER;
            else
                dir = DVB_CHANNEL_LOWER;

            if (dvb_step_channel(mpctx->stream, dir))
                mpctx->stop_play = PT_RELOAD_DEMUXER;
        }
#endif
        goto terminate_playback;
    }

    MP_VERBOSE(mpctx, "Starting playback...\n");

    mpctx->drop_frame_cnt = 0;
    mpctx->dropped_frames = 0;
    mpctx->max_frames = opts->play_frames;

    if (mpctx->max_frames == 0) {
        mpctx->stop_play = PT_NEXT_ENTRY;
        goto terminate_playback;
    }

    mpctx->time_frame = 0;
    mpctx->drop_message_shown = 0;
    mpctx->restart_playback = true;
    mpctx->video_pts = 0;
    mpctx->last_vo_pts = MP_NOPTS_VALUE;
    mpctx->last_frame_duration = 0;
    mpctx->last_seek_pts = 0;
    mpctx->playback_pts = MP_NOPTS_VALUE;
    mpctx->hrseek_active = false;
    mpctx->hrseek_framedrop = false;
    mpctx->step_frames = 0;
    mpctx->backstep_active = false;
    mpctx->total_avsync_change = 0;
    mpctx->last_chapter_seek = -2;
    mpctx->playing_msg_shown = false;
    mpctx->paused = false;
    mpctx->paused_for_cache = false;
    mpctx->eof_reached = false;
    mpctx->last_chapter = -2;
    mpctx->seek = (struct seek_params){ 0 };

    // If there's a timeline force an absolute seek to initialize state
    double startpos = rel_time_to_abs(mpctx, opts->play_start);
    if (startpos == MP_NOPTS_VALUE && mpctx->resolve_result &&
        mpctx->resolve_result->start_time > 0)
        startpos = mpctx->resolve_result->start_time;
    if (startpos == MP_NOPTS_VALUE && opts->chapterrange[0] > 0) {
        double start = chapter_start_time(mpctx, opts->chapterrange[0] - 1);
        if (start != MP_NOPTS_VALUE)
            startpos = start;
    }
    if (startpos == MP_NOPTS_VALUE && mpctx->timeline)
        startpos = 0;
    if (startpos != MP_NOPTS_VALUE) {
        queue_seek(mpctx, MPSEEK_ABSOLUTE, startpos, 0, true);
        execute_queued_seek(mpctx);
    }
    get_relative_time(mpctx); // reset current delta

    if (mpctx->opts->pause)
        pause_player(mpctx);

    mp_notify(mpctx, MPV_EVENT_FILE_LOADED, NULL);

    playback_start = mp_time_sec();
    mpctx->error_playing = false;
    while (!mpctx->stop_play)
        run_playloop(mpctx);

    MP_VERBOSE(mpctx, "EOF code: %d  \n", mpctx->stop_play);

    if (mpctx->stop_play == PT_RELOAD_DEMUXER) {
        mpctx->stop_play = KEEP_PLAYING;
        uninit_player(mpctx, INITIALIZED_ALL -
            (INITIALIZED_PLAYBACK | INITIALIZED_STREAM |
             (opts->fixed_vo ? INITIALIZED_VO : 0)));
        goto goto_reopen_demuxer;
    }

terminate_playback:

    mp_nav_destroy(mpctx);

    if (mpctx->stop_play == KEEP_PLAYING)
        mpctx->stop_play = AT_END_OF_FILE;

    if (mpctx->stop_play != AT_END_OF_FILE)
        clear_audio_output_buffers(mpctx);

    if (opts->position_save_on_quit && mpctx->stop_play == PT_QUIT)
        mp_write_watch_later_conf(mpctx);

    if (mpctx->step_frames)
        opts->pause = 1;

    MP_INFO(mpctx, "\n");

    // time to uninit all, except global stuff:
    int uninitialize_parts = INITIALIZED_ALL;
    if (opts->fixed_vo)
        uninitialize_parts -= INITIALIZED_VO;
    if ((opts->gapless_audio && mpctx->stop_play == AT_END_OF_FILE) ||
        mpctx->encode_lavc_ctx)
        uninitialize_parts -= INITIALIZED_AO;
    uninit_player(mpctx, uninitialize_parts);

    // xxx handle this as INITIALIZED_CONFIG?
    if (mpctx->stop_play != PT_RESTART)
        m_config_restore_backups(mpctx->mconfig);

    mpctx->filename = NULL;
    mpctx->resolve_result = NULL;
    talloc_free(tmp);

    // Played/paused for longer than 3 seconds -> ok
    bool playback_short = mpctx->stop_play == AT_END_OF_FILE &&
                (playback_start < 0 || mp_time_sec() - playback_start < 3.0);
    bool init_failed = mpctx->stop_play == AT_END_OF_FILE &&
                (mpctx->shown_aframes == 0 && mpctx->shown_vframes == 0);
    if (mpctx->playlist->current && !mpctx->playlist->current_was_replaced) {
        mpctx->playlist->current->playback_short = playback_short;
        mpctx->playlist->current->init_failed = init_failed;
    }

    mp_notify(mpctx, MPV_EVENT_TRACKS_CHANGED, NULL);
    struct mpv_event_end_file end_event = {0};
    switch (mpctx->stop_play) {
    case AT_END_OF_FILE:    end_event.reason = 0; break;
    case PT_RESTART:
    case PT_RELOAD_DEMUXER: end_event.reason = 1; break;
    case PT_NEXT_ENTRY:
    case PT_CURRENT_ENTRY:
    case PT_STOP:           end_event.reason = 2; break;
    case PT_QUIT:           end_event.reason = 3; break;
    default:                end_event.reason = -1; break;
    };
    mp_notify(mpctx, MPV_EVENT_END_FILE, &end_event);
}

// Determine the next file to play. Note that if this function returns non-NULL,
// it can have side-effects and mutate mpctx.
//  direction: -1 (previous) or +1 (next)
//  force: if true, don't skip playlist entries marked as failed
struct playlist_entry *mp_next_file(struct MPContext *mpctx, int direction,
                                    bool force)
{
    struct playlist_entry *next = playlist_get_next(mpctx->playlist, direction);
    if (next && direction < 0 && !force) {
        // Don't jump to files that would immediately go to next file anyway
        while (next && next->playback_short)
            next = next->prev;
        // Always allow jumping to first file
        if (!next && mpctx->opts->loop_times < 0)
            next = mpctx->playlist->first;
    }
    if (!next && mpctx->opts->loop_times >= 0) {
        if (direction > 0) {
            if (mpctx->opts->shuffle)
                playlist_shuffle(mpctx->playlist);
            next = mpctx->playlist->first;
            if (next && mpctx->opts->loop_times > 1) {
                mpctx->opts->loop_times--;
                if (mpctx->opts->loop_times == 1)
                    mpctx->opts->loop_times = -1;
            }
        } else {
            next = mpctx->playlist->last;
            // Don't jump to files that would immediately go to next file anyway
            while (next && next->playback_short)
                next = next->prev;
        }
        if (!force && next && next->init_failed) {
            // Don't endless loop if no file in playlist is playable
            bool all_failed = true;
            struct playlist_entry *cur;
            for (cur = mpctx->playlist->first; cur; cur = cur->next) {
                all_failed &= cur->init_failed;
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
    mp_input_set_main_thread(mpctx->input);
    mpctx->quit_player_rc = EXIT_NONE;
    for (;;) {
        idle_loop(mpctx);
        if (mpctx->stop_play == PT_QUIT)
            break;

        mpctx->error_playing = true;
        play_current_file(mpctx);
        if (mpctx->error_playing) {
            if (!mpctx->quit_player_rc) {
                mpctx->quit_player_rc = EXIT_NOTPLAYED;
            } else if (mpctx->quit_player_rc == EXIT_PLAYED) {
                mpctx->quit_player_rc = EXIT_SOMENOTPLAYED;
            }
        } else if (mpctx->quit_player_rc == EXIT_NOTPLAYED) {
            mpctx->quit_player_rc = EXIT_SOMENOTPLAYED;
        } else {
            mpctx->quit_player_rc = EXIT_PLAYED;
        }
        if (mpctx->stop_play == PT_QUIT)
            break;

        if (!mpctx->stop_play || mpctx->stop_play == AT_END_OF_FILE)
            mpctx->stop_play = PT_NEXT_ENTRY;

        struct playlist_entry *new_entry = NULL;

        if (mpctx->stop_play == PT_NEXT_ENTRY) {
            new_entry = mp_next_file(mpctx, +1, false);
        } else if (mpctx->stop_play == PT_CURRENT_ENTRY) {
            new_entry = mpctx->playlist->current;
        } else if (mpctx->stop_play == PT_RESTART) {
            // The same as PT_CURRENT_ENTRY, unless we decide that the current
            // playlist entry can be removed during playback.
            new_entry = mpctx->playlist->current;
        } else { // PT_STOP
            playlist_clear(mpctx->playlist);
        }

        mpctx->playlist->current = new_entry;
        mpctx->playlist->current_was_replaced = false;
        mpctx->stop_play = 0;

        if (!mpctx->playlist->current && !mpctx->opts->player_idle_mode)
            break;
    }
}

// Abort current playback and set the given entry to play next.
// e must be on the mpctx->playlist.
void mp_set_playlist_entry(struct MPContext *mpctx, struct playlist_entry *e)
{
    assert(playlist_entry_to_index(mpctx->playlist, e) >= 0);
    mpctx->playlist->current = e;
    mpctx->playlist->current_was_replaced = false;
    mpctx->stop_play = PT_CURRENT_ENTRY;
}
