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
#include "mpvcore/mp_msg.h"

#include <mpg123.h>

#if (defined MPG123_API_VERSION) && (MPG123_API_VERSION < 33)
#error "This should not happen"
#endif

struct ad_mpg123_context {
    mpg123_handle *handle;
    char new_format;
    /* Running mean for bit rate, stream length estimation. */
    float mean_rate;
    unsigned int mean_count;
    /* Time delay for updates. */
    short delay;
    /* If the stream is actually VBR. */
    char vbr;
};

/* This initializes libmpg123 and prepares the handle, including funky
 * parameters. */
static int preinit(sh_audio_t *sh)
{
    int err;
    struct ad_mpg123_context *con;
    /* Assumption: You always call preinit + init + uninit, on every file.
     * But you stop at preinit in case it fails.
     * If that is not true, one must ensure not to call mpg123_init / exit
     * twice in a row. */
    if (mpg123_init() != MPG123_OK)
        return 0;

    sh->context = malloc(sizeof(struct ad_mpg123_context));
    con = sh->context;
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
     * more than 1152 stereo samples. */
    mpg123_param(con->handle, MPG123_REMOVE_FLAGS, MPG123_AUTO_RESAMPLE, 0.);

    return 1;

  bad_end:
    if (!con->handle)
        mp_msg(MSGT_DECAUDIO, MSGL_ERR, "mpg123 preinit error: %s\n",
               mpg123_plain_strerror(err));
    else
        mp_msg(MSGT_DECAUDIO, MSGL_ERR, "mpg123 preinit error: %s\n",
               mpg123_strerror(con->handle));

    if (con->handle)
        mpg123_delete(con->handle);
    mpg123_exit();
    free(sh->context);
    sh->context = NULL;
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
    return (int) ((i->framesize + 4) * 8 * i->rate * 0.001 /
                  samples_per_frame[i->version][i->layer] + 0.5);
}

/* Opted against the header printout from old mp3lib, too much
 * irrelevant info. This is modelled after the mpg123 app's
 * standard output line.
 * If more verbosity is demanded, one can add more detail and
 * also throw in ID3v2 info which libmpg123 collects anyway. */
static void print_header_compact(struct mpg123_frameinfo *i)
{
    static const char *smodes[5] = {
        "stereo", "joint-stereo", "dual-channel", "mono", "invalid"
    };
    static const char *layers[4] = {
        "Unknown", "I", "II", "III"
    };
    static const char *versions[4] = {
        "1.0", "2.0", "2.5", "x.x"
    };

    mp_msg(MSGT_DECAUDIO, MSGL_V, "MPEG %s layer %s, ",
           versions[i->version], layers[i->layer]);
    switch (i->vbr) {
    case MPG123_CBR:
        if (i->bitrate)
            mp_msg(MSGT_DECAUDIO, MSGL_V, "%d kbit/s", i->bitrate);
        else
            mp_msg(MSGT_DECAUDIO, MSGL_V, "%d kbit/s (free format)",
                   compute_bitrate(i));
        break;
    case MPG123_VBR:
        mp_msg(MSGT_DECAUDIO, MSGL_V, "VBR");
        break;
    case MPG123_ABR:
        mp_msg(MSGT_DECAUDIO, MSGL_V, "%d kbit/s ABR", i->abr_rate);
        break;
    default:
        mp_msg(MSGT_DECAUDIO, MSGL_V, "???");
    }
    mp_msg(MSGT_DECAUDIO, MSGL_V, ", %ld Hz %s\n", i->rate,
           smodes[i->mode]);
}

/* libmpg123 has a new format ready; query and store, return return value
   of mpg123_getformat() */
static int set_format(sh_audio_t *sh, struct ad_mpg123_context *con)
{
    int ret;
    long rate;
    int channels;
    int encoding;
    ret = mpg123_getformat(con->handle, &rate, &channels, &encoding);
    if(ret == MPG123_OK) {
        mp_chmap_from_channels(&sh->channels, channels);
        sh->samplerate = rate;
        /* Without external force, mpg123 will always choose signed encoding,
         * and non-16-bit only on builds that don't support it.
         * Be reminded that it doesn't matter to the MPEG file what encoding
         * is produced from it. */
        switch (encoding) {
        case MPG123_ENC_SIGNED_8:
            sh->sample_format = AF_FORMAT_S8;
            break;
        case MPG123_ENC_SIGNED_16:
            sh->sample_format = AF_FORMAT_S16_NE;
            break;
        /* To stay compatible with the oldest libmpg123 headers, do not rely
         * on float and 32 bit encoding symbols being defined.
         * Those formats came later */
        case 0x1180: /* MPG123_ENC_SIGNED_32 */
            sh->sample_format = AF_FORMAT_S32_NE;
            break;
        case 0x200: /* MPG123_ENC_FLOAT_32 */
            sh->sample_format = AF_FORMAT_FLOAT_NE;
            break;
        default:
            /* This means we got a funny custom build of libmpg123 that only supports an unknown format. */
            mp_msg(MSGT_DECAUDIO, MSGL_ERR,
                   "Bad encoding from mpg123: %i.\n", encoding);
            return MPG123_ERR;
        }
        /* Going to decode directly to MPlayer's memory. It is important
         * to have MPG123_AUTO_RESAMPLE disabled for the buffer size
         * being an all-time limit. */
        sh->audio_out_minsize = 1152 * 2 * (af_fmt2bits(sh->sample_format) / 8);
        con->new_format = 0;
    }
    return ret;
}

/* This tries to extract a requested amount of decoded data.
 * Even when you request 0 bytes, it will feed enough input so that
 * the decoder _could_ have delivered something.
 * Returns byte count >= 0, -1 on error.
 *
 * Thoughts on exact pts keeping:
 * We have to assume that MPEG frames are cut in pieces by packet boundaries.
 * Also, it might be possible that the first packet does not contain enough
 * data to ensure initial stream sync... or re-sync on erroneous streams.
 * So we need something robust to relate the decoded byte count to the correct
 * time stamp. This is tricky, though. From the outside, you cannot tell if,
 * after having fed two packets until the first output arrives, one should
 * start counting from the first packet's pts or the second packet's.
 * So, let's just count from the last fed package's pts. If the packets are
 * exactly cut to MPEG frames, this will cause one frame mismatch in the
 * beginning (when mpg123 peeks ahead for the following header), but will
 * be corrected with the third frame already. One might add special code to
 * not increment the base pts past the first packet's after a resync before
 * the first decoded bytes arrived. */
static int decode_a_bit(sh_audio_t *sh, unsigned char *buf, int count)
{
    int ret = MPG123_OK;
    int got = 0;
    struct ad_mpg123_context *con = sh->context;

    /* There will be one MPG123_NEW_FORMAT message on first open.
     * This will be handled in init(). */
    do {
        size_t got_now = 0;
        /* Fetch new format now, after old data has been used. */
        if(con->new_format)
            ret = set_format(sh, con);

        /* Feed the decoder. This will only fire from the second round on. */
        if (ret == MPG123_NEED_MORE) {
            /* Feed more input data. */
            struct demux_packet *pkt = demux_read_packet(sh->gsh);
            if (!pkt)
                break;          /* Apparently that's it. EOF. */

            /* Next bytes from that presentation time. */
            if (pkt->pts != MP_NOPTS_VALUE) {
                sh->pts       = pkt->pts;
                sh->pts_bytes = 0;
            }

            /* Have to use mpg123_feed() to avoid decoding here. */
            ret = mpg123_feed(con->handle, pkt->buffer, pkt->len);
            talloc_free(pkt);
            if (ret == MPG123_ERR)
                break;

            /* Indication of format change is possible here (from mpg123_decode()). */
            if(ret == MPG123_NEW_FORMAT) {
                con->new_format = 1;
                if(got)
                    break; /* Do not switch format during a chunk. */

                ret = set_format(sh, con);
            }
        }
        /* Theoretically, mpg123 could return MPG123_DONE, so be prepared.
         * Should not happen in our usage, but it is a valid return code. */
        else if (ret == MPG123_ERR || ret == MPG123_DONE)
            break;

        /* Try to decode a bit. This is the return value that counts
         * for the loop condition. */
        if (!buf) { /* fake call just for feeding to get format */
            ret = set_format(sh, con);
        } else { /* This is the decoding. One frame at a time. */
            ret = mpg123_replace_buffer(con->handle, buf, count);
            if (ret == MPG123_OK)
                ret = mpg123_decode_frame(con->handle, NULL, NULL, &got_now);
        }

        got += got_now;
        sh->pts_bytes += got_now;

        /* Indication of format change should happen here. */
        if(ret == MPG123_NEW_FORMAT) {
            con->new_format = 1;
            if(got)
                break; /* Do not switch format during a chunk. */

            ret = set_format(sh, con);
        }

    } while (ret == MPG123_NEED_MORE || (got == 0 && count != 0));

    if (ret == MPG123_ERR) {
        mp_msg(MSGT_DECAUDIO, MSGL_ERR, "mpg123 decoding failed: %s\n",
               mpg123_strerror(con->handle));
    }

    return got;
}

/* Close, reopen stream. Feed data until we know the format of the stream.
 * 1 on success, 0 on error */
static int reopen_stream(sh_audio_t *sh)
{
    struct ad_mpg123_context *con = (struct ad_mpg123_context*) sh->context;

    mpg123_close(con->handle);
    /* No resetting of the context:
     * We do not want to loose the mean bitrate data. */

    /* Open and make sure we have fed enough data to get stream properties. */
    if (MPG123_OK == mpg123_open_feed(con->handle) &&
        /* Feed data until mpg123 is ready (has found stream beginning). */
        !decode_a_bit(sh, NULL, 0) &&
        set_format(sh, con) == MPG123_OK) { /* format setting again just for return value */
        return 1;
    } else {
        mp_msg(MSGT_DECAUDIO, MSGL_ERR,
               "mpg123 failed to reopen stream: %s\n",
               mpg123_strerror(con->handle));
        return 0;
    }
}

/* Now we really start accessing some data and determining file format.
 * Format now is allowed to change on-the-fly. Here is the only point
 * that has MPlayer react to errors. We have to pray that exceptional
 * erros in other places simply cannot occur. */
static int init(sh_audio_t *sh, const char *decoder)
{
    mpg123_id3v2 *v2;
    struct mpg123_frameinfo finfo;
    struct ad_mpg123_context *con = sh->context;

    con->new_format = 0;
    if (reopen_stream(sh) &&
        /* Get MPEG header info. */
        MPG123_OK == mpg123_info(con->handle, &finfo) &&
        /* Since we queried format, mpg123 should have read past ID3v2 tags.
         * We need to decide if printing of UTF-8 encoded text info is wanted. */
        MPG123_OK == mpg123_id3(con->handle, NULL, &v2)) {
        /* If we are here, we passed all hurdles. Yay! Extract the info. */
        print_header_compact(&finfo);
        /* Do we want to print out the UTF-8 Id3v2 info?
        if (v2)
            print_id3v2(v2); */

        /* Have kb/s, want B/s
         * For VBR, the first frame will be a bad estimate. */
        sh->i_bps = (finfo.bitrate ? finfo.bitrate : compute_bitrate(&finfo))
                    * 1000 / 8;
        con->delay      = 1;
        con->mean_rate  = 0.;
        con->mean_count = 0;
        con->vbr = (finfo.vbr != MPG123_CBR);

        return 1;
    } else {
        mp_msg(MSGT_DECAUDIO, MSGL_ERR, "mpg123 init error: %s\n",
               mpg123_strerror(con->handle));
        return 0;
    }
}

static void uninit(sh_audio_t *sh)
{
    struct ad_mpg123_context *con = (struct ad_mpg123_context*) sh->context;

    mpg123_close(con->handle);
    mpg123_delete(con->handle);
    free(sh->context);
    sh->context = NULL;
    mpg123_exit();
}

/* Update mean bitrate. This could be dropped if accurate time display
 * on audio file playback is not desired. */
static void update_info(sh_audio_t *sh)
{
    struct ad_mpg123_context *con = sh->context;
    if (con->vbr && --con->delay < 1) {
        struct mpg123_frameinfo finfo;
        if (MPG123_OK == mpg123_info(con->handle, &finfo)) {
            if (++con->mean_count > ((unsigned int) -1) / 2)
                con->mean_count = ((unsigned int) -1) / 4;

            /* Might not be numerically optimal, but works fine enough. */
            con->mean_rate = ((con->mean_count - 1) * con->mean_rate +
                              finfo.bitrate) / con->mean_count;
            sh->i_bps = (int) (con->mean_rate * 1000 / 8);

            con->delay = 10;
        }
    }
}

static int decode_audio(sh_audio_t *sh, unsigned char *buf, int minlen,
                        int maxlen)
{
    int bytes;

    bytes = decode_a_bit(sh, buf, maxlen);
    /* This EOF is ignored, apparently, until input data is exhausted. */
    if (bytes == 0)
        return -1;              /* EOF */

    update_info(sh);
    return bytes;
}

static int control(sh_audio_t *sh, int cmd, void *arg)
{
    switch (cmd) {
    case ADCTRL_RESYNC_STREAM:
        /* Close/reopen the stream for mpg123 to make sure it doesn't
         * think that it still knows the exact stream position.
         * Otherwise, we would have funny effects from the gapless code.
         * Oh, and it helps to minimize artifacts from jumping in the stream. */
        if (reopen_stream(sh)) {
            update_info(sh);
            return CONTROL_TRUE;
        } else {
            /* MPlayer ignores this case! It just keeps on decoding.
             * So we have to make sure resync never fails ... */
            mp_msg(MSGT_DECAUDIO, MSGL_ERR,
                   "mpg123 cannot reopen stream for resync.\n");
            return CONTROL_FALSE;
        }
        break;
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
    .preinit = preinit,
    .init = init,
    .uninit = uninit,
    .control = control,
    .decode_audio = decode_audio,
};
