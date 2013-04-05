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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "talloc.h"

#include "config.h"
#include "ao.h"

#include "core/mp_msg.h"

// there are some globals:
struct ao *global_ao;
char *ao_subdevice = NULL;

extern const struct ao_driver audio_out_oss;
extern const struct ao_driver audio_out_coreaudio;
extern const struct ao_driver audio_out_rsound;
extern const struct ao_driver audio_out_pulse;
extern const struct ao_driver audio_out_jack;
extern const struct ao_driver audio_out_openal;
extern const struct ao_driver audio_out_null;
extern const struct ao_driver audio_out_alsa;
extern const struct ao_driver audio_out_dsound;
extern const struct ao_driver audio_out_pcm;
extern const struct ao_driver audio_out_pss;
extern const struct ao_driver audio_out_lavc;
extern const struct ao_driver audio_out_portaudio;
extern const struct ao_driver audio_out_sdl;

static const struct ao_driver * const audio_out_drivers[] = {
// native:
#ifdef CONFIG_COREAUDIO
    &audio_out_coreaudio,
#endif
#ifdef CONFIG_PULSE
    &audio_out_pulse,
#endif
#ifdef CONFIG_ALSA
    &audio_out_alsa,
#endif
#ifdef CONFIG_OSS_AUDIO
    &audio_out_oss,
#endif
#ifdef CONFIG_DSOUND
    &audio_out_dsound,
#endif
#ifdef CONFIG_PORTAUDIO
    &audio_out_portaudio,
#endif
    // wrappers:
#ifdef CONFIG_JACK
    &audio_out_jack,
#endif
#ifdef CONFIG_OPENAL
    &audio_out_openal,
#endif
#ifdef CONFIG_SDL
    &audio_out_sdl,
#endif
    &audio_out_null,
    // should not be auto-selected:
    &audio_out_pcm,
#ifdef CONFIG_ENCODING
    &audio_out_lavc,
#endif
#ifdef CONFIG_RSOUND
    &audio_out_rsound,
#endif
    NULL
};

void list_audio_out(void)
{
    mp_tmsg(MSGT_AO, MSGL_INFO, "Available audio output drivers:\n");
    mp_msg(MSGT_IDENTIFY, MSGL_INFO, "ID_AUDIO_OUTPUTS\n");
    for (int i = 0; audio_out_drivers[i]; i++) {
        const ao_info_t *info = audio_out_drivers[i]->info;
        if (!audio_out_drivers[i]->encode) {
            mp_msg(MSGT_GLOBAL, MSGL_INFO, "\t%s\t%s\n",
                   info->short_name, info->name);
        }
    }
    mp_msg(MSGT_GLOBAL, MSGL_INFO,"\n");
}

struct ao *ao_create(struct MPOpts *opts, struct input_ctx *input)
{
    struct ao *r = talloc(NULL, struct ao);
    *r = (struct ao){.outburst = 512, .buffersize = -1,
                     .opts = opts, .input_ctx = input };
    return r;
}

static bool ao_try_init(struct ao *ao, char *params)
{
    if (ao->driver->encode != !!ao->encode_lavc_ctx)
        return false;
    return ao->driver->init(ao, params) >= 0;
}

void ao_init(struct ao *ao, char **ao_list)
{
    /* Caller adding child blocks is not supported as we may call
     * talloc_free_children() to clean up after failed open attempts.
     */
    assert(talloc_total_blocks(ao) == 1);
    struct ao backup = *ao;

    if (!ao_list)
        goto try_defaults;

    // first try the preferred drivers, with their optional subdevice param:
    while (*ao_list) {
        char *ao_name = *ao_list;
        if (!*ao_name)
            goto try_defaults; // empty entry means try defaults
        int ao_len;
        char *params = strchr(ao_name, ':');
        if (params) {
            ao_len = params - ao_name;
            params++;
        } else
            ao_len = strlen(ao_name);

        mp_tmsg(MSGT_AO, MSGL_V,
                "Trying preferred audio driver '%.*s', options '%s'\n",
                ao_len, ao_name, params ? params : "[none]");

        const struct ao_driver *audio_out = NULL;
        for (int i = 0; audio_out_drivers[i]; i++) {
            audio_out = audio_out_drivers[i];
            if (!strncmp(audio_out->info->short_name, ao_name, ao_len))
                break;
            audio_out = NULL;
        }
        if (audio_out) {
            // name matches, try it
            ao->driver = audio_out;
            if (ao_try_init(ao, params)) {
                ao->driver = audio_out;
                ao->initialized = true;
                return;
            }
            mp_tmsg(MSGT_AO, MSGL_WARN,
                    "Failed to initialize audio driver '%s'\n", ao_name);
            talloc_free_children(ao);
            *ao = backup;
        } else
            mp_tmsg(MSGT_AO, MSGL_WARN, "No such audio driver '%.*s'\n",
                    ao_len, ao_name);
        ++ao_list;
    }
    return;

 try_defaults:
    mp_tmsg(MSGT_AO, MSGL_V, "Trying every known audio driver...\n");

    ao->probing = false;

    // now try the rest...
    for (int i = 0; audio_out_drivers[i]; i++) {
        const struct ao_driver *audio_out = audio_out_drivers[i];
        ao->driver = audio_out;
        ao->probing = true;
        if (ao_try_init(ao, NULL)) {
            ao->initialized = true;
            ao->driver = audio_out;
            return;
        }
        talloc_free_children(ao);
        *ao = backup;
    }
    return;
}

void ao_uninit(struct ao *ao, bool cut_audio)
{
    assert(ao->buffer.len >= ao->buffer_playable_size);
    ao->buffer.len = ao->buffer_playable_size;
    if (ao->initialized)
        ao->driver->uninit(ao, cut_audio);
    if (!cut_audio && ao->buffer.len)
        mp_msg(MSGT_AO, MSGL_WARN, "Audio output truncated at end.\n");
    talloc_free(ao);
}

int ao_play(struct ao *ao, void *data, int len, int flags)
{
    return ao->driver->play(ao, data, len, flags);
}

int ao_control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    if (ao->driver->control)
        return ao->driver->control(ao, cmd, arg);
    return CONTROL_UNKNOWN;
}

double ao_get_delay(struct ao *ao)
{
    if (!ao->driver->get_delay) {
        assert(ao->untimed);
        return 0;
    }
    return ao->driver->get_delay(ao);
}

int ao_get_space(struct ao *ao)
{
    return ao->driver->get_space(ao);
}

void ao_reset(struct ao *ao)
{
    ao->buffer.len = 0;
    ao->buffer_playable_size = 0;
    if (ao->driver->reset)
        ao->driver->reset(ao);
}

void ao_pause(struct ao *ao)
{
    if (ao->driver->pause)
        ao->driver->pause(ao);
}

void ao_resume(struct ao *ao)
{
    if (ao->driver->resume)
        ao->driver->resume(ao);
}



int old_ao_init(struct ao *ao, char *params)
{
    assert(!global_ao);
    global_ao = ao;
    ao_subdevice = params ? talloc_strdup(ao, params) : NULL;
    if (ao->driver->old_functions->init(ao->samplerate, &ao->channels,
                                        ao->format, 0) == 0) {
        global_ao = NULL;
        return -1;
    }
    return 0;
}

void old_ao_uninit(struct ao *ao, bool cut_audio)
{
    ao->driver->old_functions->uninit(cut_audio);
    global_ao = NULL;
}

int old_ao_play(struct ao *ao, void *data, int len, int flags)
{
    return ao->driver->old_functions->play(data, len, flags);
}

int old_ao_control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    return ao->driver->old_functions->control(cmd, arg);
}

float old_ao_get_delay(struct ao *ao)
{
    return ao->driver->old_functions->get_delay();
}

int old_ao_get_space(struct ao *ao)
{
    return ao->driver->old_functions->get_space();
}

void old_ao_reset(struct ao *ao)
{
    ao->driver->old_functions->reset();
}

void old_ao_pause(struct ao *ao)
{
    ao->driver->old_functions->pause();
}

void old_ao_resume(struct ao *ao)
{
    ao->driver->old_functions->resume();
}
