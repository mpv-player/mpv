/*
 * null audio output driver
 *
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

#include <stdio.h>
#include <stdlib.h>

#include "talloc.h"

#include "config.h"
#include "osdep/timer.h"
#include "audio/format.h"
#include "ao.h"

struct priv {
    unsigned last_time;
    float buffered_bytes;
};

static void drain(struct ao *ao)
{
    struct priv *priv = ao->priv;

    unsigned now = GetTimer();
    priv->buffered_bytes -= (now - priv->last_time) / 1e6 * ao->bps;
    if (priv->buffered_bytes < 0)
        priv->buffered_bytes = 0;
    priv->last_time = now;
}

static int init(struct ao *ao, char *params)
{
    struct priv *priv = talloc_zero(ao, struct priv);
    ao->priv = priv;
    int samplesize = af_fmt2bits(ao->format) / 8;
    ao->outburst = 256 * ao->channels.num * samplesize;
    // A "buffer" for about 0.2 seconds of audio
    ao->buffersize = (int)(ao->samplerate * 0.2 / 256 + 1) * ao->outburst;
    ao->bps = ao->channels.num * ao->samplerate * samplesize;
    priv->last_time = GetTimer();

    return 0;
}

// close audio device
static void uninit(struct ao *ao, bool cut_audio)
{
}

// stop playing and empty buffers (for seeking/pause)
static void reset(struct ao *ao)
{
    struct priv *priv = ao->priv;
    priv->buffered_bytes = 0;
}

// stop playing, keep buffers (for pause)
static void pause(struct ao *ao)
{
    // for now, just call reset();
    reset(ao);
}

// resume playing, after audio_pause()
static void resume(struct ao *ao)
{
}

static int get_space(struct ao *ao)
{
    struct priv *priv = ao->priv;

    drain(ao);
    return ao->buffersize - priv->buffered_bytes;
}

static int play(struct ao *ao, void *data, int len, int flags)
{
    struct priv *priv = ao->priv;

    int maxbursts = (ao->buffersize - priv->buffered_bytes) / ao->outburst;
    int playbursts = len / ao->outburst;
    int bursts = playbursts > maxbursts ? maxbursts : playbursts;
    priv->buffered_bytes += bursts * ao->outburst;
    return bursts * ao->outburst;
}

static float get_delay(struct ao *ao)
{
    struct priv *priv = ao->priv;

    drain(ao);
    return priv->buffered_bytes / ao->bps;
}

const struct ao_driver audio_out_null = {
    .info = &(const struct ao_info) {
        "Null audio output",
        "null",
        "Tobias Diedrich <ranma+mplayer@tdiedrich.de>",
        "",
    },
    .init      = init,
    .uninit    = uninit,
    .reset     = reset,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = pause,
    .resume    = resume,
};

