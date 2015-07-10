/*
 * Copyright (C) 2010 Gordon Schmidt <gordon.schmidt <at> s2000.tu-chemnitz.de>
 *
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

//==includes==//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <libavutil/common.h>

#include "config.h"
#include "common/msg.h"
#include "options/options.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"
#include "options/m_option.h"

#include "vf_lavfi.h"

//==types==//
typedef enum stereo_code {
    ANAGLYPH_RC_GRAY,   //anaglyph red/cyan gray
    ANAGLYPH_RC_HALF,   //anaglyph red/cyan half colored
    ANAGLYPH_RC_COLOR,  //anaglyph red/cyan colored
    ANAGLYPH_RC_DUBOIS, //anaglyph red/cyan dubois
    ANAGLYPH_GM_GRAY,   //anaglyph green/magenta gray
    ANAGLYPH_GM_HALF,   //anaglyph green/magenta half colored
    ANAGLYPH_GM_COLOR,  //anaglyph green/magenta colored
    ANAGLYPH_GM_DUBOIS, //anaglyph green/magenta dubois
    ANAGLYPH_YB_GRAY,   //anaglyph yellow/blue gray
    ANAGLYPH_YB_HALF,   //anaglyph yellow/blue half colored
    ANAGLYPH_YB_COLOR,  //anaglyph yellow/blue colored
    ANAGLYPH_YB_DUBOIS, //anaglyph yellow/blue dubois
    MONO_L,             //mono output for debugging (left eye only)
    MONO_R,             //mono output for debugging (right eye only)
    SIDE_BY_SIDE_LR,    //side by side parallel (left eye left, right eye right)
    SIDE_BY_SIDE_RL,    //side by side crosseye (right eye left, left eye right)
    SIDE_BY_SIDE_2_LR,  //side by side parallel with half width resolution
    SIDE_BY_SIDE_2_RL,  //side by side crosseye with half width resolution
    ABOVE_BELOW_LR,     //above-below (left eye above, right eye below)
    ABOVE_BELOW_RL,     //above-below (right eye above, left eye below)
    ABOVE_BELOW_2_LR,   //above-below with half height resolution
    ABOVE_BELOW_2_RL,   //above-below with half height resolution
    INTERLEAVE_ROWS_LR, //row-interleave (left eye has top row)
    INTERLEAVE_ROWS_RL, //row-interleave (right eye has top row)
    STEREO_AUTO,        //use video metadata info (for input)
    STEREO_CODE_COUNT   //no value set - TODO: needs autodetection
} stereo_code;

struct vf_priv_s {
    int in_fmt;
    int out_fmt;
    bool auto_in;
    struct vf_lw_opts *lw_opts;
} const vf_priv_default = {
    SIDE_BY_SIDE_LR,
    ANAGLYPH_RC_DUBOIS,
};

const struct m_opt_choice_alternatives stereo_code_names[] = {
    {"arcg",                             ANAGLYPH_RC_GRAY},
    {"anaglyph_red_cyan_gray",           ANAGLYPH_RC_GRAY},
    {"arch",                             ANAGLYPH_RC_HALF},
    {"anaglyph_red_cyan_half_color",     ANAGLYPH_RC_HALF},
    {"arcc",                             ANAGLYPH_RC_COLOR},
    {"anaglyph_red_cyan_color",          ANAGLYPH_RC_COLOR},
    {"arcd",                             ANAGLYPH_RC_DUBOIS},
    {"anaglyph_red_cyan_dubios",         ANAGLYPH_RC_DUBOIS},
    {"agmg",                             ANAGLYPH_GM_GRAY},
    {"anaglyph_green_magenta_gray",      ANAGLYPH_GM_GRAY},
    {"agmh",                             ANAGLYPH_GM_HALF},
    {"anaglyph_green_magenta_half_color",ANAGLYPH_GM_HALF},
    {"agmc",                             ANAGLYPH_GM_COLOR},
    {"anaglyph_green_magenta_color",     ANAGLYPH_GM_COLOR},
    {"agmd",                             ANAGLYPH_GM_DUBOIS},
    {"anaglyph_green_magenta_dubois",    ANAGLYPH_GM_DUBOIS},
    {"aybg",                             ANAGLYPH_YB_GRAY},
    {"anaglyph_yellow_blue_gray",        ANAGLYPH_YB_GRAY},
    {"aybh",                             ANAGLYPH_YB_HALF},
    {"anaglyph_yellow_blue_half_color",  ANAGLYPH_YB_HALF},
    {"aybc",                             ANAGLYPH_YB_COLOR},
    {"anaglyph_yellow_blue_color",       ANAGLYPH_YB_COLOR},
    {"aybd",                             ANAGLYPH_YB_DUBOIS},
    {"anaglyph_yellow_blue_dubois",      ANAGLYPH_YB_DUBOIS},
    {"ml",                               MONO_L},
    {"mono_left",                        MONO_L},
    {"mr",                               MONO_R},
    {"mono_right",                       MONO_R},
    {"sbsl",                             SIDE_BY_SIDE_LR},
    {"side_by_side_left_first",          SIDE_BY_SIDE_LR},
    {"sbsr",                             SIDE_BY_SIDE_RL},
    {"side_by_side_right_first",         SIDE_BY_SIDE_RL},
    {"sbs2l",                              SIDE_BY_SIDE_2_LR},
    {"side_by_side_half_width_left_first", SIDE_BY_SIDE_2_LR},
    {"sbs2r",                              SIDE_BY_SIDE_2_RL},
    {"side_by_side_half_width_right_first",SIDE_BY_SIDE_2_RL},
    {"abl",                              ABOVE_BELOW_LR},
    {"above_below_left_first",           ABOVE_BELOW_LR},
    {"abr",                              ABOVE_BELOW_RL},
    {"above_below_right_first",          ABOVE_BELOW_RL},
    {"ab2l",                               ABOVE_BELOW_2_LR},
    {"above_below_half_height_left_first", ABOVE_BELOW_2_LR},
    {"ab2r",                               ABOVE_BELOW_2_RL},
    {"above_below_half_height_right_first",ABOVE_BELOW_2_RL},
    {"irl",                                INTERLEAVE_ROWS_LR},
    {"interleave_rows_left_first",         INTERLEAVE_ROWS_LR},
    {"irr",                                INTERLEAVE_ROWS_RL},
    {"interleave_rows_right_first",        INTERLEAVE_ROWS_RL},
    // convenience alias for MP_STEREO3D_MONO
    {"mono",                             MONO_L},
    // for filter auto-insertion
    {"auto",                             STEREO_AUTO},
    { NULL, 0}
};

// Extremely stupid; can be dropped when the internal filter is dropped,
// and OPT_CHOICE_C() can be used instead.
static int opt_to_stereo3dmode(int val)
{
    // Find x for name == MP_STEREO3D_NAME(x)
    const char *name = m_opt_choice_str(stereo_code_names, val);
    for (int n = 0; n < MP_STEREO3D_COUNT; n++) {
        const char *o = MP_STEREO3D_NAME(val);
        if (name && o && strcmp(o, name) == 0)
            return n;
    }
    return MP_STEREO3D_INVALID;
}

static int lavfi_reconfig(struct vf_instance *vf,
                          struct mp_image_params *in,
                          struct mp_image_params *out)
{
    struct vf_priv_s *p = vf_lw_old_priv(vf);
    if (p->auto_in) {
        const char *inf = MP_STEREO3D_NAME(in->stereo_in);
        if (!inf) {
            MP_ERR(vf, "Unknown/unsupported 3D mode.\n");
            return -1;
        }
        vf_lw_update_graph(vf, "stereo3d", "%s:%s",
                           inf, m_opt_choice_str(stereo_code_names, p->out_fmt));
        out->stereo_in = out->stereo_out = opt_to_stereo3dmode(p->out_fmt);
    }
    return 0;
}

static void lavfi_init(vf_instance_t *vf)
{
    if (vf->priv->in_fmt == STEREO_AUTO &&
        vf_lw_set_graph(vf, vf->priv->lw_opts, "stereo3d", "null") >= 0)
    {
        vf_lw_set_reconfig_cb(vf, lavfi_reconfig);
        return;
    }

    if (vf_lw_set_graph(vf, vf->priv->lw_opts, "stereo3d", "%s:%s",
                        m_opt_choice_str(stereo_code_names, vf->priv->in_fmt),
                        m_opt_choice_str(stereo_code_names, vf->priv->out_fmt)) >= 0)
        return;
}

static int vf_open(vf_instance_t *vf)
{
    if (vf->priv->out_fmt == STEREO_AUTO) {
        MP_FATAL(vf, "No autodetection for stereo output.\n");
        return 0;
    }
    if (vf->priv->in_fmt == STEREO_AUTO)
        vf->priv->auto_in = 1;

    lavfi_init(vf);
    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_CHOICE_C("in", in_fmt, 0, stereo_code_names),
    OPT_CHOICE_C("out", out_fmt, 0, stereo_code_names),
    OPT_SUBSTRUCT("", lw_opts, vf_lw_conf, 0),
    {0}
};

const vf_info_t vf_info_stereo3d = {
    .description = "stereoscopic 3d view",
    .name = "stereo3d",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .priv_defaults = &vf_priv_default,
    .options = vf_opts_fields,
};
