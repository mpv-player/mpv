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

#include <stdlib.h>
#include <string.h>

#include <libswresample/version.h>

#include "audio/aframe.h"
#include "audio/chmap.h"
#include "audio/format.h"
#include "common/av_common.h"
#include "common/global.h"
#include "filters/f_swresample.h"
#include "filters/filter.h"
#include "filters/frame.h"
#include "video/hwdec.h"
#include "test_utils.h"

int64_t mp_pts_to_av(double mp_pts, AVRational *tb) {
    abort();
}

double mp_pts_from_av(int64_t av_pts, AVRational *tb)
{
    abort();
}

int mp_set_avopts(struct mp_log *log, void *avobj, char **kv)
{
    assert_true(!kv);
    return 0;
}

void hwdec_devices_request_for_img_fmt(struct mp_hwdec_devices *devs,
                                       struct hwdec_imgfmt_request *params)
{
    abort();
}

struct mp_hwdec_ctx *hwdec_devices_get_by_imgfmt_and_type(
    struct mp_hwdec_devices *devs, int hw_imgfmt,
    enum AVHWDeviceType device_type)
{
    abort();
}

// Input channel c carries the constant VAL(c + 1), so routing, mixing and
// silence are all visible in the output values.
#define VAL(n) ((n) / 100.0)

#define SAMPLES 4000

struct conv_case {
    const char *name;
    const char *in_map;
    int in_format;
    int in_rate;
    const char *out_map; // NULL keeps the input channel map
    int out_format;
    int out_rate;
    double tolerance;
    unsigned min_swr_version; // skip the case below this libswresample version
    double expected[16]; // one value per output channel
};

static const struct conv_case cases[] = {
    // The same channel map on both sides, only the sample format changes.
    {"7.1 floatp to s16", "7.1", AF_FORMAT_FLOATP, 48000,
     NULL, AF_FORMAT_S16, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6), VAL(7), VAL(8)}},
    {"2.1 floatp to s16", "2.1", AF_FORMAT_FLOATP, 48000,
     NULL, AF_FORMAT_S16, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(3)}},
    {"quad float to s16", "quad", AF_FORMAT_FLOAT, 48000,
     NULL, AF_FORMAT_S16, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4)}},
    {"5.0 s16 to floatp", "5.0", AF_FORMAT_S16, 48000,
     NULL, AF_FORMAT_FLOATP, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5)}},
    {"6.1 floatp to s32", "6.1", AF_FORMAT_FLOATP, 48000,
     NULL, AF_FORMAT_S32, 48000, 1e-6, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6), VAL(7)}},
    {"unknown8 floatp to s16", "unknown8", AF_FORMAT_FLOATP, 48000,
     NULL, AF_FORMAT_S16, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6), VAL(7), VAL(8)}},
    {"unknown9 s16 to floatp", "unknown9", AF_FORMAT_S16, 48000,
     NULL, AF_FORMAT_FLOATP, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6), VAL(7), VAL(8), VAL(9)}},
    {"fl-fr-na floatp to s16", "fl-fr-na", AF_FORMAT_FLOATP, 48000,
     NULL, AF_FORMAT_S16, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(3)}},

    // Unknown layouts pass through by position.
    {"unknown8 to 7.1", "unknown8", AF_FORMAT_FLOATP, 48000,
     "7.1", AF_FORMAT_S16, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6), VAL(7), VAL(8)}},
    {"7.1 to unknown8", "7.1", AF_FORMAT_S16, 48000,
     "unknown8", AF_FORMAT_FLOATP, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6), VAL(7), VAL(8)}},
    {"unknown9 to stereo", "unknown9", AF_FORMAT_FLOATP, 48000,
     "stereo", AF_FORMAT_S16, 48000, 1e-4, 0,
     {VAL(1), VAL(2)}},
    {"stereo to unknown9", "stereo", AF_FORMAT_S16, 48000,
     "unknown9", AF_FORMAT_FLOATP, 48000, 1e-4, 0,
     {VAL(1), VAL(2), 0, 0, 0, 0, 0, 0, 0}},
    {"unknown16 to unknown6", "unknown16", AF_FORMAT_FLOATP, 48000,
     "unknown6", AF_FORMAT_FLOATP, 48000, 1e-6, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6)}},

    // Reordering between the same speakers.
    {"5.1 to 5.1(alsa) s16", "5.1", AF_FORMAT_S16, 48000,
     "5.1(alsa)", AF_FORMAT_S16, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(5), VAL(6), VAL(3), VAL(4)}},
    {"7.1 floatp to 7.1(alsa) s16", "7.1", AF_FORMAT_FLOATP, 48000,
     "7.1(alsa)", AF_FORMAT_S16, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(5), VAL(6), VAL(3), VAL(4), VAL(7), VAL(8)}},
    {"7.1(alsa) s16 to 7.1 floatp", "7.1(alsa)", AF_FORMAT_S16, 48000,
     "7.1", AF_FORMAT_FLOATP, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(5), VAL(6), VAL(3), VAL(4), VAL(7), VAL(8)}},

    // NA channels in the output, as padded device layouts have them.
    {"5.1 to 5.1 with 2 NA", "5.1", AF_FORMAT_FLOATP, 48000,
     "fl-fr-fc-lfe-bl-br-na-na", AF_FORMAT_S16, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6), 0, 0}},
    {"5.1 to 5.1(alsa) with 2 NA", "5.1", AF_FORMAT_FLOATP, 48000,
     "fl-fr-bl-br-fc-lfe-na-na", AF_FORMAT_FLOATP, 48000, 1e-6, 0,
     {VAL(1), VAL(2), VAL(5), VAL(6), VAL(3), VAL(4), 0, 0}},
    {"stereo to stereo with 2 NA", "stereo", AF_FORMAT_S16, 48000,
     "fl-fr-na-na", AF_FORMAT_S16, 48000, 1e-4, 0,
     {VAL(1), VAL(2), 0, 0}},
    {"5.1 to stereo with 2 NA", "5.1", AF_FORMAT_FLOATP, 48000,
     "fl-fr-na-na", AF_FORMAT_FLOATP, 48000, 1e-6, 0,
     {VAL(1) + M_SQRT1_2 * (VAL(3) + VAL(5)),
      VAL(2) + M_SQRT1_2 * (VAL(3) + VAL(6)), 0, 0}},
    {"5.1(side) to 5.1 with 2 NA", "5.1(side)", AF_FORMAT_FLOATP, 48000,
     "fl-fr-fc-lfe-bl-br-na-na", AF_FORMAT_S16, 48000, 1e-4, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6), 0, 0}},

    // Remixing. The center and surround mix levels are libswresample's
    // defaults, 1/sqrt(2).
    {"5.1(side) to 5.1", "5.1(side)", AF_FORMAT_FLOATP, 48000,
     "5.1", AF_FORMAT_FLOATP, 48000, 1e-6, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6)}},
    {"5.1 to stereo", "5.1", AF_FORMAT_FLOATP, 48000,
     "stereo", AF_FORMAT_FLOATP, 48000, 1e-6, 0,
     {VAL(1) + M_SQRT1_2 * (VAL(3) + VAL(5)),
      VAL(2) + M_SQRT1_2 * (VAL(3) + VAL(6))}},
    {"7.1 to stereo", "7.1", AF_FORMAT_FLOATP, 48000,
     "stereo", AF_FORMAT_FLOATP, 48000, 1e-6, 0,
     {VAL(1) + M_SQRT1_2 * (VAL(3) + VAL(5) + VAL(7)),
      VAL(2) + M_SQRT1_2 * (VAL(3) + VAL(6) + VAL(8))}},
    {"stereo to 5.1", "stereo", AF_FORMAT_FLOATP, 48000,
     "5.1", AF_FORMAT_FLOATP, 48000, 1e-6, 0,
     {VAL(1), VAL(2), 0, 0, 0, 0}},
    {"mono to stereo", "mono", AF_FORMAT_FLOATP, 48000,
     "stereo", AF_FORMAT_FLOATP, 48000, 1e-6, 0,
     {M_SQRT1_2 * VAL(1), M_SQRT1_2 * VAL(1)}},

    // Resampling on top of channel handling.
    {"7.1 to 7.1(alsa) 48000 to 44100", "7.1", AF_FORMAT_FLOATP, 48000,
     "7.1(alsa)", AF_FORMAT_FLOATP, 44100, 1e-4, 0,
     {VAL(1), VAL(2), VAL(5), VAL(6), VAL(3), VAL(4), VAL(7), VAL(8)}},
    {"5.1 to 5.1 with 2 NA 44100 to 48000", "5.1", AF_FORMAT_FLOATP, 44100,
     "fl-fr-fc-lfe-bl-br-na-na", AF_FORMAT_S16, 48000, 1e-3, 0,
     {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6), 0, 0}},
    {"unknown9 to stereo 48000 to 96000", "unknown9", AF_FORMAT_S16, 48000,
     "stereo", AF_FORMAT_S16, 96000, 1e-3, 0,
     {VAL(1), VAL(2)}},

    // NA channels in the input.
    {"fl-fr-na to stereo", "fl-fr-na", AF_FORMAT_FLOATP, 48000,
     "stereo", AF_FORMAT_S16, 48000, 1e-4, AV_VERSION_INT(7, 3, 100),
     {VAL(1), VAL(2)}},
    {"na-fl-fr to stereo", "na-fl-fr", AF_FORMAT_S16, 48000,
     "stereo", AF_FORMAT_S16, 48000, 1e-4, AV_VERSION_INT(7, 3, 100),
     {VAL(2), VAL(3)}},
};

static void put_sample(uint8_t *dst, int format, double v)
{
    switch (af_fmt_from_planar(format)) {
    case AF_FORMAT_S16: {
        int16_t s = lrint(v * 32768.0);
        memcpy(dst, &s, sizeof(s));
        break;
    }
    case AF_FORMAT_S32: {
        int32_t s = lrint(v * 2147483648.0);
        memcpy(dst, &s, sizeof(s));
        break;
    }
    case AF_FORMAT_FLOAT: {
        float f = v;
        memcpy(dst, &f, sizeof(f));
        break;
    }
    case AF_FORMAT_DOUBLE:
        memcpy(dst, &v, sizeof(v));
        break;
    default:
        abort();
    }
}

static double get_sample(const uint8_t *src, int format)
{
    switch (af_fmt_from_planar(format)) {
    case AF_FORMAT_S16: {
        int16_t s;
        memcpy(&s, src, sizeof(s));
        return s / 32768.0;
    }
    case AF_FORMAT_S32: {
        int32_t s;
        memcpy(&s, src, sizeof(s));
        return s / 2147483648.0;
    }
    case AF_FORMAT_FLOAT: {
        float f;
        memcpy(&f, src, sizeof(f));
        return f;
    }
    case AF_FORMAT_DOUBLE: {
        double d;
        memcpy(&d, src, sizeof(d));
        return d;
    }
    default:
        abort();
    }
}

static uint8_t *sample_ptr(struct mp_aframe *frame, uint8_t **data, int ch,
                           int n)
{
    int format = mp_aframe_get_format(frame);
    size_t sstride = mp_aframe_get_sstride(frame);
    if (af_fmt_is_planar(format))
        return data[ch] + n * sstride;
    return data[0] + n * sstride + ch * af_fmt_to_bytes(format);
}

static struct mp_aframe *make_frame(struct mp_chmap *map, int format, int rate)
{
    struct mp_aframe *frame = mp_aframe_create();
    assert_true(mp_aframe_set_chmap(frame, map));
    assert_true(mp_aframe_set_format(frame, format));
    assert_true(mp_aframe_set_rate(frame, rate));
    assert_true(mp_aframe_alloc_data(frame, SAMPLES));
    mp_aframe_set_pts(frame, 0);

    uint8_t **data = mp_aframe_get_data_rw(frame);
    assert_true(data);
    for (int ch = 0; ch < map->num; ch++) {
        for (int n = 0; n < SAMPLES; n++)
            put_sample(sample_ptr(frame, data, ch, n), format, VAL(ch + 1));
    }
    return frame;
}

struct result {
    struct mp_chmap chmap;
    int format;
    int rate;
    int num_samples;
    double *samples; // num_samples * chmap.num values, interleaved
};

// Push one frame through a swresample filter and collect everything it
// outputs, driving it the way the player drives a top-level filter.
static struct result run_conversion(void *ta_ctx, struct mp_chmap *in_map,
                                    int in_format, int in_rate,
                                    struct mp_chmap *out_map,
                                    int out_format, int out_rate)
{
    struct mpv_global global = {0};
    struct mp_filter *root = mp_filter_create_root(&global);
    assert_true(root);

    struct mp_resample_opts opts = MP_RESAMPLE_OPTS_DEF;
    struct mp_swresample *sw = mp_swresample_create(root, &opts);
    assert_true(sw);
    sw->out_format = out_format;
    sw->out_rate = out_rate;
    sw->out_channels = *out_map;
    struct mp_filter *f = sw->f;

    struct result r = {0};
    struct mp_frame pending =
        MAKE_FRAME(MP_FRAME_AUDIO, make_frame(in_map, in_format, in_rate));
    bool eof_sent = false;

    for (int iter = 0; ; iter++) {
        // A filter that neither produces output nor asks for input is stuck.
        assert_true(iter < 1000);
        assert_false(mp_filter_has_failed(f));

        if (mp_pin_in_needs_data(f->pins[0])) {
            if (pending.type) {
                assert_true(mp_pin_in_write(f->pins[0], pending));
                pending = MP_NO_FRAME;
            } else if (!eof_sent) {
                assert_true(mp_pin_in_write(f->pins[0], MP_EOF_FRAME));
                eof_sent = true;
            }
        }

        if (!mp_pin_out_request_data(f->pins[1]))
            continue;

        struct mp_frame frame = mp_pin_out_read(f->pins[1]);
        if (frame.type == MP_FRAME_EOF) {
            assert_false(mp_filter_has_failed(f));
            break;
        }
        assert_int_equal(frame.type, MP_FRAME_AUDIO);
        struct mp_aframe *out = frame.data;

        if (!r.num_samples) {
            assert_true(mp_aframe_get_chmap(out, &r.chmap));
            r.format = mp_aframe_get_format(out);
            r.rate = mp_aframe_get_rate(out);
        }
        int nch = mp_aframe_get_channels(out);
        assert_int_equal(nch, r.chmap.num);
        assert_int_equal(mp_aframe_get_format(out), r.format);

        int count = mp_aframe_get_size(out);
        uint8_t **data = mp_aframe_get_data_ro(out);
        assert_true(data);
        r.samples = talloc_realloc(ta_ctx, r.samples, double,
                                   (r.num_samples + count) * nch);
        for (int n = 0; n < count; n++) {
            for (int ch = 0; ch < nch; ch++) {
                r.samples[(r.num_samples + n) * nch + ch] =
                    get_sample(sample_ptr(out, data, ch, n), r.format);
            }
        }
        r.num_samples += count;
        talloc_free(out);
    }

    assert_true(eof_sent);
    talloc_free(root);
    return r;
}

static void check_case(const struct conv_case *c)
{
    printf("%s\n", c->name);
    if (LIBSWRESAMPLE_VERSION_INT < c->min_swr_version) {
        printf("  skipped, needs a newer libswresample\n");
        return;
    }

    struct mp_chmap in_map, out_map;
    assert_true(mp_chmap_from_str(&in_map, bstr0(c->in_map)));
    out_map = in_map;
    if (c->out_map)
        assert_true(mp_chmap_from_str(&out_map, bstr0(c->out_map)));
    assert_true(out_map.num <= MP_ARRAY_SIZE(c->expected));

    void *ta_ctx = talloc_new(NULL);
    struct result r = run_conversion(ta_ctx, &in_map, c->in_format, c->in_rate,
                                     &out_map, c->out_format, c->out_rate);

    assert_true(mp_chmap_equals(&r.chmap, &out_map));
    assert_int_equal(r.format, c->out_format);
    assert_int_equal(r.rate, c->out_rate);

    int first = 0;
    int last = r.num_samples;
    if (c->in_rate == c->out_rate) {
        assert_int_equal(r.num_samples, SAMPLES);
    } else {
        int expected = (int64_t)SAMPLES * c->out_rate / c->in_rate;
        assert_true(abs(r.num_samples - expected) <= expected / 20);
        first = r.num_samples / 4;
        last = r.num_samples - r.num_samples / 4;
    }

    for (int n = first; n < last; n++) {
        for (int ch = 0; ch < out_map.num; ch++) {
            assert_float_equal(r.samples[n * out_map.num + ch],
                               c->expected[ch], c->tolerance);
        }
    }

    talloc_free(ta_ctx);
}

int main(void)
{
    for (int n = 0; n < MP_ARRAY_SIZE(cases); n++)
        check_case(&cases[n]);
    return 0;
}
