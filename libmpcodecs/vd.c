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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "options.h"

#include "codec-cfg.h"

#include "img_format.h"

#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "libmpdemux/stheader.h"
#include "dec_video.h"

#include "vd.h"
#include "vf.h"
#include "libvo/video_out.h"

extern const vd_functions_t mpcodecs_vd_null;
extern const vd_functions_t mpcodecs_vd_ffmpeg;
extern const vd_functions_t mpcodecs_vd_theora;
extern const vd_functions_t mpcodecs_vd_dshow;
extern const vd_functions_t mpcodecs_vd_dmo;
extern const vd_functions_t mpcodecs_vd_vfw;
extern const vd_functions_t mpcodecs_vd_vfwex;
extern const vd_functions_t mpcodecs_vd_raw;
extern const vd_functions_t mpcodecs_vd_hmblck;
extern const vd_functions_t mpcodecs_vd_xanim;
extern const vd_functions_t mpcodecs_vd_mpng;
extern const vd_functions_t mpcodecs_vd_ijpg;
extern const vd_functions_t mpcodecs_vd_mtga;
extern const vd_functions_t mpcodecs_vd_sgi;
extern const vd_functions_t mpcodecs_vd_mpegpes;
extern const vd_functions_t mpcodecs_vd_realvid;
extern const vd_functions_t mpcodecs_vd_xvid;
extern const vd_functions_t mpcodecs_vd_libdv;
extern const vd_functions_t mpcodecs_vd_lzo;
extern const vd_functions_t mpcodecs_vd_qtvideo;

/* Please do not add any new decoders here. If you want to implement a new
 * decoder, add it to libavcodec, except for wrappers around external
 * libraries and decoders requiring binary support. */

const vd_functions_t * const mpcodecs_vd_drivers[] = {
    &mpcodecs_vd_null,
#ifdef CONFIG_FFMPEG
    &mpcodecs_vd_ffmpeg,
#endif
#ifdef CONFIG_OGGTHEORA
    &mpcodecs_vd_theora,
#endif
#ifdef CONFIG_WIN32DLL
    &mpcodecs_vd_dshow,
    &mpcodecs_vd_dmo,
    &mpcodecs_vd_vfw,
    &mpcodecs_vd_vfwex,
#endif
    &mpcodecs_vd_lzo,
    &mpcodecs_vd_raw,
    &mpcodecs_vd_hmblck,
#ifdef CONFIG_XANIM
    &mpcodecs_vd_xanim,
#endif
#ifdef CONFIG_PNG
    &mpcodecs_vd_mpng,
#endif
#ifdef CONFIG_JPEG
    &mpcodecs_vd_ijpg,
#endif
    &mpcodecs_vd_mtga,
    &mpcodecs_vd_sgi,
    &mpcodecs_vd_mpegpes,
#ifdef CONFIG_REALCODECS
    &mpcodecs_vd_realvid,
#endif
#ifdef CONFIG_XVID4
    &mpcodecs_vd_xvid,
#endif
#ifdef CONFIG_LIBDV095
    &mpcodecs_vd_libdv,
#endif
#ifdef CONFIG_QTX_CODECS
    &mpcodecs_vd_qtvideo,
#endif
    /* Please do not add any new decoders here. If you want to implement a new
     * decoder, add it to libavcodec, except for wrappers around external
     * libraries and decoders requiring binary support. */
    NULL
};

int mpcodecs_config_vo2(sh_video_t *sh, int w, int h,
                        const unsigned int *outfmts,
                        unsigned int preferred_outfmt)
{
    struct MPOpts *opts = sh->opts;
    int j;
    unsigned int out_fmt = 0;
    int screen_size_x = 0;
    int screen_size_y = 0;
    vf_instance_t *vf = sh->vfilter, *sc = NULL;
    int palette = 0;
    int vocfg_flags = 0;

    if (w)
        sh->disp_w = w;
    if (h)
        sh->disp_h = h;

    if (!sh->disp_w || !sh->disp_h)
        return 0;

    mp_msg(MSGT_DECVIDEO, MSGL_V,
           "VDec: vo config request - %d x %d (preferred colorspace: %s)\n",
           w, h, vo_format_name(preferred_outfmt));

    if (get_video_quality_max(sh) <= 0 && divx_quality) {
        // user wants postprocess but no pp filter yet:
        sh->vfilter = vf = vf_open_filter(opts, vf, "pp", NULL);
    }

    if (!outfmts || sh->codec->outfmt[0] != 0xffffffff)
        outfmts = sh->codec->outfmt;

    // check if libvo and codec has common outfmt (no conversion):
  csp_again:

    if (mp_msg_test(MSGT_DECVIDEO, MSGL_V)) {
        vf_instance_t *f = vf;
        mp_msg(MSGT_DECVIDEO, MSGL_V, "Trying filter chain:");
        for (f = vf; f; f = f->next)
            mp_msg(MSGT_DECVIDEO, MSGL_V, " %s", f->info->name);
        mp_msg(MSGT_DECVIDEO, MSGL_V, "\n");
    }

    j = -1;
    for (int i = 0; i < CODECS_MAX_OUTFMT; i++) {
        int flags;
        out_fmt = outfmts[i];
        if (out_fmt == (unsigned int) 0xFFFFFFFF)
            break;
        flags = vf->query_format(vf, out_fmt);
        mp_msg(MSGT_CPLAYER, MSGL_DBG2,
               "vo_debug: query(%s) returned 0x%X (i=%d) \n",
               vo_format_name(out_fmt), flags, i);
        if ((flags & VFCAP_CSP_SUPPORTED_BY_HW)
            || (flags & VFCAP_CSP_SUPPORTED && j < 0)) {
            // check (query) if codec really support this outfmt...
            sh->outfmtidx = j; // pass index to the control() function this way
            if (sh->vd_driver->control(sh, VDCTRL_QUERY_FORMAT, &out_fmt) ==
                CONTROL_FALSE) {
                mp_msg(MSGT_CPLAYER, MSGL_DBG2,
                       "vo_debug: codec query_format(%s) returned FALSE\n",
                       vo_format_name(out_fmt));
                continue;
            }
            j = i;
            sh->output_flags = flags;
            if (flags & VFCAP_CSP_SUPPORTED_BY_HW)
                break;
        } else if (!palette
                   && !(flags &
                        (VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_CSP_SUPPORTED))
                   && (out_fmt == IMGFMT_RGB8 || out_fmt == IMGFMT_BGR8)) {
            sh->outfmtidx = j; // pass index to the control() function this way
            if (sh->vd_driver->control(sh, VDCTRL_QUERY_FORMAT, &out_fmt) !=
                CONTROL_FALSE)
                palette = 1;
        }
    }
    if (j < 0) {
        // TODO: no match - we should use conversion...
        if (strcmp(vf->info->name, "scale") && palette != -1) {
            mp_tmsg(MSGT_DECVIDEO, MSGL_INFO, "Could not find matching colorspace - retrying with -vf scale...\n");
            sc = vf = vf_open_filter(opts, vf, "scale", NULL);
            goto csp_again;
        } else if (palette == 1) {
            mp_msg(MSGT_DECVIDEO, MSGL_V, "vd: Trying -vf palette...\n");
            palette = -1;
            vf = vf_open_filter(opts, vf, "palette", NULL);
            goto csp_again;
        } else {
            // sws failed, if the last filter (vf_vo) support MPEGPES try
            // to append vf_lavc
            vf_instance_t *vo, *vp = NULL, *ve, *vpp = NULL;
            // Remove the scale filter if we added it ourselves
            if (vf == sc) {
                ve = vf;
                vf = vf->next;
                vf_uninit_filter(ve);
            }
            // Find the last filter (vf_vo)
            for (vo = vf; vo->next; vo = vo->next) {
                vpp = vp;
                vp = vo;
            }
            if (vo->query_format(vo, IMGFMT_MPEGPES)
                && (!vp || (vp && strcmp(vp->info->name, "lavc")))) {
                ve = vf_open_filter(opts, vo, "lavc", NULL);
                if (vp)
                    vp->next = ve;
                else
                    vf = ve;
                goto csp_again;
            }
            if (vp && !strcmp(vp->info->name,"lavc")) {
		if (vpp)
                    vpp->next = vo;
		else
                    vf = vo;
		vf_uninit_filter(vp);
            }
        }
        mp_tmsg(MSGT_CPLAYER, MSGL_WARN,
            "The selected video_out device is incompatible with this codec.\n"\
            "Try appending the scale filter to your filter list,\n"\
            "e.g. -vf spp,scale instead of -vf spp.\n");
        sh->vf_initialized = -1;
        return 0;               // failed
    }
    out_fmt = outfmts[j];
    sh->outfmt = out_fmt;
    mp_msg(MSGT_CPLAYER, MSGL_V, "VDec: using %s as output csp (no %d)\n",
           vo_format_name(out_fmt), j);
    sh->outfmtidx = j;
    sh->vfilter = vf;

    // autodetect flipping
    if (opts->flip == -1) {
        opts->flip = 0;
        if (sh->codec->outflags[j] & CODECS_FLAG_FLIP)
            if (!(sh->codec->outflags[j] & CODECS_FLAG_NOFLIP))
                opts->flip = 1;
    }
    if (sh->output_flags & VFCAP_FLIPPED)
        opts->flip ^= 1;
    if (opts->flip && !(sh->output_flags & VFCAP_FLIP)) {
        // we need to flip, but no flipping filter avail.
        vf_add_before_vo(&vf, "flip", NULL);
        sh->vfilter = vf;
    }
    // time to do aspect ratio corrections...

    if (opts->movie_aspect > -1.0)
        sh->aspect = opts->movie_aspect;        // cmdline overrides autodetect
    else if (sh->stream_aspect != 0.0)
        sh->aspect = sh->stream_aspect;

    if (opts->screen_size_x || opts->screen_size_y) {
        screen_size_x = opts->screen_size_x;
        screen_size_y = opts->screen_size_y;
        if (!opts->vidmode) {
            if (!screen_size_x)
                screen_size_x = 1;
            if (!screen_size_y)
                screen_size_y = 1;
            if (screen_size_x <= 8)
                screen_size_x *= sh->disp_w;
            if (screen_size_y <= 8)
                screen_size_y *= sh->disp_h;
        }
    } else {
        // check source format aspect, calculate prescale ::atmos
        screen_size_x = sh->disp_w;
        screen_size_y = sh->disp_h;
        if (opts->screen_size_xy >= 0.001) {
            if (opts->screen_size_xy <= 8) {
                // -xy means x+y scale
                screen_size_x *= opts->screen_size_xy;
                screen_size_y *= opts->screen_size_xy;
            } else {
                // -xy means forced width while keeping correct aspect
                screen_size_x = opts->screen_size_xy;
                screen_size_y = opts->screen_size_xy * sh->disp_h / sh->disp_w;
            }
        }
        if (sh->aspect > 0.01) {
            int w;
            mp_tmsg(MSGT_CPLAYER, MSGL_INFO, "Movie-Aspect is %.2f:1 - prescaling to correct movie aspect.\n",
                   sh->aspect);
            mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_ASPECT=%1.4f\n",
                   sh->aspect);
            w = (int) ((float) screen_size_y * sh->aspect);
            w += w % 2;         // round
            // we don't like horizontal downscale || user forced width:
            if (w < screen_size_x || opts->screen_size_xy > 8) {
                screen_size_y =
                    (int) ((float) screen_size_x * (1.0 / sh->aspect));
                screen_size_y += screen_size_y % 2;     // round
            } else
                screen_size_x = w;      // keep new width
        } else {
            mp_tmsg(MSGT_CPLAYER, MSGL_INFO, "Movie-Aspect is undefined - no prescaling applied.\n");
        }
    }

    vocfg_flags = (opts->fullscreen ? VOFLAG_FULLSCREEN : 0)
        | (opts->vidmode ? VOFLAG_MODESWITCHING : 0)
        | (opts->softzoom ? VOFLAG_SWSCALE : 0)
        | (opts->flip ? VOFLAG_FLIPPING : 0);

    // Time to config libvo!
    mp_msg(MSGT_CPLAYER, MSGL_V,
           "VO Config (%dx%d->%dx%d,flags=%d,0x%X)\n", sh->disp_w,
           sh->disp_h, screen_size_x, screen_size_y, vocfg_flags, out_fmt);

    vf->w = sh->disp_w;
    vf->h = sh->disp_h;

    if (vf_config_wrapper
        (vf, sh->disp_w, sh->disp_h, screen_size_x, screen_size_y, vocfg_flags,
         out_fmt) == 0) {
        mp_tmsg(MSGT_CPLAYER, MSGL_WARN, "FATAL: Cannot initialize video driver.\n");
        sh->vf_initialized = -1;
        return 0;
    }

    sh->vf_initialized = 1;

    set_video_colorspace(sh);

    if (opts->vo_gamma_gamma != 1000)
        set_video_colors(sh, "gamma", opts->vo_gamma_gamma);
    if (opts->vo_gamma_brightness != 1000)
        set_video_colors(sh, "brightness", opts->vo_gamma_brightness);
    if (opts->vo_gamma_contrast != 1000)
        set_video_colors(sh, "contrast", opts->vo_gamma_contrast);
    if (opts->vo_gamma_saturation != 1000)
        set_video_colors(sh, "saturation", opts->vo_gamma_saturation);
    if (opts->vo_gamma_hue != 1000)
        set_video_colors(sh, "hue", opts->vo_gamma_hue);

    return 1;
}

// mp_imgtype: buffering type, see mp_image.h
// mp_imgflag: buffer requirements (read/write, preserve, stride limits), see mp_image.h
// returns NULL or allocated mp_image_t*
// Note: buffer allocation may be moved to mpcodecs_config_vo() later...
mp_image_t *mpcodecs_get_image(sh_video_t *sh, int mp_imgtype, int mp_imgflag,
                               int w, int h)
{
    mp_image_t *mpi =
        vf_get_image(sh->vfilter, sh->outfmt, mp_imgtype, mp_imgflag, w, h);
    if (mpi)
        mpi->x = mpi->y = 0;
    return mpi;
}

void mpcodecs_draw_slice(sh_video_t *sh, unsigned char **src, int *stride,
                         int w, int h, int x, int y)
{
    struct vf_instance *vf = sh->vfilter;

    if (vf->draw_slice)
        vf->draw_slice(vf, src, stride, w, h, x, y);
}
