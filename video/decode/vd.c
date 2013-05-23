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

    if (get_video_quality_max(sh) <= 0 && opts->divx_quality) {
        // user wants postprocess but no pp filter yet:
        sh->vfilter = vf = vf_open_filter(opts, vf, "pp", NULL);
    }

    // check if libvo and codec has common outfmt (no conversion):
    for (;;) {
        mp_msg(MSGT_VFILTER, MSGL_V, "Trying filter chain:\n");
        vf_print_filter_chain(MSGL_V, vf);

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
            "e.g. -vf filter,scale instead of -vf filter.\n");
        mp_tmsg(MSGT_VFILTER, MSGL_WARN, "Attempted filter chain:\n");
        vf_print_filter_chain(MSGL_WARN, vf);
        sh->vf_initialized = -1;
        return 0;               // failed
    }
    sh->outfmt = out_fmt;
    mp_msg(MSGT_CPLAYER, MSGL_V, "VDec: using %s as output csp\n",
           vo_format_name(out_fmt));
    sh->vfilter = vf;

    // autodetect flipping
    bool flip = opts->flip;
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

    int d_w = sh->disp_w;
    int d_h = sh->disp_h;

    if (sh->aspect > 0.01) {
        int new_w = d_h * sh->aspect;
        int new_h = d_h;
        // we don't like horizontal downscale
        if (new_w < d_w) {
            new_w = d_w;
            new_h = d_w / sh->aspect;
        }
        if (abs(d_w - new_w) >= 4 || abs(d_h - new_h) >= 4) {
            d_w = new_w;
            d_h = new_h;
            mp_tmsg(MSGT_CPLAYER, MSGL_V, "Aspect ratio is %.2f:1 - "
                    "scaling to correct movie aspect.\n", sh->aspect);
        }

        mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_VIDEO_ASPECT=%1.4f\n", sh->aspect);
    }

    vocfg_flags = (opts->fullscreen ? VOFLAG_FULLSCREEN : 0) |
                  (flip ? VOFLAG_FLIPPING : 0);

    // Time to config libvo!
    mp_msg(MSGT_CPLAYER, MSGL_V,
           "VO Config (%dx%d->%dx%d,flags=%d,0x%X)\n", sh->disp_w,
           sh->disp_h, d_w, d_h, vocfg_flags, out_fmt);

    if (vf_config_wrapper(vf, sh->disp_w, sh->disp_h, d_w, d_h, vocfg_flags,
                          out_fmt) == 0) {
        mp_tmsg(MSGT_CPLAYER, MSGL_WARN, "FATAL: Cannot initialize video driver.\n");
        sh->vf_initialized = -1;
        return 0;
    }

    mp_tmsg(MSGT_VFILTER, MSGL_V, "Video filter chain:\n");
    vf_print_filter_chain(MSGL_V, vf);

    sh->vf_initialized = 1;

    set_video_colorspace(sh);

    if (opts->gamma_gamma != 1000)
        set_video_colors(sh, "gamma", opts->gamma_gamma);
    if (opts->gamma_brightness != 1000)
        set_video_colors(sh, "brightness", opts->gamma_brightness);
    if (opts->gamma_contrast != 1000)
        set_video_colors(sh, "contrast", opts->gamma_contrast);
    if (opts->gamma_saturation != 1000)
        set_video_colors(sh, "saturation", opts->gamma_saturation);
    if (opts->gamma_hue != 1000)
        set_video_colors(sh, "hue", opts->gamma_hue);

    return 1;
}
