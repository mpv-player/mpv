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

#ifndef MP_AO_INTERNAL_H_
#define MP_AO_INTERNAL_H_

#include <stdbool.h>
#include <pthread.h>

#include "osdep/atomic.h"
#include "audio/out/ao.h"

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
    int device_buffer;          // device buffer in samples (guessed by
                                // common init code if not set by driver)
    const struct ao_driver *driver;
    bool driver_initialized;
    void *priv;
    struct mpv_global *global;
    struct encode_lavc_context *encode_lavc_ctx;
    void (*wakeup_cb)(void *ctx);
    void *wakeup_ctx;
    struct mp_log *log; // Using e.g. "[ao/coreaudio]" as prefix
    int init_flags; // AO_INIT_* flags
    bool stream_silence;        // if audio inactive, just play silence

    // The device as selected by the user, usually using ao_device_desc.name
    // from an entry from the list returned by driver->list_devices. If the
    // default device should be used, this is set to NULL.
    char *device;

    // Application name to report to the audio API.
    char *client_name;

    // Used during init: if init fails, redirect to this ao
    char *redirect;

    // Internal events (use ao_request_reload(), ao_hotplug_event())
    atomic_uint events_;

    // Float gain multiplicator
    mp_atomic_float gain;

    int buffer;
    double def_buffer;
    struct buffer_state *buffer_state;
};

void init_buffer_pre(struct ao *ao);
bool init_buffer_post(struct ao *ao);

struct mp_pcm_state {
    // Note: free_samples+queued_samples <= ao->device_buffer; the sum may be
    //       less if the audio API can report partial periods played, while
    //       free_samples should be period-size aligned. If free_samples is not
    //       period-size aligned, the AO thread might get into a situation where
    //       it writes a very small number of samples in each iteration, leading
    //       to extremely inefficient behavior.
    //       Keep in mind that write() may write less than free_samples (or your
    //       period size alignment) anyway.
    int free_samples;       // number of free space in ring buffer
    int queued_samples;     // number of samples to play in ring buffer
    double delay;           // total latency in seconds (includes queued_samples)
    bool playing;           // set if underlying API is actually playing audio;
                            // the AO must unset it on underrun (accidental
                            // underrun and EOF are indistinguishable; the upper
                            // layers decide what it was)
                            // real pausing may assume playing=true
};

/* Note:
 *
 * In general, there are two types of audio drivers:
 *  a) push based (the user queues data that should be played)
 *  b) pull callback based (the audio API calls a callback to get audio)
 *
 * The ao.c code can handle both. It basically implements two audio paths
 * and provides a uniform API for them. If ao_driver->write is NULL, it assumes
 * that the driver uses a callback based audio API, otherwise push based.
 *
 * Requirements:
 *  a+b) Mandatory for both types:
 *          init
 *          uninit
 *          start
 *     Optional for both types:
 *          control
 *  a) ->write is called to queue audio. push.c creates a thread to regularly
 *     refill audio device buffers with ->write, but all driver functions are
 *     always called under an exclusive lock.
 *     Mandatory:
 *          reset
 *          write
 *          get_state
 *     Optional:
 *          set_pause
 *  b) ->write must be NULL. ->start must be provided, and should make the
 *     audio API start calling the audio callback. Your audio callback should
 *     in turn call ao_read_data() to get audio data. Most functions are
 *     optional and will be emulated if missing (e.g. pausing is emulated as
 *     silence).
 *     Also, the following optional callbacks can be provided:
 *          reset       (stops the audio callback, start() restarts it)
 */
struct ao_driver {
    // If true, use with encoding only.
    bool encode;
    // Name used for --ao.
    const char *name;
    // Description shown with --ao=help.
    const char *description;
    // This requires waiting for a AO_EVENT_INITIAL_UNBLOCK event before the
    // first write() call is done. Encode mode uses this, and push mode
    // respects it automatically (don't use with pull mode).
    bool initially_blocked;
    // If true, write units of entire frames. The write() call is modified to
    // use data==mp_aframe. Useful for encoding AO only.
    bool write_frames;
    // Init the device using ao->format/ao->channels/ao->samplerate. If the
    // device doesn't accept these parameters, you can attempt to negotiate
    // fallback parameters, and set the ao format fields accordingly.
    int (*init)(struct ao *ao);
    // Optional. See ao_control() etc. in ao.c
    int (*control)(struct ao *ao, enum aocontrol cmd, void *arg);
    void (*uninit)(struct ao *ao);
    // Stop all audio playback, clear buffers, back to state after init().
    // Optional for pull AOs.
    void (*reset)(struct ao *ao);
    // push based: set pause state. Only called after start() and before reset().
    //             returns success (this is intended for paused=true; if it
    //             returns false, playback continues, and the core emulates via
    //             reset(); unpausing always works)
    //             The pausing state is also cleared by reset().
    bool (*set_pause)(struct ao *ao, bool paused);
    // pull based: start the audio callback
    // push based: start playing queued data
    //             AO should call ao_wakeup_playthread() if a period boundary
    //             is crossed, or playback stops due to external reasons
    //             (including underruns or device removal)
    //             must set mp_pcm_state.playing; unset on error/underrun/end
    void (*start)(struct ao *ao);
    // push based: queue new data. This won't try to write more data than the
    // reported free space (samples <= mp_pcm_state.free_samples).
    // This must NOT start playback. start() does that, and write() may be
    // called multiple times before start() is called. It may also happen that
    // reset() is called to discard the buffer. start() without write() will
    // immediately reported an underrun.
    // Return false on failure.
    bool (*write)(struct ao *ao, void **data, int samples);
    // push based: return mandatory stream information
    void (*get_state)(struct ao *ao, struct mp_pcm_state *state);

    // Return the list of devices currently available in the system. Use
    // ao_device_list_add() to add entries. The selected device will be set as
    // ao->device (using ao_device_desc.name).
    // Warning: the ao struct passed is not initialized with ao_driver->init().
    //          Instead, hotplug_init/hotplug_uninit is called. If these
    //          callbacks are not set, no driver initialization call is done
    //          on the ao struct.
    void (*list_devs)(struct ao *ao, struct ao_device_list *list);

    // If set, these are called before/after ao_driver->list_devs is called.
    // It is also assumed that the driver can do hotplugging - which means
    // it is expected to call ao_hotplug_event(ao) whenever the system's
    // audio device list changes. The player will then call list_devs() again.
    int (*hotplug_init)(struct ao *ao);
    void (*hotplug_uninit)(struct ao *ao);

    // For option parsing (see vo.h)
    int priv_size;
    const void *priv_defaults;
    const struct m_option *options;
    const char *options_prefix;
    const struct m_sub_options *global_opts;
};

// These functions can be called by AOs.

int ao_read_data(struct ao *ao, void **data, int samples, int64_t out_time_us);

bool ao_chmap_sel_adjust(struct ao *ao, const struct mp_chmap_sel *s,
                         struct mp_chmap *map);
bool ao_chmap_sel_adjust2(struct ao *ao, const struct mp_chmap_sel *s,
                          struct mp_chmap *map, bool safe_multichannel);
bool ao_chmap_sel_get_def(struct ao *ao, const struct mp_chmap_sel *s,
                          struct mp_chmap *map, int num);

// Add a deep copy of e to the list.
// Call from ao_driver->list_devs callback only.
void ao_device_list_add(struct ao_device_list *list, struct ao *ao,
                        struct ao_device_desc *e);

void ao_post_process_data(struct ao *ao, void **data, int num_samples);

struct ao_convert_fmt {
    int src_fmt;        // source AF_FORMAT_*
    int channels;       // number of channels
    int dst_bits;       // total target data sample size
    int pad_msb;        // padding in the MSB (i.e. required shifting)
    int pad_lsb;        // padding in LSB (required 0 bits) (ignored)
};

bool ao_can_convert_inplace(struct ao_convert_fmt *fmt);
bool ao_need_conversion(struct ao_convert_fmt *fmt);
void ao_convert_inplace(struct ao_convert_fmt *fmt, void **data, int num_samples);

void ao_wakeup_playthread(struct ao *ao);

int ao_read_data_converted(struct ao *ao, struct ao_convert_fmt *fmt,
                           void **data, int samples, int64_t out_time_us);

#endif
