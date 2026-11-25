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

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/opt.h>

#include "audio/aframe.h"
#include "audio/chmap.h"
#include "audio/format.h"
#include "audio/out/ao.h"
#include "audio/out/internal.h"
#include "common/av_common.h"
#include "common/common.h"
#include "common/global.h"
#include "demux/packet.h"
#include "demux/stheader.h"
#include "filters/f_async_queue.h"
#include "filters/f_decoder_wrapper.h"
#include "filters/filter.h"
#include "filters/frame.h"
#include "osdep/threads.h"
#include "video/hwdec.h"
#include "test_utils.h"

static const uint8_t mat_start_code[20] = {
    0x07, 0x9E, 0x00, 0x03, 0x84, 0x01, 0x01, 0x01,
    0x80, 0x00, 0x56, 0xA5, 0x3B, 0xF4, 0x81, 0x83,
    0x49, 0x80, 0x77, 0xE0,
};

static const uint8_t mat_middle_code[12] = {
    0xC3, 0xC1, 0x42, 0x49, 0x3B, 0xFA, 0x82, 0x83,
    0x49, 0x80, 0x77, 0xE0,
};

static const uint8_t mat_end_code[16] = {
    0xC3, 0xC2, 0xC0, 0xC4, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x97, 0x11,
};

static const uint8_t expected_pause_burst[16] = {
    0x72, 0xF8, 0x1F, 0x4E, 0x03, 0x00, 0x20, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void ao_post_process_data(struct ao *ao, void **data, int num_samples)
{
}

bool ao_need_conversion(struct ao_convert_fmt *fmt)
{
    return false;
}

void ao_convert_inplace(struct ao_convert_fmt *fmt, void **data, int num_samples)
{
    abort();
}

int64_t mp_pts_to_av(double mp_pts, AVRational *tb)
{
    return AV_NOPTS_VALUE;
}

double mp_pts_from_av(int64_t av_pts, AVRational *tb)
{
    return MP_NOPTS_VALUE;
}

void mp_set_av_packet(AVPacket *dst, struct demux_packet *mpkt, AVRational *tb)
{
    dst->side_data = NULL;
    dst->side_data_elems = 0;
    dst->buf = NULL;
    av_packet_unref(dst);
    dst->data = mpkt ? mpkt->buffer : NULL;
    dst->size = mpkt ? mpkt->len : 0;
}

void mp_free_av_packet(AVPacket **pkt)
{
    if (*pkt) {
        (*pkt)->side_data = NULL;
        (*pkt)->side_data_elems = 0;
        (*pkt)->buf = NULL;
    }
    av_packet_free(pkt);
}

int mp_codec_to_av_codec_id(const char *codec)
{
    const AVCodecDescriptor *desc = codec ? avcodec_descriptor_get_by_name(codec) : NULL;
    return desc ? desc->id : AV_CODEC_ID_NONE;
}

const char *mp_codec_from_av_codec_id(int codec_id)
{
    const AVCodecDescriptor *desc = avcodec_descriptor_get(codec_id);
    return desc ? desc->name : NULL;
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

static struct demux_packet *make_truehd_au(void *ta_parent, int size_bytes,
                                           uint16_t input_timing,
                                           bool major_sync, uint8_t ratebits,
                                           double pts)
{
    static const uint8_t major_sync_hdr[28] = {
        0xf8, 0x72, 0x6f, 0xba, 0x00, 0x07, 0x80, 0x0f,
        0xb7, 0x52, 0x00, 0x00, 0x00, 0x00, 0x8c, 0x7f,
        0x20, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xe4, 0xd1,
    };

    assert_true(size_bytes >= 32 && (size_bytes % 2) == 0);
    struct demux_packet *pkt = new_demux_packet(NULL, size_bytes);
    assert_true(pkt);
    talloc_steal(ta_parent, pkt);
    memset(pkt->buffer, 0x11, size_bytes);

    uint16_t words = (uint16_t)((size_bytes / 2) & 0x0fff);
    AV_WB16(pkt->buffer + 0, words);
    AV_WB16(pkt->buffer + 2, input_timing);
    if (major_sync) {
        memcpy(pkt->buffer + 4, major_sync_hdr, sizeof(major_sync_hdr));
        pkt->buffer[8] = (uint8_t)((ratebits << 4) | (pkt->buffer[8] & 0x0f));
    } else {
        pkt->buffer[4] = 0x60;
        pkt->buffer[5] = 0x05;
        pkt->buffer[6] = 0x60;
        pkt->buffer[7] = 0x07;
    }
    pkt->pts = pts;
    pkt->dts = pts;
    pkt->duration = 1.0 / 1200.0;
    return pkt;
}

static void test_truehd_pts_pacing(void)
{
    void *ctx = talloc_new(NULL);
    struct mpv_global global = {0};
    struct mp_filter *root = talloc_steal(ctx, mp_filter_create_root(&global));
    assert_true(root);

    struct mp_codec_params *codec = talloc_zero(ctx, struct mp_codec_params);
    codec->type = STREAM_AUDIO;
    codec->codec = "truehd";
    mp_chmap_from_channels(&codec->channels, 8);
    codec->samplerate = 48000;

    struct mp_decoder *dec = ad_spdif.create(root, codec, "spdif_truehd");
    assert_true(dec && dec->f);
    struct mp_filter *f = dec->f;

    // Exercise quiet spacing, VBR pre-buffering (delta=24), and peak frames (delta=68).
    const int num_aus = 300;
    struct demux_packet **pkts = talloc_array(ctx, struct demux_packet *, num_aus);
    uint16_t timing = 16000;

    for (int i = 0; i < num_aus; i++) {
        bool major = (i % 120) == 0;
        int sz;
        uint16_t next_delta;

        if (i == 0) {
            sz = 314;
            next_delta = 40;
        } else if (i < 80) {
            sz = 160;
            next_delta = 40;
        } else if (i < 160) {
            sz = 380;
            next_delta = 24;
        } else if (i < 220) {
            sz = 1712;
            next_delta = 68;
        } else {
            sz = 200;
            next_delta = 40;
        }

        double pts = (double)i / 1200.0;
        pkts[i] = make_truehd_au(ctx, sz, timing, major, 0, pts);
        timing = (uint16_t)(timing + next_delta);
    }

    int in_idx = 0;
    int zero_sample_frames = 0;
    int mat_frames = 0;

    for (int iter = 0; iter < 5000 && in_idx < num_aus; iter++) {
        assert_false(mp_filter_has_failed(f));

        if (mp_pin_in_needs_data(f->pins[0]) && in_idx < num_aus) {
            struct demux_packet *p = pkts[in_idx++];
            pkts[in_idx - 1] = NULL;
            assert_true(mp_pin_in_write(f->pins[0],
                                        MAKE_FRAME(MP_FRAME_PACKET, p)));
        }

        while (mp_pin_out_request_data(f->pins[1])) {
            struct mp_frame out = mp_pin_out_read(f->pins[1]);
            if (out.type == MP_FRAME_NONE)
                break;
            assert_int_equal(out.type, MP_FRAME_AUDIO);
            struct mp_aframe *af = out.data;
            int samples = mp_aframe_get_size(af);
            if (samples == 0) {
                zero_sample_frames++;
            } else {
                assert_int_equal(samples, 3840);
                assert_int_equal(mp_aframe_get_format(af), AF_FORMAT_S_TRUEHD);
                assert_int_equal(mp_aframe_get_rate(af), 192000);
                double got_pts = mp_aframe_get_pts(af);
                assert_float_equal(got_pts, mat_frames * 0.020, 1e-6);
                mat_frames++;
            }
            mp_frame_unref(&out);
        }
    }

    assert_int_equal(in_idx, num_aus);
    assert_int_equal(zero_sample_frames, 0);
    assert_true(mat_frames >= 10);

    // Seek reset followed by two pre-major-sync packets before pts = 10.0s.
    mp_filter_reset(f);

    const int seek_aus = 54;
    struct demux_packet **seek_pkts = talloc_array(ctx, struct demux_packet *, seek_aus);
    uint16_t seek_timing = 32000;
    seek_pkts[0] = make_truehd_au(ctx, 160, seek_timing, false, 0, 9.998333);
    seek_pkts[1] = make_truehd_au(ctx, 160, seek_timing + 40, false, 0, 9.999167);
    seek_timing += 80;
    for (int i = 2; i < seek_aus; i++) {
        bool major = (i == 2);
        double pts = 10.0 + (double)(i - 2) / 1200.0;
        seek_pkts[i] = make_truehd_au(ctx, major ? 314 : 160, seek_timing,
                                      major, 0, pts);
        seek_timing = (uint16_t)(seek_timing + 40);
    }

    in_idx = 0;
    int seek_mat_frames = 0;
    for (int iter = 0; iter < 1000 && in_idx < seek_aus; iter++) {
        assert_false(mp_filter_has_failed(f));

        if (mp_pin_in_needs_data(f->pins[0]) && in_idx < seek_aus) {
            struct demux_packet *p = seek_pkts[in_idx++];
            seek_pkts[in_idx - 1] = NULL;
            assert_true(mp_pin_in_write(f->pins[0],
                                        MAKE_FRAME(MP_FRAME_PACKET, p)));
        }

        while (mp_pin_out_request_data(f->pins[1])) {
            struct mp_frame out = mp_pin_out_read(f->pins[1]);
            if (out.type == MP_FRAME_NONE)
                break;
            assert_int_equal(out.type, MP_FRAME_AUDIO);
            struct mp_aframe *af = out.data;
            assert_int_equal(mp_aframe_get_size(af), 3840);
            double got_pts = mp_aframe_get_pts(af);
            assert_float_equal(got_pts, 10.0 + seek_mat_frames * 0.020, 1e-6);
            seek_mat_frames++;
            mp_frame_unref(&out);
        }
    }

    assert_int_equal(in_idx, seek_aus);
    assert_true(seek_mat_frames >= 2);

    // Timestamp discontinuity > 250 ms without filter reset re-anchors next_pts.
    const int jump_aus = 80;
    struct demux_packet **jump_pkts = talloc_array(ctx, struct demux_packet *, jump_aus);
    for (int i = 0; i < jump_aus; i++) {
        double pts = 20.0 + (double)i / 1200.0;
        jump_pkts[i] = make_truehd_au(ctx, 160, seek_timing, false, 0, pts);
        seek_timing = (uint16_t)(seek_timing + 40);
    }

    in_idx = 0;
    double jump_anchor = MP_NOPTS_VALUE;
    int jump_mat_frames = 0;
    for (int iter = 0; iter < 1000 && in_idx < jump_aus; iter++) {
        assert_false(mp_filter_has_failed(f));

        if (mp_pin_in_needs_data(f->pins[0]) && in_idx < jump_aus) {
            struct demux_packet *p = jump_pkts[in_idx++];
            jump_pkts[in_idx - 1] = NULL;
            assert_true(mp_pin_in_write(f->pins[0],
                                        MAKE_FRAME(MP_FRAME_PACKET, p)));
        }

        while (mp_pin_out_request_data(f->pins[1])) {
            struct mp_frame out = mp_pin_out_read(f->pins[1]);
            if (out.type == MP_FRAME_NONE)
                break;
            assert_int_equal(out.type, MP_FRAME_AUDIO);
            struct mp_aframe *af = out.data;
            assert_int_equal(mp_aframe_get_size(af), 3840);
            double got_pts = mp_aframe_get_pts(af);
            if (got_pts >= 20.0) {
                if (jump_anchor == MP_NOPTS_VALUE)
                    jump_anchor = got_pts;
                assert_float_equal(got_pts, jump_anchor + jump_mat_frames * 0.020, 1e-6);
                jump_mat_frames++;
            }
            mp_frame_unref(&out);
        }
    }

    assert_int_equal(in_idx, jump_aus);
    assert_true(jump_mat_frames >= 2);

    talloc_free(ctx);
}

static void test_truehd_44k_family(void)
{
    void *ctx = talloc_new(NULL);
    struct mpv_global global = {0};
    struct mp_filter *root = talloc_steal(ctx, mp_filter_create_root(&global));
    assert_true(root);

    struct mp_codec_params *codec = talloc_zero(ctx, struct mp_codec_params);
    codec->type = STREAM_AUDIO;
    codec->codec = "truehd";
    mp_chmap_from_channels(&codec->channels, 8);
    codec->samplerate = 44100;

    struct mp_decoder *dec = ad_spdif.create(root, codec, "spdif_truehd");
    assert_true(dec && dec->f);
    struct mp_filter *f = dec->f;

    const int num_aus = 60;
    struct demux_packet **pkts = talloc_array(ctx, struct demux_packet *, num_aus);
    uint16_t timing = 0;
    for (int i = 0; i < num_aus; i++) {
        double pts = (double)i * 40.0 / 44100.0;
        pkts[i] = make_truehd_au(ctx, i == 0 ? 314 : 160, timing, i == 0, 8, pts);
        timing = (uint16_t)(timing + 40);
    }

    int in_idx = 0;
    int mat_frames = 0;
    for (int iter = 0; iter < 1000 && in_idx < num_aus; iter++) {
        assert_false(mp_filter_has_failed(f));

        if (mp_pin_in_needs_data(f->pins[0]) && in_idx < num_aus) {
            struct demux_packet *p = pkts[in_idx++];
            pkts[in_idx - 1] = NULL;
            assert_true(mp_pin_in_write(f->pins[0],
                                        MAKE_FRAME(MP_FRAME_PACKET, p)));
        }

        while (mp_pin_out_request_data(f->pins[1])) {
            struct mp_frame out = mp_pin_out_read(f->pins[1]);
            if (out.type == MP_FRAME_NONE)
                break;
            assert_int_equal(out.type, MP_FRAME_AUDIO);
            struct mp_aframe *af = out.data;
            assert_int_equal(mp_aframe_get_size(af), 3840);
            assert_int_equal(mp_aframe_get_format(af), AF_FORMAT_S_TRUEHD);
            assert_int_equal(mp_aframe_get_rate(af), 176400);
            double expected = mat_frames * (3840.0 / 176400.0);
            assert_float_equal(mp_aframe_get_pts(af), expected, 1e-6);
            mat_frames++;
            mp_frame_unref(&out);
        }
    }

    assert_int_equal(in_idx, num_aus);
    assert_true(mat_frames >= 2);
    talloc_free(ctx);
}

static void test_spdif_pause_silence(void)
{
    const int hbr_fmts[] = { AF_FORMAT_S_TRUEHD, AF_FORMAT_S_DTSHD };
    const int twoch_fmts[] = { AF_FORMAT_S_AC3, AF_FORMAT_S_EAC3 };

    size_t test_size = (512 * 16) + 7;
    uint8_t *buf = malloc(test_size);
    assert_true(buf);

    for (size_t i = 0; i < MP_ARRAY_SIZE(hbr_fmts); i++) {
        memset(buf, 0xaa, test_size);
        af_fill_silence(buf, test_size, hbr_fmts[i]);
        for (int k = 0; k < 512; k++)
            assert_int_equal(memcmp(buf + k * 16, expected_pause_burst, 16), 0);
        for (size_t rem = 512 * 16; rem < test_size; rem++)
            assert_int_equal(buf[rem], 0x00);
    }

    for (size_t i = 0; i < MP_ARRAY_SIZE(twoch_fmts); i++) {
        memset(buf, 0xaa, test_size);
        af_fill_silence(buf, test_size, twoch_fmts[i]);
        for (int k = 0; k < 64; k++) {
            assert_int_equal(memcmp(buf + k * 128, expected_pause_burst, 16), 0);
            for (int z = 16; z < 128; z++)
                assert_int_equal(buf[k * 128 + z], 0x00);
        }
        for (size_t rem = 64 * 128; rem < test_size; rem++)
            assert_int_equal(buf[rem], 0x00);
    }

    memset(buf, 0xaa, test_size);
    af_fill_silence(buf, test_size, AF_FORMAT_S16);
    for (size_t j = 0; j < test_size; j++)
        assert_int_equal(buf[j], 0x00);

    memset(buf, 0xaa, test_size);
    af_fill_silence(buf, test_size, AF_FORMAT_U8);
    for (size_t j = 0; j < test_size; j++)
        assert_int_equal(buf[j], 0x80);

    memset(buf, 0xaa, test_size);
    af_fill_silence(buf, test_size, AF_FORMAT_DSD);
    for (size_t j = 0; j < test_size; j++)
        assert_int_equal(buf[j], 0x69);

    free(buf);
}

struct wire_capture {
    uint8_t *buf;
    size_t len;
    size_t cap;
};

static void wire_append(struct wire_capture *cap, const void *src, size_t bytes)
{
    if (cap->len + bytes > cap->cap) {
        cap->cap = MPMAX(cap->cap * 2, cap->len + bytes + 65536);
        cap->buf = realloc(cap->buf, cap->cap);
        assert_true(cap->buf);
    }
    memcpy(cap->buf + cap->len, src, bytes);
    cap->len += bytes;
}

struct mock_ao_priv {
    int starts;
    int resets;
    int wakeups;
};

static void mock_ao_wakeup(void *ctx)
{
    struct mock_ao_priv *mp = ctx;
    mp->wakeups++;
}

static void mock_ao_start(struct ao *ao)
{
    struct mock_ao_priv *mp = ao->priv;
    mp->starts++;
}

static void mock_ao_reset(struct ao *ao)
{
    struct mock_ao_priv *mp = ao->priv;
    mp->resets++;
}

static void mock_ao_uninit(struct ao *ao)
{
}

static const struct ao_driver mock_pull_ao_driver = {
    .name = "mock_pull",
    .start = mock_ao_start,
    .reset = mock_ao_reset,
    .uninit = mock_ao_uninit,
};

static struct ao *create_mock_ao(struct mpv_global *global,
                                 const struct ao_driver *driver, void *priv,
                                 void (*wakeup_cb)(void *), int format,
                                 int rate, int channels, bool stream_silence)
{
    struct ao *ao = talloc_zero(NULL, struct ao);
    assert_true(ao);
    ao->global = global;
    ao->log = mp_null_log;
    ao->driver = driver;
    ao->driver_initialized = true;
    ao->priv = priv;
    ao->wakeup_cb = wakeup_cb;
    ao->wakeup_ctx = priv;
    ao->format = format;
    ao->samplerate = rate;
    mp_chmap_from_channels(&ao->channels, channels);
    ao->num_planes = 1;
    ao->sstride = af_fmt_to_bytes(format) * channels;
    ao->bps = (int64_t)rate * ao->sstride;
    ao->buffer = rate;
    ao->device_buffer = 4096;
    ao->stream_silence = stream_silence;
    if (stream_silence)
        ao->init_flags |= AO_INIT_STREAM_SILENCE;

    init_buffer_pre(ao);
    assert_true(init_buffer_post(ao));
    return ao;
}

static struct mp_aframe *make_mat_burst_aframe(uint32_t seq_id)
{
    struct mp_aframe *f = mp_aframe_create();
    assert_true(f);
    struct mp_chmap chmap;
    mp_chmap_from_channels(&chmap, 8);
    assert_true(mp_aframe_set_format(f, AF_FORMAT_S_TRUEHD));
    assert_true(mp_aframe_set_rate(f, 192000));
    assert_true(mp_aframe_set_chmap(f, &chmap));
    assert_true(mp_aframe_alloc_data(f, 3840));

    uint8_t *hd = calloc(1, 61424);
    assert_true(hd);
    memcpy(hd + 0, mat_start_code, sizeof(mat_start_code));
    memcpy(hd + 30708, mat_middle_code, sizeof(mat_middle_code));
    memcpy(hd + 61408, mat_end_code, sizeof(mat_end_code));
    AV_WB32(hd + 20, seq_id);
    AV_WB32(hd + 61404, seq_id);

    uint8_t **data = mp_aframe_get_data_rw(f);
    assert_true(data && data[0]);
    uint8_t *out = data[0];
    AV_WL16(out + 0, 0xF872);
    AV_WL16(out + 2, 0x4E1F);
    AV_WL16(out + 4, 0x0016);
    AV_WL16(out + 6, 61424);
    for (int i = 0; i < 61424; i += 2) {
        out[8 + i]     = hd[i + 1];
        out[8 + i + 1] = hd[i];
    }
    memset(out + 61432, 0, 8);
    free(hd);
    return f;
}

static struct mp_aframe *make_dtshd_burst_aframe(uint32_t seq_id)
{
    struct mp_aframe *f = mp_aframe_create();
    assert_true(f);
    struct mp_chmap chmap;
    mp_chmap_from_channels(&chmap, 8);
    assert_true(mp_aframe_set_format(f, AF_FORMAT_S_DTSHD));
    assert_true(mp_aframe_set_rate(f, 192000));
    assert_true(mp_aframe_set_chmap(f, &chmap));
    assert_true(mp_aframe_alloc_data(f, 2048));

    uint8_t **data = mp_aframe_get_data_rw(f);
    assert_true(data && data[0]);
    uint8_t *out = data[0];
    memset(out, 0, 2048 * 16);
    AV_WL16(out + 0, 0xF872);
    AV_WL16(out + 2, 0x4E1F);
    AV_WL16(out + 4, 0x0011);
    AV_WL16(out + 6, 32752);
    AV_WL32(out + 8, seq_id);
    AV_WL32(out + 32756, seq_id);
    return f;
}

static void wire_pull(struct ao *ao, struct wire_capture *cap, int frames)
{
    size_t bytes = (size_t)frames * ao->sstride;
    if (cap->len + bytes > cap->cap) {
        cap->cap = MPMAX(cap->cap * 2, cap->len + bytes + 65536);
        cap->buf = realloc(cap->buf, cap->cap);
        assert_true(cap->buf);
    }
    void *planes[1] = { cap->buf + cap->len };
    ao_read_data(ao, planes, frames, 0, NULL, true, true);
    cap->len += bytes;
}

static void push_bursts(struct mp_filter *wroot, struct mp_filter *writer,
                        uint32_t *seq_id, int count, bool dtshd)
{
    for (int i = 0; i < count; i++) {
        mp_filter_graph_run(wroot);
        assert_true(mp_pin_in_needs_data(writer->pins[0]));
        struct mp_aframe *f = dtshd ? make_dtshd_burst_aframe((*seq_id)++)
                                    : make_mat_burst_aframe((*seq_id)++);
        assert_true(mp_pin_in_write(writer->pins[0], MAKE_FRAME(MP_FRAME_AUDIO, f)));
        mp_filter_graph_run(wroot);
    }
}

static void verify_iec61937_wire_stream(const uint8_t *buf, size_t len,
                                        int *out_data_bursts,
                                        int *out_pause_bursts)
{
    uint8_t *hd = malloc(61424);
    assert_true(hd);

    size_t pos = 0;
    int data_count = 0;
    int pause_count = 0;

    while (pos < len) {
        assert_true(pos + 16 <= len);
        uint16_t pa = AV_RL16(buf + pos + 0);
        uint16_t pb = AV_RL16(buf + pos + 2);
        uint16_t pc = AV_RL16(buf + pos + 4);
        assert_int_equal(pa, 0xF872);
        assert_int_equal(pb, 0x4E1F);

        if (pc == 0x0003) {
            assert_int_equal(memcmp(buf + pos, expected_pause_burst, 16), 0);
            pos += 16;
            pause_count++;
        } else if (pc == 0x0016) {
            assert_true(pos + 61440 <= len);
            assert_int_equal(AV_RL16(buf + pos + 6), 61424);
            for (int i = 61432; i < 61440; i++)
                assert_int_equal(buf[pos + i], 0);
            for (int i = 0; i < 61424; i += 2) {
                hd[i]     = buf[pos + 8 + i + 1];
                hd[i + 1] = buf[pos + 8 + i];
            }
            assert_int_equal(memcmp(hd + 0, mat_start_code, sizeof(mat_start_code)), 0);
            assert_int_equal(memcmp(hd + 30708, mat_middle_code, sizeof(mat_middle_code)), 0);
            assert_int_equal(memcmp(hd + 61408, mat_end_code, sizeof(mat_end_code)), 0);
            assert_int_equal(AV_RB32(hd + 20), AV_RB32(hd + 61404));
            pos += 61440;
            data_count++;
        } else if (pc == 0x0011) {
            assert_true(pos + 32768 <= len);
            assert_int_equal(AV_RL16(buf + pos + 6), 32752);
            assert_int_equal(AV_RL32(buf + pos + 8), AV_RL32(buf + pos + 32756));
            pos += 32768;
            data_count++;
        } else {
            assert_true(false);
        }
    }

    free(hd);
    assert_int_equal(pos, len);
    *out_data_bursts = data_count;
    *out_pause_bursts = pause_count;
}

static void test_partial_spdif_burst_drain(void)
{
    struct mpv_global global = {0};
    struct mock_ao_priv priv = {0};
    struct ao *ao = create_mock_ao(&global, &mock_pull_ao_driver, &priv,
                                   mock_ao_wakeup, AF_FORMAT_S_TRUEHD,
                                   192000, 8, true);
    struct mp_filter *wroot = mp_filter_create_root(&global);
    struct mp_filter *writer = mp_async_queue_create_filter(wroot, MP_PIN_IN,
                                                            ao_get_queue(ao));
    assert_true(writer);

    struct wire_capture cap = {0};
    uint32_t seq_id = 1;

    wire_pull(ao, &cap, 4096);
    wire_pull(ao, &cap, 4096);

    // Sweep across 4096-vs-3840 phase remainders with reset, pause, and restart.
    for (int round = 0; round < 18; round++) {
        push_bursts(wroot, writer, &seq_id, 6, false);
        ao_start(ao);

        int pulls = 1 + (round % 4);
        for (int k = 0; k < pulls; k++)
            wire_pull(ao, &cap, 4096);

        if (round % 5 == 0) {
            ao_reset(ao);
            wire_pull(ao, &cap, 4096);
            wire_pull(ao, &cap, 4096);
        } else if (round % 5 == 1) {
            ao_reset(ao);
            push_bursts(wroot, writer, &seq_id, 4, false);
            ao_start(ao);
            wire_pull(ao, &cap, 4096);
            ao_reset(ao);
            wire_pull(ao, &cap, 4096);
        } else if (round % 5 == 2) {
            ao_reset(ao);
            ao_reset(ao);
            wire_pull(ao, &cap, 4096);
        } else if (round % 5 == 3) {
            ao_set_paused(ao, true, false);
            wire_pull(ao, &cap, 4096);
            wire_pull(ao, &cap, 4096);
            ao_set_paused(ao, false, false);
            wire_pull(ao, &cap, 4096);
            ao_reset(ao);
            wire_pull(ao, &cap, 4096);
        } else {
            ao_set_paused(ao, true, false);
            wire_pull(ao, &cap, 4096);
            ao_reset(ao);
            ao_set_paused(ao, false, false);
            wire_pull(ao, &cap, 4096);
        }
    }

    int mat_bursts = 0;
    int pause_bursts = 0;
    verify_iec61937_wire_stream(cap.buf, cap.len, &mat_bursts, &pause_bursts);
    assert_true(mat_bursts >= 40);
    assert_true(pause_bursts >= 10000);

    free(cap.buf);
    talloc_free(wroot);
    ao_uninit(ao);

    // Pull AO with DTS-HD 2048-frame bursts across non-dividing 1536-frame quanta.
    struct mock_ao_priv dts_priv = {0};
    struct ao *dts_ao = create_mock_ao(&global, &mock_pull_ao_driver, &dts_priv,
                                       mock_ao_wakeup, AF_FORMAT_S_DTSHD,
                                       192000, 8, true);
    struct mp_filter *droot = mp_filter_create_root(&global);
    struct mp_filter *dwriter = mp_async_queue_create_filter(droot, MP_PIN_IN,
                                                             ao_get_queue(dts_ao));
    struct wire_capture dcap = {0};
    for (int r = 0; r < 6; r++) {
        push_bursts(droot, dwriter, &seq_id, 4, true);
        ao_start(dts_ao);
        wire_pull(dts_ao, &dcap, 1536);
        if (r % 2 == 0) {
            ao_reset(dts_ao);
        } else {
            ao_set_paused(dts_ao, true, false);
            wire_pull(dts_ao, &dcap, 1536);
            ao_set_paused(dts_ao, false, false);
            ao_reset(dts_ao);
        }
        wire_pull(dts_ao, &dcap, 1536);
    }
    int dts_bursts = 0, dts_pauses = 0;
    verify_iec61937_wire_stream(dcap.buf, dcap.len, &dts_bursts, &dts_pauses);
    assert_true(dts_bursts >= 6);
    assert_true(dts_pauses > 0);
    free(dcap.buf);
    talloc_free(droot);
    ao_uninit(dts_ao);

    // Non-regression: stream_silence=false discards partial tail on ao_reset().
    struct mock_ao_priv ns_priv = {0};
    struct ao *ns_ao = create_mock_ao(&global, &mock_pull_ao_driver, &ns_priv,
                                      mock_ao_wakeup, AF_FORMAT_S_TRUEHD,
                                      192000, 8, false);
    struct mp_filter *ns_wroot = mp_filter_create_root(&global);
    struct mp_filter *ns_writer = mp_async_queue_create_filter(ns_wroot, MP_PIN_IN,
                                                               ao_get_queue(ns_ao));
    push_bursts(ns_wroot, ns_writer, &seq_id, 2, false);
    ao_start(ns_ao);
    struct wire_capture ns_cap = {0};
    wire_pull(ns_ao, &ns_cap, 2048);
    ao_reset(ns_ao);
    void *planes[1] = { malloc(4096 * 16) };
    assert_true(planes[0]);
    int got = ao_read_data(ns_ao, planes, 4096, 0, NULL, true, true);
    assert_int_equal(got, 0);
    free(planes[0]);
    free(ns_cap.buf);
    talloc_free(ns_wroot);
    ao_uninit(ns_ao);

    // Non-regression: PCM with stream_silence=true discards immediately on reset.
    struct mock_ao_priv pcm_priv = {0};
    struct ao *pcm_ao = create_mock_ao(&global, &mock_pull_ao_driver, &pcm_priv,
                                       mock_ao_wakeup, AF_FORMAT_S16,
                                       48000, 2, true);
    struct mp_filter *p_wroot = mp_filter_create_root(&global);
    struct mp_filter *p_writer = mp_async_queue_create_filter(p_wroot, MP_PIN_IN,
                                                              ao_get_queue(pcm_ao));
    struct mp_aframe *pf = mp_aframe_create();
    struct mp_chmap ch2;
    mp_chmap_from_channels(&ch2, 2);
    assert_true(mp_aframe_set_format(pf, AF_FORMAT_S16));
    assert_true(mp_aframe_set_rate(pf, 48000));
    assert_true(mp_aframe_set_chmap(pf, &ch2));
    assert_true(mp_aframe_alloc_data(pf, 4096));
    memset(mp_aframe_get_data_rw(pf)[0], 0x55, 4096 * 4);
    mp_filter_graph_run(p_wroot);
    assert_true(mp_pin_in_write(p_writer->pins[0], MAKE_FRAME(MP_FRAME_AUDIO, pf)));
    mp_filter_graph_run(p_wroot);
    ao_start(pcm_ao);
    uint8_t *pbuf = malloc(4096 * 4);
    void *pplanes[1] = { pbuf };
    assert_int_equal(ao_read_data(pcm_ao, pplanes, 1024, 0, NULL, true, true), 1024);
    ao_reset(pcm_ao);
    assert_int_equal(ao_read_data(pcm_ao, pplanes, 4096, 0, NULL, true, true), 0);
    for (int i = 0; i < 4096 * 4; i++)
        assert_int_equal(pbuf[i], 0);
    free(pbuf);
    talloc_free(p_wroot);
    ao_uninit(pcm_ao);
}

struct mock_push_priv {
    mp_mutex lock;
    mp_cond cond;
    int credits;
    int writes;
    int chunk_frames;
    bool playing;
    struct wire_capture cap;
};

static void mock_push_wakeup(void *ctx)
{
}

static void mock_push_start(struct ao *ao)
{
    struct mock_push_priv *p = ao->priv;
    mp_mutex_lock(&p->lock);
    p->playing = true;
    mp_mutex_unlock(&p->lock);
}

static void mock_push_reset(struct ao *ao)
{
    struct mock_push_priv *p = ao->priv;
    mp_mutex_lock(&p->lock);
    p->playing = false;
    mp_mutex_unlock(&p->lock);
}

static void mock_push_get_state(struct ao *ao, struct mp_pcm_state *state)
{
    struct mock_push_priv *p = ao->priv;
    mp_mutex_lock(&p->lock);
    state->playing = p->playing;
    state->delay = p->playing ? 0.020 : 0.0;
    state->free_samples = p->credits > 0 ? p->chunk_frames : 0;
    mp_mutex_unlock(&p->lock);
}

static bool mock_push_write(struct ao *ao, void **data, int samples)
{
    struct mock_push_priv *p = ao->priv;
    mp_mutex_lock(&p->lock);
    wire_append(&p->cap, data[0], (size_t)samples * ao->sstride);
    if (p->credits > 0)
        p->credits--;
    p->writes++;
    mp_cond_broadcast(&p->cond);
    mp_mutex_unlock(&p->lock);
    return true;
}

static const struct ao_driver mock_push_ao_driver = {
    .name = "mock_push",
    .start = mock_push_start,
    .reset = mock_push_reset,
    .uninit = mock_ao_uninit,
    .get_state = mock_push_get_state,
    .write = mock_push_write,
};

static void push_ao_step(struct ao *ao, struct mock_push_priv *priv, int count)
{
    mp_mutex_lock(&priv->lock);
    int target = priv->writes + count;
    priv->credits += count;
    mp_mutex_unlock(&priv->lock);
    ao_wakeup(ao);
    mp_mutex_lock(&priv->lock);
    while (priv->writes < target)
        mp_cond_wait(&priv->cond, &priv->lock);
    mp_mutex_unlock(&priv->lock);
}

static void test_push_ao_partial_spdif_drain(void)
{
    struct mpv_global global = {0};
    struct mock_push_priv priv = { .chunk_frames = 1536 };
    mp_mutex_init(&priv.lock);
    mp_cond_init(&priv.cond);

    struct ao *ao = create_mock_ao(&global, &mock_push_ao_driver, &priv,
                                   mock_push_wakeup, AF_FORMAT_S_TRUEHD,
                                   192000, 8, true);
    struct mp_filter *wroot = mp_filter_create_root(&global);
    struct mp_filter *writer = mp_async_queue_create_filter(wroot, MP_PIN_IN,
                                                            ao_get_queue(ao));
    uint32_t seq_id = 100;

    // Start mid-burst (1536 of 3840 frames), then reset and drain.
    push_bursts(wroot, writer, &seq_id, 4, false);
    ao_start(ao);
    push_ao_step(ao, &priv, 1);
    ao_reset(ao);
    push_ao_step(ao, &priv, 3);

    // Restart, write 2 chunks (3072 of 3840 frames), pause mid-burst, drain.
    push_bursts(wroot, writer, &seq_id, 4, false);
    ao_start(ao);
    push_ao_step(ao, &priv, 2);
    ao_set_paused(ao, true, false);
    push_ao_step(ao, &priv, 3);

    talloc_free(wroot);
    ao_uninit(ao);

    int mat_bursts = 0, pause_bursts = 0;
    verify_iec61937_wire_stream(priv.cap.buf, priv.cap.len,
                                &mat_bursts, &pause_bursts);
    assert_int_equal(mat_bursts, 2);
    assert_true(pause_bursts > 0);

    free(priv.cap.buf);
    mp_cond_destroy(&priv.cond);
    mp_mutex_destroy(&priv.lock);
}

int main(void)
{
    test_truehd_pts_pacing();
    test_truehd_44k_family();
    test_spdif_pause_silence();
    test_partial_spdif_burst_drain();
    test_push_ao_partial_spdif_drain();
    return 0;
}
