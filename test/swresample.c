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

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

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
    for (int n = 0; kv && kv[n * 2]; n++) {
        assert_true(av_opt_set(avobj, kv[n * 2], kv[n * 2 + 1],
                               AV_OPT_SEARCH_CHILDREN) >= 0);
    }
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

// Input channel c carries VAL(c + 1), so routing, mixing and silence are all
// visible in the output values. The values are small enough for a mix of all
// channels of the largest layouts to stay below full scale.
#define VAL(n) ((n) / 512.0)

// All channels are scaled by this. It differs from sample to sample, so
// samples that move in time are visible too. It has more bits than a float
// holds, and like the values it is a multiple of a power of two. Their
// product is exact then, also for code that computes with more precision
// than a double has, as x87 code does.
static double weight(int n)
{
    return 0.5 + (n * 2654435761u >> 2) / 4294967296.0;
}

struct spec {
    struct mp_chmap map;
    int format;
    int rate;
};

struct input {
    struct spec spec;
    int samples;
    bool reset;     // reset the filter before this frame
    double speed;   // set the playback speed before this frame, if not 0
};

// How channels are mixed. The filter gets the options, the reference the
// values that they stand for.
struct mix {
    bool normalize;                 // --audio-normalize-downmix
    char **avopts;                  // --audio-swresample-o
    double clev, slev, lfe;         // levels of center, surround and LFE
    double maxval;                  // limit for the coefficients of a channel
    double volume;
    enum AVMatrixEncoding encoding;
    double gain;                    // level of a channel that changes its place
};

#define AVOPTS(...) (char *[]){__VA_ARGS__, NULL}

// What mpv sets without an option, and for --audio-normalize-downmix. The
// levels are the defaults of libswresample.
static const struct mix mix_default = {
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = 1000, .volume = 1,
    .gain = 1,
};

static const struct mix mix_normalize = {
    .normalize = true,
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = 1, .volume = 1,
    .gain = 1,
};

// Levels of its own for every kind of channel, and a volume.
static const struct mix mix_levels = {
    .avopts = AVOPTS("clev", "0.5", "slev", "0.25", "lfe_mix_level", "0.75",
                     "rmvol", "0.5"),
    .clev = 0.5, .slev = 0.25, .lfe = 0.75, .maxval = 1000, .volume = 0.5,
    .gain = 0.5,
};

// Surround channels in both output channels with opposite signs.
static const struct mix mix_dolby = {
    .normalize = true,
    .avopts = AVOPTS("matrix_encoding", "dolby", "rmvol", "2"),
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = 1, .volume = 2,
    .encoding = AV_MATRIX_ENCODING_DOLBY, .gain = 2,
};

static const struct mix mix_half = {
    .avopts = AVOPTS("rmvol", "0.5"),
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = 1000, .volume = 0.5,
    .gain = 0.5,
};

// A negative volume takes the place of the sum of the coefficients, which
// the limit is for.
static const struct mix mix_negative = {
    .normalize = true,
    .avopts = AVOPTS("rmvol", "-2"),
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = 1, .volume = -2,
    .gain = 0.5,
};

// A volume is set, and channels that are copied keep their level.
static const struct mix mix_unity = {
    .normalize = true,
    .avopts = AVOPTS("rmvol", "-1"),
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = 1, .volume = -1,
    .gain = 1,
};

// Without a limit, libswresample has none for floating point and 1 for
// integers, be it in the output or in the format that it is told to mix in.
static const struct mix mix_auto_float = {
    .avopts = AVOPTS("rematrix_maxval", "0"),
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = INT_MAX, .volume = 1,
    .gain = 1,
};

static const struct mix mix_auto_integer = {
    .avopts = AVOPTS("rematrix_maxval", "0"),
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = 1, .volume = 1,
    .gain = 1,
};

static const struct mix mix_auto_s16p = {
    .avopts = AVOPTS("rematrix_maxval", "0", "internal_sample_fmt", "s16p"),
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = 1, .volume = 1,
    .gain = 1,
};

static const struct mix mix_auto_fltp_float = {
    .avopts = AVOPTS("rematrix_maxval", "0", "internal_sample_fmt", "fltp"),
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = INT_MAX, .volume = 1,
    .gain = 1,
};

static const struct mix mix_auto_fltp_integer = {
    .avopts = AVOPTS("rematrix_maxval", "0", "internal_sample_fmt", "fltp"),
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = 1, .volume = 1,
    .gain = 1,
};

// A limit below 1 lowers a channel that only changes its place too.
static const struct mix mix_limit = {
    .avopts = AVOPTS("rematrix_maxval", "0.5"),
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = 0.5, .volume = 1,
    .gain = 0.5,
};

static const struct mix mix_limit_half = {
    .avopts = AVOPTS("rematrix_maxval", "0.5", "rmvol", "0.5"),
    .clev = M_SQRT1_2, .slev = M_SQRT1_2, .maxval = 0.5, .volume = 0.5,
    .gain = 0.25,
};

MP_NORETURN MP_PRINTF_ATTRIBUTE(1, 2)
static void fail(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);
    fflush(stdout);
    abort();
}

static struct mp_chmap parse_map(const char *s)
{
    struct mp_chmap map;
    if (!mp_chmap_from_str(&map, bstr0(s)) || !mp_chmap_is_valid(&map))
        fail("invalid channel map %s\n", s);
    return map;
}

// The layout, filled up with NA channels.
static struct mp_chmap parse_padded_map(const char *s, int num)
{
    struct mp_chmap map = parse_map(s);
    while (map.num < num)
        map.speaker[map.num++] = MP_SPEAKER_ID_NA;
    return map;
}

static bool is_passthrough(const struct mp_chmap *in,
                           const struct mp_chmap *out)
{
    return mp_chmap_is_unknown(in) || mp_chmap_is_unknown(out) ||
           mp_chmap_equals(in, out);
}

// What each output channel has to carry, for an input whose channel c carries
// VAL(c + 1), and whether all of them are copies of input channels or silent.
// There is no way to convert if this fails.
static bool expected_output(const struct mp_chmap *in,
                            const struct mp_chmap *out, const struct mix *mix,
                            double *res, bool *copies)
{
    for (int n = 0; n < out->num; n++)
        res[n] = 0;
    *copies = true;

    // Channels without speaker information keep their position, and so do
    // all channels if the map does not change. Nothing is done to them,
    // unless a volume is set.
    if (is_passthrough(in, out)) {
        double gain = mix->volume == 1 ? 1 : mix->gain;
        for (int n = 0; n < MPMIN(in->num, out->num); n++)
            res[n] = gain * VAL(n + 1);
        *copies = gain == 1;
        return true;
    }

    // Everything else follows the speakers. If they are the same on both
    // sides, every speaker gets its own channel, whatever the speakers are.
    uint64_t in_mask = mp_chmap_to_lavc_unchecked(in);
    uint64_t out_mask = mp_chmap_to_lavc_unchecked(out);
    if (in_mask == out_mask) {
        for (int o = 0; o < out->num; o++) {
            for (int i = 0; i < in->num; i++) {
                if (out->speaker[o] != MP_SPEAKER_ID_NA &&
                    out->speaker[o] == in->speaker[i])
                    res[o] = mix->gain * VAL(i + 1);
            }
        }
        *copies = mix->gain == 1;
        return true;
    }

    // Different speakers are mixed. The reference is what libswresample
    // mixes between the two sets of speakers in its native order, which
    // knows nothing about the channel order on either side.
    AVChannelLayout in_layout, out_layout;
    assert_true(av_channel_layout_from_mask(&in_layout, in_mask) >= 0);
    assert_true(av_channel_layout_from_mask(&out_layout, out_mask) >= 0);

    static double matrix[64 * 64];
    memset(matrix, 0, sizeof(matrix));
    if (swr_build_matrix2(&in_layout, &out_layout, mix->clev, mix->slev,
                          mix->lfe, mix->maxval, mix->volume, matrix, 64,
                          mix->encoding, NULL) < 0)
        return false;

    for (int o = 0; o < out->num; o++) {
        if (out->speaker[o] == MP_SPEAKER_ID_NA)
            continue;
        int mo = av_popcount64(out_mask & ((1ULL << out->speaker[o]) - 1));
        int sources = 0;
        for (int i = 0; i < in->num; i++) {
            if (in->speaker[i] == MP_SPEAKER_ID_NA)
                continue;
            int mi = av_popcount64(in_mask & ((1ULL << in->speaker[i]) - 1));
            double gain = matrix[mo * 64 + mi];
            res[o] += gain * VAL(i + 1);
            if (gain) {
                sources++;
                *copies &= gain == 1;
            }
        }
        *copies &= sources <= 1;
    }
    return true;
}

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

// The value that a sample of this format holds after put_sample().
static double stored_sample(int format, double v)
{
    uint8_t buf[8];
    put_sample(buf, format, v);
    return get_sample(buf, format);
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

// The frame starts at sample first of the whole input.
static struct mp_aframe *make_frame(const struct input *in, int first,
                                    bool vary)
{
    struct mp_chmap map = in->spec.map;
    struct mp_aframe *frame = mp_aframe_create();
    assert_true(mp_aframe_set_chmap(frame, &map));
    assert_true(mp_aframe_set_format(frame, in->spec.format));
    assert_true(mp_aframe_set_rate(frame, in->spec.rate));
    assert_true(mp_aframe_alloc_data(frame, in->samples));
    mp_aframe_set_pts(frame, 0);

    uint8_t **data = mp_aframe_get_data_rw(frame);
    assert_true(data);
    for (int ch = 0; ch < map.num; ch++) {
        for (int n = 0; n < in->samples; n++) {
            double v = VAL(ch + 1) * (vary ? weight(first + n) : 1);
            put_sample(sample_ptr(frame, data, ch, n), in->spec.format, v);
        }
    }
    return frame;
}

struct result {
    bool failed;        // the filter gave up
    int num_samples;
    double *samples;    // num_samples * channels values, interleaved
};

// Push the frames through a swresample filter and collect everything it
// outputs, driving it the way the player drives a top-level filter.
static struct result run_conversion(void *ta_ctx, const struct input *in,
                                    int num_in, const struct spec *out,
                                    const struct mix *mix, bool vary)
{
    struct mpv_global global = {0};
    struct mp_filter *root = mp_filter_create_root(&global);
    assert_true(root);

    struct mp_resample_opts opts = MP_RESAMPLE_OPTS_DEF;
    opts.normalize = mix->normalize;
    opts.avopts = mix->avopts;
    struct mp_swresample *sw = mp_swresample_create(root, &opts);
    assert_true(sw);
    sw->out_format = out->format;
    sw->out_rate = out->rate;
    sw->out_channels = out->map;
    struct mp_filter *f = sw->f;

    struct result r = {0};
    int next = 0;
    int first = 0;
    int reset_done = -1;
    bool eof_sent = false;

    for (int iter = 0; ; iter++) {
        if (iter >= 1000)
            fail("  the filter neither asks for input nor produces output\n");
        if (mp_filter_has_failed(f)) {
            r.failed = true;
            break;
        }

        if (mp_pin_in_needs_data(f->pins[0])) {
            if (next < num_in && in[next].reset && reset_done != next) {
                // The filter asks for input, so it has nothing buffered.
                mp_filter_reset(f);
                reset_done = next;
                continue;
            }
            if (next < num_in) {
                if (in[next].speed) {
                    struct mp_filter_command cmd = {
                        .type = MP_FILTER_COMMAND_SET_SPEED_RESAMPLE,
                        .speed = in[next].speed,
                    };
                    assert_true(mp_filter_command(f, &cmd));
                }
                struct mp_aframe *frame = make_frame(&in[next], first, vary);
                first += in[next++].samples;
                assert_true(mp_pin_in_write(f->pins[0],
                                            MAKE_FRAME(MP_FRAME_AUDIO, frame)));
            } else if (!eof_sent) {
                assert_true(mp_pin_in_write(f->pins[0], MP_EOF_FRAME));
                eof_sent = true;
            }
        }

        if (!mp_pin_out_request_data(f->pins[1]))
            continue;

        struct mp_frame frame = mp_pin_out_read(f->pins[1]);
        if (frame.type == MP_FRAME_EOF)
            break;
        assert_int_equal(frame.type, MP_FRAME_AUDIO);
        struct mp_aframe *aframe = frame.data;

        struct mp_chmap map;
        assert_true(mp_aframe_get_chmap(aframe, &map));
        if (!mp_chmap_equals(&map, &out->map))
            fail("  output channel map is %s\n", mp_chmap_to_str(&map));
        assert_int_equal(mp_aframe_get_format(aframe), out->format);
        assert_int_equal(mp_aframe_get_rate(aframe), out->rate);

        int count = mp_aframe_get_size(aframe);
        uint8_t **data = mp_aframe_get_data_ro(aframe);
        assert_true(data);
        r.samples = talloc_realloc(ta_ctx, r.samples, double,
                                   (r.num_samples + count) * map.num);
        for (int n = 0; n < count; n++) {
            for (int ch = 0; ch < map.num; ch++) {
                r.samples[(r.num_samples + n) * map.num + ch] =
                    get_sample(sample_ptr(aframe, data, ch, n), out->format);
            }
        }
        r.num_samples += count;
        talloc_free(aframe);
    }

    assert_true(r.failed || eof_sent);
    talloc_free(root);
    return r;
}

static bool is_s16(int format)
{
    return af_fmt_from_planar(format) == AF_FORMAT_S16;
}

static void print_conversion(const struct input *in, int num_in,
                             const struct spec *out, const struct mix *mix)
{
    for (int n = 0; n < num_in; n++) {
        printf("%s%s %s %d", n ? ", " : "", mp_chmap_to_str(&in[n].spec.map),
               af_fmt_to_str(in[n].spec.format), in[n].spec.rate);
        if (in[n].reset)
            printf(" after a reset");
        if (in[n].speed)
            printf(" at speed %g", in[n].speed);
    }
    printf(" -> %s %s %d%s", mp_chmap_to_str(&out->map),
           af_fmt_to_str(out->format), out->rate,
           mix->normalize ? " normalized" : "");
    for (int n = 0; mix->avopts && mix->avopts[n * 2]; n++)
        printf(" %s=%s", mix->avopts[n * 2], mix->avopts[n * 2 + 1]);
    printf("\n");
}

// Convert the input frames, which can differ in format, to out and check
// every sample that comes out.
static void check_conversion(const struct input *in, int num_in,
                             const struct spec *out, const struct mix *mix)
{
    print_conversion(in, num_in, out, mix);

    bool resample = false;
    bool s16 = is_s16(out->format);
    double speed = 1;
    double total = 0;
    for (int n = 0; n < num_in; n++) {
        if (in[n].speed)
            speed = in[n].speed;
        resample |= in[n].spec.rate != out->rate || speed != 1;
        s16 |= is_s16(in[n].spec.format);
        total += in[n].samples * (double)out->rate / (in[n].spec.rate * speed);
    }

    // Resampling changes the sample positions and ramps in and out, so it
    // gets a constant input, and only the middle of the output is checked.
    for (int n = 0; resample && n < num_in; n++)
        assert_true(mp_chmap_equals(&in[n].spec.map, &in[0].spec.map));
    double tolerance = resample ? 1e-3 : s16 ? 4e-4 : 1e-6;

    void *ta_ctx = talloc_new(NULL);
    struct result r = run_conversion(ta_ctx, in, num_in, out, mix, !resample);
    if (r.failed)
        fail("  the filter failed\n");

    int first = 0;
    int last = r.num_samples;
    if (resample) {
        // Nothing gets lost, and the rates are met to the sample.
        if (fabs(r.num_samples - total) > 8)
            fail("  %d samples, expected %.0f\n", r.num_samples, total);
        first = r.num_samples / 4;
        last = r.num_samples - r.num_samples / 4;
    } else if (r.num_samples != lrint(total)) {
        fail("  %d samples, expected %.0f\n", r.num_samples, total);
    }

    int cur = -1;
    int cur_end = 0;
    double expected[MP_NUM_CHANNELS];
    bool exact = false;
    for (int n = first; n < last; n++) {
        // Without resampling, every input sample yields one output sample.
        while (cur < 0 || (!resample && n >= cur_end)) {
            cur++;
            cur_end += in[cur].samples;
            if (!expected_output(&in[cur].spec.map, &out->map, mix, expected,
                                 &exact))
                fail("  there is no reference for this conversion\n");
            // Copies within a sample format leave nothing to compute, so
            // they have to come out bit by bit.
            exact &= !resample && af_fmt_from_planar(in[cur].spec.format) ==
                                  af_fmt_from_planar(out->format);
        }
        for (int ch = 0; ch < out->map.num; ch++) {
            double got = r.samples[n * out->map.num + ch];
            double want = expected[ch] * (resample ? 1 : weight(n));
            if (exact)
                want = stored_sample(out->format, want);
            if (exact ? got != want : fabs(got - want) > tolerance) {
                fail("  channel %d, sample %d: %.10f, expected %.10f%s\n", ch,
                     n, got, want, exact ? " exactly" : "");
            }
        }
    }

    talloc_free(ta_ctx);
}

// There is no way to convert between these, so the filter has to fail.
static void check_unsupported(const char *in_map, const char *out_map)
{
    struct input in = {{parse_map(in_map), AF_FORMAT_FLOATP, 48000}, 1000};
    struct spec out = {parse_map(out_map), AF_FORMAT_FLOATP, 48000};
    print_conversion(&in, 1, &out, &mix_default);

    double expected[MP_NUM_CHANNELS];
    bool copies;
    if (expected_output(&in.spec.map, &out.map, &mix_default, expected,
                        &copies))
        fail("  there is a reference for this conversion\n");

    void *ta_ctx = talloc_new(NULL);
    struct result r = run_conversion(ta_ctx, &in, 1, &out, &mix_default, true);
    if (!r.failed)
        fail("  the filter did not fail\n");
    talloc_free(ta_ctx);
}

// The reference itself, against values written down by hand.
static void check_reference(void)
{
    static const struct {
        const char *in;
        const char *out;
        double expected[6];
        const struct mix *mix; // NULL for mix_default
        bool mixed;
    } refs[] = {
        {"5.1", "stereo",
         {VAL(1) + M_SQRT1_2 * (VAL(3) + VAL(5)),
          VAL(2) + M_SQRT1_2 * (VAL(3) + VAL(6))}, .mixed = true},
        {"7.1", "stereo",
         {VAL(1) + M_SQRT1_2 * (VAL(3) + VAL(5) + VAL(7)),
          VAL(2) + M_SQRT1_2 * (VAL(3) + VAL(6) + VAL(8))}, .mixed = true},
        {"mono", "stereo", {M_SQRT1_2 * VAL(1), M_SQRT1_2 * VAL(1)},
         .mixed = true},
        {"stereo", "5.1", {VAL(1), VAL(2)}},
        {"5.1(side)", "5.1",
         {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6)}},
        {"5.1", "5.1(alsa)",
         {VAL(1), VAL(2), VAL(5), VAL(6), VAL(3), VAL(4)}},
        // Not their own inverse, so the direction of the mapping shows.
        {"fc-fl-fr-bl-br-lfe", "5.1",
         {VAL(2), VAL(3), VAL(1), VAL(6), VAL(4), VAL(5)}},
        {"5.1", "fc-fl-fr-bl-br-lfe",
         {VAL(3), VAL(1), VAL(2), VAL(5), VAL(6), VAL(4)}},
        {"fc-fl-fr-bl-br-lfe", "stereo",
         {VAL(2) + M_SQRT1_2 * (VAL(1) + VAL(4)),
          VAL(3) + M_SQRT1_2 * (VAL(1) + VAL(5))}, .mixed = true},
        {"5.1", "na-fl-fr-na", {0, VAL(1) + M_SQRT1_2 * (VAL(3) + VAL(5)),
                                VAL(2) + M_SQRT1_2 * (VAL(3) + VAL(6)), 0},
         .mixed = true},
        {"unknown9", "stereo", {VAL(1), VAL(2)}},
        {"stereo", "unknown6", {VAL(1), VAL(2)}},
        {"fl-fr-na", "fl-fr-na", {VAL(1), VAL(2), VAL(3)}},
        // The sum of the coefficients of a channel is limited to 1.
        {"5.1", "stereo",
         {(VAL(1) + M_SQRT1_2 * (VAL(3) + VAL(5))) / (1 + 2 * M_SQRT1_2),
          (VAL(2) + M_SQRT1_2 * (VAL(3) + VAL(6))) / (1 + 2 * M_SQRT1_2)},
         .mix = &mix_normalize, .mixed = true},
        // A single speaker is mono, whatever it is.
        {"fl-na", "stereo", {M_SQRT1_2 * VAL(1), M_SQRT1_2 * VAL(1)},
         .mixed = true},
        {"stereo", "na-fr", {0, M_SQRT1_2 * (VAL(1) + VAL(2))},
         .mixed = true},
        // The downmix pair is stereo.
        {"dr-dl", "stereo", {VAL(2), VAL(1)}},
        {"5.1", "dr-na-dl", {VAL(2) + M_SQRT1_2 * (VAL(3) + VAL(6)), 0,
                             VAL(1) + M_SQRT1_2 * (VAL(3) + VAL(5))},
         .mixed = true},
        // The same speakers need no mix, not even those that have none.
        {"bl-br", "br-na-bl", {VAL(2), 0, VAL(1)}},
        {"fc-na-fl", "fl-fc", {VAL(3), VAL(1)}},
        // A mix can leave every channel in its place.
        {"fc-bl-br", "fc-fl-fr",
         {VAL(1), M_SQRT1_2 * VAL(2), M_SQRT1_2 * VAL(3)}, .mixed = true},
        // Two channels can go into one at full level.
        {"7.1(wide)", "5.1",
         {VAL(1) + VAL(7), VAL(2) + VAL(8), VAL(3), VAL(4), VAL(5), VAL(6)},
         .mixed = true},
        // Every kind of channel has its level, and the volume is for all.
        {"5.1", "stereo",
         {0.5 * (VAL(1) + 0.5 * VAL(3) + 0.75 * M_SQRT1_2 * VAL(4) +
                 0.25 * VAL(5)),
          0.5 * (VAL(2) + 0.5 * VAL(3) + 0.75 * M_SQRT1_2 * VAL(4) +
                 0.25 * VAL(6))},
         .mix = &mix_levels, .mixed = true},
        {"5.1", "5.1(alsa)",
         {0.5 * VAL(1), 0.5 * VAL(2), 0.5 * VAL(5), 0.5 * VAL(6),
          0.5 * VAL(3), 0.5 * VAL(4)},
         .mix = &mix_levels, .mixed = true},
        {"unknown9", "stereo", {0.5 * VAL(1), 0.5 * VAL(2)},
         .mix = &mix_levels, .mixed = true},
        // The limit is for the mix, the volume comes on top of it.
        {"5.1", "stereo",
         {2 * (VAL(1) + M_SQRT1_2 * VAL(3) -
               M_SQRT1_2 * M_SQRT1_2 * (VAL(5) + VAL(6))) / (2 + M_SQRT1_2),
          2 * (VAL(2) + M_SQRT1_2 * VAL(3) +
               M_SQRT1_2 * M_SQRT1_2 * (VAL(5) + VAL(6))) / (2 + M_SQRT1_2)},
         .mix = &mix_dolby, .mixed = true},
        {"5.1", "5.1(alsa)",
         {2 * VAL(1), 2 * VAL(2), 2 * VAL(5), 2 * VAL(6), 2 * VAL(3),
          2 * VAL(4)},
         .mix = &mix_dolby, .mixed = true},
        // A negative volume takes the place of the sum of the coefficients.
        {"5.1", "stereo",
         {(VAL(1) + M_SQRT1_2 * (VAL(3) + VAL(5))) / 2,
          (VAL(2) + M_SQRT1_2 * (VAL(3) + VAL(6))) / 2},
         .mix = &mix_negative, .mixed = true},
        // A limit below 1 lowers channels that change their place, and leaves
        // those alone that keep their position.
        {"5.1", "5.1(alsa)",
         {0.5 * VAL(1), 0.5 * VAL(2), 0.5 * VAL(5), 0.5 * VAL(6),
          0.5 * VAL(3), 0.5 * VAL(4)},
         .mix = &mix_limit, .mixed = true},
        {"unknown9", "stereo", {VAL(1), VAL(2)}, .mix = &mix_limit},
        {"5.1", "5.1", {VAL(1), VAL(2), VAL(3), VAL(4), VAL(5), VAL(6)},
         .mix = &mix_limit},
    };

    for (int n = 0; n < MP_ARRAY_SIZE(refs); n++) {
        struct mp_chmap in = parse_map(refs[n].in);
        struct mp_chmap out = parse_map(refs[n].out);
        const struct mix *mix = refs[n].mix ? refs[n].mix : &mix_default;
        double expected[MP_NUM_CHANNELS];
        bool copies;
        assert_true(expected_output(&in, &out, mix, expected, &copies));
        if (copies == refs[n].mixed) {
            fail("reference %s -> %s is %s\n", refs[n].in, refs[n].out,
                 copies ? "not mixed" : "mixed");
        }
        assert_true(out.num <= MP_ARRAY_SIZE(refs[n].expected));
        for (int ch = 0; ch < out.num; ch++) {
            if (fabs(expected[ch] - refs[n].expected[ch]) > 1e-9) {
                fail("reference %s -> %s, channel %d: %f, expected %f\n",
                     refs[n].in, refs[n].out, ch, expected[ch],
                     refs[n].expected[ch]);
            }
        }
    }
}

#define MAP_5_1_4 "fl-fr-fc-lfe-bl-br-tfl-tfr-tbl-tbr"
#define MAP_7_1_4 "fl-fr-fc-lfe-bl-br-sl-sr-tfl-tfr-tbl-tbr"
#define MAP_9_1_4 "fl-fr-fc-lfe-bl-br-flc-frc-sl-sr-tfl-tfr-tbl-tbr"

struct conv_case {
    const char *in_map;
    int in_format;
    int in_rate;
    const char *out_map; // NULL keeps the input channel map
    int out_format;
    int out_rate;
    bool normalize;
};

static const struct conv_case cases[] = {
    // The same channel map on both sides.
    {"7.1", AF_FORMAT_FLOATP, 48000, NULL, AF_FORMAT_S16, 48000},
    {"2.1", AF_FORMAT_FLOATP, 48000, NULL, AF_FORMAT_S16, 48000},
    {"quad", AF_FORMAT_FLOAT, 48000, NULL, AF_FORMAT_S16, 48000},
    {"5.0", AF_FORMAT_S16, 48000, NULL, AF_FORMAT_FLOATP, 48000},
    {"6.1", AF_FORMAT_FLOATP, 48000, NULL, AF_FORMAT_S32, 48000},
    {"unknown8", AF_FORMAT_FLOATP, 48000, NULL, AF_FORMAT_S16, 48000},
    {"unknown9", AF_FORMAT_S16, 48000, NULL, AF_FORMAT_FLOATP, 48000},
    {"fl-fr-na", AF_FORMAT_FLOATP, 48000, NULL, AF_FORMAT_S16, 48000},
    {"fc-fl-fr-bl-br-lfe", AF_FORMAT_S16, 48000, NULL, AF_FORMAT_FLOAT, 48000},
    {"22.2", AF_FORMAT_S32, 48000, NULL, AF_FORMAT_S32P, 48000},
    {"7.1", AF_FORMAT_FLOATP, 48000, NULL, AF_FORMAT_FLOATP, 44100},

    // Unknown layouts.
    {"unknown8", AF_FORMAT_FLOATP, 48000, "7.1", AF_FORMAT_S16, 48000},
    {"7.1", AF_FORMAT_S16, 48000, "unknown8", AF_FORMAT_FLOATP, 48000},
    {"unknown9", AF_FORMAT_FLOATP, 48000, "stereo", AF_FORMAT_S16, 48000},
    {"stereo", AF_FORMAT_S16, 48000, "unknown9", AF_FORMAT_FLOATP, 48000},
    {"unknown16", AF_FORMAT_FLOATP, 48000, "unknown6", AF_FORMAT_FLOATP, 48000},
    {"unknown9", AF_FORMAT_S32, 48000, "stereo", AF_FORMAT_S32, 48000},
    {"unknown9", AF_FORMAT_S16, 48000, "stereo", AF_FORMAT_S16, 96000},

    // Reordering between the same speakers.
    {"5.1", AF_FORMAT_S16, 48000, "5.1(alsa)", AF_FORMAT_S16, 48000},
    {"5.1", AF_FORMAT_S32, 48000, "5.1(alsa)", AF_FORMAT_S32, 48000},
    {"5.1", AF_FORMAT_DOUBLEP, 48000, "5.1(alsa)", AF_FORMAT_DOUBLEP, 48000},
    {"7.1", AF_FORMAT_FLOATP, 48000, "7.1(alsa)", AF_FORMAT_S16, 48000},
    {"7.1(alsa)", AF_FORMAT_S16, 48000, "7.1", AF_FORMAT_FLOATP, 48000},
    {"7.1(alsa)", AF_FORMAT_S32P, 48000, "7.1", AF_FORMAT_S32P, 48000},
    {"fc-fl-fr-bl-br-lfe", AF_FORMAT_FLOATP, 48000,
     "5.1", AF_FORMAT_FLOATP, 48000},
    {"fc-fl-fr-bl-br-lfe", AF_FORMAT_S16, 48000, "5.1", AF_FORMAT_S16, 48000},
    {"5.1", AF_FORMAT_FLOATP, 48000,
     "fc-fl-fr-bl-br-lfe", AF_FORMAT_FLOATP, 48000},
    {"5.1", AF_FORMAT_S16, 48000, "fc-fl-fr-bl-br-lfe", AF_FORMAT_S16, 48000},
    {"fc-fl-fr-bl-br-lfe", AF_FORMAT_FLOAT, 48000,
     "lfe-fl-br-fc-bl-fr", AF_FORMAT_S16, 48000},
    {"fc-fl-fr-bl-br-lfe", AF_FORMAT_FLOATP, 48000,
     "5.1", AF_FORMAT_FLOATP, 44100},
    {"7.1", AF_FORMAT_FLOATP, 48000, "7.1(alsa)", AF_FORMAT_FLOATP, 44100},
    {"22.2", AF_FORMAT_S32, 48000,
     "bfr-bfl-bfc-tsr-tsl-lfe2-tbr-tbc-tbl-tfr-tfc-tfl-tc-sr-sl-bc-frc-flc-"
     "br-bl-lfe-fc-fr-fl", AF_FORMAT_S32, 48000},
    // libswresample has no mix for these speakers, and needs none.
    {"bl-br", AF_FORMAT_S32, 48000, "br-bl", AF_FORMAT_S32, 48000},
    {"fc-fl", AF_FORMAT_FLOATP, 48000, "fl-na-fc", AF_FORMAT_S16, 48000},
    {"dr-dl", AF_FORMAT_S16, 48000, "dl-dr", AF_FORMAT_S16, 48000},

    // NA channels in the output, as padded device layouts have them.
    {"5.1", AF_FORMAT_FLOATP, 48000,
     "fl-fr-fc-lfe-bl-br-na-na", AF_FORMAT_S16, 48000},
    {"5.1", AF_FORMAT_S32, 48000,
     "fl-fr-fc-lfe-bl-br-na-na", AF_FORMAT_S32, 48000},
    {"5.1", AF_FORMAT_FLOATP, 48000,
     "na-fc-fl-na-fr-bl-br-lfe", AF_FORMAT_FLOATP, 48000},
    {"fc-fl-fr-bl-br-lfe", AF_FORMAT_S16, 48000,
     "fl-na-fr-bl-br-fc-lfe-na", AF_FORMAT_S16, 48000},
    {"stereo", AF_FORMAT_S16, 48000, "fl-fr-na-na", AF_FORMAT_S16, 48000},
    {"stereo", AF_FORMAT_S32P, 48000, "fl-fr-na-na", AF_FORMAT_S32P, 48000},
    {"5.1", AF_FORMAT_FLOATP, 48000, "fl-fr-na-na", AF_FORMAT_FLOATP, 48000},
    {"5.1(side)", AF_FORMAT_FLOATP, 48000,
     "fl-fr-fc-lfe-bl-br-na-na", AF_FORMAT_S16, 48000},
    {"5.1", AF_FORMAT_FLOATP, 44100,
     "fl-fr-fc-lfe-bl-br-na-na", AF_FORMAT_S16, 48000},

    // Remixing.
    {"5.1(side)", AF_FORMAT_FLOATP, 48000, "5.1", AF_FORMAT_FLOATP, 48000},
    {"5.1", AF_FORMAT_FLOATP, 48000, "stereo", AF_FORMAT_FLOATP, 48000},
    {"7.1", AF_FORMAT_FLOATP, 48000, "stereo", AF_FORMAT_FLOATP, 48000},
    {"7.1", AF_FORMAT_S16, 48000, "stereo", AF_FORMAT_S16, 48000},
    {"stereo", AF_FORMAT_FLOATP, 48000, "5.1", AF_FORMAT_FLOATP, 48000},
    {"mono", AF_FORMAT_FLOATP, 48000, "stereo", AF_FORMAT_FLOATP, 48000},
    {"fc-fl-fr-bl-br-lfe", AF_FORMAT_S16, 48000,
     "stereo", AF_FORMAT_FLOATP, 48000},
    {"stereo", AF_FORMAT_FLOATP, 48000,
     "fc-fl-fr-bl-br-lfe", AF_FORMAT_S16, 48000},
    {"fc-fl-fr-bl-br-lfe", AF_FORMAT_FLOATP, 48000,
     "fr-na-fl", AF_FORMAT_FLOATP, 44100},
    // Every channel stays in its place.
    {"fc-bl-br", AF_FORMAT_FLOATP, 48000, "fc-fl-fr", AF_FORMAT_S16, 48000},
    {"fc-bl-br", AF_FORMAT_S32, 48000, "fc-fl-fr", AF_FORMAT_S32, 48000},
    {"fc-sl-sr", AF_FORMAT_S16, 48000, "fc-fl-fr", AF_FORMAT_FLOATP, 44100},
    // Two channels go into one at full level.
    {"7.1(wide)", AF_FORMAT_S32, 48000, "5.1", AF_FORMAT_S32, 48000},

    // With the sum of the coefficients limited.
    {"5.1", AF_FORMAT_FLOATP, 48000, "stereo", AF_FORMAT_FLOATP, 48000, true},
    {"7.1", AF_FORMAT_S16, 48000, "stereo", AF_FORMAT_S16, 48000, true},
    {"fc-fl-fr-bl-br-lfe", AF_FORMAT_FLOAT, 48000,
     "fr-na-fl", AF_FORMAT_S32, 48000, true},
    {"22.2", AF_FORMAT_FLOATP, 48000, "5.1(alsa)", AF_FORMAT_S16, 48000, true},
    {"5.1", AF_FORMAT_S32, 48000, "5.1(alsa)", AF_FORMAT_S32, 48000, true},

    // More speakers than 7.1 has.
    {"7.1(top)", AF_FORMAT_FLOATP, 48000, "5.1", AF_FORMAT_FLOATP, 48000},
    {MAP_5_1_4, AF_FORMAT_FLOATP, 48000, "stereo", AF_FORMAT_S16, 48000},
    {MAP_7_1_4, AF_FORMAT_FLOATP, 48000, "7.1(alsa)", AF_FORMAT_S32, 48000},
    {MAP_7_1_4, AF_FORMAT_S16, 48000, MAP_5_1_4, AF_FORMAT_FLOATP, 48000},
    {MAP_9_1_4, AF_FORMAT_FLOATP, 48000,
     "fl-fr-fc-lfe-bl-br-na-na", AF_FORMAT_S16, 48000},
    {"5.1", AF_FORMAT_FLOATP, 48000, MAP_7_1_4, AF_FORMAT_FLOATP, 48000},
    {"cube", AF_FORMAT_FLOATP, 48000, "quad", AF_FORMAT_FLOATP, 48000},
    {"hexadecagonal", AF_FORMAT_FLOATP, 48000, "7.1", AF_FORMAT_S16, 48000},
    {"22.2", AF_FORMAT_FLOATP, 48000, "5.1", AF_FORMAT_FLOATP, 48000},
    {"22.2", AF_FORMAT_S16, 48000, MAP_7_1_4, AF_FORMAT_S16, 44100},

    // A single speaker, and the downmix pair.
    {"fl-na", AF_FORMAT_FLOATP, 48000, "stereo", AF_FORMAT_FLOATP, 48000},
    {"na-bl-na", AF_FORMAT_S16, 48000, "5.1", AF_FORMAT_S16, 48000},
    {"stereo", AF_FORMAT_FLOATP, 48000, "na-fr", AF_FORMAT_FLOATP, 48000},
    {"5.1", AF_FORMAT_FLOATP, 48000, "lfe-na-na-na", AF_FORMAT_S16, 48000},
    {"sl", AF_FORMAT_FLOATP, 48000, "na-fl-fr", AF_FORMAT_FLOATP, 48000},
    {"dr-dl", AF_FORMAT_FLOATP, 48000, "stereo", AF_FORMAT_FLOATP, 48000},
    {"dl-dr-na", AF_FORMAT_S16, 48000, "5.1(alsa)", AF_FORMAT_S16, 48000},
    {"5.1", AF_FORMAT_FLOATP, 48000, "dr-na-dl", AF_FORMAT_FLOATP, 48000},
    {"7.1", AF_FORMAT_FLOATP, 48000, "dl-dr", AF_FORMAT_S16, 44100},

    // NA channels in the input.
    {"fl-fr-na", AF_FORMAT_FLOATP, 48000, "stereo", AF_FORMAT_S16, 48000},
    {"na-fl-fr", AF_FORMAT_S16, 48000, "stereo", AF_FORMAT_S16, 48000},
    {"fl-fr-na-na", AF_FORMAT_S32, 48000, "stereo", AF_FORMAT_S32, 48000},
    {"fc-na-fl-fr-bl-br-lfe", AF_FORMAT_S16, 48000,
     "5.1(alsa)", AF_FORMAT_FLOATP, 48000},
};

static void check_case(const struct conv_case *c)
{
    struct spec in = {parse_map(c->in_map), c->in_format, c->in_rate};
    struct spec out = {in.map, c->out_format, c->out_rate};
    if (c->out_map)
        out.map = parse_map(c->out_map);

    const struct mix *mix = c->normalize ? &mix_normalize : &mix_default;

    if (in.rate != out.rate) {
        check_conversion(&(struct input){in, 4000}, 1, &out, mix);
        return;
    }

    // Several frames, and none of them matches the size of the output frames.
    struct input frames[] = {{in, 1500}, {in, 7}, {in, 2493}};
    check_conversion(frames, MP_ARRAY_SIZE(frames), &out, mix);
}

static unsigned rnd(unsigned *state)
{
    *state = *state * 1103515245u + 12345u;
    return (*state >> 16) & 0x7fff;
}

static void shuffle(struct mp_chmap *map, unsigned *state)
{
    for (int n = map->num - 1; n > 0; n--) {
        int i = rnd(state) % (n + 1);
        MPSWAP(uint8_t, map->speaker[n], map->speaker[i]);
    }
}

static void pad_na(struct mp_chmap *map, unsigned *state)
{
    int count = 1 + rnd(state) % 2;
    for (int n = 0; n < count; n++) {
        int pos = rnd(state) % (map->num + 1);
        memmove(&map->speaker[pos + 1], &map->speaker[pos], map->num - pos);
        map->speaker[pos] = MP_SPEAKER_ID_NA;
        map->num++;
    }
}

// Every pair of these layouts, with the channels of both sides in a random
// order, without NA channels, with some in the output, and with some on both
// sides.
static void check_shuffled(const struct mix *mix)
{
    static const char *const layouts[] = {
        "mono", "stereo", "2.1", "3.0", "quad", "5.0", "5.1", "5.1(side)",
        "6.1", "7.1", "7.1(top)", "cube", MAP_7_1_4, "hexadecagonal", "22.2",
        "sl", "dl-dr",
    };
    static const int formats[] = {
        AF_FORMAT_S16, AF_FORMAT_FLOATP, AF_FORMAT_S32, AF_FORMAT_FLOAT,
        AF_FORMAT_S16P, AF_FORMAT_DOUBLEP,
    };

    unsigned state = 1;
    for (int i = 0; i < MP_ARRAY_SIZE(layouts); i++) {
        for (int o = 0; o < MP_ARRAY_SIZE(layouts); o++) {
            for (int na = 0; na < 3; na++) {
                struct input in = {
                    .spec = {
                        .map = parse_map(layouts[i]),
                        .format = formats[rnd(&state) % MP_ARRAY_SIZE(formats)],
                        .rate = 48000,
                    },
                    .samples = 3000,
                };
                struct spec out = {
                    .map = parse_map(layouts[o]),
                    .format = formats[rnd(&state) % MP_ARRAY_SIZE(formats)],
                    .rate = 48000,
                };
                shuffle(&in.spec.map, &state);
                shuffle(&out.map, &state);
                if (na > 0)
                    pad_na(&out.map, &state);
                if (na > 1)
                    pad_na(&in.spec.map, &state);
                check_conversion(&in, 1, &out, mix);
            }
        }
    }
}

// As many channels as a map can have.
static void check_wide(void)
{
    static const char *const maps[][2] = {
        {"stereo", "stereo"},
        {"5.1", "stereo"},
        {"5.1", "7.1(alsa)"},
        {"fc-fl-fr-bl-br-lfe", "5.1"},
        {"22.2", "5.1"},
    };
    static const int formats[] = {AF_FORMAT_FLOATP, AF_FORMAT_S16};

    for (int n = 0; n < MP_ARRAY_SIZE(maps); n++) {
        for (int f = 0; f < MP_ARRAY_SIZE(formats); f++) {
            struct spec narrow_in = {parse_map(maps[n][0]), formats[f], 48000};
            struct spec narrow_out = {parse_map(maps[n][1]), formats[f], 48000};
            struct spec wide_in = narrow_in;
            struct spec wide_out = narrow_out;
            wide_in.map = parse_padded_map(maps[n][0], MP_NUM_CHANNELS);
            wide_out.map = parse_padded_map(maps[n][1], MP_NUM_CHANNELS);

            check_conversion(&(struct input){narrow_in, 3000}, 1, &wide_out,
                             &mix_default);
            check_conversion(&(struct input){wide_in, 3000}, 1, &narrow_out,
                             &mix_default);
            check_conversion(&(struct input){wide_in, 3000}, 1, &wide_out,
                             &mix_default);
        }
    }

    struct spec unknown = {.format = AF_FORMAT_S32, .rate = 48000};
    mp_chmap_set_unknown(&unknown.map, MP_NUM_CHANNELS);
    struct spec stereo = {parse_map("stereo"), AF_FORMAT_S32, 48000};
    struct spec surround = {parse_map("7.1"), AF_FORMAT_S32, 48000};
    const struct mix *mix = &mix_default;
    check_conversion(&(struct input){unknown, 3000}, 1, &unknown, mix);
    check_conversion(&(struct input){unknown, 3000}, 1, &stereo, mix);
    check_conversion(&(struct input){unknown, 3000}, 1, &surround, mix);
    check_conversion(&(struct input){stereo, 3000}, 1, &unknown, mix);
}

// The input format changes while the filter is running.
static void check_reconfigure(void)
{
    struct input frames[] = {
        {{parse_map("5.1"), AF_FORMAT_FLOATP, 48000}, 1000},
        {{parse_map("fc-fl-fr-bl-br-lfe"), AF_FORMAT_S16, 48000}, 777},
        {{parse_map("stereo"), AF_FORMAT_FLOAT, 48000}, 2500},
        {{parse_map("7.1(alsa)"), AF_FORMAT_S16, 48000}, 300},
        {{parse_map("unknown8"), AF_FORMAT_S32, 48000}, 1921},
        {{parse_map("sl-na"), AF_FORMAT_FLOATP, 48000}, 40},
        {{parse_map(MAP_7_1_4), AF_FORMAT_FLOATP, 48000}, 2000},
        {{parse_map("lfe-br-bl-fc-fr-fl"), AF_FORMAT_S16, 48000}, 1234},
    };
    struct spec out = {
        parse_map("fl-fr-na-bl-br-fc-lfe-na"), AF_FORMAT_S16, 48000,
    };
    check_conversion(frames, MP_ARRAY_SIZE(frames), &out, &mix_default);

    // Only the channels change, and copies keep their samples.
    for (int n = 0; n < MP_ARRAY_SIZE(frames); n++)
        frames[n].spec.format = AF_FORMAT_S32;
    out.format = AF_FORMAT_S32;
    check_conversion(frames, MP_ARRAY_SIZE(frames), &out, &mix_default);
}

static const char *const stream_maps[][2] = {
    {"fc-fl-fr-bl-br-lfe", "5.1"},
    {"5.1", "fl-na-fr-bl-br-fc-lfe-na"},
    {"fc-fl-fr-bl-br-lfe", "stereo"},
    {"7.1", "7.1"},
    {"unknown9", "stereo"},
    {"dr-na-dl", "fl-fr-na-na"},
};

// Copies of s32 take another way through libswresample than all others.
static const int stream_formats[][2] = {
    {AF_FORMAT_FLOATP, AF_FORMAT_FLOATP},
    {AF_FORMAT_S16, AF_FORMAT_FLOATP},
    {AF_FORMAT_S32, AF_FORMAT_S32},
};

// A reset must not change what the filter does with the channels. For
// libswresample this is swr_close() and swr_init() on the same context.
static void check_reset(void)
{
    for (int n = 0; n < MP_ARRAY_SIZE(stream_maps); n++) {
        for (int f = 0; f < MP_ARRAY_SIZE(stream_formats); f++) {
            struct spec in = {
                parse_map(stream_maps[n][0]), stream_formats[f][0], 48000,
            };
            struct spec out = {
                parse_map(stream_maps[n][1]), stream_formats[f][1], 48000,
            };
            struct input frames[] = {{in, 2500}, {in, 1500, .reset = true}};
            check_conversion(frames, MP_ARRAY_SIZE(frames), &out,
                             &mix_default);
        }
    }
}

// Neither must a change of the playback speed. A small one makes
// libswresample initialize its context again to compensate, a large one makes
// the filter set up a new context.
static void check_speed(void)
{
    static const double speeds[][2] = {
        {1, 1.005}, {1, 0.995}, {1, 1.5}, {1.25, 1}, {2, 2.01}, {0.5, 0},
    };

    for (int n = 0; n < MP_ARRAY_SIZE(stream_maps); n++) {
        for (int f = 0; f < MP_ARRAY_SIZE(stream_formats); f++) {
            struct spec in = {
                parse_map(stream_maps[n][0]), stream_formats[f][0], 48000,
            };
            struct spec out = {
                parse_map(stream_maps[n][1]), stream_formats[f][1], 48000,
            };
            for (int s = 0; s < MP_ARRAY_SIZE(speeds); s++) {
                // The speed changes early, what is checked comes later.
                struct input frames[] = {
                    {in, 600, .speed = speeds[s][0]},
                    {in, 9000, .speed = speeds[s][1]},
                    {in, 3000},
                };
                check_conversion(frames, MP_ARRAY_SIZE(frames), &out,
                                 &mix_default);
            }
        }
    }
}

struct opt_case {
    const char *in_map;
    int in_format;
    const char *out_map;
    int out_format;
    const struct mix *mix;
};

static const struct opt_case opt_cases[] = {
    // A volume where channels keep their position. libswresample knows no
    // layout of 9 channels that it could mix on its own.
    {"unknown9", AF_FORMAT_FLOATP, "stereo", AF_FORMAT_FLOATP, &mix_half},
    {"stereo", AF_FORMAT_S32, "unknown9", AF_FORMAT_S32, &mix_half},
    {"unknown9", AF_FORMAT_S32, "unknown9", AF_FORMAT_S32, &mix_half},
    {"7.1", AF_FORMAT_FLOATP, "7.1", AF_FORMAT_S16, &mix_half},

    {"5.1", AF_FORMAT_FLOATP, "stereo", AF_FORMAT_FLOATP, &mix_negative},
    {"5.1", AF_FORMAT_FLOATP, "5.1(alsa)", AF_FORMAT_S16, &mix_negative},
    {"fc-fl-fr-bl-br-lfe", AF_FORMAT_S32, "5.1", AF_FORMAT_S32, &mix_negative},

    // The level stays, so copies keep their samples.
    {"unknown9", AF_FORMAT_S32, "unknown9", AF_FORMAT_S32, &mix_unity},
    {"stereo", AF_FORMAT_S32, "unknown9", AF_FORMAT_S32, &mix_unity},
    {"5.1", AF_FORMAT_S32, "5.1(alsa)", AF_FORMAT_S32, &mix_unity},
    {"5.1(side)", AF_FORMAT_S32, "5.1", AF_FORMAT_S32, &mix_unity},

    // The limit that libswresample picks for the sample formats.
    {"5.1", AF_FORMAT_FLOATP, "stereo", AF_FORMAT_FLOATP, &mix_auto_float},
    {"5.1", AF_FORMAT_S16, "stereo", AF_FORMAT_DOUBLEP, &mix_auto_float},
    {"5.1", AF_FORMAT_FLOATP, "stereo", AF_FORMAT_S16, &mix_auto_integer},
    {"5.1", AF_FORMAT_S32, "stereo", AF_FORMAT_S32, &mix_auto_integer},
    {"5.1", AF_FORMAT_S16, "stereo", AF_FORMAT_FLOATP, &mix_auto_s16p},
    {"5.1", AF_FORMAT_S16, "stereo", AF_FORMAT_S16, &mix_auto_s16p},
    {"5.1", AF_FORMAT_S16, "stereo", AF_FORMAT_FLOATP, &mix_auto_fltp_float},
    {"5.1", AF_FORMAT_S16, "stereo", AF_FORMAT_S16, &mix_auto_fltp_integer},

    // Without a volume, channels that keep their position are left alone.
    {"5.1", AF_FORMAT_FLOATP, "stereo", AF_FORMAT_FLOATP, &mix_limit},
    {"5.1", AF_FORMAT_FLOATP, "5.1(alsa)", AF_FORMAT_FLOATP, &mix_limit},
    {"fc-fl-fr-bl-br-lfe", AF_FORMAT_S32, "5.1", AF_FORMAT_S32, &mix_limit},
    {"unknown9", AF_FORMAT_S32, "stereo", AF_FORMAT_S32, &mix_limit},
    {"7.1", AF_FORMAT_S32, "7.1", AF_FORMAT_S32, &mix_limit},
    {"5.1", AF_FORMAT_FLOATP, "stereo", AF_FORMAT_FLOATP, &mix_limit_half},
    {"5.1", AF_FORMAT_FLOATP, "5.1(alsa)", AF_FORMAT_FLOATP, &mix_limit_half},
    {"unknown9", AF_FORMAT_S32, "stereo", AF_FORMAT_S32, &mix_limit_half},
};

// The options of libswresample that change what comes out.
static void check_options(void)
{
    check_shuffled(&mix_levels);
    check_shuffled(&mix_dolby);

    for (int n = 0; n < MP_ARRAY_SIZE(opt_cases); n++) {
        const struct opt_case *c = &opt_cases[n];
        struct spec in = {parse_map(c->in_map), c->in_format, 48000};
        struct spec out = {parse_map(c->out_map), c->out_format, 48000};
        struct input frames[] = {{in, 1500}, {in, 7}, {in, 2493}};
        check_conversion(frames, MP_ARRAY_SIZE(frames), &out, c->mix);

        // Neither a reset nor a change of the speed must change them.
        struct input reset[] = {{in, 2500}, {in, 1500, .reset = true}};
        check_conversion(reset, MP_ARRAY_SIZE(reset), &out, c->mix);
        struct input speed[] = {
            {in, 600, .speed = 1}, {in, 9000, .speed = 1.005}, {in, 3000},
        };
        check_conversion(speed, MP_ARRAY_SIZE(speed), &out, c->mix);
    }
}

int main(void)
{
    unsigned lib = swresample_version();
    printf("libswresample %u.%u.%u, built for %u.%u.%u\n",
           AV_VERSION_MAJOR(lib), AV_VERSION_MINOR(lib), AV_VERSION_MICRO(lib),
           LIBSWRESAMPLE_VERSION_MAJOR, LIBSWRESAMPLE_VERSION_MINOR,
           LIBSWRESAMPLE_VERSION_MICRO);

    check_reference();
    for (int n = 0; n < MP_ARRAY_SIZE(cases); n++)
        check_case(&cases[n]);
    check_shuffled(&mix_default);
    check_wide();
    check_reconfigure();
    check_reset();
    check_speed();
    check_options();

    // Speakers that libswresample has no mix for.
    check_unsupported("fl-fc", "stereo");
    check_unsupported("bl-br", "stereo");
    check_unsupported("stereo", "sl-sr-na");
    check_unsupported("dr-dl", "dl-dr-fc");
    return 0;
}
