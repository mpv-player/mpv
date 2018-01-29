/*
 * null audio output driver
 *
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

/*
 * Note: this does much more than just ignoring audio output. It simulates
 *       (to some degree) an ideal AO.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "mpv_talloc.h"

#include "config.h"
#include "osdep/timer.h"
#include "options/m_option.h"
#include "common/common.h"
#include "common/msg.h"
#include "audio/format.h"
#include "ao.h"
#include "internal.h"

struct priv {
    bool paused;
    double last_time;
    bool playing_final;
    float buffered;     // samples
    int buffersize;     // samples

    int untimed;
    float bufferlen;    // seconds
    float speed;        // multiplier
    float latency_sec;  // seconds
    float latency;      // samples
    int broken_eof;
    int broken_delay;

    // Minimal unit of audio samples that can be written at once. If play() is
    // called with sizes not aligned to this, a rounded size will be returned.
    // (This is not needed by the AO API, but many AOs behave this way.)
    int outburst;       // samples

    struct m_channels channel_layouts;
    int format;
};

static void drain(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (ao->untimed) {
        priv->buffered = 0;
        return;
    }

    if (priv->paused)
        return;

    double now = mp_time_sec();
    if (priv->buffered > 0) {
        priv->buffered -= (now - priv->last_time) * ao->samplerate * priv->speed;
        if (priv->buffered < 0) {
            if (!priv->playing_final)
                MP_ERR(ao, "buffer underrun\n");
            priv->buffered = 0;
        }
    }
    priv->last_time = now;
}

static int init(struct ao *ao)
{
    struct priv *priv = ao->priv;

    if (priv->format)
        ao->format = priv->format;

    ao->untimed = priv->untimed;

    struct mp_chmap_sel sel = {.tmp = ao};
    if (priv->channel_layouts.num_chmaps) {
        for (int n = 0; n < priv->channel_layouts.num_chmaps; n++)
            mp_chmap_sel_add_map(&sel, &priv->channel_layouts.chmaps[n]);
    } else {
        mp_chmap_sel_add_any(&sel);
    }
    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        mp_chmap_from_channels(&ao->channels, 2);

    priv->latency = priv->latency_sec * ao->samplerate;

    // A "buffer" for this many seconds of audio
    int bursts = (int)(ao->samplerate * priv->bufferlen + 1) / priv->outburst;
    priv->buffersize = priv->outburst * bursts + priv->latency;

    priv->last_time = mp_time_sec();

    ao->period_size = priv->outburst;

    return 0;
}

// close audio device
static void uninit(struct ao *ao)
{
}

static void wait_drain(struct ao *ao)
{
    struct priv *priv = ao->priv;
    drain(ao);
    if (!priv->paused)
        mp_sleep_us(1000000.0 * priv->buffered / ao->samplerate / priv->speed);
}

// stop playing and empty buffers (for seeking/pause)
static void reset(struct ao *ao)
{
    struct priv *priv = ao->priv;
    priv->buffered = 0;
    priv->playing_final = false;
}

// stop playing, keep buffers (for pause)
static void pause(struct ao *ao)
{
    struct priv *priv = ao->priv;

    drain(ao);
    priv->paused = true;
}

// resume playing, after pause()
static void resume(struct ao *ao)
{
    struct priv *priv = ao->priv;

    drain(ao);
    priv->paused = false;
    priv->last_time = mp_time_sec();
}

static int get_space(struct ao *ao)
{
    struct priv *priv = ao->priv;

    drain(ao);
    int samples = priv->buffersize - priv->latency - priv->buffered;
    return samples / priv->outburst * priv->outburst;
}

static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *priv = ao->priv;
    int accepted;

    resume(ao);

    if (priv->buffered <= 0)
        priv->buffered = priv->latency; // emulate fixed latency

    priv->playing_final = flags & AOPLAY_FINAL_CHUNK;
    if (priv->playing_final) {
        // Last audio chunk - don't round to outburst.
        accepted = MPMIN(priv->buffersize - priv->buffered, samples);
    } else {
        int maxbursts = (priv->buffersize - priv->buffered) / priv->outburst;
        int playbursts = samples / priv->outburst;
        int bursts = playbursts > maxbursts ? maxbursts : playbursts;
        accepted = bursts * priv->outburst;
    }
    priv->buffered += accepted;
    return accepted;
}

static double get_delay(struct ao *ao)
{
    struct priv *priv = ao->priv;

    drain(ao);

    // Note how get_delay returns the delay in audio device time (instead of
    // adjusting for speed), since most AOs seem to also do that.
    double delay = priv->buffered;

    // Drivers with broken EOF handling usually always report the same device-
    // level delay that is additional to the buffer time.
    if (priv->broken_eof && priv->buffered < priv->latency)
        delay = priv->latency;

    delay /= ao->samplerate;

    if (priv->broken_delay) { // Report only multiples of outburst
        double q = priv->outburst / (double)ao->samplerate;
        if (delay > 0)
            delay = (int)(delay / q) * q;
    }

    return delay;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_null = {
    .description = "Null audio output",
    .name      = "null",
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = pause,
    .resume    = resume,
    .drain     = wait_drain,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .bufferlen = 0.2,
        .outburst = 256,
        .speed = 1,
    },
    .options = (const struct m_option[]) {
        OPT_FLAG("untimed", untimed, 0),
        OPT_FLOATRANGE("buffer", bufferlen, 0, 0, 100),
        OPT_INTRANGE("outburst", outburst, 0, 1, 100000),
        OPT_FLOATRANGE("speed", speed, 0, 0, 10000),
        OPT_FLOATRANGE("latency", latency_sec, 0, 0, 100),
        OPT_FLAG("broken-eof", broken_eof, 0),
        OPT_FLAG("broken-delay", broken_delay, 0),
        OPT_CHANNELS("channel-layouts", channel_layouts, 0),
        OPT_AUDIOFORMAT("format", format, 0),
        {0}
    },
    .options_prefix = "ao-null",
};
