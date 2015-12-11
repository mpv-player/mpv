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
    VDCTRL_RESET = 1, // reset decode state after seeking
    VDCTRL_QUERY_UNSEEN_FRAMES, // current decoder lag
    VDCTRL_FORCE_HWDEC_FALLBACK, // force software decoding fallback
    VDCTRL_GET_HWDEC,
};

#endif /* MPLAYER_VD_H */
