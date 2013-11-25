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

#ifndef MPLAYER_VD_H
#define MPLAYER_VD_H

#include "video/mp_image.h"
#include "demux/stheader.h"
#include "dec_video.h"

struct demux_packet;
struct mp_decoder_list;

/* interface of video decoder drivers */
typedef struct vd_functions
{
    const char *name;
    void (*add_decoders)(struct mp_decoder_list *list);
    int (*init)(struct dec_video *vd, const char *decoder);
    void (*uninit)(struct dec_video *vd);
    int (*control)(struct dec_video *vd, int cmd, void *arg);
    struct mp_image *(*decode)(struct dec_video *vd, struct demux_packet *pkt,
                               int flags);
} vd_functions_t;

// NULL terminated array of all drivers
extern const vd_functions_t *const mpcodecs_vd_drivers[];

enum vd_ctrl {
    VDCTRL_GET_PARAMS = 1, // retrieve struct mp_image_params
    VDCTRL_RESYNC_STREAM, // reset decode state after seeking
    VDCTRL_QUERY_UNSEEN_FRAMES, // current decoder lag
    VDCTRL_REINIT_VO, // reinit filter/VO chain
};

int mpcodecs_reconfig_vo(struct dec_video *vd, const struct mp_image_params *params);

#endif /* MPLAYER_VD_H */
