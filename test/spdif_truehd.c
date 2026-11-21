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
#include "common/av_common.h"
#include "common/common.h"
#include "common/global.h"
#include "demux/packet.h"
#include "demux/stheader.h"
#include "filters/f_decoder_wrapper.h"
#include "filters/filter.h"
#include "filters/frame.h"
#include "video/hwdec.h"
#include "test_utils.h"

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
    static const uint8_t expected_pause[16] = {
        0x72, 0xF8, 0x1F, 0x4E, 0x03, 0x00, 0x20, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    const int hbr_fmts[] = { AF_FORMAT_S_TRUEHD, AF_FORMAT_S_DTSHD };
    const int twoch_fmts[] = { AF_FORMAT_S_AC3, AF_FORMAT_S_EAC3 };

    size_t test_size = (512 * 16) + 7;
    uint8_t *buf = malloc(test_size);
    assert_true(buf);

    for (size_t i = 0; i < MP_ARRAY_SIZE(hbr_fmts); i++) {
        memset(buf, 0xaa, test_size);
        af_fill_silence(buf, test_size, hbr_fmts[i]);
        for (int k = 0; k < 512; k++)
            assert_int_equal(memcmp(buf + k * 16, expected_pause, 16), 0);
        for (size_t rem = 512 * 16; rem < test_size; rem++)
            assert_int_equal(buf[rem], 0x00);
    }

    for (size_t i = 0; i < MP_ARRAY_SIZE(twoch_fmts); i++) {
        memset(buf, 0xaa, test_size);
        af_fill_silence(buf, test_size, twoch_fmts[i]);
        for (int k = 0; k < 64; k++) {
            assert_int_equal(memcmp(buf + k * 128, expected_pause, 16), 0);
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

int main(void)
{
    test_truehd_pts_pacing();
    test_truehd_44k_family();
    test_spdif_pause_silence();
    return 0;
}
