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
#include "osdep/endian.h"

static bool test_conversion(int src_format, int dst_format)
{
    return (src_format == AF_FORMAT_U24 && dst_format == AF_FORMAT_U32) ||
           (src_format == AF_FORMAT_S24 && dst_format == AF_FORMAT_S32) ||
           (src_format == AF_FORMAT_U32 && dst_format == AF_FORMAT_U24) ||
           (src_format == AF_FORMAT_S32 && dst_format == AF_FORMAT_S24);
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

        return mp_audio_config_equals(in, &orig_in) ? AF_OK : AF_FALSE;
    }
    case AF_CONTROL_SET_FORMAT: {
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

static int filter(struct af_instance *af, struct mp_audio *data)
{
    if (!data)
        return 0;
    struct mp_audio *out =
        mp_audio_pool_get(af->out_pool, af->data, data->samples);
    if (!out) {
        talloc_free(data);
        return -1;
    }
    mp_audio_copy_attributes(out, data);

    size_t len = mp_audio_psize(data) / data->bps;
    if (data->bps == 4) {
        for (int s = 0; s < len; s++) {
            uint32_t val = *((uint32_t *)data->planes[0] + s);
            uint8_t *ptr = (uint8_t *)out->planes[0] + s * 3;
            ptr[0] = val >> SHIFT(0);
            ptr[1] = val >> SHIFT(1);
            ptr[2] = val >> SHIFT(2);
        }
    } else {
        for (int s = 0; s < len; s++) {
            uint8_t *ptr = (uint8_t *)data->planes[0] + s * 3;
            uint32_t val = ptr[0] << SHIFT(0)
                         | ptr[1] << SHIFT(1)
                         | ptr[2] << SHIFT(2);
            *((uint32_t *)out->planes[0] + s) = val;
        }
    }

    talloc_free(data);
    af_add_output_frame(af, out);
    return 0;
}

static int af_open(struct af_instance *af)
{
    af->control = control;
    af->filter_frame = filter;
    return AF_OK;
}

const struct af_info af_info_convert24 = {
    .info = "Convert between 24 and 32 bit sample format",
    .name = "convert24",
    .open = af_open,
    .test_conversion = test_conversion,
};
