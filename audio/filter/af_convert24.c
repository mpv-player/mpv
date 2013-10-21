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

#include <stdlib.h>
#include <assert.h>

#include "audio/format.h"
#include "af.h"

static bool test_conversion(int src_format, int dst_format)
{
    if (!(src_format & AF_FORMAT_POINT_MASK) == AF_FORMAT_I)
        return false;
    if ((src_format & ~AF_FORMAT_BITS_MASK) !=
        (dst_format & ~AF_FORMAT_BITS_MASK))
        return false;
    int srcbits = src_format & AF_FORMAT_BITS_MASK;
    int dstbits = dst_format & AF_FORMAT_BITS_MASK;
    return (srcbits == AF_FORMAT_24BIT && dstbits == AF_FORMAT_32BIT) ||
           (srcbits == AF_FORMAT_32BIT && dstbits == AF_FORMAT_24BIT);
}

static int control(struct af_instance *af, int cmd, void *arg)
{
    switch (cmd) {
    case AF_CONTROL_REINIT: {
        struct mp_audio *in = arg;
        struct mp_audio orig_in = *in;
        struct mp_audio *out = af->data;

        if (!test_conversion(in->format, out->format))
            return AF_DETACH;

        if ((in->format & AF_FORMAT_BITS_MASK) == AF_FORMAT_24BIT) {
            mp_audio_set_format(out, af_fmt_change_bits(in->format, 32));
        } else if ((in->format & AF_FORMAT_BITS_MASK) == AF_FORMAT_32BIT) {
            mp_audio_set_format(out, af_fmt_change_bits(in->format, 24));
        } else {
            abort();
        }

        out->rate = in->rate;
        mp_audio_set_channels(out, &in->channels);

        assert(test_conversion(in->format, out->format));

        af->mul = (double)out->bps / in->bps;

        return mp_audio_config_equals(in, &orig_in) ? AF_OK : AF_FALSE;
    }
    case AF_CONTROL_FORMAT_FMT | AF_CONTROL_SET: {
        mp_audio_set_format(af->data, *(int*)arg);
        return AF_OK;
    }
    }
    return AF_UNKNOWN;
}

// The LSB is always ignored.
#if BYTE_ORDER == BIG_ENDIAN
#define SHIFT(x) ((3-(x))*8)
#else
#define SHIFT(x) (((x)+1)*8)
#endif

static struct mp_audio *play(struct af_instance *af, struct mp_audio *data)
{
    if (RESIZE_LOCAL_BUFFER(af, data) != AF_OK)
        return NULL;

    struct mp_audio *out = af->data;
    size_t len = data->len / data->bps;

    if (data->bps == 4) {
        for (int s = 0; s < len; s++) {
            uint32_t val = *((uint32_t *)data->audio + s);
            uint8_t *ptr = (uint8_t *)out->audio + s * 3;
            ptr[0] = val >> SHIFT(0);
            ptr[1] = val >> SHIFT(1);
            ptr[2] = val >> SHIFT(2);
        }
        mp_audio_set_format(data, af_fmt_change_bits(data->format, 24));
    } else {
        for (int s = 0; s < len; s++) {
            uint8_t *ptr = (uint8_t *)data->audio + s * 3;
            uint32_t val = ptr[0] << SHIFT(0)
                         | ptr[1] << SHIFT(1)
                         | ptr[2] << SHIFT(2);
            *((uint32_t *)out->audio + s) = val;
        }
        mp_audio_set_format(data, af_fmt_change_bits(data->format, 32));
    }

    data->audio = out->audio;
    data->len = len * data->bps;
    return data;
}

static void uninit(struct af_instance* af)
{
    if (af->data)
        free(af->data->audio);
}

static int af_open(struct af_instance *af)
{
    af->control = control;
    af->play = play;
    af->uninit = uninit;
    af->data = talloc_zero(af, struct mp_audio);
    return AF_OK;
}

struct af_info af_info_convert24 = {
    "Convert between 24 and 32 bit sample format",
    "convert24",
    "",
    "",
    0,
    af_open,
    .test_conversion = test_conversion,
};
