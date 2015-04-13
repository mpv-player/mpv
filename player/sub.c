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

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>

#include "config.h"
#include "talloc.h"

#include "common/msg.h"
#include "options/options.h"
#include "common/common.h"
#include "common/global.h"

#include "stream/stream.h"
#include "sub/ass_mp.h"
#include "sub/dec_sub.h"
#include "demux/demux.h"
#include "video/mp_image.h"
#include "video/decode/dec_video.h"
#include "video/filter/vf.h"

#include "core.h"

#if HAVE_LIBASS

static const char *const font_mimetypes[] = {
    "application/x-truetype-font",
    "application/vnd.ms-opentype",
    "application/x-font-ttf",
    "application/x-font", // probably incorrect
    NULL
};

static const char *const font_exts[] = {".ttf", ".ttc", ".otf", NULL};

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
}

static void init_sub_renderer(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    if (mpctx->ass_renderer)
        return;

    if (!mpctx->ass_log)
        mpctx->ass_log = mp_log_new(mpctx, mpctx->global->log, "!libass");

    mpctx->ass_library = mp_ass_init(mpctx->global, mpctx->ass_log);

    add_subtitle_fonts_from_sources(mpctx);

    if (opts->ass_style_override)
        ass_set_style_overrides(mpctx->ass_library, opts->ass_force_style_list);

    mpctx->ass_renderer = ass_renderer_init(mpctx->ass_library);
}

void uninit_sub_renderer(struct MPContext *mpctx)
{
    if (mpctx->ass_renderer)
        ass_renderer_done(mpctx->ass_renderer);
    mpctx->ass_renderer = NULL;
    if (mpctx->ass_library)
        ass_library_done(mpctx->ass_library);
    mpctx->ass_library = NULL;
}

#else /* HAVE_LIBASS */

static void init_sub_renderer(struct MPContext *mpctx) {}
void uninit_sub_renderer(struct MPContext *mpctx) {}

static void mp_ass_configure_fonts(struct ass_renderer *a, struct osd_style_opts *b,
                                   struct mpv_global *c, struct mp_log *d) {}

#endif

static void reset_subtitles(struct MPContext *mpctx, int order)
{
    int obj = order ? OSDTYPE_SUB2 : OSDTYPE_SUB;
    if (mpctx->d_sub[order])
        sub_reset(mpctx->d_sub[order]);
    set_osd_subtitle(mpctx, NULL);
    osd_set_text(mpctx->osd, obj, NULL);
}

void reset_subtitle_state(struct MPContext *mpctx)
{
    reset_subtitles(mpctx, 0);
    reset_subtitles(mpctx, 1);
}

void uninit_stream_sub_decoders(struct demuxer *demuxer)
{
    for (int i = 0; i < demuxer->num_streams; i++) {
        struct sh_stream *sh = demuxer->streams[i];
        if (sh->sub) {
            sub_destroy(sh->sub->dec_sub);
            sh->sub->dec_sub = NULL;
        }
    }
}

void uninit_sub(struct MPContext *mpctx, int order)
{
    if (mpctx->d_sub[order]) {
        reset_subtitles(mpctx, order);
        mpctx->d_sub[order] = NULL; // Note: not free'd.
        update_osd_sub_state(mpctx, order, NULL); // unset
        reselect_demux_streams(mpctx);
    }
}

void uninit_sub_all(struct MPContext *mpctx)
{
    uninit_sub(mpctx, 0);
    uninit_sub(mpctx, 1);
}

// When reading subtitles from a demuxer, and we read video or audio from the
// demuxer, we should not explicitly read subtitle packets. (With external
// subs, we have to.)
static bool is_interleaved(struct MPContext *mpctx, struct track *track)
{
    if (track->is_external || !track->demuxer)
        return false;

    struct demuxer *demuxer = track->demuxer;
    for (int t = 0; t < mpctx->num_tracks; t++) {
        struct track *other = mpctx->tracks[t];
        if (other != track && other->selected && other->demuxer == demuxer &&
            (other->type == STREAM_VIDEO || other->type == STREAM_AUDIO))
            return true;
    }
    return track->demuxer == mpctx->demuxer;
}

void update_osd_sub_state(struct MPContext *mpctx, int order,
                          struct osd_sub_state *out_state)
{
    struct MPOpts *opts = mpctx->opts;
    struct track *track = mpctx->current_track[order][STREAM_SUB];
    struct dec_sub *dec_sub = mpctx->d_sub[order];
    int obj = order ? OSDTYPE_SUB2 : OSDTYPE_SUB;
    bool textsub = dec_sub && sub_has_get_text(dec_sub);

    struct osd_sub_state state = {
        .dec_sub = dec_sub,
        // Decides whether to use OSD path or normal subtitle rendering path.
        .render_bitmap_subs = opts->ass_enabled || !textsub,
        .video_offset = get_track_video_offset(mpctx, track),
    };

    // Secondary subs are rendered with the "text" renderer to transform them
    // to toptitles.
    if (order == 1 && textsub)
        state.render_bitmap_subs = false;

    if (!mpctx->current_track[0][STREAM_VIDEO])
        state.render_bitmap_subs = false;

    osd_set_sub(mpctx->osd, obj, &state);
    if (out_state)
        *out_state = state;
}

static void update_subtitle(struct MPContext *mpctx, int order)
{
    struct MPOpts *opts = mpctx->opts;
    struct track *track = mpctx->current_track[order][STREAM_SUB];
    struct dec_sub *dec_sub = mpctx->d_sub[order];

    if (!track || !dec_sub)
        return;

    int obj = order ? OSDTYPE_SUB2 : OSDTYPE_SUB;

    if (mpctx->d_video) {
        struct mp_image_params params = mpctx->d_video->vfilter->override_params;
        if (params.imgfmt)
            sub_control(dec_sub, SD_CTRL_SET_VIDEO_PARAMS, &params);
    }

    struct osd_sub_state state;
    update_osd_sub_state(mpctx, order, &state);

    double refpts_s = mpctx->playback_pts - state.video_offset;
    double curpts_s = refpts_s - opts->sub_delay;

    if (!track->preloaded && track->stream) {
        struct sh_stream *sh_stream = track->stream;
        bool interleaved = is_interleaved(mpctx, track);

        assert(sh_stream->sub->dec_sub == dec_sub);

        while (1) {
            if (interleaved && !demux_has_packet(sh_stream))
                break;
            double subpts_s = demux_get_next_pts(sh_stream);
            if (!demux_has_packet(sh_stream))
                break;
            if (subpts_s > curpts_s) {
                MP_DBG(mpctx, "Sub early: c_pts=%5.3f s_pts=%5.3f\n",
                       curpts_s, subpts_s);
                // Libass handled subs can be fed to it in advance
                if (!sub_accept_packets_in_advance(dec_sub))
                    break;
                // Try to avoid demuxing whole file at once
                if (subpts_s > curpts_s + 1 && !interleaved)
                    break;
            }
            struct demux_packet *pkt = demux_read_packet(sh_stream);
            MP_DBG(mpctx, "Sub: c_pts=%5.3f s_pts=%5.3f duration=%5.3f len=%d\n",
                   curpts_s, pkt->pts, pkt->duration, pkt->len);
            sub_decode(dec_sub, pkt);
            talloc_free(pkt);
        }
    }

    // Handle displaying subtitles on terminal; never done for secondary subs
    if (order == 0) {
        if (!state.render_bitmap_subs || !mpctx->video_out)
            set_osd_subtitle(mpctx, sub_get_text(dec_sub, curpts_s));
    } else if (order == 1) {
        osd_set_text(mpctx->osd, obj, sub_get_text(dec_sub, curpts_s));
    }
}

void update_subtitles(struct MPContext *mpctx)
{
    update_subtitle(mpctx, 0);
    update_subtitle(mpctx, 1);
}

static void reinit_subdec(struct MPContext *mpctx, struct track *track,
                          struct dec_sub *dec_sub)
{
    struct MPOpts *opts = mpctx->opts;

    if (sub_is_initialized(dec_sub))
        return;

    struct sh_video *sh_video =
        mpctx->d_video ? mpctx->d_video->header->video : NULL;
    int w = sh_video ? sh_video->disp_w : 0;
    int h = sh_video ? sh_video->disp_h : 0;
    float fps = sh_video ? sh_video->fps : 25;

    init_sub_renderer(mpctx);

    sub_set_video_res(dec_sub, w, h);
    sub_set_video_fps(dec_sub, fps);
    sub_set_ass_renderer(dec_sub, mpctx->ass_library, mpctx->ass_renderer);
    sub_init_from_sh(dec_sub, track->stream);

    if (mpctx->ass_renderer) {
        mp_ass_configure_fonts(mpctx->ass_renderer, opts->sub_text_style,
                               mpctx->global, mpctx->ass_log);
    }

    // Don't do this if the file has video/audio streams. Don't do it even
    // if it has only sub streams, because reading packets will change the
    // demuxer position.
    if (!track->preloaded && track->is_external && !opts->sub_clear_on_seek) {
        demux_seek(track->demuxer, 0, SEEK_ABSOLUTE);
        track->preloaded = sub_read_all_packets(dec_sub, track->stream);
        if (track->preloaded)
            demux_stop_thread(track->demuxer);
    }
}

void reinit_subs(struct MPContext *mpctx, int order)
{
    struct track *track = mpctx->current_track[order][STREAM_SUB];

    assert(!mpctx->d_sub[order]);

    struct sh_stream *sh = track ? track->stream : NULL;
    if (!sh)
        return;

    // The decoder is cached in the stream header in order to make ordered
    // chapters work better.
    if (!sh->sub->dec_sub)
        sh->sub->dec_sub = sub_create(mpctx->global);
    mpctx->d_sub[order] = sh->sub->dec_sub;

    struct dec_sub *dec_sub = mpctx->d_sub[order];
    reinit_subdec(mpctx, track, dec_sub);

    update_osd_sub_state(mpctx, order, NULL);
}
