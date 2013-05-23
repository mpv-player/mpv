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
#include "core/options.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include "demux/codec_tags.h"

#include "core/mp_msg.h"

#include "osdep/timer.h"
#include "osdep/shmem.h"

#include "stream/stream.h"
#include "demux/demux.h"

#include "core/codecs.h"

#include "video/out/vo.h"
#include "video/csputils.h"

#include "demux/stheader.h"
#include "video/decode/vd.h"
#include "video/filter/vf.h"

#include "video/decode/dec_video.h"


int get_video_quality_max(sh_video_t *sh_video)
{
    vf_instance_t *vf = sh_video->vfilter;
    if (vf) {
        int ret = vf->control(vf, VFCTRL_QUERY_MAX_PP_LEVEL, NULL);
        if (ret > 0) {
            mp_tmsg(MSGT_DECVIDEO, MSGL_INFO, "[PP] Using external postprocessing filter, max q = %d.\n", ret);
            return ret;
        }
    }
    return 0;
}

int set_video_colors(sh_video_t *sh_video, const char *item, int value)
{
    vf_instance_t *vf = sh_video->vfilter;
    vf_equalizer_t data;

    data.item = item;
    data.value = value;

    mp_dbg(MSGT_DECVIDEO, MSGL_V, "set video colors %s=%d \n", item, value);
    if (vf) {
        int ret = vf->control(vf, VFCTRL_SET_EQUALIZER, &data);
        if (ret == CONTROL_TRUE)
            return 1;
    }
    mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Video attribute '%s' is not supported by selected vo.\n",
           item);
    return 0;
}

int get_video_colors(sh_video_t *sh_video, const char *item, int *value)
{
    vf_instance_t *vf = sh_video->vfilter;
    vf_equalizer_t data;

    data.item = item;

    mp_dbg(MSGT_DECVIDEO, MSGL_V, "get video colors %s \n", item);
    if (vf) {
        int ret = vf->control(vf, VFCTRL_GET_EQUALIZER, &data);
        if (ret == CONTROL_TRUE) {
            *value = data.value;
            return 1;
        }
    }
    return 0;
}

void get_detected_video_colorspace(struct sh_video *sh, struct mp_csp_details *csp)
{
    struct MPOpts *opts = sh->opts;

    csp->format = opts->requested_colorspace;
    csp->levels_in = opts->requested_input_range;
    csp->levels_out = opts->requested_output_range;

    if (csp->format == MP_CSP_AUTO)
        csp->format = sh->colorspace;
    if (csp->format == MP_CSP_AUTO)
        csp->format = mp_csp_guess_colorspace(sh->disp_w, sh->disp_h);

    if (csp->levels_in == MP_CSP_LEVELS_AUTO)
        csp->levels_in = sh->color_range;
    if (csp->levels_in == MP_CSP_LEVELS_AUTO)
        csp->levels_in = MP_CSP_LEVELS_TV;

    if (csp->levels_out == MP_CSP_LEVELS_AUTO)
        csp->levels_out = MP_CSP_LEVELS_PC;
}

void set_video_colorspace(struct sh_video *sh)
{
    struct vf_instance *vf = sh->vfilter;

    struct mp_csp_details requested;
    get_detected_video_colorspace(sh, &requested);
    vf->control(vf, VFCTRL_SET_YUV_COLORSPACE, &requested);

    struct mp_csp_details actual = MP_CSP_DETAILS_DEFAULTS;
    vf->control(vf, VFCTRL_GET_YUV_COLORSPACE, &actual);

    int success = actual.format == requested.format
               && actual.levels_in == requested.levels_in
               && actual.levels_out == requested.levels_out;

    if (!success)
        mp_tmsg(MSGT_DECVIDEO, MSGL_WARN,
                "Colorspace details not fully supported by selected vo.\n");

    if (actual.format != requested.format
            && requested.format == MP_CSP_SMPTE_240M) {
        // BT.709 is pretty close, much better than BT.601
        requested.format = MP_CSP_BT_709;
        vf->control(vf, VFCTRL_SET_YUV_COLORSPACE, &requested);
    }

}

void resync_video_stream(sh_video_t *sh_video)
{
    const struct vd_functions *vd = sh_video->vd_driver;
    if (vd)
        vd->control(sh_video, VDCTRL_RESYNC_STREAM, NULL);
    sh_video->prev_codec_reordered_pts = MP_NOPTS_VALUE;
    sh_video->prev_sorted_pts = MP_NOPTS_VALUE;
}

void video_reinit_vo(struct sh_video *sh_video)
{
    sh_video->vd_driver->control(sh_video, VDCTRL_REINIT_VO, NULL);
}

int get_current_video_decoder_lag(sh_video_t *sh_video)
{
    const struct vd_functions *vd = sh_video->vd_driver;
    if (!vd)
        return -1;
    int ret = -1;
    vd->control(sh_video, VDCTRL_QUERY_UNSEEN_FRAMES, &ret);
    return ret;
}

void uninit_video(sh_video_t *sh_video)
{
    if (!sh_video->initialized)
        return;
    mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Uninit video.\n");
    sh_video->vd_driver->uninit(sh_video);
    vf_uninit_filter_chain(sh_video->vfilter);
    sh_video->vfilter = NULL;
    talloc_free(sh_video->gsh->decoder_desc);
    sh_video->gsh->decoder_desc = NULL;
    sh_video->initialized = 0;
}

static int init_video_codec(sh_video_t *sh_video, const char *decoder)
{
    assert(!sh_video->initialized);

    if (!sh_video->vd_driver->init(sh_video, decoder)) {
        mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Video decoder init failed.\n");
        //uninit_video(sh_video);
        return 0;
    }

    sh_video->initialized = 1;
    sh_video->prev_codec_reordered_pts = MP_NOPTS_VALUE;
    sh_video->prev_sorted_pts = MP_NOPTS_VALUE;
    return 1;
}

struct mp_decoder_list *mp_video_decoder_list(void)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);
    for (int i = 0; mpcodecs_vd_drivers[i] != NULL; i++)
        mpcodecs_vd_drivers[i]->add_decoders(list);
    return list;
}

static struct mp_decoder_list *mp_select_video_decoders(const char *codec,
                                                        char *selection)
{
    struct mp_decoder_list *list = mp_video_decoder_list();
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

int init_best_video_codec(sh_video_t *sh_video, char* video_decoders)
{
    assert(!sh_video->initialized);

    struct mp_decoder_entry *decoder = NULL;
    struct mp_decoder_list *list =
        mp_select_video_decoders(sh_video->gsh->codec, video_decoders);

    mp_print_decoders(MSGT_DECVIDEO, MSGL_V, "Codec list:", list);

    for (int n = 0; n < list->num_entries; n++) {
        struct mp_decoder_entry *sel = &list->entries[n];
        const struct vd_functions *driver = find_driver(sel->family);
        if (!driver)
            continue;
        mp_tmsg(MSGT_DECVIDEO, MSGL_V, "Opening video decoder %s:%s\n",
                sel->family, sel->decoder);
        sh_video->vd_driver = driver;
        if (init_video_codec(sh_video, sel->decoder)) {
            decoder = sel;
            break;
        }
        sh_video->vd_driver = NULL;
        mp_tmsg(MSGT_DECVIDEO, MSGL_WARN, "Video decoder init failed for "
                "%s:%s\n", sel->family, sel->decoder);
    }

    if (sh_video->initialized) {
        sh_video->gsh->decoder_desc =
            talloc_asprintf(NULL, "%s [%s:%s]", decoder->desc, decoder->family,
                            decoder->decoder);
        mp_msg(MSGT_DECVIDEO, MSGL_INFO, "Selected video codec: %s\n",
               sh_video->gsh->decoder_desc);
    } else {
        mp_msg(MSGT_DECVIDEO, MSGL_ERR,
               "Failed to initialize a video decoder for codec '%s'.\n",
               sh_video->gsh->codec ? sh_video->gsh->codec : "<unknown>");
    }

    talloc_free(list);
    return sh_video->initialized;
}

void *decode_video(sh_video_t *sh_video, struct demux_packet *packet,
                   int drop_frame, double pts)
{
    mp_image_t *mpi = NULL;
    struct MPOpts *opts = sh_video->opts;

    if (opts->correct_pts && pts != MP_NOPTS_VALUE) {
        int delay = get_current_video_decoder_lag(sh_video);
        if (delay >= 0) {
            if (delay > sh_video->num_buffered_pts)
#if 0
                // this is disabled because vd_ffmpeg reports the same lag
                // after seek even when there are no buffered frames,
                // leading to incorrect error messages
                mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Not enough buffered pts\n");
#else
                ;
#endif
            else
                sh_video->num_buffered_pts = delay;
        }
        if (sh_video->num_buffered_pts ==
            sizeof(sh_video->buffered_pts) / sizeof(double))
            mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Too many buffered pts\n");
        else {
            int i, j;
            for (i = 0; i < sh_video->num_buffered_pts; i++)
                if (sh_video->buffered_pts[i] < pts)
                    break;
            for (j = sh_video->num_buffered_pts; j > i; j--)
                sh_video->buffered_pts[j] = sh_video->buffered_pts[j - 1];
            sh_video->buffered_pts[i] = pts;
            sh_video->num_buffered_pts++;
        }
    }

    mpi = sh_video->vd_driver->decode(sh_video, packet, drop_frame, &pts);

    //------------------------ frame decoded. --------------------

    if (!mpi || drop_frame) {
        talloc_free(mpi);
        return NULL;            // error / skipped frame
    }

    if (opts->field_dominance == 0)
        mpi->fields |= MP_IMGFIELD_TOP_FIRST;
    else if (opts->field_dominance == 1)
        mpi->fields &= ~MP_IMGFIELD_TOP_FIRST;

    double prevpts = sh_video->codec_reordered_pts;
    sh_video->prev_codec_reordered_pts = prevpts;
    sh_video->codec_reordered_pts = pts;
    if (prevpts != MP_NOPTS_VALUE && pts <= prevpts
        || pts == MP_NOPTS_VALUE)
        sh_video->num_reordered_pts_problems++;
    prevpts = sh_video->sorted_pts;
    if (opts->correct_pts) {
        if (sh_video->num_buffered_pts) {
            sh_video->num_buffered_pts--;
            sh_video->sorted_pts =
                sh_video->buffered_pts[sh_video->num_buffered_pts];
        } else {
            mp_msg(MSGT_CPLAYER, MSGL_ERR,
                   "No pts value from demuxer to use for frame!\n");
            sh_video->sorted_pts = MP_NOPTS_VALUE;
        }
    }
    pts = sh_video->sorted_pts;
    if (prevpts != MP_NOPTS_VALUE && pts <= prevpts
        || pts == MP_NOPTS_VALUE)
        sh_video->num_sorted_pts_problems++;
    return mpi;
}
