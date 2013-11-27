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

#include "config.h"
#include "mpvcore/options.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "mpvcore/mp_msg.h"

#include "osdep/timer.h"

#include "stream/stream.h"
#include "demux/packet.h"

#include "mpvcore/codecs.h"

#include "video/out/vo.h"
#include "video/csputils.h"

#include "demux/stheader.h"
#include "video/decode/vd.h"
#include "video/filter/vf.h"

#include "video/decode/dec_video.h"

extern const vd_functions_t mpcodecs_vd_ffmpeg;

/* Please do not add any new decoders here. If you want to implement a new
 * decoder, add it to libavcodec, except for wrappers around external
 * libraries and decoders requiring binary support. */

const vd_functions_t * const mpcodecs_vd_drivers[] = {
    &mpcodecs_vd_ffmpeg,
    /* Please do not add any new decoders here. If you want to implement a new
     * decoder, add it to libavcodec, except for wrappers around external
     * libraries and decoders requiring binary support. */
    NULL
};

int video_vd_control(struct dec_video *d_video, int cmd, void *arg)
{
    const struct vd_functions *vd = d_video->vd_driver;
    if (vd)
        return vd->control(d_video, cmd, arg);
    return CONTROL_UNKNOWN;
}

int video_set_colors(struct dec_video *d_video, const char *item, int value)
{
    vf_instance_t *vf = d_video->vfilter;
    vf_equalizer_t data;

    data.item = item;
    data.value = value;

    mp_dbg(MSGT_DECVIDEO, MSGL_V, "set video colors %s=%d \n", item, value);
    if (vf) {
        int ret = vf_control(vf, VFCTRL_SET_EQUALIZER, &data);
        if (ret == CONTROL_TRUE)
            return 1;
    }
    mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Video attribute '%s' is not supported by selected vo.\n",
           item);
    return 0;
}

int video_get_colors(struct dec_video *d_video, const char *item, int *value)
{
    vf_instance_t *vf = d_video->vfilter;
    vf_equalizer_t data;

    data.item = item;

    mp_dbg(MSGT_DECVIDEO, MSGL_V, "get video colors %s \n", item);
    if (vf) {
        int ret = vf_control(vf, VFCTRL_GET_EQUALIZER, &data);
        if (ret == CONTROL_TRUE) {
            *value = data.value;
            return 1;
        }
    }
    return 0;
}

void video_resync_stream(struct dec_video *d_video)
{
    video_vd_control(d_video, VDCTRL_RESYNC_STREAM, NULL);
    d_video->prev_codec_reordered_pts = MP_NOPTS_VALUE;
    d_video->prev_sorted_pts = MP_NOPTS_VALUE;
}

void video_reinit_vo(struct dec_video *d_video)
{
    video_vd_control(d_video, VDCTRL_REINIT_VO, NULL);
}

void video_uninit(struct dec_video *d_video)
{
    if (d_video->vd_driver) {
        mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Uninit video.\n");
        d_video->vd_driver->uninit(d_video);
    }
    talloc_free(d_video->priv);
    vf_uninit_filter_chain(d_video->vfilter);
    talloc_free(d_video);
}

static int init_video_codec(struct dec_video *d_video, const char *decoder)
{
    if (!d_video->vd_driver->init(d_video, decoder)) {
        mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Video decoder init failed.\n");
        return 0;
    }

    d_video->prev_codec_reordered_pts = MP_NOPTS_VALUE;
    d_video->prev_sorted_pts = MP_NOPTS_VALUE;
    return 1;
}

struct mp_decoder_list *video_decoder_list(void)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);
    for (int i = 0; mpcodecs_vd_drivers[i] != NULL; i++)
        mpcodecs_vd_drivers[i]->add_decoders(list);
    return list;
}

static struct mp_decoder_list *mp_select_video_decoders(const char *codec,
                                                        char *selection)
{
    struct mp_decoder_list *list = video_decoder_list();
    struct mp_decoder_list *new = mp_select_decoders(list, codec, selection);
    talloc_free(list);
    return new;
}

static const struct vd_functions *find_driver(const char *name)
{
    for (int i = 0; mpcodecs_vd_drivers[i] != NULL; i++) {
        if (strcmp(mpcodecs_vd_drivers[i]->name, name) == 0)
            return mpcodecs_vd_drivers[i];
    }
    return NULL;
}

bool video_init_best_codec(struct dec_video *d_video, char* video_decoders)
{
    assert(!d_video->vd_driver);

    struct mp_decoder_entry *decoder = NULL;
    struct mp_decoder_list *list =
        mp_select_video_decoders(d_video->header->codec, video_decoders);

    mp_print_decoders(MSGT_DECVIDEO, MSGL_V, "Codec list:", list);

    for (int n = 0; n < list->num_entries; n++) {
        struct mp_decoder_entry *sel = &list->entries[n];
        const struct vd_functions *driver = find_driver(sel->family);
        if (!driver)
            continue;
        mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Opening video decoder %s:%s\n",
                sel->family, sel->decoder);
        d_video->vd_driver = driver;
        if (init_video_codec(d_video, sel->decoder)) {
            decoder = sel;
            break;
        }
        d_video->vd_driver = NULL;
        mp_tmsg(MSGT_DECVIDEO, MSGL_WARN, "Video decoder init failed for "
                "%s:%s\n", sel->family, sel->decoder);
    }

    if (d_video->vd_driver) {
        d_video->decoder_desc =
            talloc_asprintf(d_video, "%s [%s:%s]", decoder->desc, decoder->family,
                            decoder->decoder);
        mp_msg(MSGT_DECVIDEO, MSGL_INFO, "Selected video codec: %s\n",
               d_video->decoder_desc);
    } else {
        mp_msg(MSGT_DECVIDEO, MSGL_ERR,
               "Failed to initialize a video decoder for codec '%s'.\n",
               d_video->header->codec ? d_video->header->codec : "<unknown>");
    }

    talloc_free(list);
    return !!d_video->vd_driver;
}

static void determine_frame_pts(struct dec_video *d_video)
{
    struct MPOpts *opts = d_video->opts;

    if (!opts->correct_pts) {
        double frame_time = 1.0f / (d_video->fps > 0 ? d_video->fps : 25);
        double pkt_pts = d_video->last_packet_pdts;
        if (d_video->pts == MP_NOPTS_VALUE)
            d_video->pts = pkt_pts == MP_NOPTS_VALUE ? 0 : pkt_pts;

        d_video->pts = d_video->pts + frame_time;
        return;
    }

    if (opts->user_pts_assoc_mode)
        d_video->pts_assoc_mode = opts->user_pts_assoc_mode;
    else if (d_video->pts_assoc_mode == 0) {
        if (d_video->codec_reordered_pts != MP_NOPTS_VALUE)
            d_video->pts_assoc_mode = 1;
        else
            d_video->pts_assoc_mode = 2;
    } else {
        int probcount1 = d_video->num_reordered_pts_problems;
        int probcount2 = d_video->num_sorted_pts_problems;
        if (d_video->pts_assoc_mode == 2) {
            int tmp = probcount1;
            probcount1 = probcount2;
            probcount2 = tmp;
        }
        if (probcount1 >= probcount2 * 1.5 + 2) {
            d_video->pts_assoc_mode = 3 - d_video->pts_assoc_mode;
            mp_msg(MSGT_DECVIDEO, MSGL_WARN,
                   "Switching to pts association mode %d.\n",
                   d_video->pts_assoc_mode);
        }
    }
    d_video->pts = d_video->pts_assoc_mode == 1 ?
                   d_video->codec_reordered_pts : d_video->sorted_pts;
}

struct mp_image *video_decode(struct dec_video *d_video,
                              struct demux_packet *packet,
                              int drop_frame)
{
    struct MPOpts *opts = d_video->opts;
    bool sort_pts = opts->user_pts_assoc_mode != 1 && opts->correct_pts;
    double pkt_pts = packet ? packet->pts : MP_NOPTS_VALUE;
    double pkt_dts = packet ? packet->dts : MP_NOPTS_VALUE;

    double pkt_pdts = pkt_pts == MP_NOPTS_VALUE ? pkt_dts : pkt_pts;
    if (pkt_pdts != MP_NOPTS_VALUE)
        d_video->last_packet_pdts = pkt_pdts;

    if (sort_pts && pkt_pdts != MP_NOPTS_VALUE) {
        int delay = -1;
        video_vd_control(d_video, VDCTRL_QUERY_UNSEEN_FRAMES, &delay);
        if (delay >= 0) {
            if (delay > d_video->num_buffered_pts)
#if 0
                // this is disabled because vd_ffmpeg reports the same lag
                // after seek even when there are no buffered frames,
                // leading to incorrect error messages
                mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Not enough buffered pts\n");
#else
                ;
#endif
            else
                d_video->num_buffered_pts = delay;
        }
        if (d_video->num_buffered_pts ==
            sizeof(d_video->buffered_pts) / sizeof(double))
            mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Too many buffered pts\n");
        else {
            int i, j;
            for (i = 0; i < d_video->num_buffered_pts; i++)
                if (d_video->buffered_pts[i] < pkt_pdts)
                    break;
            for (j = d_video->num_buffered_pts; j > i; j--)
                d_video->buffered_pts[j] = d_video->buffered_pts[j - 1];
            d_video->buffered_pts[i] = pkt_pdts;
            d_video->num_buffered_pts++;
        }
    }

    struct mp_image *mpi = d_video->vd_driver->decode(d_video, packet, drop_frame);

    //------------------------ frame decoded. --------------------

    if (!mpi || drop_frame) {
        talloc_free(mpi);
        return NULL;            // error / skipped frame
    }

    if (opts->field_dominance == 0)
        mpi->fields |= MP_IMGFIELD_TOP_FIRST;
    else if (opts->field_dominance == 1)
        mpi->fields &= ~MP_IMGFIELD_TOP_FIRST;

    double pts = mpi->pts;

    double prevpts = d_video->codec_reordered_pts;
    d_video->prev_codec_reordered_pts = prevpts;
    d_video->codec_reordered_pts = pts;
    if (prevpts != MP_NOPTS_VALUE && pts <= prevpts
        || pts == MP_NOPTS_VALUE)
        d_video->num_reordered_pts_problems++;
    prevpts = d_video->sorted_pts;
    if (sort_pts) {
        if (d_video->num_buffered_pts) {
            d_video->num_buffered_pts--;
            d_video->sorted_pts =
                d_video->buffered_pts[d_video->num_buffered_pts];
        } else {
            mp_msg(MSGT_CPLAYER, MSGL_ERR,
                   "No pts value from demuxer to use for frame!\n");
            d_video->sorted_pts = MP_NOPTS_VALUE;
        }
    }
    pts = d_video->sorted_pts;
    if (prevpts != MP_NOPTS_VALUE && pts <= prevpts
        || pts == MP_NOPTS_VALUE)
        d_video->num_sorted_pts_problems++;
    determine_frame_pts(d_video);
    mpi->pts = d_video->pts;
    return mpi;
}

int mpcodecs_reconfig_vo(struct dec_video *d_video,
                         const struct mp_image_params *params)
{
    struct MPOpts *opts = d_video->opts;
    vf_instance_t *vf = d_video->vfilter;
    int vocfg_flags = 0;
    struct mp_image_params p = *params;
    struct sh_video *sh = d_video->header->video;

    d_video->vf_reconfig_count++;

    mp_msg(MSGT_DECVIDEO, MSGL_V,
           "VIDEO:  %dx%d  %5.3f fps  %5.1f kbps (%4.1f kB/s)\n",
           p.w, p.h, sh->fps, sh->i_bps * 0.008,
           sh->i_bps / 1000.0);

    mp_msg(MSGT_DECVIDEO, MSGL_V, "VDec: vo config request - %d x %d (%s)\n",
           p.w, p.h, vo_format_name(p.imgfmt));

    // check if libvo and codec has common outfmt (no conversion):
    int flags = 0;
    for (;;) {
        mp_msg(MSGT_VFILTER, MSGL_V, "Trying filter chain:\n");
        vf_print_filter_chain(MSGL_V, vf);

        flags = vf->query_format(vf, p.imgfmt);
        mp_msg(MSGT_CPLAYER, MSGL_DBG2, "vo_debug: query(%s) returned 0x%X \n",
               vo_format_name(p.imgfmt), flags);
        if ((flags & VFCAP_CSP_SUPPORTED_BY_HW)
            || (flags & VFCAP_CSP_SUPPORTED))
        {
            break;
        }
        // TODO: no match - we should use conversion...
        if (strcmp(vf->info->name, "scale")) {
            mp_tmsg(MSGT_DECVIDEO, MSGL_INFO, "Could not find matching colorspace - retrying with -vf scale...\n");
            vf = vf_open_filter(opts, vf, "scale", NULL);
            continue;
        }
        mp_tmsg(MSGT_CPLAYER, MSGL_WARN,
            "The selected video_out device is incompatible with this codec.\n"\
            "Try appending the scale filter to your filter list,\n"\
            "e.g. -vf filter,scale instead of -vf filter.\n");
        mp_tmsg(MSGT_VFILTER, MSGL_WARN, "Attempted filter chain:\n");
        vf_print_filter_chain(MSGL_WARN, vf);
        d_video->vf_initialized = -1;
        return -1;               // failed
    }
    d_video->vfilter = vf;

    // autodetect flipping
    bool flip = opts->flip;
    if (flip && !(flags & VFCAP_FLIP)) {
        // we need to flip, but no flipping filter avail.
        vf_add_before_vo(&vf, "flip", NULL);
        d_video->vfilter = vf;
        flip = false;
    }

    float decoder_aspect = p.d_w / (float)p.d_h;
    if (d_video->initial_decoder_aspect == 0)
        d_video->initial_decoder_aspect = decoder_aspect;

    // We normally prefer the container aspect, unless the decoder aspect
    // changes at least once.
    if (d_video->initial_decoder_aspect == decoder_aspect) {
        if (sh->aspect > 0)
            vf_set_dar(&p.d_w, &p.d_h, p.w, p.h, sh->aspect);
    } else {
        // Even if the aspect switches back, don't use container aspect again.
        d_video->initial_decoder_aspect = -1;
    }

    float force_aspect = opts->movie_aspect;
    if (force_aspect > -1.0 && d_video->stream_aspect != 0.0)
        force_aspect = d_video->stream_aspect;

    if (force_aspect > 0)
        vf_set_dar(&p.d_w, &p.d_h, p.w, p.h, force_aspect);

    if (abs(p.d_w - p.w) >= 4 || abs(p.d_h - p.h) >= 4) {
        mp_tmsg(MSGT_CPLAYER, MSGL_V, "Aspect ratio is %.2f:1 - "
                "scaling to correct movie aspect.\n", sh->aspect);
        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_ASPECT=%1.4f\n", sh->aspect);
    } else {
        p.d_w = p.w;
        p.d_h = p.h;
    }

    // Apply user overrides
    if (opts->requested_colorspace != MP_CSP_AUTO)
        p.colorspace = opts->requested_colorspace;
    if (opts->requested_input_range != MP_CSP_LEVELS_AUTO)
        p.colorlevels = opts->requested_input_range;
    p.outputlevels = opts->requested_output_range;

    // Detect colorspace from resolution.
    // Make sure the user-overrides are consistent (no RGB csp for YUV, etc.).
    mp_image_params_guess_csp(&p);

    vocfg_flags = (flip ? VOFLAG_FLIPPING : 0);

    // Time to config libvo!
    mp_msg(MSGT_CPLAYER, MSGL_V, "VO Config (%dx%d->%dx%d,flags=%d,0x%X)\n",
           p.w, p.h, p.d_w, p.d_h, vocfg_flags, p.imgfmt);

    if (vf_reconfig_wrapper(vf, &p, vocfg_flags) < 0) {
        mp_tmsg(MSGT_CPLAYER, MSGL_WARN, "FATAL: Cannot initialize video driver.\n");
        d_video->vf_initialized = -1;
        return -1;
    }

    mp_tmsg(MSGT_VFILTER, MSGL_V, "Video filter chain:\n");
    vf_print_filter_chain(MSGL_V, vf);

    d_video->vf_initialized = 1;

    d_video->vf_input = p;

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

    return 0;
}
