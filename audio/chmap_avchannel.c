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
#include <libavutil/error.h>
#include <libavutil/mem.h>

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

static int custom_init(AVChannelLayout *dst, int nb_channels)
{
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 37, 100)
    return av_channel_layout_custom_init(dst, nb_channels);
#else
    AVChannelCustom *map = av_calloc(nb_channels, sizeof(*map));
    if (!map)
        return AVERROR(ENOMEM);
    dst->order       = AV_CHANNEL_ORDER_CUSTOM;
    dst->nb_channels = nb_channels;
    dst->u.map       = map;
    return 0;
#endif
}

void mp_chmap_to_av_layout_custom(AVChannelLayout *dst,
                                  const struct mp_chmap *src)
{
    *dst = (AVChannelLayout){0};

    if (mp_chmap_is_unknown(src) || src->num <= 0 ||
        custom_init(dst, src->num) < 0)
    {
        dst->order = AV_CHANNEL_ORDER_UNSPEC;
        dst->nb_channels = src->num;
        return;
    }

    // mp_speaker_id values < 64 match the AVChannel enum directly. NA maps to
    // AV_CHAN_UNUSED. Anything else is unknown.
    for (int n = 0; n < src->num; n++) {
        if (src->speaker[n] == MP_SPEAKER_ID_NA) {
            dst->u.map[n].id = AV_CHAN_UNUSED;
        } else if (src->speaker[n] < 64) {
            dst->u.map[n].id = src->speaker[n];
        } else {
            dst->u.map[n].id = AV_CHAN_UNKNOWN;
        }
    }
}
