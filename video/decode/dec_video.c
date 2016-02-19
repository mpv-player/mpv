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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <libavutil/rational.h>

#include "config.h"
#include "options/options.h"
#include "common/msg.h"

#include "osdep/timer.h"

#include "stream/stream.h"
#include "demux/demux.h"
#include "demux/packet.h"

#include "common/codecs.h"

#include "video/out/vo.h"
#include "video/csputils.h"

#include "demux/stheader.h"
#include "video/decode/vd.h"

#include "video/decode/dec_video.h"

extern const vd_functions_t mpcodecs_vd_ffmpeg;

/* Please do not add any new decoders here. If you want to implement a new
 * decoder, add it to libavcodec, except for wrappers around external
 * libraries and decoders requiring binary support. */

const vd_functions_t * const mpcodecs_vd_drivers[] = {
    &mpcodecs_vd_ffmpeg,
    /* Please do not add any new decoders here. If you want to implement a new
     * decoder, add it to libavcodec, except for wrappers around external
     * libraries and decoders requiring binary support. */
    NULL
};

void video_reset(struct dec_video *d_video)
{
    video_vd_control(d_video, VDCTRL_RESET, NULL);
    d_video->first_packet_pdts = MP_NOPTS_VALUE;
    d_video->start_pts = MP_NOPTS_VALUE;
    d_video->decoded_pts = MP_NOPTS_VALUE;
    d_video->codec_pts = MP_NOPTS_VALUE;
    d_video->codec_dts = MP_NOPTS_VALUE;
    d_video->last_format = d_video->fixed_format = (struct mp_image_params){0};
    d_video->dropped_frames = 0;
    d_video->current_state = DATA_AGAIN;
    mp_image_unrefp(&d_video->current_mpi);
    talloc_free(d_video->packet);
    d_video->packet = NULL;
    talloc_free(d_video->new_segment);
    d_video->new_segment = NULL;
    d_video->start = d_video->end = MP_NOPTS_VALUE;
}

int video_vd_control(struct dec_video *d_video, int cmd, void *arg)
{
    const struct vd_functions *vd = d_video->vd_driver;
    if (vd)
        return vd->control(d_video, cmd, arg);
    return CONTROL_UNKNOWN;
}

void video_uninit(struct dec_video *d_video)
{
    if (!d_video)
        return;
    mp_image_unrefp(&d_video->current_mpi);
    mp_image_unrefp(&d_video->cover_art_mpi);
    if (d_video->vd_driver) {
        MP_VERBOSE(d_video, "Uninit video.\n");
        d_video->vd_driver->uninit(d_video);
    }
    talloc_free(d_video->packet);
    talloc_free(d_video->new_segment);
    talloc_free(d_video);
}

static int init_video_codec(struct dec_video *d_video, const char *decoder)
{
    if (!d_video->vd_driver->init(d_video, decoder)) {
        MP_VERBOSE(d_video, "Video decoder init failed.\n");
        return 0;
    }
    return 1;
}

struct mp_decoder_list *video_decoder_list(void)
{
    struct mp_decoder_list *list = talloc_zero(NULL, struct mp_decoder_list);
    for (int i = 0; mpcodecs_vd_drivers[i] != NULL; i++)
        mpcodecs_vd_drivers[i]->add_decoders(list);
    return list;
}

static struct mp_decoder_list *mp_select_video_decoders(const char *codec,
                                                        char *selection)
{
    struct mp_decoder_list *list = video_decoder_list();
    struct mp_decoder_list *new = mp_select_decoders(list, codec, selection);
    talloc_free(list);
    return new;
}

static const struct vd_functions *find_driver(const char *name)
{
    for (int i = 0; mpcodecs_vd_drivers[i] != NULL; i++) {
        if (strcmp(mpcodecs_vd_drivers[i]->name, name) == 0)
            return mpcodecs_vd_drivers[i];
    }
    return NULL;
}

bool video_init_best_codec(struct dec_video *d_video)
{
    struct MPOpts *opts = d_video->opts;

    assert(!d_video->vd_driver);
    video_reset(d_video);
    d_video->has_broken_packet_pts = -10; // needs 10 packets to reach decision

    struct mp_decoder_entry *decoder = NULL;
    struct mp_decoder_list *list =
        mp_select_video_decoders(d_video->codec->codec, opts->video_decoders);

    mp_print_decoders(d_video->log, MSGL_V, "Codec list:", list);

    for (int n = 0; n < list->num_entries; n++) {
        struct mp_decoder_entry *sel = &list->entries[n];
        const struct vd_functions *driver = find_driver(sel->family);
        if (!driver)
            continue;
        MP_VERBOSE(d_video, "Opening video decoder %s:%s\n",
                   sel->family, sel->decoder);
        d_video->vd_driver = driver;
        if (init_video_codec(d_video, sel->decoder)) {
            decoder = sel;
            break;
        }
        d_video->vd_driver = NULL;
        MP_WARN(d_video, "Video decoder init failed for "
                "%s:%s\n", sel->family, sel->decoder);
    }

    if (d_video->vd_driver) {
        d_video->decoder_desc =
            talloc_asprintf(d_video, "%s [%s:%s]", decoder->desc, decoder->family,
                            decoder->decoder);
        MP_VERBOSE(d_video, "Selected video codec: %s\n", d_video->decoder_desc);
    } else {
        MP_ERR(d_video, "Failed to initialize a video decoder for codec '%s'.\n",
               d_video->codec->codec);
    }

    if (d_video->header->missing_timestamps) {
        MP_WARN(d_video, "This stream has no timestamps!\n");
        MP_WARN(d_video, "Making up playback time using %f FPS.\n", d_video->fps);
        MP_WARN(d_video, "Seeking will probably fail badly.\n");
    }

    talloc_free(list);
    return !!d_video->vd_driver;
}

static void fix_image_params(struct dec_video *d_video,
                             struct mp_image_params *params)
{
    struct MPOpts *opts = d_video->opts;
    struct mp_image_params p = *params;
    struct mp_codec_params *c = d_video->codec;

    MP_VERBOSE(d_video, "Decoder format: %s\n", mp_image_params_to_str(params));

    // While mp_image_params normally always have to have d_w/d_h set, the
    // decoder signals unknown bitstream aspect ratio with both set to 0.
    float dec_aspect = p.p_w > 0 && p.p_h > 0 ? p.p_w / (float)p.p_h : 0;
    if (d_video->initial_decoder_aspect == 0)
        d_video->initial_decoder_aspect = dec_aspect;

    bool use_container = true;
    switch (opts->aspect_method) {
    case 0:
        // We normally prefer the container aspect, unless the decoder aspect
        // changes at least once.
        if (dec_aspect > 0 && d_video->initial_decoder_aspect != dec_aspect) {
            MP_VERBOSE(d_video, "Using bitstream aspect ratio.\n");
            // Even if the aspect switches back, don't use container aspect again.
            d_video->initial_decoder_aspect = -1;
            use_container = false;
        }
        break;
    case 1:
        use_container = false;
        break;
    }

    if (use_container && c->par_w > 0 && c->par_h) {
        MP_VERBOSE(d_video, "Using container aspect ratio.\n");
        p.p_w = c->par_w;
        p.p_h = c->par_h;
    }

    if (opts->movie_aspect >= 0) {
        MP_VERBOSE(d_video, "Forcing user-set aspect ratio.\n");
        if (opts->movie_aspect == 0) {
            p.p_w = p.p_h = 1;
        } else {
            AVRational a = av_d2q(opts->movie_aspect, INT_MAX);
            mp_image_params_set_dsize(&p, a.num, a.den);
        }
    }

    // Assume square pixels if no aspect ratio is set at all.
    if (p.p_w <= 0 || p.p_h <= 0)
        p.p_w = p.p_h = 1;

    // Detect colorspace from resolution.
    mp_image_params_guess_csp(&p);

    d_video->last_format = *params;
    d_video->fixed_format = p;
}

static struct mp_image *decode_packet(struct dec_video *d_video,
                                      struct demux_packet *packet,
                                      int drop_frame)
{
    struct MPOpts *opts = d_video->opts;

    if (!d_video->vd_driver)
        return NULL;

    double pkt_pts = packet ? packet->pts : MP_NOPTS_VALUE;
    double pkt_dts = packet ? packet->dts : MP_NOPTS_VALUE;

    if (pkt_pts == MP_NOPTS_VALUE)
        d_video->has_broken_packet_pts = 1;

    double pkt_pdts = pkt_pts == MP_NOPTS_VALUE ? pkt_dts : pkt_pts;
    if (pkt_pdts != MP_NOPTS_VALUE && d_video->first_packet_pdts == MP_NOPTS_VALUE)
        d_video->first_packet_pdts = pkt_pdts;

    MP_STATS(d_video, "start decode video");

    struct mp_image *mpi = d_video->vd_driver->decode(d_video, packet, drop_frame);

    MP_STATS(d_video, "end decode video");

    // Error, discarded frame, dropped frame, or initial codec delay.
    if (!mpi || drop_frame) {
        talloc_free(mpi);
        return NULL;
    }

    if (opts->field_dominance == 0) {
        mpi->fields |= MP_IMGFIELD_TOP_FIRST | MP_IMGFIELD_INTERLACED;
    } else if (opts->field_dominance == 1) {
        mpi->fields &= ~MP_IMGFIELD_TOP_FIRST;
        mpi->fields |= MP_IMGFIELD_INTERLACED;
    }

    // Note: the PTS is reordered, but the DTS is not. Both should be monotonic.
    double pts = mpi->pts;
    double dts = mpi->dts;

    if (pts != MP_NOPTS_VALUE) {
        if (pts < d_video->codec_pts)
            d_video->num_codec_pts_problems++;
        d_video->codec_pts = mpi->pts;
    }

    if (dts != MP_NOPTS_VALUE) {
        if (dts <= d_video->codec_dts)
            d_video->num_codec_dts_problems++;
        d_video->codec_dts = mpi->dts;
    }

    if (d_video->has_broken_packet_pts < 0)
        d_video->has_broken_packet_pts++;
    if (d_video->num_codec_pts_problems)
        d_video->has_broken_packet_pts = 1;

    // If PTS is unset, or non-monotonic, fall back to DTS.
    if ((d_video->num_codec_pts_problems > d_video->num_codec_dts_problems ||
         pts == MP_NOPTS_VALUE) && dts != MP_NOPTS_VALUE)
        pts = dts;

    if (!opts->correct_pts || pts == MP_NOPTS_VALUE) {
        if (opts->correct_pts && !d_video->header->missing_timestamps)
            MP_WARN(d_video, "No video PTS! Making something up.\n");

        double frame_time = 1.0f / (d_video->fps > 0 ? d_video->fps : 25);
        double base = d_video->first_packet_pdts;
        pts = d_video->decoded_pts;
        if (pts == MP_NOPTS_VALUE) {
            pts = base == MP_NOPTS_VALUE ? 0 : base;
        } else {
            pts += frame_time;
        }
    }

    if (!mp_image_params_equal(&d_video->last_format, &mpi->params))
        fix_image_params(d_video, &mpi->params);

    mpi->params = d_video->fixed_format;

    mpi->pts = pts;
    d_video->decoded_pts = pts;

    // Compensate for incorrectly using mpeg-style DTS for avi timestamps.
    if (d_video->codec->avi_dts && opts->correct_pts &&
        mpi->pts != MP_NOPTS_VALUE && d_video->fps > 0)
    {
        int delay = -1;
        video_vd_control(d_video, VDCTRL_GET_BFRAMES, &delay);
        mpi->pts -= MPMAX(delay, 0) / d_video->fps;
    }

    return mpi;
}

void video_reset_aspect(struct dec_video *d_video)
{
    d_video->last_format = (struct mp_image_params){0};
}

void video_set_framedrop(struct dec_video *d_video, bool enabled)
{
    d_video->framedrop_enabled = enabled;
}

// Frames before the start timestamp can be dropped. (Used for hr-seek.)
void video_set_start(struct dec_video *d_video, double start_pts)
{
    d_video->start_pts = start_pts;
}

void video_work(struct dec_video *d_video)
{
    if (d_video->current_mpi)
        return;

    if (d_video->header->attached_picture) {
        if (d_video->current_state == DATA_AGAIN && !d_video->cover_art_mpi) {
            d_video->cover_art_mpi =
                decode_packet(d_video, d_video->header->attached_picture, 0);
            // Might need flush.
            if (!d_video->cover_art_mpi)
                d_video->cover_art_mpi = decode_packet(d_video, NULL, 0);
            d_video->current_state = DATA_OK;
        }
        if (d_video->current_state == DATA_OK)
            d_video->current_mpi = mp_image_new_ref(d_video->cover_art_mpi);
        // (DATA_OK is returned the first time, when current_mpi is sill set)
        d_video->current_state = DATA_EOF;
        return;
    }

    if (!d_video->packet && !d_video->new_segment &&
        demux_read_packet_async(d_video->header, &d_video->packet) == 0)
    {
        d_video->current_state = DATA_WAIT;
        return;
    }

    if (d_video->packet) {
        if (d_video->packet->dts == MP_NOPTS_VALUE && !d_video->codec->avi_dts)
            d_video->packet->dts = d_video->packet->pts;
    }

    if (d_video->packet && d_video->packet->new_segment) {
        assert(!d_video->new_segment);
        d_video->new_segment = d_video->packet;
        d_video->packet = NULL;
    }

    bool had_input_packet = !!d_video->packet;
    bool had_packet = had_input_packet || d_video->new_segment;

    double start_pts = d_video->start_pts;
    if (d_video->start != MP_NOPTS_VALUE && (start_pts == MP_NOPTS_VALUE ||
                                             d_video->start > start_pts))
        start_pts = d_video->start;

    int framedrop_type = d_video->framedrop_enabled ? 1 : 0;
    if (start_pts != MP_NOPTS_VALUE && d_video->packet &&
        d_video->packet->pts < start_pts - .005 &&
        !d_video->has_broken_packet_pts)
    {
        framedrop_type = 2;
    }
    d_video->current_mpi = decode_packet(d_video, d_video->packet, framedrop_type);
    if (d_video->packet && d_video->packet->len == 0) {
        talloc_free(d_video->packet);
        d_video->packet = NULL;
    }

    d_video->current_state = DATA_OK;
    if (!d_video->current_mpi) {
        d_video->current_state = DATA_EOF;
        if (had_packet) {
            if (framedrop_type == 1)
                d_video->dropped_frames += 1;
            d_video->current_state = DATA_AGAIN;
        }
    }

    bool segment_ended = !d_video->current_mpi && !had_input_packet;

    if (d_video->current_mpi && d_video->current_mpi->pts != MP_NOPTS_VALUE) {
        double vpts = d_video->current_mpi->pts;
        segment_ended = d_video->end != MP_NOPTS_VALUE && vpts >= d_video->end;
        if ((start_pts != MP_NOPTS_VALUE && vpts < start_pts) || segment_ended) {
            talloc_free(d_video->current_mpi);
            d_video->current_mpi = NULL;
        }
    }

    // If there's a new segment, start it as soon as we're drained/finished.
    if (segment_ended && d_video->new_segment) {
        struct demux_packet *new_segment = d_video->new_segment;
        d_video->new_segment = NULL;

        // Could avoid decoder reinit; would still need flush.
        d_video->codec = new_segment->codec;
        if (d_video->vd_driver)
            d_video->vd_driver->uninit(d_video);
        d_video->vd_driver = NULL;
        video_init_best_codec(d_video);

        d_video->start = new_segment->start;
        d_video->end = new_segment->end;

        new_segment->new_segment = false;

        d_video->packet = new_segment;
        d_video->current_state = DATA_AGAIN;
    }
}

// Fetch an image decoded with video_work(). Returns one of:
//  DATA_OK:    *out_mpi is set to a new image
//  DATA_WAIT:  waiting for demuxer; will receive a wakeup signal
//  DATA_EOF:   end of file, no more frames to be expected
//  DATA_AGAIN: dropped frame or something similar
int video_get_frame(struct dec_video *d_video, struct mp_image **out_mpi)
{
    *out_mpi = NULL;
    if (d_video->current_mpi) {
        *out_mpi = d_video->current_mpi;
        d_video->current_mpi = NULL;
        return DATA_OK;
    }
    if (d_video->current_state == DATA_OK)
        return DATA_AGAIN;
    return d_video->current_state;
}
