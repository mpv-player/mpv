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
#include "mpvcore/encode.h"
#include "mpvcore/m_property.h"

#include "audio/out/ao.h"
#include "demux/demux.h"
#include "stream/stream.h"
#include "sub/osd.h"
#include "video/hwdec.h"
#include "video/filter/vf.h"
#include "video/decode/dec_video.h"
#include "video/decode/vd.h"
#include "video/out/vo.h"

#include "mp_core.h"
#include "command.h"

void update_fps(struct MPContext *mpctx)
{
#if HAVE_ENCODING
    struct dec_video *d_video = mpctx->d_video;
    if (mpctx->encode_lavc_ctx && d_video)
        encode_lavc_set_video_fps(mpctx->encode_lavc_ctx, d_video->fps);
#endif
}

static void recreate_video_filters(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct dec_video *d_video = mpctx->d_video;
    assert(d_video);

    vf_destroy(d_video->vfilter);
    d_video->vfilter = vf_new(opts);
    d_video->vfilter->hwdec = &d_video->hwdec_info;

    vf_append_filter(d_video->vfilter, "vo", NULL);
    vf_control_any(d_video->vfilter, VFCTRL_SET_VO, mpctx->video_out);

    vf_append_filter_list(d_video->vfilter, opts->vf_settings);

    // for vf_sub
    vf_control_any(d_video->vfilter, VFCTRL_SET_OSD_OBJ, mpctx->osd);
    mpctx->osd->render_subs_in_filter
        = vf_control_any(d_video->vfilter, VFCTRL_INIT_OSD, NULL) == CONTROL_OK;
}

int reinit_video_filters(struct MPContext *mpctx)
{
    struct dec_video *d_video = mpctx->d_video;

    if (!d_video)
        return -2;

    recreate_video_filters(mpctx);
    video_reconfig_filters(d_video, &d_video->decoder_output);

    return d_video->vfilter && d_video->vfilter->initialized > 0 ? 0 : -1;
}

int reinit_video_chain(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    assert(!(mpctx->initialized_flags & INITIALIZED_VCODEC));
    assert(!mpctx->d_video);
    init_demux_stream(mpctx, STREAM_VIDEO);
    struct sh_stream *sh = mpctx->sh[STREAM_VIDEO];
    if (!sh)
        goto no_video;

    MP_VERBOSE(mpctx, "[V] fourcc:0x%X  size:%dx%d  fps:%5.3f\n",
               sh->format,
               sh->video->disp_w, sh->video->disp_h,
               sh->video->fps);

    double ar = -1.0;
    //================== Init VIDEO (codec & libvo) ==========================
    if (!opts->fixed_vo || !(mpctx->initialized_flags & INITIALIZED_VO)) {
        mpctx->video_out = init_best_video_out(mpctx->global, mpctx->input,
                                               mpctx->encode_lavc_ctx);
        if (!mpctx->video_out) {
            MP_FATAL(mpctx, "Error opening/initializing "
                    "the selected video_out (-vo) device.\n");
            goto err_out;
        }
        mpctx->mouse_cursor_visible = true;
        mpctx->initialized_flags |= INITIALIZED_VO;
    }

    update_window_title(mpctx, true);

    struct dec_video *d_video = talloc_zero(NULL, struct dec_video);
    mpctx->d_video = d_video;
    d_video->opts = mpctx->opts;
    d_video->header = sh;
    d_video->fps = sh->video->fps;
    d_video->vo = mpctx->video_out;
    mpctx->initialized_flags |= INITIALIZED_VCODEC;

    vo_control(mpctx->video_out, VOCTRL_GET_HWDEC_INFO, &d_video->hwdec_info);

    if (stream_control(sh->demuxer->stream, STREAM_CTRL_GET_ASPECT_RATIO, &ar)
            != STREAM_UNSUPPORTED)
        d_video->stream_aspect = ar;

    recreate_video_filters(mpctx);

    if (!video_init_best_codec(d_video, opts->video_decoders))
        goto err_out;

    bool saver_state = opts->pause || !opts->stop_screensaver;
    vo_control(mpctx->video_out, saver_state ? VOCTRL_RESTORE_SCREENSAVER
                                             : VOCTRL_KILL_SCREENSAVER, NULL);

    vo_control(mpctx->video_out, mpctx->paused ? VOCTRL_PAUSE
                                               : VOCTRL_RESUME, NULL);

    mpctx->restart_playback = true;
    mpctx->sync_audio_to_video = !sh->attached_picture;
    mpctx->delay = 0;
    mpctx->video_next_pts = MP_NOPTS_VALUE;
    mpctx->vo_pts_history_seek_ts++;

    vo_seek_reset(mpctx->video_out);
    reset_subtitles(mpctx);

    if (opts->force_fps) {
        d_video->fps = opts->force_fps;
        MP_INFO(mpctx, "FPS forced to be %5.3f.\n", d_video->fps);
    }
    if (!sh->video->fps && !opts->force_fps && !opts->correct_pts) {
        MP_ERR(mpctx, "FPS not specified in the "
               "header or invalid, use the -fps option.\n");
    }
    update_fps(mpctx);

    return 1;

err_out:
no_video:
    uninit_player(mpctx, INITIALIZED_VCODEC | (opts->force_vo ? 0 : INITIALIZED_VO));
    cleanup_demux_stream(mpctx, STREAM_VIDEO);
    mpctx->current_track[STREAM_VIDEO] = NULL;
    handle_force_window(mpctx, true);
    MP_INFO(mpctx, "Video: no video\n");
    return 0;
}

// Try to refresh the video by doing a precise seek to the currently displayed
// frame. This can go wrong in all sorts of ways, so use sparingly.
void mp_force_video_refresh(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    // If not paused, the next frame should come soon enough.
    if (opts->pause && mpctx->last_vo_pts != MP_NOPTS_VALUE)
        queue_seek(mpctx, MPSEEK_ABSOLUTE, mpctx->last_vo_pts, 1);
}

static bool filter_output_queued_frame(struct MPContext *mpctx)
{
    struct dec_video *d_video = mpctx->d_video;
    struct vo *video_out = mpctx->video_out;

    struct mp_image *img = vf_output_queued_frame(d_video->vfilter);
    if (img)
        vo_queue_image(video_out, img);
    talloc_free(img);

    return !!img;
}

static bool load_next_vo_frame(struct MPContext *mpctx, bool eof)
{
    if (vo_get_buffered_frame(mpctx->video_out, eof) >= 0)
        return true;
    if (filter_output_queued_frame(mpctx))
        return true;
    return false;
}

static void init_filter_params(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    // Note that the filter chain is already initialized. This code might
    // recreate the chain a second time, which is not very elegant, but allows
    // us to test whether enabling deinterlacing works with the current video
    // format and other filters.
    if (opts->deinterlace >= 0)
        mp_property_do("deinterlace", M_PROPERTY_SET, &opts->deinterlace, mpctx);
}

static void reconfig_video(struct MPContext *mpctx,
                           const struct mp_image_params *params)
{
    struct dec_video *d_video = mpctx->d_video;

    if (!mp_image_params_equals(&d_video->decoder_output, params) ||
        d_video->vfilter->initialized < 1)
    {
        d_video->decoder_output = *params;
        if (video_reconfig_filters(d_video, params) < 0) {
            // Most video filters don't work with hardware decoding, so this
            // might be the reason filter reconfig failed.
            if (video_vd_control(d_video, VDCTRL_FORCE_HWDEC_FALLBACK, NULL)
                == CONTROL_OK)
            {
                // Fallback active; decoder will return software format next
                // time. Don't abort video decoding.
                d_video->vfilter->initialized = 0;
            }
            return;
        }
        if (d_video->vfilter->initialized > 0)
            init_filter_params(mpctx);
    }
}

static void filter_video(struct MPContext *mpctx, struct mp_image *frame)
{
    struct dec_video *d_video = mpctx->d_video;

    struct mp_image_params params;
    mp_image_params_from_image(&params, frame);
    reconfig_video(mpctx, &params);

    if (d_video->vfilter->initialized < 1) {
        talloc_free(frame);
        return;
    }

    mp_image_set_params(frame, &d_video->vf_input); // force csp/aspect overrides
    vf_filter_frame(d_video->vfilter, frame);
    filter_output_queued_frame(mpctx);
}

static int check_framedrop(struct MPContext *mpctx, double frame_time)
{
    struct MPOpts *opts = mpctx->opts;
    // check for frame-drop:
    if (mpctx->d_audio && !mpctx->ao->untimed &&
        !demux_stream_eof(mpctx->sh[STREAM_AUDIO]))
    {
        float delay = opts->playback_speed * ao_get_delay(mpctx->ao);
        float d = delay - mpctx->delay;
        float fps = mpctx->d_video->fps;
        if (frame_time < 0)
            frame_time = fps > 0 ? 1.0 / fps : 0;
        // we should avoid dropping too many frames in sequence unless we
        // are too late. and we allow 100ms A-V delay here:
        if (d < -mpctx->dropped_frames * frame_time - 0.100 && !mpctx->paused
            && !mpctx->restart_playback) {
            mpctx->drop_frame_cnt++;
            mpctx->dropped_frames++;
            return mpctx->opts->frame_dropping;
        } else
            mpctx->dropped_frames = 0;
    }
    return 0;
}

static double update_video_attached_pic(struct MPContext *mpctx)
{
    struct dec_video *d_video = mpctx->d_video;

    // Try to decode the picture multiple times, until it is displayed.
    if (mpctx->video_out->hasframe)
        return -1;

    struct mp_image *decoded_frame =
            video_decode(d_video, d_video->header->attached_picture, 0);
    if (decoded_frame)
        filter_video(mpctx, decoded_frame);
    load_next_vo_frame(mpctx, true);
    mpctx->video_next_pts = MP_NOPTS_VALUE;
    return 0;
}

double update_video(struct MPContext *mpctx, double endpts)
{
    struct dec_video *d_video = mpctx->d_video;
    struct vo *video_out = mpctx->video_out;

    if (d_video->header->attached_picture)
        return update_video_attached_pic(mpctx);

    if (!load_next_vo_frame(mpctx, false)) {
        struct demux_packet *pkt = demux_read_packet(d_video->header);
        if (pkt && pkt->pts != MP_NOPTS_VALUE)
            pkt->pts += mpctx->video_offset;
        if ((pkt && pkt->pts >= mpctx->hrseek_pts - .005) ||
            d_video->has_broken_packet_pts)
        {
            mpctx->hrseek_framedrop = false;
        }
        int framedrop_type = mpctx->hrseek_active && mpctx->hrseek_framedrop ?
                             1 : check_framedrop(mpctx, -1);
        struct mp_image *decoded_frame =
            video_decode(d_video, pkt, framedrop_type);
        talloc_free(pkt);
        if (decoded_frame) {
            filter_video(mpctx, decoded_frame);
        } else if (!pkt) {
            if (!load_next_vo_frame(mpctx, true))
                return -1;
        }
    }

    if (!video_out->frame_loaded)
        return 0;

    double pts = video_out->next_pts;
    if (endpts == MP_NOPTS_VALUE || pts < endpts)
        add_frame_pts(mpctx, pts);
    if (mpctx->hrseek_active && pts < mpctx->hrseek_pts - .005) {
        vo_skip_frame(video_out);
        return 0;
    }
    mpctx->hrseek_active = false;
    double last_pts = mpctx->video_next_pts;
    if (last_pts == MP_NOPTS_VALUE)
        last_pts = pts;
    double frame_time = pts - last_pts;
    if (frame_time < 0 || frame_time >= 60) {
        // Assume a PTS difference >= 60 seconds is a discontinuity.
        MP_WARN(mpctx, "Jump in video pts: %f -> %f\n", last_pts, pts);
        frame_time = 0;
    }
    mpctx->video_next_pts = pts;
    if (mpctx->d_audio)
        mpctx->delay -= frame_time;
    return frame_time;
}
