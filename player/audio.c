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

#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include "config.h"
#include "mpv_talloc.h"

#include "common/msg.h"
#include "common/encode.h"
#include "options/options.h"
#include "common/common.h"
#include "osdep/timer.h"

#include "audio/audio_buffer.h"
#include "audio/format.h"
#include "audio/out/ao.h"
#include "demux/demux.h"
#include "filters/f_decoder_wrapper.h"

#include "core.h"
#include "command.h"

enum {
    AD_OK = 0,
    AD_EOF = -2,
    AD_WAIT = -4,
};

// Try to reuse the existing filters to change playback speed. If it works,
// return true; if filter recreation is needed, return false.
static void update_speed_filters(struct MPContext *mpctx)
{
    struct ao_chain *ao_c = mpctx->ao_chain;
    if (!ao_c)
        return;

    double speed = mpctx->opts->playback_speed;
    double resample = mpctx->speed_factor_a;

    if (!mpctx->opts->pitch_correction) {
        resample *= speed;
        speed = 1.0;
    }

    mp_output_chain_set_audio_speed(ao_c->filter, speed, resample);
}

static int recreate_audio_filters(struct MPContext *mpctx)
{
    struct ao_chain *ao_c = mpctx->ao_chain;
    assert(ao_c);

    if (!mp_output_chain_update_filters(ao_c->filter, mpctx->opts->af_settings))
        goto fail;

    update_speed_filters(mpctx);

    mp_notify(mpctx, MPV_EVENT_AUDIO_RECONFIG, NULL);

    return 0;

fail:
    MP_ERR(mpctx, "Audio filter initialized failed!\n");
    return -1;
}

int reinit_audio_filters(struct MPContext *mpctx)
{
    struct ao_chain *ao_c = mpctx->ao_chain;
    if (!ao_c)
        return 0;

    double delay = mp_output_get_measured_total_delay(ao_c->filter);

    if (recreate_audio_filters(mpctx) < 0)
        return -1;

    double ndelay = mp_output_get_measured_total_delay(ao_c->filter);

    // Only force refresh if the amount of dropped buffered data is going to
    // cause "issues" for the A/V sync logic.
    if (mpctx->audio_status == STATUS_PLAYING && delay - ndelay >= 0.2)
        issue_refresh_seek(mpctx, MPSEEK_EXACT);
    return 1;
}

static double db_gain(double db)
{
    return pow(10.0, db/20.0);
}

static float compute_replaygain(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;

    float rgain = 1.0;

    struct replaygain_data *rg = NULL;
    struct track *track = mpctx->current_track[0][STREAM_AUDIO];
    if (track)
        rg = track->stream->codec->replaygain_data;
    if (opts->rgain_mode && rg) {
        MP_VERBOSE(mpctx, "Replaygain: Track=%f/%f Album=%f/%f\n",
                   rg->track_gain, rg->track_peak,
                   rg->album_gain, rg->album_peak);

        float gain, peak;
        if (opts->rgain_mode == 1) {
            gain = rg->track_gain;
            peak = rg->track_peak;
        } else {
            gain = rg->album_gain;
            peak = rg->album_peak;
        }

        gain += opts->rgain_preamp;
        rgain = db_gain(gain);

        MP_VERBOSE(mpctx, "Applying replay-gain: %f\n", rgain);

        if (!opts->rgain_clip) { // clipping prevention
            rgain = MPMIN(rgain, 1.0 / peak);
            MP_VERBOSE(mpctx, "...with clipping prevention: %f\n", rgain);
        }
    } else if (opts->rgain_fallback) {
        rgain = db_gain(opts->rgain_fallback);
        MP_VERBOSE(mpctx, "Applying fallback gain: %f\n", rgain);
    }

    return rgain;
}

// Called when opts->softvol_volume or opts->softvol_mute were changed.
void audio_update_volume(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct ao_chain *ao_c = mpctx->ao_chain;
    if (!ao_c || !ao_c->ao)
        return;

    float gain = MPMAX(opts->softvol_volume / 100.0, 0);
    gain = pow(gain, 3);
    gain *= compute_replaygain(mpctx);
    if (opts->softvol_mute == 1)
        gain = 0.0;

    ao_set_gain(ao_c->ao, gain);
}

// Call this if opts->playback_speed or mpctx->speed_factor_* change.
void update_playback_speed(struct MPContext *mpctx)
{
    mpctx->audio_speed = mpctx->opts->playback_speed * mpctx->speed_factor_a;
    mpctx->video_speed = mpctx->opts->playback_speed * mpctx->speed_factor_v;

    update_speed_filters(mpctx);
}

static void ao_chain_reset_state(struct ao_chain *ao_c)
{
    ao_c->last_out_pts = MP_NOPTS_VALUE;
    TA_FREEP(&ao_c->output_frame);
    ao_c->out_eof = false;

    mp_audio_buffer_clear(ao_c->ao_buffer);
}

void reset_audio_state(struct MPContext *mpctx)
{
    if (mpctx->ao_chain)
        ao_chain_reset_state(mpctx->ao_chain);
    mpctx->audio_status = mpctx->ao_chain ? STATUS_SYNCING : STATUS_EOF;
    mpctx->delay = 0;
    mpctx->audio_drop_throttle = 0;
    mpctx->audio_stat_start = 0;
}

void uninit_audio_out(struct MPContext *mpctx)
{
    if (mpctx->ao) {
        // Note: with gapless_audio, stop_play is not correctly set
        if (mpctx->opts->gapless_audio || mpctx->stop_play == AT_END_OF_FILE)
            ao_drain(mpctx->ao);
        ao_uninit(mpctx->ao);

        mp_notify(mpctx, MPV_EVENT_AUDIO_RECONFIG, NULL);
    }
    mpctx->ao = NULL;
    TA_FREEP(&mpctx->ao_filter_fmt);
}

static void ao_chain_uninit(struct ao_chain *ao_c)
{
    struct track *track = ao_c->track;
    if (track) {
        assert(track->ao_c == ao_c);
        track->ao_c = NULL;
        if (ao_c->dec_src)
            assert(track->dec->f->pins[0] == ao_c->dec_src);
        talloc_free(track->dec->f);
        track->dec = NULL;
    }

    if (ao_c->filter_src)
        mp_pin_disconnect(ao_c->filter_src);

    talloc_free(ao_c->filter->f);
    talloc_free(ao_c->output_frame);
    talloc_free(ao_c->ao_buffer);
    talloc_free(ao_c);
}

void uninit_audio_chain(struct MPContext *mpctx)
{
    if (mpctx->ao_chain) {
        ao_chain_uninit(mpctx->ao_chain);
        mpctx->ao_chain = NULL;

        mpctx->audio_status = STATUS_EOF;

        mp_notify(mpctx, MPV_EVENT_AUDIO_RECONFIG, NULL);
    }
}

static char *audio_config_to_str_buf(char *buf, size_t buf_sz, int rate,
                                     int format, struct mp_chmap channels)
{
    char ch[128];
    mp_chmap_to_str_buf(ch, sizeof(ch), &channels);
    char *hr_ch = mp_chmap_to_str_hr(&channels);
    if (strcmp(hr_ch, ch) != 0)
        mp_snprintf_cat(ch, sizeof(ch), " (%s)", hr_ch);
    snprintf(buf, buf_sz, "%dHz %s %dch %s", rate,
             ch, channels.num, af_fmt_to_str(format));
    return buf;
}

// Decide whether on a format change, we should reinit the AO.
static bool keep_weak_gapless_format(struct mp_aframe *old, struct mp_aframe* new)
{
    bool res = false;
    struct mp_aframe *new_mod = mp_aframe_new_ref(new);
    if (!new_mod)
        abort();

    // If the sample formats are compatible (== libswresample generally can
    // convert them), keep the AO. On other changes, recreate it.

    int old_fmt = mp_aframe_get_format(old);
    int new_fmt = mp_aframe_get_format(new);

    if (af_format_conversion_score(old_fmt, new_fmt) == INT_MIN)
        goto done; // completely incompatible formats

    if (!mp_aframe_set_format(new_mod, old_fmt))
        goto done;

    res = mp_aframe_config_equals(old, new_mod);

done:
    talloc_free(new_mod);
    return res;
}

static void reinit_audio_filters_and_output(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    struct ao_chain *ao_c = mpctx->ao_chain;
    assert(ao_c);
    struct track *track = ao_c->track;

    if (!ao_c->filter->ao_needs_update)
        return;

    TA_FREEP(&ao_c->output_frame); // stale?

    // The "ideal" filter output format
    struct mp_aframe *out_fmt = mp_aframe_new_ref(ao_c->filter->output_aformat);
    if (!out_fmt)
        abort();

    if (!mp_aframe_config_is_valid(out_fmt)) {
        talloc_free(out_fmt);
        goto init_error;
    }

    if (af_fmt_is_pcm(mp_aframe_get_format(out_fmt))) {
        if (opts->force_srate)
            mp_aframe_set_rate(out_fmt, opts->force_srate);
        if (opts->audio_output_format)
            mp_aframe_set_format(out_fmt, opts->audio_output_format);
        if (opts->audio_output_channels.num_chmaps == 1)
            mp_aframe_set_chmap(out_fmt, &opts->audio_output_channels.chmaps[0]);
    }

    // Weak gapless audio: if the filter output format is the same as the
    // previous one, keep the AO and don't reinit anything.
    // Strong gapless: always keep the AO
    if ((mpctx->ao_filter_fmt && mpctx->ao && opts->gapless_audio < 0 &&
         keep_weak_gapless_format(mpctx->ao_filter_fmt, out_fmt)) ||
        (mpctx->ao && opts->gapless_audio > 0))
    {
        mp_output_chain_set_ao(ao_c->filter, mpctx->ao);
        talloc_free(out_fmt);
        return;
    }

    uninit_audio_out(mpctx);

    int out_rate = mp_aframe_get_rate(out_fmt);
    int out_format = mp_aframe_get_format(out_fmt);
    struct mp_chmap out_channels = {0};
    mp_aframe_get_chmap(out_fmt, &out_channels);

    int ao_flags = 0;
    bool spdif_fallback = af_fmt_is_spdif(out_format) &&
                          ao_c->spdif_passthrough;

    if (opts->ao_null_fallback && !spdif_fallback)
        ao_flags |= AO_INIT_NULL_FALLBACK;

    if (opts->audio_stream_silence)
        ao_flags |= AO_INIT_STREAM_SILENCE;

    if (opts->audio_exclusive)
        ao_flags |= AO_INIT_EXCLUSIVE;

    if (af_fmt_is_pcm(out_format)) {
        if (!opts->audio_output_channels.set ||
            opts->audio_output_channels.auto_safe)
            ao_flags |= AO_INIT_SAFE_MULTICHANNEL_ONLY;

        mp_chmap_sel_list(&out_channels,
                          opts->audio_output_channels.chmaps,
                          opts->audio_output_channels.num_chmaps);
    }

    mpctx->ao_filter_fmt = out_fmt;

    mpctx->ao = ao_init_best(mpctx->global, ao_flags, mp_wakeup_core_cb,
                             mpctx, mpctx->encode_lavc_ctx, out_rate,
                             out_format, out_channels);
    ao_c->ao = mpctx->ao;

    int ao_rate = 0;
    int ao_format = 0;
    struct mp_chmap ao_channels = {0};
    if (mpctx->ao)
        ao_get_format(mpctx->ao, &ao_rate, &ao_format, &ao_channels);

    // Verify passthrough format was not changed.
    if (mpctx->ao && af_fmt_is_spdif(out_format)) {
        if (out_rate != ao_rate || out_format != ao_format ||
            !mp_chmap_equals(&out_channels, &ao_channels))
        {
            MP_ERR(mpctx, "Passthrough format unsupported.\n");
            ao_uninit(mpctx->ao);
            mpctx->ao = NULL;
            ao_c->ao = NULL;
        }
    }

    if (!mpctx->ao) {
        // If spdif was used, try to fallback to PCM.
        if (spdif_fallback && ao_c->track && ao_c->track->dec) {
            MP_VERBOSE(mpctx, "Falling back to PCM output.\n");
            ao_c->spdif_passthrough = false;
            ao_c->spdif_failed = true;
            ao_c->track->dec->try_spdif = false;
            if (!mp_decoder_wrapper_reinit(ao_c->track->dec))
                goto init_error;
            reset_audio_state(mpctx);
            mp_output_chain_reset_harder(ao_c->filter);
            mp_wakeup_core(mpctx); // reinit with new format next time
            return;
        }

        MP_ERR(mpctx, "Could not open/initialize audio device -> no sound.\n");
        mpctx->error_playing = MPV_ERROR_AO_INIT_FAILED;
        goto init_error;
    }

    mp_audio_buffer_reinit_fmt(ao_c->ao_buffer, ao_format, &ao_channels,
                                ao_rate);

    char tmp[80];
    MP_INFO(mpctx, "AO: [%s] %s\n", ao_get_name(mpctx->ao),
            audio_config_to_str_buf(tmp, sizeof(tmp), ao_rate, ao_format,
                                    ao_channels));
    MP_VERBOSE(mpctx, "AO: Description: %s\n", ao_get_description(mpctx->ao));
    update_window_title(mpctx, true);

    ao_c->ao_resume_time =
        opts->audio_wait_open > 0 ? mp_time_sec() + opts->audio_wait_open : 0;

    mp_output_chain_set_ao(ao_c->filter, mpctx->ao);

    audio_update_volume(mpctx);

    mp_notify(mpctx, MPV_EVENT_AUDIO_RECONFIG, NULL);

    return;

init_error:
    uninit_audio_chain(mpctx);
    uninit_audio_out(mpctx);
    error_on_track(mpctx, track);
}

int init_audio_decoder(struct MPContext *mpctx, struct track *track)
{
    assert(!track->dec);
    if (!track->stream)
        goto init_error;

    track->dec = mp_decoder_wrapper_create(mpctx->filter_root, track->stream);
    if (!track->dec)
        goto init_error;

    if (track->ao_c)
        track->dec->try_spdif = true;

    if (!mp_decoder_wrapper_reinit(track->dec))
        goto init_error;

    return 1;

init_error:
    if (track->sink)
        mp_pin_disconnect(track->sink);
    track->sink = NULL;
    error_on_track(mpctx, track);
    return 0;
}

void reinit_audio_chain(struct MPContext *mpctx)
{
    struct track *track = NULL;
    track = mpctx->current_track[0][STREAM_AUDIO];
    if (!track || !track->stream) {
        uninit_audio_out(mpctx);
        error_on_track(mpctx, track);
        return;
    }
    reinit_audio_chain_src(mpctx, track);
}

// (track=NULL creates a blank chain, used for lavfi-complex)
void reinit_audio_chain_src(struct MPContext *mpctx, struct track *track)
{
    assert(!mpctx->ao_chain);

    mp_notify(mpctx, MPV_EVENT_AUDIO_RECONFIG, NULL);

    struct ao_chain *ao_c = talloc_zero(NULL, struct ao_chain);
    mpctx->ao_chain = ao_c;
    ao_c->log = mpctx->log;
    ao_c->filter =
        mp_output_chain_create(mpctx->filter_root, MP_OUTPUT_CHAIN_AUDIO);
    ao_c->spdif_passthrough = true;
    ao_c->last_out_pts = MP_NOPTS_VALUE;
    ao_c->ao_buffer = mp_audio_buffer_create(NULL);
    ao_c->ao = mpctx->ao;

    if (track) {
        ao_c->track = track;
        track->ao_c = ao_c;
        if (!init_audio_decoder(mpctx, track))
            goto init_error;
        ao_c->dec_src = track->dec->f->pins[0];
        mp_pin_connect(ao_c->filter->f->pins[0], ao_c->dec_src);
    }

    reset_audio_state(mpctx);

    if (recreate_audio_filters(mpctx) < 0)
        goto init_error;

    if (mpctx->ao) {
        int rate;
        int format;
        struct mp_chmap channels;
        ao_get_format(mpctx->ao, &rate, &format, &channels);
        mp_audio_buffer_reinit_fmt(ao_c->ao_buffer, format, &channels, rate);

        audio_update_volume(mpctx);
    }

    mp_wakeup_core(mpctx);
    return;

init_error:
    uninit_audio_chain(mpctx);
    uninit_audio_out(mpctx);
    error_on_track(mpctx, track);
}

// Return pts value corresponding to the end point of audio written to the
// ao so far.
double written_audio_pts(struct MPContext *mpctx)
{
    struct ao_chain *ao_c = mpctx->ao_chain;
    if (!ao_c)
        return MP_NOPTS_VALUE;

    // end pts of audio that has been output by filters
    double a_pts = ao_c->last_out_pts;
    if (a_pts == MP_NOPTS_VALUE)
        return MP_NOPTS_VALUE;

    // Data that was ready for ao but was buffered because ao didn't fully
    // accept everything to internal buffers yet. This also does not correctly
    // track playback speed, so we use the current speed.
    a_pts -= mp_audio_buffer_seconds(ao_c->ao_buffer) * mpctx->audio_speed;

    return a_pts;
}

// Return pts value corresponding to currently playing audio.
double playing_audio_pts(struct MPContext *mpctx)
{
    double pts = written_audio_pts(mpctx);
    if (pts == MP_NOPTS_VALUE || !mpctx->ao)
        return pts;
    return pts - mpctx->audio_speed * ao_get_delay(mpctx->ao);
}

static int write_to_ao(struct MPContext *mpctx, uint8_t **planes, int samples,
                       int flags)
{
    if (mpctx->paused)
        return 0;
    struct ao *ao = mpctx->ao;
    int samplerate;
    int format;
    struct mp_chmap channels;
    ao_get_format(ao, &samplerate, &format, &channels);
    encode_lavc_set_audio_pts(mpctx->encode_lavc_ctx, playing_audio_pts(mpctx));
    if (samples == 0)
        return 0;
    double real_samplerate = samplerate / mpctx->audio_speed;
    int played = ao_play(mpctx->ao, (void **)planes, samples, flags);
    assert(played <= samples);
    if (played > 0) {
        mpctx->shown_aframes += played;
        mpctx->delay += played / real_samplerate;
        mpctx->written_audio += played / (double)samplerate;
        return played;
    }
    return 0;
}

static void dump_audio_stats(struct MPContext *mpctx)
{
    if (!mp_msg_test(mpctx->log, MSGL_STATS))
        return;
    if (mpctx->audio_status != STATUS_PLAYING || !mpctx->ao || mpctx->paused) {
        mpctx->audio_stat_start = 0;
        return;
    }

    double delay = ao_get_delay(mpctx->ao);
    if (!mpctx->audio_stat_start) {
        mpctx->audio_stat_start = mp_time_us();
        mpctx->written_audio = delay;
    }
    double current_audio = mpctx->written_audio - delay;
    double current_time = (mp_time_us() - mpctx->audio_stat_start) / 1e6;
    MP_STATS(mpctx, "value %f ao-dev", current_audio - current_time);
}

// Return the number of samples that must be skipped or prepended to reach the
// target audio pts after a seek (for A/V sync or hr-seek).
// Return value (*skip):
//   >0: skip this many samples
//   =0: don't do anything
//   <0: prepend this many samples of silence
// Returns false if PTS is not known yet.
static bool get_sync_samples(struct MPContext *mpctx, int *skip)
{
    struct MPOpts *opts = mpctx->opts;
    *skip = 0;

    if (mpctx->audio_status != STATUS_SYNCING)
        return true;

    int ao_rate;
    int ao_format;
    struct mp_chmap ao_channels;
    ao_get_format(mpctx->ao, &ao_rate, &ao_format, &ao_channels);

    double play_samplerate = ao_rate / mpctx->audio_speed;

    if (!opts->initial_audio_sync) {
        mpctx->audio_status = STATUS_FILLING;
        return true;
    }

    double written_pts = written_audio_pts(mpctx);
    if (written_pts == MP_NOPTS_VALUE &&
        !mp_audio_buffer_samples(mpctx->ao_chain->ao_buffer))
        return false; // no audio read yet

    bool sync_to_video = mpctx->vo_chain && !mpctx->vo_chain->is_coverart &&
                         mpctx->video_status != STATUS_EOF;

    double sync_pts = MP_NOPTS_VALUE;
    if (sync_to_video) {
        if (mpctx->video_status < STATUS_READY)
            return false; // wait until we know a video PTS
        if (mpctx->video_pts != MP_NOPTS_VALUE)
            sync_pts = mpctx->video_pts - opts->audio_delay;
    } else if (mpctx->hrseek_active) {
        sync_pts = mpctx->hrseek_pts;
    } else {
        // If audio-only is enabled mid-stream during playback, sync accordingly.
        sync_pts = mpctx->playback_pts;
    }
    if (sync_pts == MP_NOPTS_VALUE) {
        mpctx->audio_status = STATUS_FILLING;
        return true; // syncing disabled
    }

    double ptsdiff = written_pts - sync_pts;

    // Missing timestamp, or PTS reset, or just broken.
    if (written_pts == MP_NOPTS_VALUE) {
        MP_WARN(mpctx, "Failed audio resync.\n");
        mpctx->audio_status = STATUS_FILLING;
        return true;
    }
    ptsdiff = MPCLAMP(ptsdiff, -3600, 3600);

    int align = af_format_sample_alignment(ao_format);
    *skip = (int)(-ptsdiff * play_samplerate) / align * align;
    return true;
}


static bool copy_output(struct MPContext *mpctx, struct ao_chain *ao_c,
                        int minsamples, double endpts, bool *seteof)
{
    struct mp_audio_buffer *outbuf = ao_c->ao_buffer;

    int ao_rate;
    int ao_format;
    struct mp_chmap ao_channels;
    ao_get_format(ao_c->ao, &ao_rate, &ao_format, &ao_channels);

    while (mp_audio_buffer_samples(outbuf) < minsamples) {
        int cursamples = mp_audio_buffer_samples(outbuf);
        int maxsamples = INT_MAX;
        if (endpts != MP_NOPTS_VALUE) {
            double rate = ao_rate / mpctx->audio_speed;
            double curpts = written_audio_pts(mpctx);
            if (curpts != MP_NOPTS_VALUE) {
                double remaining =
                    (endpts - curpts - mpctx->opts->audio_delay) * rate;
                maxsamples = MPCLAMP(remaining, 0, INT_MAX);
            }
        }

        if (!ao_c->output_frame || !mp_aframe_get_size(ao_c->output_frame)) {
            TA_FREEP(&ao_c->output_frame);

            struct mp_frame frame = mp_pin_out_read(ao_c->filter->f->pins[1]);
            if (frame.type == MP_FRAME_AUDIO) {
                ao_c->output_frame = frame.data;
                ao_c->out_eof = false;
                ao_c->last_out_pts = mp_aframe_end_pts(ao_c->output_frame);
            } else if (frame.type == MP_FRAME_EOF) {
                ao_c->out_eof = true;
            } else if (frame.type) {
                MP_ERR(mpctx, "unknown frame type\n");
                mp_frame_unref(&frame);
            }
        }

        // out of data
        if (!ao_c->output_frame) {
            if (ao_c->out_eof) {
                *seteof = true;
                return true;
            }
            return false;
        }

        if (cursamples + mp_aframe_get_size(ao_c->output_frame) > maxsamples) {
            if (cursamples < maxsamples) {
                uint8_t **data = mp_aframe_get_data_ro(ao_c->output_frame);
                mp_audio_buffer_append(outbuf, (void **)data,
                                       maxsamples - cursamples);
                mp_aframe_skip_samples(ao_c->output_frame,
                                       maxsamples - cursamples);
            }
            *seteof = true;
            return true;
        }

        uint8_t **data = mp_aframe_get_data_ro(ao_c->output_frame);
        mp_audio_buffer_append(outbuf, (void **)data,
                               mp_aframe_get_size(ao_c->output_frame));
        TA_FREEP(&ao_c->output_frame);
    }
    return true;
}

/* Try to get at least minsamples decoded+filtered samples in outbuf
 * (total length including possible existing data).
 * Return 0 on success, or negative AD_* error code.
 * In the former case outbuf has at least minsamples buffered on return.
 * In case of EOF/error it might or might not be. */
static int filter_audio(struct MPContext *mpctx, struct mp_audio_buffer *outbuf,
                        int minsamples)
{
    struct ao_chain *ao_c = mpctx->ao_chain;

    double endpts = get_play_end_pts(mpctx);

    bool eof = false;
    if (!copy_output(mpctx, ao_c, minsamples, endpts, &eof))
        return AD_WAIT;
    return eof ? AD_EOF : AD_OK;
}

void reload_audio_output(struct MPContext *mpctx)
{
    if (!mpctx->ao)
        return;

    ao_reset(mpctx->ao);
    uninit_audio_out(mpctx);
    reinit_audio_filters(mpctx); // mostly to issue refresh seek

    struct ao_chain *ao_c = mpctx->ao_chain;

    if (ao_c) {
        reset_audio_state(mpctx);
        mp_output_chain_reset_harder(ao_c->filter);
    }

    // Whether we can use spdif might have changed. If we failed to use spdif
    // in the previous initialization, try it with spdif again (we'll fallback
    // to PCM again if necessary).
    if (ao_c && ao_c->track) {
        struct mp_decoder_wrapper *dec = ao_c->track->dec;
        if (dec && ao_c->spdif_failed) {
            ao_c->spdif_passthrough = true;
            ao_c->spdif_failed = false;
            dec->try_spdif = true;
            if (!mp_decoder_wrapper_reinit(dec)) {
                MP_ERR(mpctx, "Error reinitializing audio.\n");
                error_on_track(mpctx, ao_c->track);
            }
        }
    }

    mp_wakeup_core(mpctx);
}

void fill_audio_out_buffers(struct MPContext *mpctx)
{
    struct MPOpts *opts = mpctx->opts;
    bool was_eof = mpctx->audio_status == STATUS_EOF;

    dump_audio_stats(mpctx);

    if (mpctx->ao && ao_query_and_reset_events(mpctx->ao, AO_EVENT_RELOAD))
        reload_audio_output(mpctx);

    if (mpctx->ao && ao_query_and_reset_events(mpctx->ao,
                                               AO_EVENT_INITIAL_UNBLOCK))
        ao_unblock(mpctx->ao);

    struct ao_chain *ao_c = mpctx->ao_chain;
    if (!ao_c)
        return;

    if (ao_c->filter->failed_output_conversion) {
        error_on_track(mpctx, ao_c->track);
        return;
    }

    // (if AO is set due to gapless from previous file, then we can try to
    // filter normally until the filter tells us to change the AO)
    if (!mpctx->ao) {
        // Probe the initial audio format.
        mp_pin_out_request_data(ao_c->filter->f->pins[1]);
        reinit_audio_filters_and_output(mpctx);
        if (ao_c->filter->got_output_eof &&
            mpctx->audio_status != STATUS_EOF)
        {
            mpctx->audio_status = STATUS_EOF;
            MP_VERBOSE(mpctx, "audio EOF without any data\n");
            mp_filter_reset(ao_c->filter->f);
            encode_lavc_stream_eof(mpctx->encode_lavc_ctx, STREAM_AUDIO);
        }
        return; // try again next iteration
    }

    if (ao_c->ao_resume_time > mp_time_sec()) {
        double remaining = ao_c->ao_resume_time - mp_time_sec();
        mp_set_timeout(mpctx, remaining);
        return;
    }

    if (mpctx->vo_chain && ao_c->track && ao_c->track->dec &&
        ao_c->track->dec->pts_reset)
    {
        MP_VERBOSE(mpctx, "Reset playback due to audio timestamp reset.\n");
        reset_playback_state(mpctx);
        mp_wakeup_core(mpctx);
        return;
    }

    int ao_rate;
    int ao_format;
    struct mp_chmap ao_channels;
    ao_get_format(mpctx->ao, &ao_rate, &ao_format, &ao_channels);
    double play_samplerate = ao_rate / mpctx->audio_speed;
    int align = af_format_sample_alignment(ao_format);

    // If audio is infinitely fast, somehow try keeping approximate A/V sync.
    if (mpctx->audio_status == STATUS_PLAYING && ao_untimed(mpctx->ao) &&
        mpctx->video_status != STATUS_EOF && mpctx->delay > 0)
        return;

    int playsize = ao_get_space(mpctx->ao);

    int skip = 0;
    bool sync_known = get_sync_samples(mpctx, &skip);
    if (skip > 0) {
        playsize = MPMIN(skip + 1, MPMAX(playsize, 2500)); // buffer extra data
    } else if (skip < 0) {
        playsize = MPMAX(1, playsize + skip); // silence will be prepended
    }

    int skip_duplicate = 0; // >0: skip, <0: duplicate
    double drop_limit =
        (opts->sync_max_audio_change + opts->sync_max_video_change) / 100;
    if (mpctx->display_sync_active && opts->video_sync == VS_DISP_ADROP &&
        fabs(mpctx->last_av_difference) >= opts->sync_audio_drop_size &&
        mpctx->audio_drop_throttle < drop_limit &&
        mpctx->audio_status == STATUS_PLAYING)
    {
        int samples = ceil(opts->sync_audio_drop_size * play_samplerate);
        samples = (samples + align / 2) / align * align;

        skip_duplicate = mpctx->last_av_difference >= 0 ? -samples : samples;

        playsize = MPMAX(playsize, samples);

        mpctx->audio_drop_throttle += 1 - drop_limit - samples / play_samplerate;
    }

    playsize = playsize / align * align;

    int status = mpctx->audio_status >= STATUS_DRAINING ? AD_EOF : AD_OK;
    bool working = false;
    if (playsize > mp_audio_buffer_samples(ao_c->ao_buffer)) {
        status = filter_audio(mpctx, ao_c->ao_buffer, playsize);
        if (ao_c->filter->ao_needs_update) {
            reinit_audio_filters_and_output(mpctx);
            mp_wakeup_core(mpctx);
            return; // retry on next iteration
        }
        if (status == AD_WAIT)
            return;
        working = true;
    }

    // If EOF was reached before, but now something can be decoded, try to
    // restart audio properly. This helps with video files where audio starts
    // later. Retrying is needed to get the correct sync PTS.
    if (mpctx->audio_status >= STATUS_DRAINING &&
        mp_audio_buffer_samples(ao_c->ao_buffer) > 0)
    {
        mpctx->audio_status = STATUS_SYNCING;
        return; // retry on next iteration
    }

    bool end_sync = false;
    if (skip >= 0) {
        int max = mp_audio_buffer_samples(ao_c->ao_buffer);
        mp_audio_buffer_skip(ao_c->ao_buffer, MPMIN(skip, max));
        // If something is left, we definitely reached the target time.
        end_sync |= sync_known && skip < max;
        working |= skip > 0;
    } else if (skip < 0) {
        if (-skip > playsize) { // heuristic against making the buffer too large
            ao_reset(mpctx->ao); // some AOs repeat data on underflow
            mpctx->audio_status = STATUS_DRAINING;
            mpctx->delay = 0;
            return;
        }
        mp_audio_buffer_prepend_silence(ao_c->ao_buffer, -skip);
        end_sync = true;
    }

    if (skip_duplicate) {
        int max = mp_audio_buffer_samples(ao_c->ao_buffer);
        if (abs(skip_duplicate) > max)
            skip_duplicate = skip_duplicate >= 0 ? max : -max;
        mpctx->last_av_difference += skip_duplicate / play_samplerate;
        if (skip_duplicate >= 0) {
            mp_audio_buffer_skip(ao_c->ao_buffer, skip_duplicate);
            MP_STATS(mpctx, "drop-audio");
        } else {
            mp_audio_buffer_duplicate(ao_c->ao_buffer, -skip_duplicate);
            MP_STATS(mpctx, "duplicate-audio");
        }
        MP_VERBOSE(mpctx, "audio skip_duplicate=%d\n", skip_duplicate);
    }

    if (mpctx->audio_status == STATUS_SYNCING) {
        if (end_sync)
            mpctx->audio_status = STATUS_FILLING;
        if (status != AD_OK && !mp_audio_buffer_samples(ao_c->ao_buffer))
            mpctx->audio_status = STATUS_EOF;
        if (working || end_sync)
            mp_wakeup_core(mpctx);
        return; // continue on next iteration
    }

    assert(mpctx->audio_status >= STATUS_FILLING);

    // We already have as much data as the audio device wants, and can start
    // writing it any time.
    if (mpctx->audio_status == STATUS_FILLING)
        mpctx->audio_status = STATUS_READY;

    // Even if we're done decoding and syncing, let video start first - this is
    // required, because sending audio to the AO already starts playback.
    if (mpctx->audio_status == STATUS_READY) {
        // Warning: relies on handle_playback_restart() being called afterwards.
        return;
    }

    bool audio_eof = status == AD_EOF;
    bool partial_fill = false;
    int playflags = 0;

    if (playsize > mp_audio_buffer_samples(ao_c->ao_buffer)) {
        playsize = mp_audio_buffer_samples(ao_c->ao_buffer);
        partial_fill = true;
    }

    audio_eof &= partial_fill;

    // With gapless audio, delay this to ao_uninit. There must be only
    // 1 final chunk, and that is handled when calling ao_uninit().
    if (audio_eof && !opts->gapless_audio)
        playflags |= AOPLAY_FINAL_CHUNK;

    uint8_t **planes;
    int samples;
    mp_audio_buffer_peek(ao_c->ao_buffer, &planes, &samples);
    if (audio_eof || samples >= align)
        samples = samples / align * align;
    samples = MPMIN(samples, mpctx->paused ? 0 : playsize);
    int played = write_to_ao(mpctx, planes, samples, playflags);
    assert(played >= 0 && played <= samples);
    mp_audio_buffer_skip(ao_c->ao_buffer, played);

    mpctx->audio_drop_throttle =
        MPMAX(0, mpctx->audio_drop_throttle - played / play_samplerate);

    dump_audio_stats(mpctx);

    mpctx->audio_status = STATUS_PLAYING;
    if (audio_eof && !playsize) {
        mpctx->audio_status = STATUS_DRAINING;
        // Wait until the AO has played all queued data. In the gapless case,
        // we trigger EOF immediately, and let it play asynchronously.
        if (ao_eof_reached(mpctx->ao) || opts->gapless_audio) {
            mpctx->audio_status = STATUS_EOF;
            if (!was_eof) {
                MP_VERBOSE(mpctx, "audio EOF reached\n");
                mp_wakeup_core(mpctx);
                encode_lavc_stream_eof(mpctx->encode_lavc_ctx, STREAM_AUDIO);
            }
        }
    }
}

// Drop data queued for output, or which the AO is currently outputting.
void clear_audio_output_buffers(struct MPContext *mpctx)
{
    if (mpctx->ao)
        ao_reset(mpctx->ao);
}
