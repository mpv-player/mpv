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

#ifndef MP_AO_INTERNAL_H_
#define MP_AO_INTERNAL_H_

#include <stdbool.h>
#include <pthread.h>

#include "audio/chmap.h"
#include "audio/chmap_sel.h"

// Minimum buffer size in seconds.
#define MIN_BUFFER 0.2

// If ao_get_delay() reaches this value after ao_play() was called with the
// AOPLAY_FINAL_CHUNK flag set, the playback core expects that the audio has
// all been played.
#define AO_EOF_DELAY 0.05

/* global data used by ao.c and ao drivers */
struct ao {
    int samplerate;
    struct mp_chmap channels;
    int format;                 // one of AF_FORMAT_...
    int bps;                    // bytes per second (per plane)
    int sstride;                // size of a sample on each plane
                                // (format_size*num_channels/num_planes)
    int num_planes;
    bool probing;               // if true, don't fail loudly on init
    bool untimed;               // don't assume realtime playback
    bool no_persistent_volume;  // the AO does the equivalent of af_volume
    bool per_application_mixer; // like above, but volume persists (per app)
    int device_buffer;          // device buffer in samples (guessed by
                                // common init code if not set by driver)
    const struct ao_driver *api; // entrypoints to the wrapper (push.c/pull.c)
    const struct ao_driver *driver;
    void *priv;
    struct encode_lavc_context *encode_lavc_ctx;
    struct input_ctx *input_ctx;
    struct mp_log *log; // Using e.g. "[ao/coreaudio]" as prefix

    int buffer;
    void *api_priv;
};

extern const struct ao_driver ao_api_push;
extern const struct ao_driver ao_api_pull;


/* Note:
 *
 * In general, there are two types of audio drivers:
 *  a) push based (the user queues data that should be played)
 *  b) pull callback based (the audio API calls a callback to get audio)
 *
 * The ao.c code can handle both. It basically implements two audio paths
 * and provides a uniform API for them. If ao_driver->play is NULL, it assumes
 * that the driver uses a callback based audio API, otherwise push based.
 *
 * Requirements:
 *  a) ->play is called to queue audio. push.c creates a thread to regularly
 *     refill audio device buffers with ->play, but all driver functions are
 *     always called under an exclusive lock.
 *     Mandatory:
 *          init
 *          uninit
 *          reset
 *          get_space
 *          play
 *          get_delay
 *          pause
 *          resume
 *     Optional:
 *          control
 *          drain
 *          wait
 *          wakeup
 *  b) ->play must be NULL. ->resume must be provided, and should make the
 *     audio API start calling the audio callback. Your audio callback should
 *     in turn call ao_read_data() to get audio data. Most functions are
 *     optional and will be emulated if missing (e.g. pausing is emulated as
 *     silence). ->get_delay and ->get_space are never called.
 *     Mandatory:
 *          init
 *          uninit
 *          resume      (starts the audio callback)
 *     Also, the following optional callbacks can be provided:
 *          reset       (stops the audio callback, resume() restarts it)
 *          control
 */
struct ao_driver {
    // If true, use with encoding only.
    bool encode;
    // Name used for --ao.
    const char *name;
    // Description shown with --ao=help.
    const char *description;
    // Init the device using ao->format/ao->channels/ao->samplerate. If the
    // device doesn't accept these parameters, you can attempt to negotiate
    // fallback parameters, and set the ao format fields accordingly.
    int (*init)(struct ao *ao);
    // Optional. See ao_control() etc. in ao.c
    int (*control)(struct ao *ao, enum aocontrol cmd, void *arg);
    void (*uninit)(struct ao *ao);
    // push based: see ao_reset()
    // pull based: stop the audio callback
    void (*reset)(struct ao*ao);
    // push based: see ao_pause()
    void (*pause)(struct ao *ao);
    // push based: see ao_resume()
    // pull based: start the audio callback
    void (*resume)(struct ao *ao);
    // push based: see ao_play()
    int (*get_space)(struct ao *ao);
    // push based: see ao_play()
    int (*play)(struct ao *ao, void **data, int samples, int flags);
    // push based: see ao_get_delay()
    float (*get_delay)(struct ao *ao);
    // push based: block until all queued audio is played (optional)
    void (*drain)(struct ao *ao);
    // Wait until the audio buffer needs to be refilled. The lock is the
    // internal mutex usually protecting the internal AO state (and used to
    // protect driver calls), and must be temporarily unlocked while waiting.
    // ->wakeup will be called (with lock held) if the wait should be canceled.
    // Returns 0 on success, -1 on error.
    // Optional; if this is not provided, generic code using audio timing is
    // used to estimate when the AO needs to be refilled.
    // Warning: it's only called if the feed thread truly needs to know when
    //          the audio thread takes data again. Often, it will just copy
    //          the complete soft-buffer to the AO, and then wait for the
    //          decoder instead. Don't do necessary work in this callback.
    int (*wait)(struct ao *ao, pthread_mutex_t *lock);
    // In combination with wait(). Lock may or may not be held.
    void (*wakeup)(struct ao *ao);

    // For option parsing (see vo.h)
    int priv_size;
    const void *priv_defaults;
    const struct m_option *options;
};

// These functions can be called by AOs.

int ao_play_silence(struct ao *ao, int samples);
void ao_wait_drain(struct ao *ao);
int ao_read_data(struct ao *ao, void **data, int samples, int64_t out_time_us);

bool ao_chmap_sel_adjust(struct ao *ao, const struct mp_chmap_sel *s,
                         struct mp_chmap *map);
bool ao_chmap_sel_get_def(struct ao *ao, const struct mp_chmap_sel *s,
                          struct mp_chmap *map, int num);

#endif
