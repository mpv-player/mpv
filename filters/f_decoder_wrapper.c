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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>

#include <libavutil/buffer.h>
#include <libavutil/rational.h>

#include "config.h"
#include "options/options.h"
#include "common/msg.h"
#include "options/m_config.h"
#include "osdep/timer.h"

#include "demux/demux.h"
#include "demux/packet.h"

#include "common/codecs.h"
#include "common/global.h"
#include "common/recorder.h"

#include "audio/aframe.h"
#include "video/out/vo.h"
#include "video/csputils.h"

#include "demux/stheader.h"

#include "f_decoder_wrapper.h"
#include "f_demux_in.h"
#include "filter_internal.h"

struct priv {
    struct mp_filter *f;
    struct mp_log *log;
    struct m_config_cache *opt_cache;

    struct sh_stream *header;
    struct mp_codec_params *codec;

    struct mp_decoder *decoder;

    // Demuxer output.
    struct mp_pin *demux;

    // Last PTS from decoder (set with each vd_driver->decode() call)
    double codec_pts;
    int num_codec_pts_problems;

    // Last packet DTS from decoder (passed through from source packets)
    double codec_dts;
    int num_codec_dts_problems;

    // PTS or DTS of packet first read
    double first_packet_pdts;

    // There was at least one packet with nonsense timestamps.
    int has_broken_packet_pts; // <0: uninitialized, 0: no problems, 1: broken

    int has_broken_decoded_pts;

    int packets_without_output; // number packets sent without frame received

    // Final PTS of previously decoded frame
    double pts;

    struct mp_image_params dec_format, last_format, fixed_format;

    double start_pts;
    double start, end;
    struct demux_packet *new_segment;
    struct mp_frame packet;
    bool packet_fed;
    int preroll_discard;

    size_t reverse_queue_byte_size;
    struct mp_frame *reverse_queue;
    int num_reverse_queue;
    bool reverse_queue_complete;

    struct mp_frame decoded_coverart;
    int coverart_returned; // 0: no, 1: coverart frame itself, 2: EOF returned

    struct mp_decoder_wrapper public;
};

static void reset_decoder(struct priv *p)
{
    p->first_packet_pdts = MP_NOPTS_VALUE;
    p->start_pts = MP_NOPTS_VALUE;
    p->pts = MP_NOPTS_VALUE;
    p->codec_pts = MP_NOPTS_VALUE;
    p->codec_dts = MP_NOPTS_VALUE;
    p->has_broken_decoded_pts = 0;
    p->last_format = p->fixed_format = (struct mp_image_params){0};
    p->public.dropped_frames = 0;
    p->public.attempt_framedrops = 0;
    p->public.pts_reset = false;
    p->packets_without_output = 0;
    mp_frame_unref(&p->packet);
    p->packet_fed = false;
    p->preroll_discard = 0;
    talloc_free(p->new_segment);
    p->new_segment = NULL;
    p->start = p->end = MP_NOPTS_VALUE;
    p->coverart_returned = 0;

    for (int n = 0; n < p->num_reverse_queue; n++)
        mp_frame_unref(&p->reverse_queue[n]);
    p->num_reverse_queue = 0;
    p->reverse_queue_byte_size = 0;
    p->reverse_queue_complete = false;

    if (p->decoder)
        mp_filter_reset(p->decoder->f);
}

static void reset(struct mp_filter *f)
{
    struct priv *p = f->priv;

    reset_decoder(p);
}

int mp_decoder_wrapper_control(struct mp_decoder_wrapper *d,
                               enum dec_ctrl cmd, void *arg)
{
    struct priv *p = d->f->priv;
    if (p->decoder && p->decoder->control)
        return p->decoder->control(p->decoder->f, cmd, arg);
    return CONTROL_UNKNOWN;
}

static void destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;
    if (p->decoder) {
        MP_VERBOSE(f, "Uninit decoder.\n");
        talloc_free(p->decoder->f);
        p->decoder = NULL;
    }
    reset_decoder(p);
    mp_frame_unref(&p->decoded_coverart);
}

struct mp_decoder_list *video_decoder_list(void)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);
    vd_lavc.add_decoders(list);
    return list;
}

struct mp_decoder_list *audio_decoder_list(void)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);
    ad_lavc.add_decoders(list);
    return list;
}

bool mp_decoder_wrapper_reinit(struct mp_decoder_wrapper *d)
{
    struct priv *p = d->f->priv;
    struct MPOpts *opts = p->opt_cache->opts;
    m_config_cache_update(p->opt_cache);

    if (p->decoder)
        talloc_free(p->decoder->f);
    p->decoder = NULL;

    reset_decoder(p);
    p->has_broken_packet_pts = -10; // needs 10 packets to reach decision

    const struct mp_decoder_fns *driver = NULL;
    struct mp_decoder_list *list = NULL;
    char *user_list = NULL;

    if (p->codec->type == STREAM_VIDEO) {
        driver = &vd_lavc;
        user_list = opts->video_decoders;
    } else if (p->codec->type == STREAM_AUDIO) {
        driver = &ad_lavc;
        user_list = opts->audio_decoders;

        if (p->public.try_spdif && p->codec->codec) {
            struct mp_decoder_list *spdif =
                select_spdif_codec(p->codec->codec, opts->audio_spdif);
            if (spdif->num_entries) {
                driver = &ad_spdif;
                list = spdif;
            } else {
                talloc_free(spdif);
            }
        }
    }

    if (!list) {
        struct mp_decoder_list *full = talloc_zero(NULL, struct mp_decoder_list);
        if (driver)
            driver->add_decoders(full);
        list = mp_select_decoders(p->log, full, p->codec->codec, user_list);
        talloc_free(full);
    }

    mp_print_decoders(p->log, MSGL_V, "Codec list:", list);

    for (int n = 0; n < list->num_entries; n++) {
        struct mp_decoder_entry *sel = &list->entries[n];
        MP_VERBOSE(p, "Opening decoder %s\n", sel->decoder);

        p->decoder = driver->create(p->f, p->codec, sel->decoder);
        if (p->decoder) {
            p->public.decoder_desc =
                talloc_asprintf(p, "%s (%s)", sel->decoder, sel->desc);
            MP_VERBOSE(p, "Selected codec: %s\n", p->public.decoder_desc);
            break;
        }

        MP_WARN(p, "Decoder init failed for %s\n", sel->decoder);
    }

    if (!p->decoder) {
        MP_ERR(p, "Failed to initialize a decoder for codec '%s'.\n",
               p->codec->codec ? p->codec->codec : "<?>");
    }

    talloc_free(list);
    return !!p->decoder;
}

static bool is_valid_peak(float sig_peak)
{
    return !sig_peak || (sig_peak >= 1 && sig_peak <= 100);
}

static void fix_image_params(struct priv *p,
                             struct mp_image_params *params)
{
    struct mp_image_params m = *params;
    struct mp_codec_params *c = p->codec;
    struct MPOpts *opts = p->opt_cache->opts;
    m_config_cache_update(p->opt_cache);

    MP_VERBOSE(p, "Decoder format: %s\n", mp_image_params_to_str(params));
    p->dec_format = *params;

    // While mp_image_params normally always have to have d_w/d_h set, the
    // decoder signals unknown bitstream aspect ratio with both set to 0.
    bool use_container = true;
    if (opts->aspect_method == 1 && m.p_w > 0 && m.p_h > 0) {
        MP_VERBOSE(p, "Using bitstream aspect ratio.\n");
        use_container = false;
    }

    if (use_container && c->par_w > 0 && c->par_h) {
        MP_VERBOSE(p, "Using container aspect ratio.\n");
        m.p_w = c->par_w;
        m.p_h = c->par_h;
    }

    if (opts->movie_aspect >= 0) {
        MP_VERBOSE(p, "Forcing user-set aspect ratio.\n");
        if (opts->movie_aspect == 0) {
            m.p_w = m.p_h = 1;
        } else {
            AVRational a = av_d2q(opts->movie_aspect, INT_MAX);
            mp_image_params_set_dsize(&m, a.num, a.den);
        }
    }

    // Assume square pixels if no aspect ratio is set at all.
    if (m.p_w <= 0 || m.p_h <= 0)
        m.p_w = m.p_h = 1;

    m.rotate = p->codec->rotate;
    m.stereo3d = p->codec->stereo_mode;

    if (opts->video_rotate < 0) {
        m.rotate = 0;
    } else {
        m.rotate = (m.rotate + opts->video_rotate) % 360;
    }

    mp_colorspace_merge(&m.color, &c->color);

    // Sanitize the HDR peak. Sadly necessary
    if (!is_valid_peak(m.color.sig_peak)) {
        MP_WARN(p, "Invalid HDR peak in stream: %f\n", m.color.sig_peak);
        m.color.sig_peak = 0.0;
    }

    m.spherical = c->spherical;
    if (m.spherical.type == MP_SPHERICAL_AUTO)
        m.spherical.type = MP_SPHERICAL_NONE;

    // Guess missing colorspace fields from metadata. This guarantees all
    // fields are at least set to legal values afterwards.
    mp_image_params_guess_csp(&m);

    p->last_format = *params;
    p->fixed_format = m;
}

static void process_video_frame(struct priv *p, struct mp_image *mpi)
{
    struct MPOpts *opts = p->opt_cache->opts;
    m_config_cache_update(p->opt_cache);

    int dir = p->public.play_dir;

    // Note: the PTS is reordered, but the DTS is not. Both should be monotonic.
    double pts = mpi->pts;
    double dts = mpi->dts;

    if (pts != MP_NOPTS_VALUE) {
        pts *= dir;
        if (pts < p->codec_pts && dir > 0)
            p->num_codec_pts_problems++;
        p->codec_pts = mpi->pts;
    }

    if (dts != MP_NOPTS_VALUE) {
        dts *= dir;
        if (dts <= p->codec_dts && dir > 0)
            p->num_codec_dts_problems++;
        p->codec_dts = mpi->dts;
    }

    if (p->has_broken_packet_pts < 0)
        p->has_broken_packet_pts++;
    if (p->num_codec_pts_problems)
        p->has_broken_packet_pts = 1;

    // If PTS is unset, or non-monotonic, fall back to DTS.
    if ((p->num_codec_pts_problems > p->num_codec_dts_problems ||
         pts == MP_NOPTS_VALUE) && dts != MP_NOPTS_VALUE)
        pts = dts;

    if (!opts->correct_pts || pts == MP_NOPTS_VALUE) {
        double fps = p->public.fps > 0 ? p->public.fps : 25;

        if (opts->correct_pts) {
            if (p->has_broken_decoded_pts <= 1) {
                MP_WARN(p, "No video PTS! Making something up. Using "
                        "%f FPS.\n", fps);
                if (p->has_broken_decoded_pts == 1)
                    MP_WARN(p, "Ignoring further missing PTS warnings.\n");
                p->has_broken_decoded_pts++;
            }
        }

        double frame_time = 1.0f / fps;
        double base = p->first_packet_pdts;
        pts = p->pts;
        if (pts == MP_NOPTS_VALUE) {
            pts = base == MP_NOPTS_VALUE ? 0 : base;
        } else {
            pts += frame_time;
        }
    }

    if (!mp_image_params_equal(&p->last_format, &mpi->params))
        fix_image_params(p, &mpi->params);

    mpi->params = p->fixed_format;
    mpi->nominal_fps = p->public.fps;

    mpi->pts = pts;
    p->pts = pts;

    // Compensate for incorrectly using mpeg-style DTS for avi timestamps.
    if (p->decoder && p->decoder->control && p->codec->avi_dts &&
        opts->correct_pts && mpi->pts != MP_NOPTS_VALUE && p->public.fps > 0)
    {
        int delay = -1;
        p->decoder->control(p->decoder->f, VDCTRL_GET_BFRAMES, &delay);
        mpi->pts -= MPMAX(delay, 0) / p->public.fps;
    }

    struct demux_packet *ccpkt = new_demux_packet_from_buf(mpi->a53_cc);
    if (ccpkt) {
        av_buffer_unref(&mpi->a53_cc);
        ccpkt->pts = mpi->pts;
        ccpkt->dts = mpi->dts;
        demuxer_feed_caption(p->header, ccpkt);
    }

    if (mpi->pts == MP_NOPTS_VALUE || mpi->pts >= p->start_pts)
        p->start_pts = MP_NOPTS_VALUE;
}

void mp_decoder_wrapper_reset_params(struct mp_decoder_wrapper *d)
{
    struct priv *p = d->f->priv;
    p->last_format = (struct mp_image_params){0};
}

void mp_decoder_wrapper_get_video_dec_params(struct mp_decoder_wrapper *d,
                                             struct mp_image_params *m)
{
    struct priv *p = d->f->priv;
    *m = p->dec_format;
}

static void process_audio_frame(struct priv *p, struct mp_aframe *aframe)
{
    double dir = p->public.play_dir;

    double frame_pts = mp_aframe_get_pts(aframe);
    if (frame_pts != MP_NOPTS_VALUE) {
        frame_pts *= dir;

        if (p->pts != MP_NOPTS_VALUE)
            MP_STATS(p, "value %f audio-pts-err", p->pts - frame_pts);

        double diff = fabs(p->pts - frame_pts);

        // Attempt to detect jumps in PTS. Even for the lowest sample rates and
        // with worst container rounded timestamp, this should be a margin more
        // than enough.
        if (p->pts != MP_NOPTS_VALUE && diff > 0.1) {
            MP_WARN(p, "Invalid audio PTS: %f -> %f\n", p->pts, frame_pts);
            if (diff >= 5)
                p->public.pts_reset = true;
        }

        // Keep the interpolated timestamp if it doesn't deviate more
        // than 1 ms from the real one. (MKV rounded timestamps.)
        if (p->pts == MP_NOPTS_VALUE || diff > 0.001)
            p->pts = frame_pts;
    }

    if (p->pts == MP_NOPTS_VALUE && p->header->missing_timestamps)
        p->pts = 0;

    mp_aframe_set_pts(aframe, p->pts);

    if (p->pts != MP_NOPTS_VALUE)
        p->pts += mp_aframe_duration(aframe) * dir;

    if (dir < 0)
        mp_aframe_set_pts(aframe, p->pts);
}


// Frames before the start timestamp can be dropped. (Used for hr-seek.)
void mp_decoder_wrapper_set_start_pts(struct mp_decoder_wrapper *d, double pts)
{
    struct priv *p = d->f->priv;
    p->start_pts = pts;
}

static bool is_new_segment(struct priv *p, struct mp_frame frame)
{
    if (frame.type != MP_FRAME_PACKET)
        return false;
    struct demux_packet *pkt = frame.data;
    return (pkt->segmented && (pkt->start != p->start || pkt->end != p->end ||
                               pkt->codec != p->codec)) ||
           (p->public.play_dir < 0 && pkt->back_restart && p->packet_fed);
}

static void feed_packet(struct priv *p)
{
    if (!p->decoder || !mp_pin_in_needs_data(p->decoder->f->pins[0]))
        return;

    if (!p->packet.type && !p->new_segment) {
        p->packet = mp_pin_out_read(p->demux);
        if (!p->packet.type)
            return;
        if (p->packet.type != MP_FRAME_EOF && p->packet.type != MP_FRAME_PACKET) {
            MP_ERR(p, "invalid frame type from demuxer\n");
            mp_frame_unref(&p->packet);
            mp_filter_internal_mark_failed(p->f);
            return;
        }
    }

    if (!p->packet.type)
        return;

    // Flush current data if the packet is a new segment.
    if (is_new_segment(p, p->packet)) {
        assert(!p->new_segment);
        p->new_segment = p->packet.data;
        p->packet = MP_EOF_FRAME;
    }

    assert(p->packet.type == MP_FRAME_PACKET || p->packet.type == MP_FRAME_EOF);
    struct demux_packet *packet =
        p->packet.type == MP_FRAME_PACKET ? p->packet.data : NULL;

    // For video framedropping, including parts of the hr-seek logic.
    if (p->decoder->control) {
        double start_pts = p->start_pts;
        if (p->start != MP_NOPTS_VALUE && (start_pts == MP_NOPTS_VALUE ||
                                           p->start > start_pts))
            start_pts = p->start;

        int framedrop_type = 0;

        if (p->public.attempt_framedrops)
            framedrop_type = 1;

        if (start_pts != MP_NOPTS_VALUE && packet && p->public.play_dir > 0 &&
            packet->pts < start_pts - .005 && !p->has_broken_packet_pts)
            framedrop_type = 2;

        p->decoder->control(p->decoder->f, VDCTRL_SET_FRAMEDROP, &framedrop_type);
    }

    if (p->public.recorder_sink)
        mp_recorder_feed_packet(p->public.recorder_sink, packet);

    double pkt_pts = packet ? packet->pts : MP_NOPTS_VALUE;
    double pkt_dts = packet ? packet->dts : MP_NOPTS_VALUE;

    if (pkt_pts == MP_NOPTS_VALUE)
        p->has_broken_packet_pts = 1;

    if (packet && packet->dts == MP_NOPTS_VALUE && !p->codec->avi_dts)
        packet->dts = packet->pts;

    double pkt_pdts = pkt_pts == MP_NOPTS_VALUE ? pkt_dts : pkt_pts;
    if (p->first_packet_pdts == MP_NOPTS_VALUE)
        p->first_packet_pdts = pkt_pdts;

    if (packet && packet->back_preroll)
        p->preroll_discard += 1;

    mp_pin_in_write(p->decoder->f->pins[0], p->packet);
    p->packet = MP_NO_FRAME;
    p->packet_fed = true;

    p->packets_without_output += 1;
}

// Return true if the current frame is outside segment range.
static bool process_decoded_frame(struct priv *p, struct mp_frame *frame)
{
    if (frame->type == MP_FRAME_EOF) {
        // if we were just draining current segment, don't propagate EOF
        if (p->new_segment)
            mp_frame_unref(frame);
        return true;
    }

    bool segment_ended = false;

    if (frame->type == MP_FRAME_VIDEO) {
        struct mp_image *mpi = frame->data;

        process_video_frame(p, mpi);

        if (mpi->pts != MP_NOPTS_VALUE) {
            double vpts = mpi->pts;
            segment_ended = p->end != MP_NOPTS_VALUE && vpts >= p->end;
            if ((p->start != MP_NOPTS_VALUE && vpts < p->start) || segment_ended)
                mp_frame_unref(frame);
        }
    } else if (frame->type == MP_FRAME_AUDIO) {
        struct mp_aframe *aframe = frame->data;

        process_audio_frame(p, aframe);

        mp_aframe_clip_timestamps(aframe, p->start, p->end);
        double pts = mp_aframe_get_pts(aframe);
        if (pts != MP_NOPTS_VALUE && p->start != MP_NOPTS_VALUE)
            segment_ended = pts >= p->end;

        if (p->public.play_dir < 0 && !mp_aframe_reverse(aframe))
            MP_ERR(p, "Couldn't reverse audio frame.\n");

        if (mp_aframe_get_size(aframe) == 0)
            mp_frame_unref(frame);
    } else {
        MP_ERR(p, "unknown frame type from decoder\n");
    }

    return segment_ended;
}

static void enqueue_backward_frame(struct priv *p, struct mp_frame frame)
{
    bool eof = frame.type == MP_FRAME_EOF;

    if (!eof) {
        struct MPOpts *opts = p->opt_cache->opts;

        uint64_t queue_size = 0;
        switch (p->header->type) {
        case STREAM_VIDEO: queue_size = opts->video_reverse_size; break;
        case STREAM_AUDIO: queue_size = opts->audio_reverse_size; break;
        }

        if (p->reverse_queue_byte_size >= queue_size) {
            MP_ERR(p, "Reversal queue overflow, discarding frame.\n");
            mp_frame_unref(&frame);
            return;
        }

        p->reverse_queue_byte_size += mp_frame_approx_size(frame);
    }

    // Note: EOF (really BOF) is propagated, but not reversed.
    MP_TARRAY_INSERT_AT(p, p->reverse_queue, p->num_reverse_queue,
                        eof ? 0 : p->num_reverse_queue, frame);

    p->reverse_queue_complete = eof;
}

static void read_frame(struct priv *p)
{
    struct mp_pin *pin = p->f->ppins[0];

    if (!p->decoder || !mp_pin_in_needs_data(pin))
        return;

    if (p->decoded_coverart.type) {
        if (p->coverart_returned == 0) {
            mp_pin_in_write(pin, mp_frame_ref(p->decoded_coverart));
            p->coverart_returned = 1;
        } else if (p->coverart_returned == 1) {
            mp_pin_in_write(pin, MP_EOF_FRAME);
            p->coverart_returned = 2;
        }
        return;
    }

    if (p->reverse_queue_complete && p->num_reverse_queue) {
        struct mp_frame frame = p->reverse_queue[p->num_reverse_queue - 1];
        p->num_reverse_queue -= 1;
        //MP_WARN(p, "getq %f\n", mp_frame_get_pts(frame));
        mp_pin_in_write(pin, frame);
        return;
    }
    p->reverse_queue_complete = false;

    struct mp_frame frame = mp_pin_out_read(p->decoder->f->pins[1]);
    if (!frame.type)
        return;

    if (p->header->attached_picture && frame.type == MP_FRAME_VIDEO) {
        p->decoded_coverart = mp_frame_ref(frame);
        p->coverart_returned = 1;
    }

    if (p->public.attempt_framedrops) {
        int dropped = MPMAX(0, p->packets_without_output - 1);
        p->public.attempt_framedrops =
            MPMAX(0, p->public.attempt_framedrops - dropped);
        p->public.dropped_frames += dropped;
    }
    p->packets_without_output = 0;

    if (p->preroll_discard > 0 && frame.type != MP_FRAME_EOF) {
        p->preroll_discard -= 1;
        mp_frame_unref(&frame);
        mp_filter_internal_mark_progress(p->f);
        return;
    }

    bool segment_ended = process_decoded_frame(p, &frame);

    if (p->public.play_dir < 0 && frame.type) {
        enqueue_backward_frame(p, frame);
        frame = MP_NO_FRAME;
    }

    // If there's a new segment, start it as soon as we're drained/finished.
    if (segment_ended && p->new_segment) {
        struct demux_packet *new_segment = p->new_segment;
        p->new_segment = NULL;

        struct mp_frame *reverse_queue = p->reverse_queue;
        int num_reverse_queue = p->num_reverse_queue;
        p->reverse_queue = NULL;
        p->num_reverse_queue = 0;

        //MP_WARN(p, "reset\n");
        reset_decoder(p);

        if (new_segment->segmented) {
            if (p->codec != new_segment->codec) {
                p->codec = new_segment->codec;
                if (!mp_decoder_wrapper_reinit(&p->public))
                    mp_filter_internal_mark_failed(p->f);
            }

            p->start = new_segment->start;
            p->end = new_segment->end;
        }

        assert(!p->reverse_queue);
        p->reverse_queue = reverse_queue;
        p->num_reverse_queue = num_reverse_queue;
        p->reverse_queue_complete = p->num_reverse_queue > 0;

        p->packet = MAKE_FRAME(MP_FRAME_PACKET, new_segment);
        mp_filter_internal_mark_progress(p->f);
    }

    if (!frame.type) {
        mp_filter_internal_mark_progress(p->f); // make it retry
        return;
    }

    mp_pin_in_write(pin, frame);
}

static void process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    feed_packet(p);
    read_frame(p);
}

static const struct mp_filter_info decode_wrapper_filter = {
    .name = "decode",
    .priv_size = sizeof(struct priv),
    .process = process,
    .reset = reset,
    .destroy = destroy,
};

struct mp_decoder_wrapper *mp_decoder_wrapper_create(struct mp_filter *parent,
                                                     struct sh_stream *src)
{
    struct mp_filter *f = mp_filter_create(parent, &decode_wrapper_filter);
    if (!f)
        return NULL;

    struct priv *p = f->priv;
    struct mp_decoder_wrapper *w = &p->public;
    p->opt_cache = m_config_cache_alloc(p, f->global, GLOBAL_CONFIG);
    p->log = f->log;
    p->f = f;
    p->header = src;
    p->codec = p->header->codec;
    w->f = f;

    w->play_dir = 1;

    struct MPOpts *opts = p->opt_cache->opts;

    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    if (p->header->type == STREAM_VIDEO) {
        p->log = f->log = mp_log_new(f, parent->log, "!vd");

        p->public.fps = src->codec->fps;

        MP_VERBOSE(p, "Container reported FPS: %f\n", p->public.fps);

        if (opts->force_fps) {
            p->public.fps = opts->force_fps;
            MP_INFO(p, "FPS forced to %5.3f.\n", p->public.fps);
            MP_INFO(p, "Use --no-correct-pts to force FPS based timing.\n");
        }
    } else if (p->header->type == STREAM_AUDIO) {
        p->log = f->log = mp_log_new(f, parent->log, "!ad");
    }

    struct mp_filter *demux = mp_demux_in_create(f, p->header);
    if (!demux)
        goto error;
    p->demux = demux->pins[0];

    return w;
error:
    talloc_free(f);
    return NULL;
}

void lavc_process(struct mp_filter *f, bool *eof_flag,
                  bool (*send)(struct mp_filter *f, struct demux_packet *pkt),
                  bool (*receive)(struct mp_filter *f, struct mp_frame *res))
{
    if (!mp_pin_in_needs_data(f->ppins[1]))
        return;

    struct mp_frame frame = {0};
    if (!receive(f, &frame)) {
        if (!*eof_flag)
            mp_pin_in_write(f->ppins[1], MP_EOF_FRAME);
        *eof_flag = true;
    } else if (frame.type) {
        *eof_flag = false;
        mp_pin_in_write(f->ppins[1], frame);
    } else {
        // Need to feed a packet.
        frame = mp_pin_out_read(f->ppins[0]);
        struct demux_packet *pkt = NULL;
        if (frame.type == MP_FRAME_PACKET) {
            pkt = frame.data;
        } else if (frame.type != MP_FRAME_EOF) {
            if (frame.type) {
                MP_ERR(f, "unexpected frame type\n");
                mp_frame_unref(&frame);
                mp_filter_internal_mark_failed(f);
            }
            return;
        }
        if (!send(f, pkt)) {
            // Should never happen, but can happen with broken decoders.
            MP_WARN(f, "could not consume packet\n");
            mp_pin_out_unread(f->ppins[0], frame);
            mp_filter_wakeup(f);
            return;
        }
        talloc_free(pkt);
        mp_filter_internal_mark_progress(f);
    }
}
