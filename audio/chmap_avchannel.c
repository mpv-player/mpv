/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>

#include "common/common.h"
#include "chmap.h"
#include "chmap_avchannel.h"

bool mp_chmap_from_av_layout(struct mp_chmap *dst, const AVChannelLayout *src)
{
    *dst = (struct mp_chmap) {0};

    switch (src->order) {
    case AV_CHANNEL_ORDER_UNSPEC:
        mp_chmap_from_channels(dst, src->nb_channels);
        return dst->num == src->nb_channels;
    case AV_CHANNEL_ORDER_NATIVE:
        mp_chmap_from_lavc(dst, src->u.mask);
        return dst->num == src->nb_channels;
    case AV_CHANNEL_ORDER_CUSTOM: {
        if (src->nb_channels > MP_NUM_CHANNELS)
            return false;
        struct mp_chmap map = {.num = src->nb_channels};
        for (int n = 0; n < map.num; n++) {
            // Speaker IDs below 64 are the AVChannel values.
            enum AVChannel id = src->u.map[n].id;
            if (id >= 0 && id < 64)
                map.speaker[n] = id;
            else if (id == AV_CHAN_UNUSED)
                map.speaker[n] = MP_SPEAKER_ID_NA;
            else
                return false;
        }
        if (!mp_chmap_is_valid(&map))
            return false;
        *dst = map;
        return true;
    }
    default:
        return false;
    }
}

void mp_chmap_to_av_layout(AVChannelLayout *dst, const struct mp_chmap *src)
{
    av_channel_layout_uninit(dst);
    dst->nb_channels = src->num;

    if (mp_chmap_is_empty(src) || mp_chmap_is_unknown(src))
        return;

    if (mp_chmap_is_lavc(src)) {
        av_channel_layout_from_mask(dst, mp_chmap_to_lavc(src));
        return;
    }

    dst->order = AV_CHANNEL_ORDER_CUSTOM;
    dst->u.map = av_calloc(src->num, sizeof(*dst->u.map));
    MP_HANDLE_OOM(dst->u.map);
    for (int n = 0; n < src->num; n++) {
        int sp = src->speaker[n];
        dst->u.map[n].id = sp == MP_SPEAKER_ID_NA ? AV_CHAN_UNUSED : sp;
    }
}
