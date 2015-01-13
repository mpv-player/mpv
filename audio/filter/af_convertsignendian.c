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

#include "af.h"
#include "audio/format.h"
#include "osdep/endian.h"

static bool test_conversion(int src_format, int dst_format)
{
    if ((src_format & AF_FORMAT_PLANAR) ||
        (dst_format & AF_FORMAT_PLANAR))
        return false;
    if (((src_format & ~AF_FORMAT_SIGN_MASK) ==
         (dst_format & ~AF_FORMAT_SIGN_MASK)) &&
        ((src_format & AF_FORMAT_TYPE_MASK) == AF_FORMAT_I))
        return true;
    return false;
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

        out->rate = in->rate;
        mp_audio_set_channels(out, &in->channels);

        return mp_audio_config_equals(in, &orig_in) ? AF_OK : AF_FALSE;
    }
    case AF_CONTROL_SET_FORMAT: {
        mp_audio_set_format(af->data, *(int*)arg);
        return AF_OK;
    }
    }
    return AF_UNKNOWN;
}

static void si2us(void *data, int len, int bps)
{
    ptrdiff_t i = -(len * bps);
    uint8_t *p = &((uint8_t *)data)[len * bps];
    if (BYTE_ORDER == LITTLE_ENDIAN && bps > 1)
        p += bps - 1;
    if (len <= 0)
        return;
    do {
        p[i] ^= 0x80;
    } while (i += bps);
}

static int filter(struct af_instance *af, struct mp_audio *data)
{
    if (!data)
        return 0;
    if (af_make_writeable(af, data) < 0) {
        talloc_free(data);
        return -1;
    }

    int infmt = data->format;
    int outfmt = af->data->format;
    size_t len = data->samples * data->nch;

    if ((infmt & AF_FORMAT_SIGN_MASK) != (outfmt & AF_FORMAT_SIGN_MASK))
        si2us(data->planes[0], len, data->bps);

    mp_audio_set_format(data, outfmt);
    af_add_output_frame(af, data);
    return 0;
}

static int af_open(struct af_instance *af)
{
    af->control = control;
    af->filter_frame = filter;
    return AF_OK;
}

const struct af_info af_info_convertsignendian = {
    .info = "Convert between sample format sign",
    .name = "convertsign",
    .open = af_open,
    .test_conversion = test_conversion,
};
