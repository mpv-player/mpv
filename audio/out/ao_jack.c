/*
 * JACK audio output driver for MPlayer
 *
 * Copyleft 2001 by Felix Bünemann (atmosfear@users.sf.net)
 * and Reimar Döffinger (Reimar.Doeffinger@stud.uni-karlsruhe.de)
 *
 * Copyleft 2013 by William Light <wrl@illest.net> for the mpv project
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
 * along with MPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "mpvcore/mp_msg.h"

#include "ao.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "mpvcore/m_option.h"

#include "mpvcore/mp_ring.h"

#include <jack/jack.h>

//! size of one chunk, if this is too small MPlayer will start to "stutter"
//! after a short time of playback
#define CHUNK_SIZE (8 * 1024)
//! number of "virtual" chunks the buffer consists of
#define NUM_CHUNKS 8

struct port_ring {
    jack_port_t *port;
    struct mp_ring *ring;
};

struct priv {
    jack_client_t *client;
    float jack_latency;
    char *cfg_port;
    char *cfg_client_name;
    int estimate;
    int connect;
    int autostart;
    int stdlayout;
    volatile int paused;
    volatile int underrun;
    volatile float callback_interval;
    volatile float callback_time;

    int num_ports;
    struct port_ring ports[MP_NUM_CHANNELS];
};

/**
 * \brief fill the buffers with silence
 * \param bufs num_bufs float buffers, each will contain the data of one channel
 * \param cnt number of samples in each buffer
 * \param num_bufs number of buffers
 */
static void
silence(float *buf, jack_nframes_t nframes)
{
    memset(buf, 0, nframes * sizeof(*buf));
}

static int
process_port(struct ao *ao, struct port_ring *pr, jack_nframes_t nframes)
{
    struct priv *p = ao->priv;
    int buffered;
    float *buf;

    buf = jack_port_get_buffer(pr->port, nframes);

    if (p->paused || p->underrun) {
        silence(buf, nframes);
        return 0;
    }

    buffered = mp_ring_buffered(pr->ring) / sizeof(float);
    if (buffered < nframes) {
        mp_ring_read(pr->ring, (void *) buf, buffered * sizeof(float));
        silence(&buf[buffered], nframes - buffered);
        return 1;
    }

    mp_ring_read(pr->ring, (void *) buf, nframes * sizeof(float));
    return 0;
}

/**
 * \brief JACK Callback function
 * \param nframes number of frames to fill into buffers
 * \param arg unused
 * \return currently always 0
 *
 * Write silence into buffers if paused or an underrun occured
 */
static int
process(jack_nframes_t nframes, void *arg)
{
    struct ao *ao = arg;
    struct priv *p = ao->priv;
    int i, underrun;

    underrun = 0;

    for (i = 0; i < p->num_ports; i++) {
        if (process_port(ao, &p->ports[i], nframes))
            underrun = 1;
    }

    if (underrun)
        p->underrun = 1;

    if (p->estimate) {
        float now = mp_time_us() / 1000000.0;
        float diff = p->callback_time + p->callback_interval - now;
        if ((diff > -0.002) && (diff < 0.002))
            p->callback_time += p->callback_interval;
        else
            p->callback_time = now;
        p->callback_interval = (float)nframes / (float)ao->samplerate;
    }

    return 0;
}

static int
connect_to_outports(struct ao *ao)
{
    struct priv *p = ao->priv;

    char *port_name = (p->cfg_port && p->cfg_port[0]) ? p->cfg_port : NULL;
    const char **matching_ports = NULL;
    int port_flags = JackPortIsInput;
    int i;

    if (!port_name)
        port_flags |= JackPortIsPhysical;

    matching_ports = jack_get_ports(p->client, port_name, NULL, port_flags);

    if (!matching_ports || !matching_ports[0]) {
        MP_FATAL(ao, "no ports to connect to\n");
        goto err_get_ports;
    }

    for (i = 0; i < p->num_ports && matching_ports[i]; i++) {
        if (jack_connect(p->client, jack_port_name(p->ports[i].port),
                         matching_ports[i])) {
            MP_FATAL(ao, "connecting failed\n");
            goto err_connect;
        }
    }

    free(matching_ports);
    return 0;

err_connect:
    free(matching_ports);
err_get_ports:
    return -1;
}

static int
create_ports(struct ao *ao, int nports)
{
    struct priv *p = ao->priv;
    struct port_ring *pr;
    char pname[30];
    int i;

    for (i = 0; i < nports; i++) {
        pr = &p->ports[i];

        snprintf(pname, sizeof(pname), "out_%d", i);
        pr->port = jack_port_register(p->client, pname, JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsOutput, 0);

        if (!pr->port) {
            MP_FATAL(ao, "not enough ports available\n");
            goto err_port_register;
        }

        pr->ring = mp_ring_new(p, NUM_CHUNKS * CHUNK_SIZE);
    }

    p->num_ports = nports;
    return 0;

err_port_register:
    return -1;
}

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;
    struct mp_chmap_sel sel = {0};
    jack_options_t open_options;

    ao->format = AF_FORMAT_FLOATP;

    switch (p->stdlayout) {
    case 0:
        mp_chmap_sel_add_waveext(&sel);
        break;

    case 1:
        mp_chmap_sel_add_alsa_def(&sel);
        break;

    default:
        mp_chmap_sel_add_any(&sel);
    }

    if (!ao_chmap_sel_adjust(ao, &sel, &ao->channels))
        goto err_chmap;

    open_options = JackNullOption;
    if (!p->autostart)
        open_options |= JackNoStartServer;

    p->client = jack_client_open(p->cfg_client_name, open_options, NULL);
    if (!p->client) {
        MP_FATAL(ao, "cannot open server\n");
        goto err_client_open;
    }

    if (create_ports(ao, ao->channels.num))
        goto err_create_ports;

    jack_set_process_callback(p->client, process, ao);

    if (jack_activate(p->client)) {
        MP_FATAL(ao, "activate failed\n");
        goto err_activate;
    }

    ao->samplerate = jack_get_sample_rate(p->client);

    if (p->connect)
        if (connect_to_outports(ao))
            goto err_connect;

    jack_latency_range_t jack_latency_range;
    jack_port_get_latency_range(p->ports[0].port, JackPlaybackLatency,
                                &jack_latency_range);
    p->jack_latency = (float)(jack_latency_range.max + jack_get_buffer_size(p->client))
                      / (float)ao->samplerate;
    p->callback_interval = 0;

    if (!ao_chmap_sel_get_def(ao, &sel, &ao->channels, p->num_ports))
        goto err_chmap_sel_get_def;

    return 0;

err_chmap_sel_get_def:
err_connect:
    jack_deactivate(p->client);
err_activate:
err_create_ports:
    jack_client_close(p->client);
err_client_open:
err_chmap:
    return -1;
}

static float get_delay(struct ao *ao)
{
    struct priv *p = ao->priv;
    int buffered = mp_ring_buffered(p->ports[0].ring); // could be less
    float in_jack = p->jack_latency;

    if (p->estimate && p->callback_interval > 0) {
        float elapsed = mp_time_us() / 1000000.0 - p->callback_time;
        in_jack += p->callback_interval - elapsed;
        if (in_jack < 0)
            in_jack = 0;
    }

    return (float)buffered / (float)ao->bps + in_jack;
}

/**
 * \brief stop playing and empty buffers (for seeking/pause)
 */
static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;
    int i;
    p->paused = 1;

    for (i = 0; i < p->num_ports; i++)
        mp_ring_reset(p->ports[i].ring);

    p->paused = 0;
}

// close audio device
static void uninit(struct ao *ao, bool immed)
{
    struct priv *p = ao->priv;

    if (!immed)
        mp_sleep_us(get_delay(ao) * 1000 * 1000);

    mp_sleep_us(100 * 1000);
    jack_client_close(p->client);
}

/**
 * \brief stop playing, keep buffers (for pause)
 */
static void audio_pause(struct ao *ao)
{
    struct priv *p = ao->priv;
    p->paused = 1;
}

/**
 * \brief resume playing, after audio_pause()
 */
static void audio_resume(struct ao *ao)
{
    struct priv *p = ao->priv;
    p->paused = 0;
}

static int get_space(struct ao *ao)
{
    struct priv *p = ao->priv;
    return mp_ring_available(p->ports[0].ring) / ao->sstride;
}

/**
 * \brief write data into buffer and reset underrun flag
 */
static int play(struct ao *ao, void **data, int samples, int flags)
{
    struct priv *p = ao->priv;
    struct port_ring *pr;
    int i, len, ret;

    len = samples * ao->sstride;
    ret = 0;

    for (i = 0; i < p->num_ports; i++) {
        pr  = &p->ports[i];
        ret = mp_ring_write(pr->ring, data[i], len);
    }

    p->underrun = 0;
    return ret / ao->sstride;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_jack = {
    .description = "JACK audio output",
    .name        = "jack",
    .init      = init,
    .uninit    = uninit,
    .get_space = get_space,
    .play      = play,
    .get_delay = get_delay,
    .pause     = audio_pause,
    .resume    = audio_resume,
    .reset     = reset,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .cfg_client_name = "mpv",
        .estimate = 1,
        .connect = 1,
    },
    .options = (const struct m_option[]) {
        OPT_STRING("port", cfg_port, 0),
        OPT_STRING("name", cfg_client_name, 0),
        OPT_FLAG("estimate", estimate, 0),
        OPT_FLAG("autostart", autostart, 0),
        OPT_FLAG("connect", connect, 0),
        OPT_CHOICE("std-channel-layout", stdlayout, 0,
                ({"waveext", 0}, {"alsa", 1}, {"any", 2})),
        {0}
    },
};
