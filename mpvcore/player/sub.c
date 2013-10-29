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
#include <math.h>
#include <assert.h>

#include "config.h"
#include "talloc.h"

#include "mpvcore/mp_msg.h"
#include "mpvcore/options.h"
#include "mpvcore/mp_common.h"

#include "stream/stream.h"
#include "sub/dec_sub.h"
#include "demux/demux.h"
#include "video/mp_image.h"

#include "mp_core.h"
#include "mp_osd.h"

void uninit_subs(struct demuxer *demuxer)
{
    for (int i = 0; i < demuxer->num_streams; i++) {
        struct sh_stream *sh = demuxer->streams[i];
        if (sh->sub) {
            sub_destroy(sh->sub->dec_sub);
            sh->sub->dec_sub = NULL;
        }
    }
}

// When reading subtitles from a demuxer, and we read video or audio from the
// demuxer, we should not explicitly read subtitle packets. (With external
// subs, we have to.)
static bool is_interleaved(struct MPContext *mpctx, struct track *track)
{
    if (track->is_external || !track->demuxer)
        return false;

    struct demuxer *demuxer = track->demuxer;
    for (int type = 0; type < STREAM_TYPE_COUNT; type++) {
        struct track *other = mpctx->current_track[type];
        if (other && other != track && other->demuxer && other->demuxer == demuxer)
            return true;
    }
    return false;
}

void reset_subtitles(struct MPContext *mpctx)
{
    if (mpctx->sh_sub)
        sub_reset(mpctx->sh_sub->dec_sub);
    set_osd_subtitle(mpctx, NULL);
    osd_changed(mpctx->osd, OSDTYPE_SUB);
}

void update_subtitles(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    if (!(mpctx->initialized_flags & INITIALIZED_SUB))
        return;

    struct track *track = mpctx->current_track[STREAM_SUB];
    struct sh_sub *sh_sub = mpctx->sh_sub;
    assert(track && sh_sub);
    struct dec_sub *dec_sub = sh_sub->dec_sub;

    if (mpctx->sh_video && mpctx->sh_video->vf_input) {
        struct mp_image_params params = *mpctx->sh_video->vf_input;
        sub_control(dec_sub, SD_CTRL_SET_VIDEO_PARAMS, &params);
    }

    mpctx->osd->video_offset = track->under_timeline ? mpctx->video_offset : 0;

    double refpts_s = mpctx->playback_pts - mpctx->osd->video_offset;
    double curpts_s = refpts_s + opts->sub_delay;

    if (!track->preloaded) {
        bool interleaved = is_interleaved(mpctx, track);

        while (1) {
            if (interleaved && !demux_has_packet(sh_sub->gsh))
                break;
            double subpts_s = demux_get_next_pts(sh_sub->gsh);
            if (!demux_has_packet(sh_sub->gsh))
                break;
            if (subpts_s > curpts_s) {
                mp_dbg(MSGT_CPLAYER, MSGL_DBG2,
                       "Sub early: c_pts=%5.3f s_pts=%5.3f\n",
                       curpts_s, subpts_s);
                // Libass handled subs can be fed to it in advance
                if (!sub_accept_packets_in_advance(dec_sub))
                    break;
                // Try to avoid demuxing whole file at once
                if (subpts_s > curpts_s + 1 && !interleaved)
                    break;
            }
            struct demux_packet *pkt = demux_read_packet(sh_sub->gsh);
            mp_dbg(MSGT_CPLAYER, MSGL_V, "Sub: c_pts=%5.3f s_pts=%5.3f "
                   "duration=%5.3f len=%d\n", curpts_s, pkt->pts, pkt->duration,
                   pkt->len);
            sub_decode(dec_sub, pkt);
            talloc_free(pkt);
        }
    }

    if (!mpctx->osd->render_bitmap_subs || !mpctx->video_out)
        set_osd_subtitle(mpctx, sub_get_text(dec_sub, curpts_s));
}

static void set_dvdsub_fake_extradata(struct dec_sub *dec_sub, struct stream *st,
                                      int width, int height)
{
#ifdef CONFIG_DVDREAD
    if (!st)
        return;

    struct stream_dvd_info_req info;
    if (stream_control(st, STREAM_CTRL_GET_DVD_INFO, &info) < 0)
        return;

    struct mp_csp_params csp = MP_CSP_PARAMS_DEFAULTS;
    csp.int_bits_in = 8;
    csp.int_bits_out = 8;
    float cmatrix[3][4];
    mp_get_yuv2rgb_coeffs(&csp, cmatrix);

    if (width == 0 || height == 0) {
        width = 720;
        height = 480;
    }

    char *s = NULL;
    s = talloc_asprintf_append(s, "size: %dx%d\n", width, height);
    s = talloc_asprintf_append(s, "palette: ");
    for (int i = 0; i < 16; i++) {
        int color = info.palette[i];
        int c[3] = {(color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff};
        mp_map_int_color(cmatrix, 8, c);
        color = (c[2] << 16) | (c[1] << 8) | c[0];

        if (i != 0)
            talloc_asprintf_append(s, ", ");
        s = talloc_asprintf_append(s, "%06x", color);
    }
    s = talloc_asprintf_append(s, "\n");

    sub_set_extradata(dec_sub, s, strlen(s));
    talloc_free(s);
#endif
}

void reinit_subs(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct track *track = mpctx->current_track[STREAM_SUB];

    assert(!(mpctx->initialized_flags & INITIALIZED_SUB));

    init_demux_stream(mpctx, STREAM_SUB);
    if (!mpctx->sh_sub)
        return;

    if (!mpctx->sh_sub->dec_sub)
        mpctx->sh_sub->dec_sub = sub_create(opts);

    assert(track->demuxer);
    // Lazily added DVD track - will be created on first sub packet
    if (!track->stream)
        return;

    mpctx->initialized_flags |= INITIALIZED_SUB;

    struct sh_sub *sh_sub = mpctx->sh_sub;
    struct dec_sub *dec_sub = sh_sub->dec_sub;
    assert(dec_sub);

    if (!sub_is_initialized(dec_sub)) {
        int w = mpctx->sh_video ? mpctx->sh_video->disp_w : 0;
        int h = mpctx->sh_video ? mpctx->sh_video->disp_h : 0;
        float fps = mpctx->sh_video ? mpctx->sh_video->fps : 25;

        set_dvdsub_fake_extradata(dec_sub, track->demuxer->stream, w, h);
        sub_set_video_res(dec_sub, w, h);
        sub_set_video_fps(dec_sub, fps);
        sub_set_ass_renderer(dec_sub, mpctx->osd->ass_library,
                             mpctx->osd->ass_renderer);
        sub_init_from_sh(dec_sub, sh_sub);

        // Don't do this if the file has video/audio streams. Don't do it even
        // if it has only sub streams, because reading packets will change the
        // demuxer position.
        if (!track->preloaded && track->is_external) {
            demux_seek(track->demuxer, 0, SEEK_ABSOLUTE);
            track->preloaded = sub_read_all_packets(dec_sub, sh_sub);
        }
    }

    mpctx->osd->dec_sub = dec_sub;

    // Decides whether to use OSD path or normal subtitle rendering path.
    mpctx->osd->render_bitmap_subs =
        opts->ass_enabled || !sub_has_get_text(dec_sub);

    reset_subtitles(mpctx);
}
