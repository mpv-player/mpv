/*
 * JACK audio output driver for MPlayer
 *
 * Copyleft 2001 by Felix Bünemann (atmosfear@users.sf.net)
 * and Reimar Döffinger (Reimar.Doeffinger@stud.uni-karlsruhe.de)
 *
 * Copyleft 2013 by William Light <wrl@illest.net> for the mpv project
 *
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
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "common/msg.h"

#include "ao.h"
#include "internal.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "options/m_option.h"

#include <jack/jack.h>

struct priv {
    jack_client_t *client;
    float jack_latency;
    char *cfg_port;
    char *cfg_client_name;
    int connect;
    int autostart;
    int stdlayout;
    int last_chunk;

    int num_ports;
    jack_port_t *ports[MP_NUM_CHANNELS];

    int activated;
};

static int process(jack_nframes_t nframes, void *arg)
{
    struct ao *ao = arg;
    struct priv *p = ao->priv;

    void *buffers[MP_NUM_CHANNELS];

    for (int i = 0; i < p->num_ports; i++)
        buffers[i] = jack_port_get_buffer(p->ports[i], nframes);

    int64_t end_time = mp_time_us();
    end_time += (p->jack_latency + nframes / (double)ao->samplerate) * 1000000.0;

    ao_read_data(ao, buffers, nframes, end_time);

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
        if (jack_connect(p->client, jack_port_name(p->ports[i]),
                         matching_ports[i]))
        {
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
    char pname[30];
    int i;

    for (i = 0; i < nports; i++) {
        snprintf(pname, sizeof(pname), "out_%d", i);
        p->ports[i] = jack_port_register(p->client, pname, JACK_DEFAULT_AUDIO_TYPE,
                                         JackPortIsOutput, 0);

        if (!p->ports[i]) {
            MP_FATAL(ao, "not enough ports available\n");
            goto err_port_register;
        }
    }

    p->num_ports = nports;
    return 0;

err_port_register:
    return -1;
}

static void resume(struct ao *ao)
{
    struct priv *p = ao->priv;
    if (!p->activated) {
        p->activated = true;

        if (jack_activate(p->client))
            MP_FATAL(ao, "activate failed\n");

        if (p->connect)
            connect_to_outports(ao);
    }
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

    ao->samplerate = jack_get_sample_rate(p->client);

    jack_latency_range_t jack_latency_range;
    jack_port_get_latency_range(p->ports[0], JackPlaybackLatency,
                                &jack_latency_range);
    p->jack_latency = (float)(jack_latency_range.max + jack_get_buffer_size(p->client))
                      / (float)ao->samplerate;

    if (!ao_chmap_sel_get_def(ao, &sel, &ao->channels, p->num_ports))
        goto err_chmap_sel_get_def;

    return 0;

err_chmap_sel_get_def:
err_create_ports:
    jack_client_close(p->client);
err_client_open:
err_chmap:
    return -1;
}

// close audio device
static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;

    jack_client_close(p->client);
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_jack = {
    .description = "JACK audio output",
    .name        = "jack",
    .init      = init,
    .uninit    = uninit,
    .resume    = resume,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
        .cfg_client_name = "mpv",
        .connect = 1,
    },
    .options = (const struct m_option[]) {
        OPT_STRING("port", cfg_port, 0),
        OPT_STRING("name", cfg_client_name, 0),
        OPT_FLAG("autostart", autostart, 0),
        OPT_FLAG("connect", connect, 0),
        OPT_CHOICE("std-channel-layout", stdlayout, 0,
                ({"waveext", 0}, {"alsa", 1}, {"any", 2})),
        {0}
    },
};
