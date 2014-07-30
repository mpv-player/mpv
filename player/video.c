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
};

static const char av_desync_help_text[] =
"\n\n"
"           *************************************************\n"
"           **** Audio/Video desynchronisation detected! ****\n"
"           *************************************************\n\n"
"This means either the audio or the video is played too slowly.\n"
"Possible reasons, problems, workarounds:\n"
"- Your system is simply too slow for this file.\n"
"     Transcode it to a lower bitrate file with tools like HandBrake.\n"
"- Broken/buggy _audio_ driver.\n"
"     Experiment with different values for --autosync, 30 is a good start.\n"
"     If you have PulseAudio, try --ao=alsa .\n"
"- Slow video output.\n"
"     Try a different --vo driver (--vo=help for a list) or try --framedrop!\n"
"- Playing a video file with --vo=opengl with higher FPS than the monitor.\n"
"     This is due to vsync limiting the framerate.\n"
"- Playing from a slow network source.\n"
"     Download the file instead.\n"
"- Try to find out whether audio/video/subs are causing this by experimenting\n"
"  with --no-video, --no-audio, or --no-sub.\n"
"- If you swiched audio or video tracks, try seeking to force synchronization.\n"
"If none of this helps you, file a bug report.\n\n";

void update_fps(struct MPContext *mpctx)
{
#if HAVE_ENCODING
    struct dec_video *d_video = mpctx->d_video;
    if (mpctx->encode_lavc_ctx && d_video)
        encode_lavc_set_video_fps(mpctx->encode_lavc_ctx, d_video->fps);
#endif
}

static void set_allowed_vo_formats(struct vf_chain *c, struct vo *vo)
{
    for (int fmt = IMGFMT_START; fmt < IMGFMT_END; fmt++) {
        c->allowed_output_formats[fmt - IMGFMT_START] =
            vo->driver->query_format(vo, fmt);
    }
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
            char deg[10];
            snprintf(deg, sizeof(deg), "%d", params.rotate);
            char *args[] = {"angle", deg, NULL, NULL};
            if (try_filter(mpctx, params, "rotate", "autorotate", args) >= 0) {
                params.rotate = 0;
            } else {
                MP_ERR(mpctx, "Can't insert rotation filter.\n");
            }
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
    d_video->vfilter->hwdec = &d_video->hwdec_info;

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

    if (!d_video->vfilter)
        return 0;

    return d_video->vfilter->initialized;
}

void reset_video_state(struct MPContext *mpctx)
{
    if (mpctx->d_video)
        video_reset_decoding(mpctx->d_video);
    if (mpctx->video_out)
        vo_seek_reset(mpctx->video_out);

    mpctx->delay = 0;
    mpctx->time_frame = 0;
    mpctx->video_next_pts = MP_NOPTS_VALUE;
    mpctx->playing_last_frame = false;
    mpctx->last_frame_duration = 0;
    mpctx->total_avsync_change = 0;
    mpctx->drop_frame_cnt = 0;
    mpctx->dropped_frames = 0;
    mpctx->drop_message_shown = 0;

    mpctx->video_status = mpctx->d_video ? STATUS_SYNCING : STATUS_EOF;
}

int reinit_video_chain(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    assert(!(mpctx->initialized_flags & INITIALIZED_VCODEC));
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
    if (!opts->fixed_vo || !(mpctx->initialized_flags & INITIALIZED_VO)) {
        mpctx->video_out = init_best_video_out(mpctx->global, mpctx->input,
                                               mpctx->osd,
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
    d_video->global = mpctx->global;
    d_video->log = mp_log_new(d_video, mpctx->log, "!vd");
    d_video->opts = mpctx->opts;
    d_video->header = sh;
    d_video->fps = sh->video->fps;
    d_video->vo = mpctx->video_out;
    mpctx->initialized_flags |= INITIALIZED_VCODEC;

    vo_control(mpctx->video_out, VOCTRL_GET_HWDEC_INFO, &d_video->hwdec_info);

    recreate_video_filters(mpctx);

    if (!video_init_best_codec(d_video, opts->video_decoders))
        goto err_out;

    bool saver_state = opts->pause || !opts->stop_screensaver;
    vo_control(mpctx->video_out, saver_state ? VOCTRL_RESTORE_SCREENSAVER
                                             : VOCTRL_KILL_SCREENSAVER, NULL);

    vo_control(mpctx->video_out, mpctx->paused ? VOCTRL_PAUSE
                                               : VOCTRL_RESUME, NULL);

    mpctx->sync_audio_to_video = !sh->attached_picture;
    mpctx->vo_pts_history_seek_ts++;

    // If we switch on video again, ensure audio position matches up.
    if (mpctx->d_audio)
        mpctx->audio_status = STATUS_SYNCING;

    reset_video_state(mpctx);
    reset_subtitle_state(mpctx);

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
    mp_deselect_track(mpctx, track);
    handle_force_window(mpctx, true);
    MP_INFO(mpctx, "Video: no video\n");
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
    if (opts->pause && mpctx->last_vo_pts != MP_NOPTS_VALUE)
        queue_seek(mpctx, MPSEEK_ABSOLUTE, mpctx->last_vo_pts, 2, true);
}

static int check_framedrop(struct MPContext *mpctx, double frame_time)
{
    struct MPOpts *opts = mpctx->opts;
    struct track *t_audio = mpctx->current_track[0][STREAM_AUDIO];
    struct sh_stream *sh_audio = t_audio ? t_audio->stream : NULL;
    // check for frame-drop:
    if (mpctx->d_audio && mpctx->ao && !ao_untimed(mpctx->ao) && sh_audio &&
        !demux_stream_eof(sh_audio))
    {
        float delay = opts->playback_speed * ao_get_delay(mpctx->ao);
        float d = delay - mpctx->delay;
        float fps = mpctx->d_video->fps;
        if (frame_time < 0)
            frame_time = fps > 0 ? 1.0 / fps : 0;
        // we should avoid dropping too many frames in sequence unless we
        // are too late. and we allow 100ms A-V delay here:
        if (d < -mpctx->dropped_frames * frame_time - 0.100 && !mpctx->paused
            && mpctx->video_status == STATUS_PLAYING) {
            mpctx->drop_frame_cnt++;
            mpctx->dropped_frames++;
            return mpctx->opts->frame_dropping;
        } else
            mpctx->dropped_frames = 0;
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
        return VD_EOF;
    }

    struct demux_packet *pkt;
    if (demux_read_packet_async(d_video->header, &pkt) == 0)
        return VD_WAIT;
    if (pkt && pkt->pts != MP_NOPTS_VALUE)
        pkt->pts += mpctx->video_offset;
    if ((pkt && pkt->pts >= mpctx->hrseek_pts - .005) ||
        d_video->has_broken_packet_pts ||
        !mpctx->opts->hr_seek_framedrop)
    {
        mpctx->hrseek_framedrop = false;
    }
    bool hrseek = mpctx->hrseek_active && mpctx->video_status == STATUS_SYNCING;
    int framedrop_type = hrseek && mpctx->hrseek_framedrop ?
                         1 : check_framedrop(mpctx, -1);
    d_video->waiting_decoded_mpi =
        video_decode(d_video, pkt, framedrop_type);
    bool had_packet = !!pkt;
    talloc_free(pkt);

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

// Make sure at least 1 filtered image is available.
// returns VD_* code
// A return value of VD_PROGRESS doesn't necessarily output a frame, but makes
// the promise that calling this function again will eventually do something.
static int video_decode_and_filter(struct MPContext *mpctx)
{
    struct dec_video *d_video = mpctx->d_video;
    struct vf_chain *vf = d_video->vfilter;

    if (vf->initialized < 0)
        return VD_ERROR;

    // There is already a filtered frame available.
    if (vf_output_frame(vf, false) > 0)
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
        return VD_PROGRESS;
    }

    // If something was decoded, and the filter chain is ready, filter it.
    if (!need_vf_reconfig && d_video->waiting_decoded_mpi) {
        vf_filter_frame(vf, d_video->waiting_decoded_mpi);
        d_video->waiting_decoded_mpi = NULL;
        return VD_PROGRESS;
    }

    if (!d_video->waiting_decoded_mpi) {
        // Decode a new image, or at least feed the decoder a packet.
        int r = decode_image(mpctx);
        if (r == VD_WAIT)
            return r;
        if (d_video->waiting_decoded_mpi)
            d_video->decoder_output = d_video->waiting_decoded_mpi->params;
        if (!d_video->waiting_decoded_mpi && (r == VD_EOF || r < 0)) {
            if (vf_output_frame(vf, true) > 0)
                return VD_PROGRESS;
            return VD_EOF; // true EOF
        }
    }

    // Image will be filtered on the next iteration.
    return VD_PROGRESS;
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

// Fill the VO buffer with a newly filtered or decoded image.
// returns VD_* code
static int video_output_image(struct MPContext *mpctx, double endpts,
                              bool reconfig_ok)
{
    struct vf_chain *vf = mpctx->d_video->vfilter;
    struct vo *vo = mpctx->video_out;

    // Already enough video buffered in VO?
    // (This implies vo_has_next_frame(vo, false/true) returns true.)
    if (!vo_needs_new_image(vo) && vo->params)
        return 1;

    // Filter a new frame.
    int r = video_decode_and_filter(mpctx);
    if (r < 0)
        return r; // error

    vf_output_frame(vf, false);
    if (vf->output) {
        double pts = vf->output->pts;

        // Always add these; they make backstepping after seeking faster.
        add_frame_pts(mpctx, pts);

        bool drop = false;
        bool hrseek = mpctx->hrseek_active && mpctx->video_status == STATUS_SYNCING;
        if (hrseek && pts < mpctx->hrseek_pts - .005)
            drop = true;
        if (endpts != MP_NOPTS_VALUE && pts >= endpts) {
            drop = true;
            r = VD_EOF;
        }
        if (drop) {
            talloc_free(vf->output);
            vf->output = NULL;
            return r;
        }
    }

    // Filter output is different from VO input?
    bool need_vo_reconfig = !vo->params  ||
        !mp_image_params_equal(&vf->output_params, vo->params);

    if (need_vo_reconfig) {
        // Draining VO buffers.
        if (vo_has_next_frame(vo, true))
            return 0; // EOF so that caller displays remaining VO frames

        // There was no decoded image yet - must not signal fake EOF.
        // Likewise, if there's no filtered frame yet, don't reconfig yet.
        if (!vf->output_params.imgfmt || !vf->output)
            return r;

        // Force draining.
        if (!reconfig_ok)
            return 0;

        struct mp_image_params p = vf->output_params;

        const struct vo_driver *info = mpctx->video_out->driver;
        MP_INFO(mpctx, "VO: [%s] %dx%d => %dx%d %s\n",
                info->name, p.w, p.h, p.d_w, p.d_h, vo_format_name(p.imgfmt));
        MP_VERBOSE(mpctx, "VO: Description: %s\n", info->description);

        int vo_r = vo_reconfig(vo, &p, 0);
        if (vo_r < 0) {
            vf->initialized = -1;
            return VD_ERROR;
        }
        init_vo(mpctx);
        // Display the frame queued after this immediately.
        // (Neutralizes frame time calculation in update_video.)
        mpctx->video_next_pts = MP_NOPTS_VALUE;
    }

    // Queue new frame, if there's one.
    struct mp_image *img = vf_read_output_frame(vf);
    if (img) {
        vo_queue_image(vo, img);
        return VD_PROGRESS;
    }

    return r; // includes the true EOF case
}

// returns VD_* code
static int update_video(struct MPContext *mpctx, double endpts, bool reconfig_ok,
                        double *frame_duration)
{
    struct vo *video_out = mpctx->video_out;

    if (mpctx->d_video->header->attached_picture) {
        if (video_out->hasframe)
            return VD_EOF;
        if (vo_has_next_frame(video_out, true))
            return VD_NEW_FRAME;
    }

    int r = video_output_image(mpctx, endpts, reconfig_ok);
    if (r < 0 || r == VD_WAIT)
        return r;

    // On EOF, we always drain the VO; otherwise we must ensure that
    // the VO will have enough frames buffered (matters especially for VO based
    // frame dropping).
    if (!vo_has_next_frame(video_out, r == VD_EOF))
        return r ? VD_PROGRESS : VD_EOF;

    if (mpctx->d_video->header->attached_picture) {
        mpctx->video_next_pts = MP_NOPTS_VALUE;
        return VD_NEW_FRAME;
    }

    double pts = vo_get_next_pts(video_out, 0);
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
    *frame_duration = frame_time;
    return VD_NEW_FRAME;
}

static double timing_sleep(struct MPContext *mpctx, double time_frame)
{
    // assume kernel HZ=100 for softsleep, works with larger HZ but with
    // unnecessarily high CPU usage
    struct MPOpts *opts = mpctx->opts;
    double margin = opts->softsleep ? 0.011 : 0;
    while (time_frame > margin) {
        mp_sleep_us(1000000 * (time_frame - margin));
        time_frame -= get_relative_time(mpctx);
    }
    if (opts->softsleep) {
        if (time_frame < 0)
            MP_WARN(mpctx, "Warning! Softsleep underflow!\n");
        while (time_frame > 0)
            time_frame -= get_relative_time(mpctx);  // burn the CPU
    }
    return time_frame;
}

static void update_avsync(struct MPContext *mpctx)
{
    if (mpctx->audio_status != STATUS_PLAYING ||
        mpctx->video_status != STATUS_PLAYING)
        return;

    double a_pos = playing_audio_pts(mpctx);

    mpctx->last_av_difference = a_pos - mpctx->video_pts + mpctx->audio_delay;
    if (mpctx->time_frame > 0)
        mpctx->last_av_difference +=
                mpctx->time_frame * mpctx->opts->playback_speed;
    if (a_pos == MP_NOPTS_VALUE || mpctx->video_pts == MP_NOPTS_VALUE)
        mpctx->last_av_difference = MP_NOPTS_VALUE;
    if (mpctx->last_av_difference > 0.5 && mpctx->drop_frame_cnt > 50
        && !mpctx->drop_message_shown) {
        MP_WARN(mpctx, "%s", av_desync_help_text);
        mpctx->drop_message_shown = true;
    }
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
static void adjust_sync(struct MPContext *mpctx, double frame_time)
{
    struct MPOpts *opts = mpctx->opts;

    if (mpctx->audio_status != STATUS_PLAYING)
        return;

    double a_pts = written_audio_pts(mpctx) - mpctx->delay;
    double v_pts = mpctx->video_next_pts;
    double av_delay = a_pts - v_pts;
    // Try to sync vo_flip() so it will *finish* at given time
    av_delay += mpctx->last_vo_flip_duration;
    av_delay += mpctx->audio_delay;   // This much pts difference is desired

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

void write_video(struct MPContext *mpctx, double endpts)
{
    struct MPOpts *opts = mpctx->opts;
    struct vo *vo = mpctx->video_out;

    if (!mpctx->d_video)
        return;

    update_fps(mpctx);

    // Whether there's still at least 1 video frame that can be shown.
    // If false, it means we can reconfig the VO if needed (normally, this
    // would disrupt playback, so only do it on !still_playing).
    bool still_playing = vo_has_next_frame(vo, true);
    // For the last frame case (frame is being displayed).
    still_playing |= mpctx->playing_last_frame;
    still_playing |= mpctx->last_frame_duration > 0;

    double frame_time = 0;
    int r = update_video(mpctx, endpts, !still_playing, &frame_time);
    MP_TRACE(mpctx, "update_video: %d (still_playing=%d)\n", r, still_playing);

    if (r == VD_WAIT) // Demuxer will wake us up for more packets to decode.
        return;

    if (r < 0) {
        MP_FATAL(mpctx, "Could not initialize video chain.\n");
        int uninit = INITIALIZED_VCODEC;
        if (!opts->force_vo)
            uninit |= INITIALIZED_VO;
        uninit_player(mpctx, uninit);
        if (!mpctx->current_track[STREAM_AUDIO])
            mpctx->stop_play = PT_NEXT_ENTRY;
        mpctx->error_playing = true;
        handle_force_window(mpctx, true);
        return; // restart loop
    }

    if (r == VD_EOF) {
        if (!mpctx->playing_last_frame && mpctx->last_frame_duration > 0) {
            mpctx->time_frame += mpctx->last_frame_duration;
            mpctx->last_frame_duration = 0;
            mpctx->playing_last_frame = true;
            MP_VERBOSE(mpctx, "showing last frame\n");
        }
    }

    if (r == VD_NEW_FRAME) {
        MP_TRACE(mpctx, "frametime=%5.3f\n", frame_time);

        if (mpctx->video_status > STATUS_PLAYING)
            mpctx->video_status = STATUS_PLAYING;

        if (mpctx->video_status >= STATUS_READY) {
            mpctx->time_frame += frame_time / opts->playback_speed;
            adjust_sync(mpctx, frame_time);
        }
    } else if (r == VD_EOF && mpctx->playing_last_frame) {
        // Let video timing code continue displaying.
        mpctx->video_status = STATUS_DRAINING;
        MP_VERBOSE(mpctx, "still showing last frame\n");
    } else if (r <= 0) {
        // EOF or error
        mpctx->delay = 0;
        mpctx->last_av_difference = 0;
        mpctx->video_status = STATUS_EOF;
        MP_VERBOSE(mpctx, "video EOF\n");
    } else {
        if (mpctx->video_status > STATUS_PLAYING)
            mpctx->video_status = STATUS_PLAYING;

        // Decode more in next iteration.
        mpctx->sleeptime = 0;
        MP_TRACE(mpctx, "filtering more video\n");
    }

    // Actual playback starts when both audio and video are ready.
    if (mpctx->video_status == STATUS_READY)
        return;

    if (mpctx->paused && mpctx->video_status >= STATUS_READY)
        return;

    mpctx->time_frame -= get_relative_time(mpctx);
    double audio_pts = playing_audio_pts(mpctx);
    if (!mpctx->sync_audio_to_video || mpctx->video_status < STATUS_READY) {
        mpctx->time_frame = 0;
    } else if (mpctx->audio_status == STATUS_PLAYING &&
               mpctx->video_status == STATUS_PLAYING)
    {
        double buffered_audio = ao_get_delay(mpctx->ao);
        MP_TRACE(mpctx, "audio delay=%f\n", buffered_audio);

        if (opts->autosync) {
            /* Smooth reported playback position from AO by averaging
             * it with the value expected based on previus value and
             * time elapsed since then. May help smooth video timing
             * with audio output that have inaccurate position reporting.
             * This is badly implemented; the behavior of the smoothing
             * now undesirably depends on how often this code runs
             * (mainly depends on video frame rate). */
            float predicted = (mpctx->delay / opts->playback_speed +
                                mpctx->time_frame);
            float difference = buffered_audio - predicted;
            buffered_audio = predicted + difference / opts->autosync;
        }

        mpctx->time_frame = (buffered_audio -
                                mpctx->delay / opts->playback_speed);
    } else {
        /* If we're more than 200 ms behind the right playback
         * position, don't try to speed up display of following
         * frames to catch up; continue with default speed from
         * the current frame instead.
         * If untimed is set always output frames immediately
         * without sleeping.
         */
        if (mpctx->time_frame < -0.2 || opts->untimed || vo->untimed)
            mpctx->time_frame = 0;
    }

    double vsleep = mpctx->time_frame - vo->flip_queue_offset;
    if (vsleep > 0.050) {
        mpctx->sleeptime = MPMIN(mpctx->sleeptime, vsleep - 0.040);
        return;
    }
    mpctx->sleeptime = 0;
    mpctx->playing_last_frame = false;

    // last frame case
    if (r != VD_NEW_FRAME)
        return;

    //=================== FLIP PAGE (VIDEO BLT): ======================


    mpctx->video_pts = mpctx->video_next_pts;
    mpctx->last_vo_pts = mpctx->video_pts;
    mpctx->playback_pts = mpctx->video_pts;

    update_subtitles(mpctx);
    update_osd_msg(mpctx);

    MP_STATS(mpctx, "vo draw frame");

    vo_new_frame_imminent(vo);

    MP_STATS(mpctx, "vo sleep");

    mpctx->time_frame -= get_relative_time(mpctx);
    mpctx->time_frame -= vo->flip_queue_offset;
    if (mpctx->time_frame > 0.001)
        mpctx->time_frame = timing_sleep(mpctx, mpctx->time_frame);
    mpctx->time_frame += vo->flip_queue_offset;

    int64_t t2 = mp_time_us();
    /* Playing with playback speed it's possible to get pathological
     * cases with mpctx->time_frame negative enough to cause an
     * overflow in pts_us calculation, thus the MPMAX. */
    double time_frame = MPMAX(mpctx->time_frame, -1);
    int64_t pts_us = mpctx->last_time + time_frame * 1e6;
    int duration = -1;
    double pts2 = vo_get_next_pts(vo, 0); // this is the next frame PTS
    if (mpctx->video_pts != MP_NOPTS_VALUE && pts2 == MP_NOPTS_VALUE) {
        // Make up a frame duration. Using the frame rate is not a good
        // choice, since the frame rate could be unset/broken/random.
        float fps = mpctx->d_video->fps;
        double frame_duration = fps > 0 ? 1.0 / fps : 0;
        pts2 = mpctx->video_pts + MPCLAMP(frame_duration, 0.0, 5.0);
    }
    if (pts2 != MP_NOPTS_VALUE) {
        // expected A/V sync correction is ignored
        double diff = (pts2 - mpctx->video_pts);
        diff /= opts->playback_speed;
        if (mpctx->time_frame < 0)
            diff += mpctx->time_frame;
        if (diff < 0)
            diff = 0;
        if (diff > 10)
            diff = 10;
        duration = diff * 1e6;
        mpctx->last_frame_duration = diff;
    }
    if (mpctx->video_status != STATUS_PLAYING)
        duration = -1;

    MP_STATS(mpctx, "start flip");
    vo_flip_page(vo, pts_us | 1, duration);
    MP_STATS(mpctx, "end flip");

    if (audio_pts != MP_NOPTS_VALUE)
        MP_STATS(mpctx, "value %f ptsdiff", mpctx->video_pts - audio_pts);

    mpctx->last_vo_flip_duration = (mp_time_us() - t2) * 0.000001;
    if (vo->driver->flip_page_timed) {
        // No need to adjust sync based on flip speed
        mpctx->last_vo_flip_duration = 0;
        // For print_status - VO call finishing early is OK for sync
        mpctx->time_frame -= get_relative_time(mpctx);
    }
    mpctx->shown_vframes++;
    if (mpctx->video_status < STATUS_PLAYING)
        mpctx->video_status = STATUS_READY;
    update_avsync(mpctx);
    screenshot_flip(mpctx);

    mp_notify(mpctx, MPV_EVENT_TICK, NULL);

    if (!mpctx->sync_audio_to_video)
        mpctx->video_status = STATUS_EOF;
}
