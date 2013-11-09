/*
 * This file is part of MPlayer.
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

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>
#include <assert.h>

#include "config.h"
#include "talloc.h"

#include "mpvcore/mp_msg.h"
#include "mpvcore/options.h"
#include "mpvcore/mp_common.h"

#include "audio/mixer.h"
#include "audio/decode/dec_audio.h"
#include "audio/filter/af.h"
#include "audio/out/ao.h"
#include "demux/demux.h"

#include "mp_core.h"

static int build_afilter_chain(struct MPContext *mpctx)
{
    struct sh_audio *sh_audio = mpctx->sh_audio;
    struct ao *ao = mpctx->ao;
    struct MPOpts *opts = mpctx->opts;
    int new_srate;
    if (af_control_any_rev(sh_audio->afilter,
                           AF_CONTROL_PLAYBACK_SPEED | AF_CONTROL_SET,
                           &opts->playback_speed))
        new_srate = sh_audio->samplerate;
    else {
        new_srate = sh_audio->samplerate * opts->playback_speed;
        if (new_srate != ao->samplerate) {
            // limits are taken from libaf/af_resample.c
            if (new_srate < 8000)
                new_srate = 8000;
            if (new_srate > 192000)
                new_srate = 192000;
            opts->playback_speed = (double)new_srate / sh_audio->samplerate;
        }
    }
    return init_audio_filters(sh_audio, new_srate,
                              &ao->samplerate, &ao->channels, &ao->format);
}

static int recreate_audio_filters(struct MPContext *mpctx)
{
    assert(mpctx->sh_audio);

    // init audio filters:
    if (!build_afilter_chain(mpctx)) {
        MP_ERR(mpctx, "Couldn't find matching filter/ao format!\n");
        return -1;
    }

    mixer_reinit_audio(mpctx->mixer, mpctx->ao, mpctx->sh_audio->afilter);

    return 0;
}

int reinit_audio_filters(struct MPContext *mpctx)
{
    struct sh_audio *sh_audio = mpctx->sh_audio;
    if (!sh_audio)
        return -2;

    af_uninit(mpctx->sh_audio->afilter);
    if (af_init(mpctx->sh_audio->afilter) < 0)
        return -1;
    if (recreate_audio_filters(mpctx) < 0)
        return -1;

    return 0;
}

void reinit_audio_chain(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    init_demux_stream(mpctx, STREAM_AUDIO);
    if (!mpctx->sh_audio) {
        uninit_player(mpctx, INITIALIZED_AO);
        goto no_audio;
    }

    if (!(mpctx->initialized_flags & INITIALIZED_ACODEC)) {
        if (!init_best_audio_codec(mpctx->sh_audio, opts->audio_decoders))
            goto init_error;
        mpctx->initialized_flags |= INITIALIZED_ACODEC;
    }

    int ao_srate = opts->force_srate;
    int ao_format = opts->audio_output_format;
    struct mp_chmap ao_channels = {0};
    if (mpctx->initialized_flags & INITIALIZED_AO) {
        ao_srate    = mpctx->ao->samplerate;
        ao_format   = mpctx->ao->format;
        ao_channels = mpctx->ao->channels;
    } else {
        // Automatic downmix
        if (mp_chmap_is_stereo(&opts->audio_output_channels) &&
            !mp_chmap_is_stereo(&mpctx->sh_audio->channels))
        {
            mp_chmap_from_channels(&ao_channels, 2);
        }
    }

    // Determine what the filter chain outputs. build_afilter_chain() also
    // needs this for testing whether playback speed is changed by resampling
    // or using a special filter.
    if (!init_audio_filters(mpctx->sh_audio,  // preliminary init
                            // input:
                            mpctx->sh_audio->samplerate,
                            // output:
                            &ao_srate, &ao_channels, &ao_format)) {
        MP_ERR(mpctx, "Error at audio filter chain pre-init!\n");
        goto init_error;
    }

    if (!(mpctx->initialized_flags & INITIALIZED_AO)) {
        mpctx->initialized_flags |= INITIALIZED_AO;
        mp_chmap_remove_useless_channels(&ao_channels,
                                         &opts->audio_output_channels);
        mpctx->ao = ao_init_best(mpctx->global, mpctx->input,
                                 mpctx->encode_lavc_ctx, ao_srate, ao_format,
                                 ao_channels);
        struct ao *ao = mpctx->ao;
        if (!ao) {
            MP_ERR(mpctx, "Could not open/initialize audio device -> no sound.\n");
            goto init_error;
        }
        ao->buffer.start = talloc_new(ao);
        char *s = mp_audio_fmt_to_str(ao->samplerate, &ao->channels, ao->format);
        MP_INFO(mpctx, "AO: [%s] %s\n", ao->driver->name, s);
        talloc_free(s);
        MP_VERBOSE(mpctx, "AO: Description: %s\n", ao->driver->description);
    }

    if (recreate_audio_filters(mpctx) < 0)
        goto init_error;

    mpctx->syncing_audio = true;
    return;

init_error:
    uninit_player(mpctx, INITIALIZED_ACODEC | INITIALIZED_AO);
    cleanup_demux_stream(mpctx, STREAM_AUDIO);
no_audio:
    mpctx->current_track[STREAM_AUDIO] = NULL;
    MP_INFO(mpctx, "Audio: no audio\n");
}

// Return pts value corresponding to the end point of audio written to the
// ao so far.
double written_audio_pts(struct MPContext *mpctx)
{
    sh_audio_t *sh_audio = mpctx->sh_audio;
    if (!sh_audio)
        return MP_NOPTS_VALUE;

    double bps = sh_audio->channels.num * sh_audio->samplerate *
                 (af_fmt2bits(sh_audio->sample_format) / 8);

    // first calculate the end pts of audio that has been output by decoder
    double a_pts = sh_audio->pts;
    if (a_pts == MP_NOPTS_VALUE)
        return MP_NOPTS_VALUE;

    // sh_audio->pts is the timestamp of the latest input packet with
    // known pts that the decoder has decoded. sh_audio->pts_bytes is
    // the amount of bytes the decoder has written after that timestamp.
    a_pts += sh_audio->pts_bytes / bps;

    // Now a_pts hopefully holds the pts for end of audio from decoder.
    // Subtract data in buffers between decoder and audio out.

    // Decoded but not filtered
    a_pts -= sh_audio->a_buffer_len / bps;

    // Data buffered in audio filters, measured in bytes of "missing" output
    double buffered_output = af_calc_delay(sh_audio->afilter);

    // Data that was ready for ao but was buffered because ao didn't fully
    // accept everything to internal buffers yet
    buffered_output += mpctx->ao->buffer.len;

    // Filters divide audio length by playback_speed, so multiply by it
    // to get the length in original units without speedup or slowdown
    a_pts -= buffered_output * mpctx->opts->playback_speed / mpctx->ao->bps;

    return a_pts + mpctx->video_offset;
}

// Return pts value corresponding to currently playing audio.
double playing_audio_pts(struct MPContext *mpctx)
{
    double pts = written_audio_pts(mpctx);
    if (pts == MP_NOPTS_VALUE)
        return pts;
    return pts - mpctx->opts->playback_speed * ao_get_delay(mpctx->ao);
}

static int write_to_ao(struct MPContext *mpctx, void *data, int len, int flags,
                       double pts)
{
    if (mpctx->paused)
        return 0;
    struct ao *ao = mpctx->ao;
    double bps = ao->bps / mpctx->opts->playback_speed;
    int unitsize = ao->channels.num * af_fmt2bits(ao->format) / 8;
    ao->pts = pts;
    int played = ao_play(mpctx->ao, data, len, flags);
    assert(played <= len);
    assert(played % unitsize == 0);
    if (played > 0) {
        mpctx->shown_aframes += played / unitsize;
        mpctx->delay += played / bps;
        // Keep correct pts for remaining data - could be used to flush
        // remaining buffer when closing ao.
        ao->pts += played / bps;
        return played;
    }
    return 0;
}

#define ASYNC_PLAY_DONE -3
static int audio_start_sync(struct MPContext *mpctx, int playsize)
{
    struct ao *ao = mpctx->ao;
    struct MPOpts *opts = mpctx->opts;
    sh_audio_t * const sh_audio = mpctx->sh_audio;
    int res;

    // Timing info may not be set without
    res = decode_audio(sh_audio, &ao->buffer, 1);
    if (res < 0)
        return res;

    int bytes;
    bool did_retry = false;
    double written_pts;
    double bps = ao->bps / opts->playback_speed;
    bool hrseek = mpctx->hrseek_active;   // audio only hrseek
    mpctx->hrseek_active = false;
    while (1) {
        written_pts = written_audio_pts(mpctx);
        double ptsdiff;
        if (hrseek)
            ptsdiff = written_pts - mpctx->hrseek_pts;
        else
            ptsdiff = written_pts - mpctx->sh_video->pts - mpctx->delay
                      - mpctx->audio_delay;
        bytes = ptsdiff * bps;
        bytes -= bytes % (ao->channels.num * af_fmt2bits(ao->format) / 8);

        // ogg demuxers give packets without timing
        if (written_pts <= 1 && sh_audio->pts == MP_NOPTS_VALUE) {
            if (!did_retry) {
                // Try to read more data to see packets that have pts
                res = decode_audio(sh_audio, &ao->buffer, ao->bps);
                if (res < 0)
                    return res;
                did_retry = true;
                continue;
            }
            bytes = 0;
        }

        if (fabs(ptsdiff) > 300 || isnan(ptsdiff))   // pts reset or just broken?
            bytes = 0;

        if (bytes > 0)
            break;

        mpctx->syncing_audio = false;
        int a = MPMIN(-bytes, MPMAX(playsize, 20000));
        res = decode_audio(sh_audio, &ao->buffer, a);
        bytes += ao->buffer.len;
        if (bytes >= 0) {
            memmove(ao->buffer.start,
                    ao->buffer.start + ao->buffer.len - bytes, bytes);
            ao->buffer.len = bytes;
            if (res < 0)
                return res;
            return decode_audio(sh_audio, &ao->buffer, playsize);
        }
        ao->buffer.len = 0;
        if (res < 0)
            return res;
    }
    if (hrseek)
        // Don't add silence in audio-only case even if position is too late
        return 0;
    int fillbyte = 0;
    if ((ao->format & AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
        fillbyte = 0x80;
    if (bytes >= playsize) {
        /* This case could fall back to the one below with
         * bytes = playsize, but then silence would keep accumulating
         * in a_out_buffer if the AO accepts less data than it asks for
         * in playsize. */
        char *p = malloc(playsize);
        memset(p, fillbyte, playsize);
        write_to_ao(mpctx, p, playsize, 0, written_pts - bytes / bps);
        free(p);
        return ASYNC_PLAY_DONE;
    }
    mpctx->syncing_audio = false;
    decode_audio_prepend_bytes(&ao->buffer, bytes, fillbyte);
    return decode_audio(sh_audio, &ao->buffer, playsize);
}

int fill_audio_out_buffers(struct MPContext *mpctx, double endpts)
{
    struct MPOpts *opts = mpctx->opts;
    struct ao *ao = mpctx->ao;
    int playsize;
    int playflags = 0;
    bool audio_eof = false;
    bool signal_eof = false;
    bool partial_fill = false;
    sh_audio_t * const sh_audio = mpctx->sh_audio;
    bool modifiable_audio_format = !(ao->format & AF_FORMAT_SPECIAL_MASK);
    int unitsize = ao->channels.num * af_fmt2bits(ao->format) / 8;

    if (mpctx->paused)
        playsize = 1;   // just initialize things (audio pts at least)
    else
        playsize = ao_get_space(ao);

    // Coming here with hrseek_active still set means audio-only
    if (!mpctx->sh_video || !mpctx->sync_audio_to_video)
        mpctx->syncing_audio = false;
    if (!opts->initial_audio_sync || !modifiable_audio_format) {
        mpctx->syncing_audio = false;
        mpctx->hrseek_active = false;
    }

    int res;
    if (mpctx->syncing_audio || mpctx->hrseek_active)
        res = audio_start_sync(mpctx, playsize);
    else
        res = decode_audio(sh_audio, &ao->buffer, playsize);

    if (res < 0) {  // EOF, error or format change
        if (res == -2) {
            /* The format change isn't handled too gracefully. A more precise
             * implementation would require draining buffered old-format audio
             * while displaying video, then doing the output format switch.
             */
            if (!mpctx->opts->gapless_audio)
                uninit_player(mpctx, INITIALIZED_AO);
            reinit_audio_chain(mpctx);
            return -1;
        } else if (res == ASYNC_PLAY_DONE)
            return 0;
        else if (demux_stream_eof(mpctx->sh_audio->gsh))
            audio_eof = true;
    }

    if (endpts != MP_NOPTS_VALUE && modifiable_audio_format) {
        double bytes = (endpts - written_audio_pts(mpctx) + mpctx->audio_delay)
                       * ao->bps / opts->playback_speed;
        if (playsize > bytes) {
            playsize = MPMAX(bytes, 0);
            audio_eof = true;
            partial_fill = true;
        }
    }

    assert(ao->buffer.len % unitsize == 0);
    if (playsize > ao->buffer.len) {
        partial_fill = true;
        playsize = ao->buffer.len;
    }
    playsize -= playsize % unitsize;
    if (!playsize)
        return partial_fill && audio_eof ? -2 : -partial_fill;

    if (audio_eof && partial_fill) {
        if (opts->gapless_audio) {
            // With gapless audio, delay this to ao_uninit. There must be only
            // 1 final chunk, and that is handled when calling ao_uninit().
            signal_eof = true;
        } else {
            playflags |= AOPLAY_FINAL_CHUNK;
        }
    }

    assert(ao->buffer_playable_size <= ao->buffer.len);
    int played = write_to_ao(mpctx, ao->buffer.start, playsize, playflags,
                             written_audio_pts(mpctx));
    ao->buffer_playable_size = playsize - played;

    if (played > 0) {
        ao->buffer.len -= played;
        memmove(ao->buffer.start, ao->buffer.start + played, ao->buffer.len);
    } else if (!mpctx->paused && audio_eof && ao_get_delay(ao) < .04) {
        // Sanity check to avoid hanging in case current ao doesn't output
        // partial chunks and doesn't check for AOPLAY_FINAL_CHUNK
        signal_eof = true;
    }

    return signal_eof ? -2 : -partial_fill;
}

// Drop data queued for output, or which the AO is currently outputting.
void clear_audio_output_buffers(struct MPContext *mpctx)
{
    if (mpctx->ao) {
        ao_reset(mpctx->ao);
        mpctx->ao->buffer.len = 0;
        mpctx->ao->buffer_playable_size = 0;
    }
}

// Drop decoded data queued for filtering.
void clear_audio_decode_buffers(struct MPContext *mpctx)
{
    if (mpctx->sh_audio)
        mpctx->sh_audio->a_buffer_len = 0;
}
