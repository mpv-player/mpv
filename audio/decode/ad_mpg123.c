/*
 * MPEG 1.0/2.0/2.5 audio layer I, II, III decoding with libmpg123
 *
 * Copyright (C) 2010-2013 Thomas Orgis <thomas@orgis.org>
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"

#include "ad.h"
#include "common/msg.h"

#include <mpg123.h>

#if (defined MPG123_API_VERSION) && (MPG123_API_VERSION < 33)
#error "This should not happen"
#endif

struct ad_mpg123_context {
    mpg123_handle *handle;
    bool new_format;
    int sample_size;
    bool need_data;
    /* Running mean for bit rate, stream length estimation. */
    float mean_rate;
    unsigned int mean_count;
    /* Time delay for updates. */
    short delay;
    /* If the stream is actually VBR. */
    char vbr;
};

static void uninit(struct dec_audio *da)
{
    struct ad_mpg123_context *con = da->priv;

    mpg123_close(con->handle);
    mpg123_delete(con->handle);
    mpg123_exit();
}

/* This initializes libmpg123 and prepares the handle, including funky
 * parameters. */
static int preinit(struct dec_audio *da)
{
    int err;
    struct ad_mpg123_context *con;
    /* Assumption: You always call preinit + init + uninit, on every file.
     * But you stop at preinit in case it fails.
     * If that is not true, one must ensure not to call mpg123_init / exit
     * twice in a row. */
    if (mpg123_init() != MPG123_OK)
        return 0;

    da->priv = talloc_zero(NULL, struct ad_mpg123_context);
    con = da->priv;
    /* Auto-choice of optimized decoder (first argument NULL). */
    con->handle = mpg123_new(NULL, &err);
    if (!con->handle)
        goto bad_end;

    /* Basic settings.
     * Don't spill messages, enable better resync with non-seekable streams.
     * Give both flags individually without error checking to keep going with
     * old libmpg123. Generally, it is not fatal if the flags are not
     * honored */
    mpg123_param(con->handle, MPG123_ADD_FLAGS, MPG123_QUIET, 0.0);
    /* Do not bail out on malformed streams at all.
     * MPlayer does not handle a decoder throwing the towel on crappy input. */
    mpg123_param(con->handle, MPG123_RESYNC_LIMIT, -1, 0.0);

    /* Open decisions: Configure libmpg123 to force encoding (or stay open about
     * library builds that support only float or int32 output), (de)configure
     * gapless decoding (won't work with seeking in MPlayer, though).
     * Don't forget to eventually enable ReplayGain/RVA support, too.
     * Let's try to run with the default for now. */

    /* That would produce floating point output.
     * You can get 32 and 24 bit ints, even 8 bit via format matrix.
     * If wanting a specific encoding here, configure format matrix and
     * make sure it is in set_format(). */
    /* mpg123_param(con->handle, MPG123_ADD_FLAGS, MPG123_FORCE_FLOAT, 0.); */

    /* Example for RVA choice (available since libmpg123 1.0.0):
    mpg123_param(con->handle, MPG123_RVA, MPG123_RVA_MIX, 0.0) */

    /* Prevent funky automatic resampling.
     * This way, we can be sure that one frame will never produce
     * more than 1152 stereo samples.
     * Background:
     * Going to decode directly to the output buffer. It is important to have
     * MPG123_AUTO_RESAMPLE disabled for the buffer size being an all-time
     * limit.
     * We need at least 1152 samples. dec_audio.c normally guarantees this. */
    mpg123_param(con->handle, MPG123_REMOVE_FLAGS, MPG123_AUTO_RESAMPLE, 0.);

    return 1;

  bad_end:
    if (!con->handle)
        MP_ERR(da, "mpg123 preinit error: %s\n",
               mpg123_plain_strerror(err));
    else
        MP_ERR(da, "mpg123 preinit error: %s\n",
               mpg123_strerror(con->handle));

    uninit(da);
    return 0;
}

static int mpg123_format_to_af(int mpg123_encoding)
{
    /* Without external force, mpg123 will always choose signed encoding,
     * and non-16-bit only on builds that don't support it.
     * Be reminded that it doesn't matter to the MPEG file what encoding
     * is produced from it. */
    switch (mpg123_encoding) {
    case MPG123_ENC_SIGNED_8:   return AF_FORMAT_S8;
    case MPG123_ENC_SIGNED_16:  return AF_FORMAT_S16;
    case MPG123_ENC_SIGNED_32:  return AF_FORMAT_S32;
    case MPG123_ENC_FLOAT_32:   return AF_FORMAT_FLOAT;
    }
    return 0;
}

/* libmpg123 has a new format ready; query and store, return return value
   of mpg123_getformat() */
static int set_format(struct dec_audio *da)
{
    struct ad_mpg123_context *con = da->priv;
    int ret;
    long rate;
    int channels;
    int encoding;
    ret = mpg123_getformat(con->handle, &rate, &channels, &encoding);
    if (ret == MPG123_OK) {
        mp_audio_set_num_channels(&da->decoded, channels);
        da->decoded.rate = rate;
        int af = mpg123_format_to_af(encoding);
        if (!af) {
            /* This means we got a funny custom build of libmpg123 that only supports an unknown format. */
            MP_ERR(da, "Bad encoding from mpg123: %i.\n", encoding);
            return MPG123_ERR;
        }
        mp_audio_set_format(&da->decoded, af);
        con->sample_size = channels * (af_fmt2bits(af) / 8);
        con->new_format = 0;
    }
    return ret;
}

static int feed_new_packet(struct dec_audio *da)
{
    struct ad_mpg123_context *con = da->priv;
    int ret;

    struct demux_packet *pkt = demux_read_packet(da->header);
    if (!pkt)
        return -1; /* EOF. */

    /* Next bytes from that presentation time. */
    if (pkt->pts != MP_NOPTS_VALUE) {
        da->pts        = pkt->pts;
        da->pts_offset = 0;
    }

    /* Have to use mpg123_feed() to avoid decoding here. */
    ret = mpg123_feed(con->handle, pkt->buffer, pkt->len);
    talloc_free(pkt);

    if (ret == MPG123_ERR)
        return -1;

    if (ret == MPG123_NEW_FORMAT)
        con->new_format = 1;

    return 0;
}

/* Now we really start accessing some data and determining file format.
 * Format now is allowed to change on-the-fly. Here is the only point
 * that has MPlayer react to errors. We have to pray that exceptional
 * erros in other places simply cannot occur. */
static int init(struct dec_audio *da, const char *decoder)
{
    if (!preinit(da))
        return 0;

    struct ad_mpg123_context *con = da->priv;
    int ret;

    ret = mpg123_open_feed(con->handle);
    if (ret != MPG123_OK)
        goto fail;

    for (int n = 0; ; n++) {
        if (feed_new_packet(da) < 0) {
            ret = MPG123_NEED_MORE;
            goto fail;
        }
        size_t got_now = 0;
        ret = mpg123_decode_frame(con->handle, NULL, NULL, &got_now);
        if (ret == MPG123_OK || ret == MPG123_NEW_FORMAT) {
            ret = set_format(da);
            if (ret == MPG123_OK)
                break;
        }
        if (ret != MPG123_NEED_MORE)
            goto fail;
        // max. 16 retries (randomly chosen number)
        if (n > 16) {
            ret = MPG123_NEED_MORE;
            goto fail;
        }
    }

    return 1;

fail:
    if (ret == MPG123_NEED_MORE) {
        MP_ERR(da, "Could not find mp3 stream.\n");
    } else {
        MP_ERR(da, "mpg123 init error: %s\n",
               mpg123_strerror(con->handle));
    }

    uninit(da);
    return 0;
}

/* Compute bitrate from frame size. */
static int compute_bitrate(struct mpg123_frameinfo *i)
{
    static const int samples_per_frame[4][4] = {
        {-1, 384, 1152, 1152},  /* MPEG 1 */
        {-1, 384, 1152,  576},  /* MPEG 2 */
        {-1, 384, 1152,  576},  /* MPEG 2.5 */
        {-1,  -1,   -1,   -1},  /* Unknown */
    };
    return (int) ((i->framesize + 4) * 8 * i->rate /
                  samples_per_frame[i->version][i->layer] + 0.5);
}

/* Update mean bitrate. This could be dropped if accurate time display
 * on audio file playback is not desired. */
static void update_info(struct dec_audio *da)
{
    struct ad_mpg123_context *con = da->priv;
    struct mpg123_frameinfo finfo;
    if (mpg123_info(con->handle, &finfo) != MPG123_OK)
        return;

    /* finfo.bitrate is expressed in kilobits */
    const int bitrate = finfo.bitrate * 1000;

    if (finfo.vbr != MPG123_CBR) {
        if (--con->delay < 1) {
            if (++con->mean_count > ((unsigned int) -1) / 2)
                con->mean_count = ((unsigned int) -1) / 4;

            /* Might not be numerically optimal, but works fine enough. */
            con->mean_rate = ((con->mean_count - 1) * con->mean_rate +
                              bitrate) / con->mean_count;
            da->bitrate = (int) (con->mean_rate + 0.5);

            con->delay = 10;
        }
    } else {
        da->bitrate = bitrate ? bitrate : compute_bitrate(&finfo);
        con->delay      = 1;
        con->mean_rate  = 0.;
        con->mean_count = 0;
    }
}

static int decode_audio(struct dec_audio *da, struct mp_audio *buffer, int maxlen)
{
    struct ad_mpg123_context *con = da->priv;
    void *buf = buffer->planes[0];
    int ret;

    if (con->new_format) {
        ret = set_format(da);
        if (ret == MPG123_OK) {
            return 0; // let caller handle format change
        } else if (ret == MPG123_NEED_MORE) {
            con->need_data = true;
        } else {
            goto mpg123_fail;
        }
    }

    if (con->need_data) {
        if (feed_new_packet(da) < 0)
            return -1;
    }

    if (!mp_audio_config_equals(&da->decoded, buffer))
        return 0;

    size_t got_now = 0;
    ret = mpg123_replace_buffer(con->handle, buf, maxlen * con->sample_size);
    if (ret != MPG123_OK)
        goto mpg123_fail;

    ret = mpg123_decode_frame(con->handle, NULL, NULL, &got_now);

    int got_samples = got_now / con->sample_size;
    buffer->samples += got_samples;
    da->pts_offset += got_samples;

    if (ret == MPG123_NEW_FORMAT) {
        con->new_format = true;
    } else if (ret == MPG123_NEED_MORE) {
        con->need_data = true;
    } else if (ret != MPG123_OK && ret != MPG123_DONE) {
        goto mpg123_fail;
    }

    update_info(da);
    return 0;

mpg123_fail:
    MP_ERR(da, "mpg123 decoding error: %s\n",
           mpg123_strerror(con->handle));
    return -1;
}

static int control(struct dec_audio *da, int cmd, void *arg)
{
    struct ad_mpg123_context *con = da->priv;

    switch (cmd) {
    case ADCTRL_RESET:
        mpg123_close(con->handle);

        if (mpg123_open_feed(con->handle) != MPG123_OK) {
            MP_ERR(da, "mpg123 failed to reopen stream: %s\n",
                   mpg123_strerror(con->handle));
            return CONTROL_FALSE;
        }
        return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

static void add_decoders(struct mp_decoder_list *list)
{
    mp_add_decoder(list, "mpg123", "mp3", "mp3",
                   "High-performance decoder using libmpg123");
}

const struct ad_functions ad_mpg123 = {
    .name = "mpg123",
    .add_decoders = add_decoders,
    .init = init,
    .uninit = uninit,
    .control = control,
    .decode_audio = decode_audio,
};
