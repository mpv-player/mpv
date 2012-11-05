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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "config.h"
#include "core/mp_msg.h"
#include "core/options.h"

#include "core/codec-cfg.h"

#include "video/img_format.h"

#include "stream/stream.h"
#include "demux/demux.h"
#include "demux/stheader.h"
#include "dec_video.h"

#include "vd.h"
#include "video/filter/vf.h"
#include "video/out/vo.h"

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

int mpcodecs_config_vo(sh_video_t *sh, int w, int h, unsigned int out_fmt)
{
    struct MPOpts *opts = sh->opts;
    int screen_size_x = 0;
    int screen_size_y = 0;
    vf_instance_t *vf = sh->vfilter;
    int vocfg_flags = 0;

    if (w)
        sh->disp_w = w;
    if (h)
        sh->disp_h = h;

    mp_msg(MSGT_DECVIDEO, MSGL_V,
           "VIDEO:  %dx%d  %5.3f fps  %5.1f kbps (%4.1f kB/s)\n",
           sh->disp_w, sh->disp_h, sh->fps, sh->i_bps * 0.008,
           sh->i_bps / 1000.0);

    if (!sh->disp_w || !sh->disp_h)
        return 0;

    mp_msg(MSGT_DECVIDEO, MSGL_V, "VDec: vo config request - %d x %d (%s)\n",
           w, h, vo_format_name(out_fmt));

    if (get_video_quality_max(sh) <= 0 && divx_quality) {
        // user wants postprocess but no pp filter yet:
        sh->vfilter = vf = vf_open_filter(opts, vf, "pp", NULL);
    }

    // check if libvo and codec has common outfmt (no conversion):
    for (;;) {
        if (mp_msg_test(MSGT_DECVIDEO, MSGL_V)) {
            mp_msg(MSGT_DECVIDEO, MSGL_V, "Trying filter chain:");
            for (vf_instance_t *f = vf; f; f = f->next)
                mp_msg(MSGT_DECVIDEO, MSGL_V, " %s", f->info->name);
            mp_msg(MSGT_DECVIDEO, MSGL_V, "\n");
        }

        int flags = vf->query_format(vf, out_fmt);
        mp_msg(MSGT_CPLAYER, MSGL_DBG2, "vo_debug: query(%s) returned 0x%X \n",
               vo_format_name(out_fmt), flags);
        if ((flags & VFCAP_CSP_SUPPORTED_BY_HW)
            || (flags & VFCAP_CSP_SUPPORTED))
        {
            sh->output_flags = flags;
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
            "e.g. -vf spp,scale instead of -vf spp.\n");
        sh->vf_initialized = -1;
        return 0;               // failed
    }
    sh->outfmt = out_fmt;
    mp_msg(MSGT_CPLAYER, MSGL_V, "VDec: using %s as output csp\n",
           vo_format_name(out_fmt));
    sh->vfilter = vf;

    // autodetect flipping
    bool flip = !!opts->flip != !!(sh->codec->flags & CODECS_FLAG_FLIP);
    if (flip && !(sh->output_flags & VFCAP_FLIP)) {
        // we need to flip, but no flipping filter avail.
        vf_add_before_vo(&vf, "flip", NULL);
        sh->vfilter = vf;
        flip = false;
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
            mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_ASPECT=%1.4f\n",
                   sh->aspect);
            int w = screen_size_y * sh->aspect;
            int h = screen_size_y;
            // we don't like horizontal downscale || user forced width:
            if (w < screen_size_x || opts->screen_size_xy > 8) {
                w = screen_size_x;
                h = screen_size_x / sh->aspect;
            }
            if (abs(screen_size_x - w) >= 4 || abs(screen_size_y - h) >= 4) {
                screen_size_x = w;
                screen_size_y = h;
                mp_tmsg(MSGT_CPLAYER, MSGL_V, "Aspect ratio is %.2f:1 - "
                        "scaling to correct movie aspect.\n", sh->aspect);
            }
        } else {
            mp_tmsg(MSGT_CPLAYER, MSGL_V, "Movie-Aspect is undefined - no prescaling applied.\n");
        }
    }

    vocfg_flags = (opts->fullscreen ? VOFLAG_FULLSCREEN : 0)
        | (opts->vidmode ? VOFLAG_MODESWITCHING : 0)
        | (flip ? VOFLAG_FLIPPING : 0);

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
    return vf_get_image(sh->vfilter, sh->outfmt, mp_imgtype, mp_imgflag, w, h);
}
