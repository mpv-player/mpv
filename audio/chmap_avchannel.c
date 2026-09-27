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
    default:
        // TODO: handle custom layouts
        return false;
    }
}

void mp_chmap_to_av_layout(AVChannelLayout *dst, const struct mp_chmap *src)
{
    *dst = (AVChannelLayout){
        .order = AV_CHANNEL_ORDER_UNSPEC,
        .nb_channels = src->num,
    };

    // TODO: handle custom layouts
    if (!mp_chmap_is_unknown(src)) {
        av_channel_layout_from_mask(dst, mp_chmap_to_lavc(src));
    }
}
