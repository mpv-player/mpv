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
#include "options/m_config.h"
#include "options/m_option.h"
#include "common/common.h"
#include "common/encode.h"
#include "options/m_property.h"
#include "osdep/timer.h"

#include "audio/out/ao.h"
#include "demux/demux.h"
#include "stream/stream.h"
#include "sub/osd.h"
#include "video/hwdec.h"
#include "video/filter/vf.h"
#include "video/decode/dec_video.h"
#include "video/decode/vd.h"
#include "video/out/vo.h"

#include "core.h"
#include "command.h"
#include "screenshot.h"

enum {
    // update_video() - code also uses: <0 error, 0 eof, >0 progress
    VD_ERROR = -1,
    VD_EOF = 0,         // end of file - no new output
    VD_PROGRESS = 1,    // progress, but no output; repeat call with no waiting
    VD_NEW_FRAME = 2,   // the call produced a new frame
    VD_WAIT = 3,        // no EOF, but no output; wait until wakeup
    VD_RECONFIG = 4,
};

static const char av_desync_help_text[] =
"\n\n"
"           *************************************************\n"
"           **** Audio/Video desynchronisation detected! ****\n"
"           *************************************************\n\n"
"This means either the audio or the video is played too slowly.\n"
"Possible reasons, problems, workarounds:\n"
"- Your system is simply too slow for this file.\n"
"     Transcode it to a lower bitrate file with e.g. mpv encoding support.\n"
"- Slow video output.\n"
"     Try a different --vo driver (--vo=help for a list). Make sure framedrop\n"
"     is not disabled, or experiment with different values for --framedrop.\n"
"     Make sure you have proper drivers for your GPU installed. If mpv\n"
"     autoselects 'VO: [x11]', it's a sure sign your drivers are messed up.\n"
"- Playing from a slow network source. Download the file instead.\n"
"- Try to find out whether audio/video/subs are causing this by experimenting\n"
"  with --no-video, --no-audio, or --no-sub.\n"
"If none of this helps you, file a bug report.\n\n";

static void set_allowed_vo_formats(struct vf_chain *c, struct vo *vo)
{
    vo_query_formats(vo, c->allowed_output_formats);
}

static int try_filter(struct MPContext *mpctx, struct mp_image_params params,
                      char *name, char *label, char **args)
{
    struct dec_video *d_video = mpctx->d_video;

    struct vf_instance *vf = vf_append_filter(d_video->vfilter, name, args);
    if (!vf)
        return -1;

    vf->label = talloc_strdup(vf, label);

    if (video_reconfig_filters(d_video, &params) < 0) {
        vf_remove_filter(d_video->vfilter, vf);
        // restore
        video_reconfig_filters(d_video, &params);
        return -1;
    }
    return 0;
}

// Reconfigure the filter chain according to decoder output.
// probe_only: don't force fallback to software when doing hw decoding, and
//             the filter chain couldn't be configured
static void filter_reconfig(struct MPContext *mpctx,
                            bool probe_only)
{
    struct dec_video *d_video = mpctx->d_video;

    struct mp_image_params params = d_video->decoder_output;

    mp_notify(mpctx, MPV_EVENT_VIDEO_RECONFIG, NULL);

    set_allowed_vo_formats(d_video->vfilter, mpctx->video_out);

    if (video_reconfig_filters(d_video, &params) < 0) {
        // Most video filters don't work with hardware decoding, so this
        // might be the reason why filter reconfig failed.
        if (!probe_only &&
            video_vd_control(d_video, VDCTRL_FORCE_HWDEC_FALLBACK, NULL) == CONTROL_OK)
        {
            // Fallback active; decoder will return software format next
            // time. Don't abort video decoding.
            d_video->vfilter->initialized = 0;
            mp_image_unrefp(&d_video->waiting_decoded_mpi);
            d_video->decoder_output = (struct mp_image_params){0};
            MP_VERBOSE(mpctx, "hwdec falback due to filters.\n");
        }
        return;
    }

    if (d_video->vfilter->initialized < 1)
        return;

    if (params.rotate && (params.rotate % 90 == 0)) {
        if (!(mpctx->video_out->driver->caps & VO_CAP_ROTATE90)) {
            // Try to insert a rotation filter.
            char *args[] = {"angle", "auto", NULL};
            if (try_filter(mpctx, params, "rotate", "autorotate", args) >= 0) {
                params.rotate = 0;
            } else {
                MP_ERR(mpctx, "Can't insert rotation filter.\n");
            }
        }
    }

    if (params.stereo_in != params.stereo_out &&
        params.stereo_in > 0 && params.stereo_out >= 0)
    {
        char *to = (char *)MP_STEREO3D_NAME(params.stereo_out);
        if (to) {
            char *args[] = {"in", "auto", "out", to, NULL, NULL};
            if (try_filter(mpctx, params, "stereo3d", "stereo3d", args) < 0)
                MP_ERR(mpctx, "Can't insert 3D conversion filter.\n");
        }
    }
}

static void recreate_video_filters(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct dec_video *d_video = mpctx->d_video;
    assert(d_video);

    vf_destroy(d_video->vfilter);
    d_video->vfilter = vf_new(mpctx->global);
    d_video->vfilter->hwdec = d_video->hwdec_info;
    d_video->vfilter->wakeup_callback = wakeup_playloop;
    d_video->vfilter->wakeup_callback_ctx = mpctx;
    d_video->vfilter->container_fps = d_video->fps;
    vo_control(mpctx->video_out, VOCTRL_GET_DISPLAY_FPS,
        &d_video->vfilter->display_fps);

    vf_append_filter_list(d_video->vfilter, opts->vf_settings);

    // for vf_sub
    vf_control_any(d_video->vfilter, VFCTRL_SET_OSD_OBJ, mpctx->osd);
    osd_set_render_subs_in_filter(mpctx->osd,
        vf_control_any(d_video->vfilter, VFCTRL_INIT_OSD, NULL) == CONTROL_OK);

    set_allowed_vo_formats(d_video->vfilter, mpctx->video_out);
}

int reinit_video_filters(struct MPContext *mpctx)
{
    struct dec_video *d_video = mpctx->d_video;

    if (!d_video)
        return 0;
    bool need_reconfig = d_video->vfilter->initialized != 0;

    recreate_video_filters(mpctx);

    if (need_reconfig)
        filter_reconfig(mpctx, true);

    return d_video->vfilter->initialized;
}

void reset_video_state(struct MPContext *mpctx)
{
    if (mpctx->d_video)
        video_reset_decoding(mpctx->d_video);
    if (mpctx->video_out)
        vo_seek_reset(mpctx->video_out);

    mp_image_unrefp(&mpctx->next_frame[0]);
    mp_image_unrefp(&mpctx->next_frame[1]);
    mp_image_unrefp(&mpctx->saved_frame);

    mpctx->delay = 0;
    mpctx->time_frame = 0;
    mpctx->video_pts = MP_NOPTS_VALUE;
    mpctx->video_next_pts = MP_NOPTS_VALUE;
    mpctx->total_avsync_change = 0;
    mpctx->dropped_frames_total = 0;
    mpctx->dropped_frames = 0;
    mpctx->drop_message_shown = 0;

    mpctx->video_status = mpctx->d_video ? STATUS_SYNCING : STATUS_EOF;
}

void uninit_video_out(struct MPContext *mpctx)
{
    uninit_video_chain(mpctx);
    if (mpctx->video_out)
        vo_destroy(mpctx->video_out);
    mpctx->video_out = NULL;
    mp_notify(mpctx, MPV_EVENT_VIDEO_RECONFIG, NULL);
}

void uninit_video_chain(struct MPContext *mpctx)
{
    if (mpctx->d_video) {
        reset_video_state(mpctx);
        video_uninit(mpctx->d_video);
        mpctx->d_video = NULL;
        mpctx->video_status = STATUS_EOF;
        mpctx->sync_audio_to_video = false;
        reselect_demux_streams(mpctx);
    }
    mp_notify(mpctx, MPV_EVENT_VIDEO_RECONFIG, NULL);
}

int reinit_video_chain(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    assert(!mpctx->d_video);
    struct track *track = mpctx->current_track[0][STREAM_VIDEO];
    struct sh_stream *sh = track ? track->stream : NULL;
    if (!sh)
        goto no_video;

    MP_VERBOSE(mpctx, "[V] fourcc:0x%X  size:%dx%d  fps:%5.3f\n",
               sh->format,
               sh->video->disp_w, sh->video->disp_h,
               sh->video->fps);

    //================== Init VIDEO (codec & libvo) ==========================
    if (!mpctx->video_out) {
        struct vo_extra ex = {
            .input_ctx = mpctx->input,
            .osd = mpctx->osd,
            .encode_lavc_ctx = mpctx->encode_lavc_ctx,
            .opengl_cb_context = mpctx->gl_cb_ctx,
        };
        mpctx->video_out = init_best_video_out(mpctx->global, &ex);
        if (!mpctx->video_out) {
            MP_FATAL(mpctx, "Error opening/initializing "
                    "the selected video_out (-vo) device.\n");
            mpctx->error_playing = MPV_ERROR_VO_INIT_FAILED;
            goto err_out;
        }
        mpctx->mouse_cursor_visible = true;
    }

    update_window_title(mpctx, true);

    struct dec_video *d_video = talloc_zero(NULL, struct dec_video);
    mpctx->d_video = d_video;
    d_video->global = mpctx->global;
    d_video->log = mp_log_new(d_video, mpctx->log, "!vd");
    d_video->opts = mpctx->opts;
    d_video->header = sh;
    d_video->fps = sh->video->fps;
    d_video->vo = mpctx->video_out;

    MP_VERBOSE(d_video, "Container reported FPS: %f\n", sh->video->fps);

    if (opts->force_fps) {
        d_video->fps = opts->force_fps;
        MP_INFO(mpctx, "FPS forced to %5.3f.\n", d_video->fps);
        MP_INFO(mpctx, "Use --no-correct-pts to force FPS based timing.\n");
    }

#if HAVE_ENCODING
    if (mpctx->encode_lavc_ctx && d_video)
        encode_lavc_set_video_fps(mpctx->encode_lavc_ctx, d_video->fps);
#endif

    vo_control(mpctx->video_out, VOCTRL_GET_HWDEC_INFO, &d_video->hwdec_info);

    recreate_video_filters(mpctx);

    if (!video_init_best_codec(d_video, opts->video_decoders))
        goto err_out;

    bool saver_state = opts->pause || !opts->stop_screensaver;
    vo_control(mpctx->video_out, saver_state ? VOCTRL_RESTORE_SCREENSAVER
                                             : VOCTRL_KILL_SCREENSAVER, NULL);

    vo_set_paused(mpctx->video_out, mpctx->paused);

    mpctx->sync_audio_to_video = !sh->attached_picture;
    mpctx->vo_pts_history_seek_ts++;

    // If we switch on video again, ensure audio position matches up.
    if (mpctx->d_audio)
        mpctx->audio_status = STATUS_SYNCING;

    reset_video_state(mpctx);
    reset_subtitle_state(mpctx);

    return 1;

err_out:
no_video:
    uninit_video_chain(mpctx);
    if (track)
        error_on_track(mpctx, track);
    handle_force_window(mpctx, true);
    return 0;
}

// Try to refresh the video by doing a precise seek to the currently displayed
// frame. This can go wrong in all sorts of ways, so use sparingly.
void mp_force_video_refresh(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct dec_video *d_video = mpctx->d_video;

    if (!d_video || !d_video->decoder_output.imgfmt)
        return;

    // If not paused, the next frame should come soon enough.
    if (opts->pause && mpctx->last_vo_pts != MP_NOPTS_VALUE) {
        queue_seek(mpctx, MPSEEK_ABSOLUTE, mpctx->last_vo_pts,
                   MPSEEK_VERY_EXACT, true);
    }
}

static int check_framedrop(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    // check for frame-drop:
    if (mpctx->video_status == STATUS_PLAYING && !mpctx->paused &&
        mpctx->audio_status == STATUS_PLAYING && !ao_untimed(mpctx->ao))
    {
        float fps = mpctx->d_video->fps;
        double frame_time = fps > 0 ? 1.0 / fps : 0;
        // we should avoid dropping too many frames in sequence unless we
        // are too late. and we allow 100ms A-V delay here:
        if (mpctx->last_av_difference - 0.100 > mpctx->dropped_frames * frame_time)
            return !!(opts->frame_dropping & 2);
    }
    return 0;
}

// Read a packet, store decoded image into d_video->waiting_decoded_mpi
// returns VD_* code
static int decode_image(struct MPContext *mpctx)
{
    struct dec_video *d_video = mpctx->d_video;

    if (d_video->header->attached_picture) {
        d_video->waiting_decoded_mpi =
                    video_decode(d_video, d_video->header->attached_picture, 0);
        return d_video->waiting_decoded_mpi ? VD_EOF : VD_PROGRESS;
    }

    struct demux_packet *pkt;
    if (demux_read_packet_async(d_video->header, &pkt) == 0)
        return VD_WAIT;
    if (pkt && pkt->pts != MP_NOPTS_VALUE)
        pkt->pts += mpctx->video_offset;
    if (pkt && pkt->dts != MP_NOPTS_VALUE)
        pkt->dts += mpctx->video_offset;
    if ((pkt && pkt->pts >= mpctx->hrseek_pts - .005) ||
        d_video->has_broken_packet_pts ||
        !mpctx->opts->hr_seek_framedrop)
    {
        mpctx->hrseek_framedrop = false;
    }
    bool hrseek = mpctx->hrseek_active && mpctx->video_status == STATUS_SYNCING;
    int framedrop_type = hrseek && mpctx->hrseek_framedrop ?
                         2 : check_framedrop(mpctx);
    d_video->waiting_decoded_mpi =
        video_decode(d_video, pkt, framedrop_type);
    bool had_packet = !!pkt;
    talloc_free(pkt);

    if (had_packet && !d_video->waiting_decoded_mpi &&
        mpctx->video_status == STATUS_PLAYING &&
        (mpctx->opts->frame_dropping & 2))
    {
        mpctx->dropped_frames_total++;
        mpctx->dropped_frames++;
    }

    return had_packet ? VD_PROGRESS : VD_EOF;
}


// Called after video reinit. This can be generally used to try to insert more
// filters using the filter chain edit functionality in command.c.
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

// Feed newly decoded frames to the filter, take care of format changes.
// If eof=true, drain the filter chain, and return VD_EOF if empty.
static int video_filter(struct MPContext *mpctx, bool eof)
{
    struct dec_video *d_video = mpctx->d_video;
    struct vf_chain *vf = d_video->vfilter;

    if (vf->initialized < 0)
        return VD_ERROR;

    // There is already a filtered frame available.
    // If vf_needs_input() returns > 0, the filter wants input anyway.
    if (vf_output_frame(vf, eof) > 0 && vf_needs_input(vf) < 1)
        return VD_PROGRESS;

    // Decoder output is different from filter input?
    bool need_vf_reconfig = !vf->input_params.imgfmt || vf->initialized < 1 ||
        !mp_image_params_equal(&d_video->decoder_output, &vf->input_params);

    // (If imgfmt==0, nothing was decoded yet, and the format is unknown.)
    if (need_vf_reconfig && d_video->decoder_output.imgfmt) {
        // Drain the filter chain.
        if (vf_output_frame(vf, true) > 0)
            return VD_PROGRESS;

        // The filter chain is drained; execute the filter format change.
        filter_reconfig(mpctx, false);
        if (vf->initialized == 0)
            return VD_PROGRESS; // hw decoding fallback; try again
        if (vf->initialized < 1)
            return VD_ERROR;
        init_filter_params(mpctx);
        return VD_RECONFIG;
    }

    // If something was decoded, and the filter chain is ready, filter it.
    if (!need_vf_reconfig && d_video->waiting_decoded_mpi) {
        vf_filter_frame(vf, d_video->waiting_decoded_mpi);
        d_video->waiting_decoded_mpi = NULL;
        return VD_PROGRESS;
    }

    return eof ? VD_EOF : VD_PROGRESS;
}

// Make sure at least 1 filtered image is available, decode new video if needed.
// returns VD_* code
// A return value of VD_PROGRESS doesn't necessarily output a frame, but makes
// the promise that calling this function again will eventually do something.
static int video_decode_and_filter(struct MPContext *mpctx)
{
    struct dec_video *d_video = mpctx->d_video;

    int r = video_filter(mpctx, false);
    if (r < 0)
        return r;

    if (!d_video->waiting_decoded_mpi) {
        // Decode a new image, or at least feed the decoder a packet.
        r = decode_image(mpctx);
        if (r == VD_WAIT)
            return r;
        if (d_video->waiting_decoded_mpi)
            d_video->decoder_output = d_video->waiting_decoded_mpi->params;
    }

    bool eof = !d_video->waiting_decoded_mpi && (r == VD_EOF || r < 0);
    r = video_filter(mpctx, eof);
    if (r == VD_RECONFIG) // retry feeding decoded image
        r = video_filter(mpctx, eof);
    return r;
}

static int video_feed_async_filter(struct MPContext *mpctx)
{
    struct dec_video *d_video = mpctx->d_video;
    struct vf_chain *vf = d_video->vfilter;

    if (vf->initialized < 0)
        return VD_ERROR;

    if (vf_needs_input(vf) < 1)
        return 0;
    mpctx->sleeptime = 0; // retry until done
    return video_decode_and_filter(mpctx);
}

/* Modify video timing to match the audio timeline. There are two main
 * reasons this is needed. First, video and audio can start from different
 * positions at beginning of file or after a seek (MPlayer starts both
 * immediately even if they have different pts). Second, the file can have
 * audio timestamps that are inconsistent with the duration of the audio
 * packets, for example two consecutive timestamp values differing by
 * one second but only a packet with enough samples for half a second
 * of playback between them.
 */
static void adjust_sync(struct MPContext *mpctx, double v_pts, double frame_time)
{
    struct MPOpts *opts = mpctx->opts;

    if (mpctx->audio_status != STATUS_PLAYING)
        return;

    double a_pts = written_audio_pts(mpctx) + opts->audio_delay - mpctx->delay;
    double av_delay = a_pts - v_pts;

    double change = av_delay * 0.1;
    double max_change = opts->default_max_pts_correction >= 0 ?
                        opts->default_max_pts_correction : frame_time * 0.1;
    if (change < -max_change)
        change = -max_change;
    else if (change > max_change)
        change = max_change;
    mpctx->delay += change;
    mpctx->total_avsync_change += change;
}

// Move the frame in next_frame[1] to next_frame[0]. This makes the frame
// "known" to the playback logic. A frame in next_frame[0] is either "known" or
// NULL, so the moving must always be done by this function.
static void shift_new_frame(struct MPContext *mpctx)
{
    if (mpctx->next_frame[0] || !mpctx->next_frame[1])
        return;

    mpctx->next_frame[0] = mpctx->next_frame[1];
    mpctx->next_frame[1] = NULL;

    double frame_time = 0;
    double pts = mpctx->next_frame[0]->pts;
    if (mpctx->video_pts != MP_NOPTS_VALUE) {
        frame_time = pts - mpctx->video_pts;
        if (frame_time <= 0 || frame_time >= 60) {
            // Assume a PTS difference >= 60 seconds is a discontinuity.
            MP_WARN(mpctx, "Invalid video timestamp: %f -> %f\n",
                    mpctx->video_pts, pts);
            frame_time = 0;
        }
    }
    mpctx->video_next_pts = pts;
    mpctx->delay -= frame_time;
    if (mpctx->video_status >= STATUS_PLAYING) {
        mpctx->time_frame += frame_time / mpctx->opts->playback_speed;
        adjust_sync(mpctx, pts, frame_time);
    }
    mpctx->dropped_frames = 0;
    MP_TRACE(mpctx, "frametime=%5.3f\n", frame_time);
}

// Whether it's fine to call add_new_frame() now.
static bool needs_new_frame(struct MPContext *mpctx)
{
    return !mpctx->next_frame[1];
}

// Queue a frame to mpctx->next_frame[]. Call only if needs_new_frame() signals ok.
static void add_new_frame(struct MPContext *mpctx, struct mp_image *frame)
{
    assert(needs_new_frame(mpctx));
    assert(frame);
    mpctx->next_frame[1] = frame;
    shift_new_frame(mpctx);
}

// Enough video filtered already to push one frame to the VO?
// Set eof to true if no new frames are to be expected.
static bool have_new_frame(struct MPContext *mpctx, bool eof)
{
    bool need_2nd = !!(mpctx->opts->frame_dropping & 1) // we need the duration
        && mpctx->video_pts != MP_NOPTS_VALUE   // ...except for the 1st frame
        && !eof;    // on EOF, drain the remaining frames

    return mpctx->next_frame[0] && (!need_2nd || mpctx->next_frame[1]);
}

// Fill mpctx->next_frame[] with a newly filtered or decoded image.
// returns VD_* code
static int video_output_image(struct MPContext *mpctx, double endpts)
{
    bool hrseek = mpctx->hrseek_active && mpctx->video_status == STATUS_SYNCING;

    if (mpctx->d_video->header->attached_picture) {
        if (vo_has_frame(mpctx->video_out))
            return VD_EOF;
        if (mpctx->next_frame[0])
            return VD_NEW_FRAME;
        int r = video_decode_and_filter(mpctx);
        video_filter(mpctx, true); // force EOF filtering (avoid decoding more)
        mpctx->next_frame[0] = vf_read_output_frame(mpctx->d_video->vfilter);
        if (mpctx->next_frame[0])
            mpctx->next_frame[0]->pts = MP_NOPTS_VALUE;
        return r <= 0 ? VD_EOF : VD_PROGRESS;
    }

    if (have_new_frame(mpctx, false))
        return VD_NEW_FRAME;

    // Get a new frame if we need one.
    int r = VD_PROGRESS;
    if (needs_new_frame(mpctx)) {
        // Filter a new frame.
        r = video_decode_and_filter(mpctx);
        if (r < 0)
            return r; // error
        struct mp_image *img = vf_read_output_frame(mpctx->d_video->vfilter);
        if (img) {
            // Always add these; they make backstepping after seeking faster.
            add_frame_pts(mpctx, img->pts);

            if (endpts != MP_NOPTS_VALUE && img->pts >= endpts) {
                r = VD_EOF;
            } else if (mpctx->max_frames == 0) {
                r = VD_EOF;
            } else if (hrseek && mpctx->hrseek_lastframe) {
                mp_image_setrefp(&mpctx->saved_frame, img);
            } else if (hrseek && img->pts < mpctx->hrseek_pts - .005) {
                /* just skip */
            } else {
                add_new_frame(mpctx, img);
                img = NULL;
            }
            talloc_free(img);
        }
    }

    // Last-frame seek
    if (r <= 0 && hrseek && mpctx->hrseek_lastframe && mpctx->saved_frame) {
        add_new_frame(mpctx, mpctx->saved_frame);
        mpctx->saved_frame = NULL;
        r = VD_PROGRESS;
    }

    return have_new_frame(mpctx, r <= 0) ? VD_NEW_FRAME : r;
}

/* Update avsync before a new video frame is displayed. Actually, this can be
 * called arbitrarily often before the actual display.
 * This adjusts the time of the next video frame */
static void update_avsync_before_frame(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct vo *vo = mpctx->video_out;

    if (!mpctx->sync_audio_to_video || mpctx->video_status < STATUS_READY) {
        mpctx->time_frame = 0;
    } else if (mpctx->audio_status == STATUS_PLAYING &&
               mpctx->video_status == STATUS_PLAYING &&
               !ao_untimed(mpctx->ao))
    {
        double buffered_audio = ao_get_delay(mpctx->ao);

        double predicted = mpctx->delay / opts->playback_speed +
                           mpctx->time_frame;
        double difference = buffered_audio - predicted;
        MP_STATS(mpctx, "value %f audio-diff", difference);

        if (opts->autosync) {
            /* Smooth reported playback position from AO by averaging
             * it with the value expected based on previus value and
             * time elapsed since then. May help smooth video timing
             * with audio output that have inaccurate position reporting.
             * This is badly implemented; the behavior of the smoothing
             * now undesirably depends on how often this code runs
             * (mainly depends on video frame rate). */
            buffered_audio = predicted + difference / opts->autosync;
        }

        mpctx->time_frame = buffered_audio - mpctx->delay / opts->playback_speed;
    } else {
        /* If we're more than 200 ms behind the right playback
         * position, don't try to speed up display of following
         * frames to catch up; continue with default speed from
         * the current frame instead.
         * If untimed is set always output frames immediately
         * without sleeping.
         */
        if (mpctx->time_frame < -0.2 || opts->untimed || vo->driver->untimed)
            mpctx->time_frame = 0;
    }
}

// Update the A/V sync difference after a video frame has been shown.
static void update_avsync_after_frame(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    mpctx->last_av_difference = 0;

    if (mpctx->audio_status != STATUS_PLAYING ||
        mpctx->video_status != STATUS_PLAYING)
        return;

    double a_pos = playing_audio_pts(mpctx);

    mpctx->last_av_difference = a_pos - mpctx->video_pts + opts->audio_delay;
    if (mpctx->time_frame > 0)
        mpctx->last_av_difference += mpctx->time_frame * opts->playback_speed;
    if (a_pos == MP_NOPTS_VALUE || mpctx->video_pts == MP_NOPTS_VALUE) {
        mpctx->last_av_difference = MP_NOPTS_VALUE;
    } else if (fabs(mpctx->last_av_difference) > 0.5 && !mpctx->drop_message_shown) {
        MP_WARN(mpctx, "%s", av_desync_help_text);
        mpctx->drop_message_shown = true;
    }
}

static void init_vo(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct dec_video *d_video = mpctx->d_video;

    if (opts->gamma_gamma != 1000)
        video_set_colors(d_video, "gamma", opts->gamma_gamma);
    if (opts->gamma_brightness != 1000)
        video_set_colors(d_video, "brightness", opts->gamma_brightness);
    if (opts->gamma_contrast != 1000)
        video_set_colors(d_video, "contrast", opts->gamma_contrast);
    if (opts->gamma_saturation != 1000)
        video_set_colors(d_video, "saturation", opts->gamma_saturation);
    if (opts->gamma_hue != 1000)
        video_set_colors(d_video, "hue", opts->gamma_hue);

    mp_notify(mpctx, MPV_EVENT_VIDEO_RECONFIG, NULL);
}

void write_video(struct MPContext *mpctx, double endpts)
{
    struct MPOpts *opts = mpctx->opts;
    struct vo *vo = mpctx->video_out;

    if (!mpctx->d_video)
        return;

    // Actual playback starts when both audio and video are ready.
    if (mpctx->video_status == STATUS_READY)
        return;

    if (mpctx->paused && mpctx->video_status >= STATUS_READY)
        return;

    int r = video_output_image(mpctx, endpts);
    MP_TRACE(mpctx, "video_output_image: %d\n", r);

    if (r < 0)
        goto error;

    if (r == VD_WAIT) // Demuxer will wake us up for more packets to decode.
        return;

    if (r == VD_EOF) {
        mpctx->video_status =
            vo_still_displaying(vo) ? STATUS_DRAINING : STATUS_EOF;
        mpctx->delay = 0;
        mpctx->last_av_difference = 0;
        MP_DBG(mpctx, "video EOF (status=%d)\n", mpctx->video_status);
        return;
    }

    if (mpctx->video_status > STATUS_PLAYING)
        mpctx->video_status = STATUS_PLAYING;

    if (r != VD_NEW_FRAME) {
        mpctx->sleeptime = 0; // Decode more in next iteration.
        return;
    }

    // Filter output is different from VO input?
    struct mp_image_params p = mpctx->next_frame[0]->params;
    if (!vo->params || !mp_image_params_equal(&p, vo->params)) {
        // Changing config deletes the current frame; wait until it's finished.
        if (vo_still_displaying(vo))
            return;

        const struct vo_driver *info = mpctx->video_out->driver;
        char extra[20] = {0};
        if (p.w != p.d_w || p.h != p.d_h)
            snprintf(extra, sizeof(extra), " => %dx%d", p.d_w, p.d_h);
        MP_INFO(mpctx, "VO: [%s] %dx%d%s %s\n",
                info->name, p.w, p.h, extra, vo_format_name(p.imgfmt));
        MP_VERBOSE(mpctx, "VO: Description: %s\n", info->description);

        int vo_r = vo_reconfig(vo, &p, 0);
        if (vo_r < 0) {
            mpctx->error_playing = MPV_ERROR_VO_INIT_FAILED;
            goto error;
        }
        init_vo(mpctx);
    }

    mpctx->time_frame -= get_relative_time(mpctx);
    update_avsync_before_frame(mpctx);

    double time_frame = MPMAX(mpctx->time_frame, -1);
    int64_t pts = mp_time_us() + (int64_t)(time_frame * 1e6);

    // wait until VO wakes us up to get more frames
    if (!vo_is_ready_for_frame(vo, pts)) {
        if (video_feed_async_filter(mpctx) < 0)
            goto error;
        return;
    }

    int64_t duration = -1;
    double diff = -1;
    double vpts0 = mpctx->next_frame[0] ? mpctx->next_frame[0]->pts : MP_NOPTS_VALUE;
    double vpts1 = mpctx->next_frame[1] ? mpctx->next_frame[1]->pts : MP_NOPTS_VALUE;
    if (vpts0 != MP_NOPTS_VALUE && vpts1 != MP_NOPTS_VALUE)
        diff = vpts1 - vpts0;
    if (diff < 0 && mpctx->d_video->fps > 0)
        diff = 1.0 / mpctx->d_video->fps; // fallback to demuxer-reported fps
    if (opts->untimed || vo->driver->untimed)
        diff = -1; // disable frame dropping and aspects of frame timing
    if (diff >= 0) {
        // expected A/V sync correction is ignored
        diff /= opts->playback_speed;
        if (mpctx->time_frame < 0)
            diff += mpctx->time_frame;
        duration = MPCLAMP(diff, 0, 10) * 1e6;
    }

    mpctx->video_pts = mpctx->next_frame[0]->pts;
    mpctx->last_vo_pts = mpctx->video_pts;
    mpctx->playback_pts = mpctx->video_pts;

    update_avsync_after_frame(mpctx);

    mpctx->osd_force_update = true;
    update_osd_msg(mpctx);
    update_subtitles(mpctx);

    vo_queue_frame(vo, mpctx->next_frame[0], pts, duration);
    mpctx->next_frame[0] = NULL;

    shift_new_frame(mpctx);

    mpctx->shown_vframes++;
    if (mpctx->video_status < STATUS_PLAYING) {
        mpctx->video_status = STATUS_READY;
        // After a seek, make sure to wait until the first frame is visible.
        vo_wait_frame(vo);
    }
    screenshot_flip(mpctx);

    mp_notify(mpctx, MPV_EVENT_TICK, NULL);

    if (!mpctx->sync_audio_to_video)
        mpctx->video_status = STATUS_EOF;

    if (mpctx->video_status != STATUS_EOF) {
        if (mpctx->step_frames > 0) {
            mpctx->step_frames--;
            if (!mpctx->step_frames && !opts->pause)
                pause_player(mpctx);
        }
        if (mpctx->max_frames == 0)
            mpctx->stop_play = AT_END_OF_FILE;
        if (mpctx->max_frames > 0)
            mpctx->max_frames--;
    }

    mpctx->sleeptime = 0;
    return;

error:
    MP_FATAL(mpctx, "Could not initialize video chain.\n");
    uninit_video_chain(mpctx);
    error_on_track(mpctx, mpctx->current_track[STREAM_VIDEO][0]);
    handle_force_window(mpctx, true);
    mpctx->sleeptime = 0;
}
