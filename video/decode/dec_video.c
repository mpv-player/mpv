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

void video_reset_decoding(struct dec_video *d_video)
{
    video_vd_control(d_video, VDCTRL_RESET, NULL);
    if (d_video->vfilter && d_video->vfilter->initialized == 1)
        vf_seek_reset(d_video->vfilter);
    mp_image_unrefp(&d_video->waiting_decoded_mpi);
    d_video->num_buffered_pts = 0;
    d_video->last_pts = MP_NOPTS_VALUE;
    d_video->last_packet_pdts = MP_NOPTS_VALUE;
    d_video->decoded_pts = MP_NOPTS_VALUE;
    d_video->codec_pts = MP_NOPTS_VALUE;
    d_video->codec_dts = MP_NOPTS_VALUE;
    d_video->sorted_pts = MP_NOPTS_VALUE;
    d_video->unsorted_pts = MP_NOPTS_VALUE;
}

int video_vd_control(struct dec_video *d_video, int cmd, void *arg)
{
    const struct vd_functions *vd = d_video->vd_driver;
    if (vd)
        return vd->control(d_video, cmd, arg);
    return CONTROL_UNKNOWN;
}

int video_set_colors(struct dec_video *d_video, const char *item, int value)
{
    vf_equalizer_t data;

    data.item = item;
    data.value = value;

    mp_dbg(MSGT_DECVIDEO, MSGL_V, "set video colors %s=%d \n", item, value);
    if (d_video->vfilter) {
        int ret = video_vf_vo_control(d_video, VFCTRL_SET_EQUALIZER, &data);
        if (ret == CONTROL_TRUE)
            return 1;
    }
    mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Video attribute '%s' is not supported by selected vo.\n",
           item);
    return 0;
}

int video_get_colors(struct dec_video *d_video, const char *item, int *value)
{
    vf_equalizer_t data;

    data.item = item;

    mp_dbg(MSGT_DECVIDEO, MSGL_V, "get video colors %s \n", item);
    if (d_video->vfilter) {
        int ret = video_vf_vo_control(d_video, VFCTRL_GET_EQUALIZER, &data);
        if (ret == CONTROL_TRUE) {
            *value = data.value;
            return 1;
        }
    }
    return 0;
}

void video_uninit(struct dec_video *d_video)
{
    mp_image_unrefp(&d_video->waiting_decoded_mpi);
    if (d_video->vd_driver) {
        mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Uninit video.\n");
        d_video->vd_driver->uninit(d_video);
    }
    talloc_free(d_video->priv);
    vf_destroy(d_video->vfilter);
    talloc_free(d_video);
}

static int init_video_codec(struct dec_video *d_video, const char *decoder)
{
    if (!d_video->vd_driver->init(d_video, decoder)) {
        mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Video decoder init failed.\n");
        return 0;
    }
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
    video_reset_decoding(d_video);
    d_video->has_broken_packet_pts = -10; // needs 10 packets to reach decision

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

static void add_pts_to_sort(struct dec_video *d_video, double pts)
{
    if (pts != MP_NOPTS_VALUE) {
        int delay = -1;
        video_vd_control(d_video, VDCTRL_QUERY_UNSEEN_FRAMES, &delay);
        if (delay >= 0 && delay < d_video->num_buffered_pts)
            d_video->num_buffered_pts = delay;
        if (d_video->num_buffered_pts ==
            sizeof(d_video->buffered_pts) / sizeof(double))
            mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Too many buffered pts\n");
        else {
            int i, j;
            for (i = 0; i < d_video->num_buffered_pts; i++)
                if (d_video->buffered_pts[i] < pts)
                    break;
            for (j = d_video->num_buffered_pts; j > i; j--)
                d_video->buffered_pts[j] = d_video->buffered_pts[j - 1];
            d_video->buffered_pts[i] = pts;
            d_video->num_buffered_pts++;
        }
    }
}

// Return true if pts1 comes before pts2. pts1 can be MP_NOPTS_VALUE, but pts2
// always has to be valid. pts1 can't be equal or larger than pts2.
#define PTS_IS_ORDERED(pts1, pts2) \
    ((pts2) != MP_NOPTS_VALUE && ((pts1) == MP_NOPTS_VALUE || ((pts1) < (pts2))))

static double retrieve_sorted_pts(struct dec_video *d_video, double codec_pts)
{
    struct MPOpts *opts = d_video->opts;

    double sorted_pts;
    if (d_video->num_buffered_pts) {
        d_video->num_buffered_pts--;
        sorted_pts = d_video->buffered_pts[d_video->num_buffered_pts];
    } else {
        mp_msg(MSGT_CPLAYER, MSGL_ERR,
                "No pts value from demuxer to use for frame!\n");
        sorted_pts = MP_NOPTS_VALUE;
    }

    if (!PTS_IS_ORDERED(d_video->sorted_pts, sorted_pts))
        d_video->num_sorted_pts_problems++;
    d_video->sorted_pts = sorted_pts;

    if (!PTS_IS_ORDERED(d_video->unsorted_pts, codec_pts))
        d_video->num_unsorted_pts_problems++;
    d_video->unsorted_pts = codec_pts;

    if (d_video->header->video->avi_dts) {
        // Actually, they don't need to be sorted, we just reuse the buffering.
        d_video->pts_assoc_mode = 2;
    } else if (opts->user_pts_assoc_mode) {
        d_video->pts_assoc_mode = opts->user_pts_assoc_mode;
    } else if (d_video->pts_assoc_mode == 0) {
        if (codec_pts != MP_NOPTS_VALUE)
            d_video->pts_assoc_mode = 1;
        else
            d_video->pts_assoc_mode = 2;
    } else {
        int probcount1 = d_video->num_unsorted_pts_problems;
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
    return d_video->pts_assoc_mode == 1 ? codec_pts : sorted_pts;
}

struct mp_image *video_decode(struct dec_video *d_video,
                              struct demux_packet *packet,
                              int drop_frame)
{
    struct MPOpts *opts = d_video->opts;
    bool sort_pts =
        (opts->user_pts_assoc_mode != 1 || d_video->header->video->avi_dts)
        && opts->correct_pts;
    double pkt_pts = packet ? packet->pts : MP_NOPTS_VALUE;
    double pkt_dts = packet ? packet->dts : MP_NOPTS_VALUE;

    double pkt_pdts = pkt_pts == MP_NOPTS_VALUE ? pkt_dts : pkt_pts;
    if (pkt_pdts != MP_NOPTS_VALUE)
        d_video->last_packet_pdts = pkt_pdts;

    if (sort_pts)
        add_pts_to_sort(d_video, pkt_pdts);

    double prev_codec_pts = d_video->codec_pts;
    double prev_codec_dts = d_video->codec_dts;

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

    // Note: the PTS is reordered, but the DTS is not. Both should be monotonic.
    double pts = d_video->codec_pts;
    double dts = d_video->codec_dts;

    if (pts == MP_NOPTS_VALUE) {
        d_video->codec_pts = prev_codec_pts;
    } else if (pts <= prev_codec_pts) {
        d_video->num_codec_pts_problems++;
    }

    if (dts == MP_NOPTS_VALUE) {
        d_video->codec_dts = prev_codec_dts;
    } else if (dts <= prev_codec_dts) {
        d_video->num_codec_dts_problems++;
    }

    // If PTS is unset, or non-monotonic, fall back to DTS.
    if ((d_video->num_codec_pts_problems > d_video->num_codec_dts_problems ||
         pts == MP_NOPTS_VALUE) && dts != MP_NOPTS_VALUE)
        pts = dts;

    // Alternative PTS determination methods
    if (sort_pts)
        pts = retrieve_sorted_pts(d_video, pts);

    if (!opts->correct_pts || pts == MP_NOPTS_VALUE) {
        if (opts->correct_pts)
            mp_msg(MSGT_DECVIDEO, MSGL_WARN, "No video PTS! Making something up.\n");

        double frame_time = 1.0f / (d_video->fps > 0 ? d_video->fps : 25);
        double base = d_video->last_packet_pdts;
        pts = d_video->decoded_pts;
        if (pts == MP_NOPTS_VALUE)
            pts = base == MP_NOPTS_VALUE ? 0 : base;

        pts += frame_time;
    }

    if (d_video->decoded_pts != MP_NOPTS_VALUE && pts <= d_video->decoded_pts) {
        mp_msg(MSGT_DECVIDEO, MSGL_WARN, "Non-monotonic video pts: %f <= %f\n",
               pts, d_video->decoded_pts);
        pts = d_video->decoded_pts;
    }

    if (d_video->has_broken_packet_pts < 0)
        d_video->has_broken_packet_pts++;
    if (d_video->num_codec_pts_problems || pkt_pts == MP_NOPTS_VALUE)
        d_video->has_broken_packet_pts = 1;

    mpi->pts = pts;
    d_video->decoded_pts = pts;
    return mpi;
}

int video_reconfig_filters(struct dec_video *d_video,
                           const struct mp_image_params *params)
{
    struct MPOpts *opts = d_video->opts;
    struct mp_image_params p = *params;
    struct sh_video *sh = d_video->header->video;

    mp_msg(MSGT_DECVIDEO, MSGL_V,
           "VIDEO:  %dx%d  %5.3f fps  %5.1f kbps (%4.1f kB/s)\n",
           p.w, p.h, sh->fps, sh->i_bps * 0.008,
           sh->i_bps / 1000.0);

    mp_msg(MSGT_DECVIDEO, MSGL_V, "VDec: vo config request - %d x %d (%s)\n",
           p.w, p.h, vo_format_name(p.imgfmt));

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

    // Time to config libvo!
    mp_msg(MSGT_CPLAYER, MSGL_V, "VO Config (%dx%d->%dx%d,0x%X)\n",
           p.w, p.h, p.d_w, p.d_h, p.imgfmt);

    if (vf_reconfig(d_video->vfilter, &p) < 0) {
        mp_tmsg(MSGT_CPLAYER, MSGL_WARN, "FATAL: Cannot initialize video driver.\n");
        return -1;
    }

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

// Send a VCTRL, or if it doesn't work, translate it to a VOCTRL and try the VO.
int video_vf_vo_control(struct dec_video *d_video, int vf_cmd, void *data)
{
    if (d_video->vfilter && d_video->vfilter->initialized > 0) {
        int r = vf_control_any(d_video->vfilter, vf_cmd, data);
        if (r != CONTROL_UNKNOWN)
            return r;
    }

    switch (vf_cmd) {
    case VFCTRL_GET_DEINTERLACE:
        return vo_control(d_video->vo, VOCTRL_GET_DEINTERLACE, data) == VO_TRUE;
    case VFCTRL_SET_DEINTERLACE:
        return vo_control(d_video->vo, VOCTRL_SET_DEINTERLACE, data) == VO_TRUE;
    case VFCTRL_SET_EQUALIZER: {
        vf_equalizer_t *eq = data;
        if (!d_video->vo->config_ok)
            return CONTROL_FALSE;                       // vo not configured?
        struct voctrl_set_equalizer_args param = {
            eq->item, eq->value
        };
        return vo_control(d_video->vo, VOCTRL_SET_EQUALIZER, &param) == VO_TRUE;
    }
    case VFCTRL_GET_EQUALIZER: {
        vf_equalizer_t *eq = data;
        if (!d_video->vo->config_ok)
            return CONTROL_FALSE;                       // vo not configured?
        struct voctrl_get_equalizer_args param = {
            eq->item, &eq->value
        };
        return vo_control(d_video->vo, VOCTRL_GET_EQUALIZER, &param) == VO_TRUE;
    }
    }
    return CONTROL_UNKNOWN;
}
