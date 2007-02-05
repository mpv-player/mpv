#include <assert.h>
#include <string.h>

#include <polyp/polyplib.h>
#include <polyp/polyplib-error.h>
#include <polyp/mainloop.h>

#include "config.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"
#include "mp_msg.h"

#define	POLYP_CLIENT_NAME "MPlayer"

/** General driver info */
static ao_info_t info = {
    "Polypaudio audio output",
    "polyp",
    "Lennart Poettering",
    ""
};

/** The sink to connect to */
static char *sink = NULL;

/** Polypaudio playback stream object */
static struct pa_stream *stream = NULL;

/** Polypaudio connection context */
static struct pa_context *context = NULL;

/** Main event loop object */
static struct pa_mainloop *mainloop = NULL;

/** Some special libao macro magic */
LIBAO_EXTERN(polyp)

/** Wait until no further actions are pending on the connection context */
static void wait_for_completion(void) {
    assert(context && mainloop);

    while (pa_context_is_pending(context))
        pa_mainloop_iterate(mainloop, 1, NULL);
}

/** Make sure that the connection context doesn't starve to death */
static void keep_alive(void) {
    assert(context && mainloop);

    while (pa_mainloop_iterate(mainloop, 0, NULL) > 0);
}

/** Wait until the specified operation completes */
static void wait_for_operation(struct pa_operation *o) {
    assert(o && context && mainloop);

    while (pa_operation_get_state(o) == PA_OPERATION_RUNNING)
        pa_mainloop_iterate(mainloop, 1, NULL);

    pa_operation_unref(o);
}

/** libao initialization function, arguments are sampling frequency,
 * number of channels, sample type and some flags */
static int init(int rate_hz, int channels, int format, int flags) {
    struct pa_sample_spec ss;
    struct pa_buffer_attr a;
    char hn[128];
    char *host = NULL;

    assert(!context && !stream && !mainloop);

    if (ao_subdevice) {
        int i = strcspn(ao_subdevice, ":");
        if (i >= sizeof(hn))
            i = sizeof(hn)-1;

        if (i > 0) {
            strncpy(host = hn, ao_subdevice, i);
            hn[i] = 0;
        }

        if (ao_subdevice[i] == ':')
            sink = ao_subdevice+i+1;
    }

    mp_msg(MSGT_AO, MSGL_ERR, "AO: [polyp] -%s-%s-\n", host, sink);

    
    ss.channels = channels;
    ss.rate = rate_hz;

    switch (format) {
        case AF_FORMAT_U8:
            ss.format = PA_SAMPLE_U8;
            break;
        case AF_FORMAT_S16_LE:
            ss.format = PA_SAMPLE_S16LE;
            break;
        case AF_FORMAT_S16_BE:
            ss.format = PA_SAMPLE_S16BE;
            break;
        case AF_FORMAT_FLOAT_NE:
            ss.format = PA_SAMPLE_FLOAT32;
            break;
        default:
            mp_msg(MSGT_AO, MSGL_ERR, "AO: [polyp] Unsupported sample spec\n");
            goto fail;
    }


    if (!pa_sample_spec_valid(&ss)) {
        mp_msg(MSGT_AO, MSGL_ERR, "AO: [polyp] Invalid sample spec\n");
        goto fail;
    }
        

    mainloop = pa_mainloop_new();
    assert(mainloop);

    context = pa_context_new(pa_mainloop_get_api(mainloop), POLYP_CLIENT_NAME);
    assert(context);

    pa_context_connect(context, host, 1, NULL);

    wait_for_completion();

    if (pa_context_get_state(context) != PA_CONTEXT_READY) {
        mp_msg(MSGT_AO, MSGL_ERR, "AO: [polyp] Failed to connect to server: %s\n", pa_strerror(pa_context_errno(context)));
        goto fail;
    }

    stream = pa_stream_new(context, "audio stream", &ss);
    assert(stream);

    a.maxlength = pa_bytes_per_second(&ss)*1;
    a.tlength = a.maxlength*9/10;
    a.prebuf = a.tlength/2;
    a.minreq = a.tlength/10;
    
    pa_stream_connect_playback(stream, sink, &a, PA_STREAM_INTERPOLATE_LATENCY, PA_VOLUME_NORM);

    wait_for_completion();

    if (pa_stream_get_state(stream) != PA_STREAM_READY) {
        mp_msg(MSGT_AO, MSGL_ERR, "AO: [polyp] Failed to connect to server: %s\n", pa_strerror(pa_context_errno(context)));
        goto fail;
    }
    
    return 1;

fail:
    uninit(1);
    return 0;
}

/** Destroy libao driver */
static void uninit(int immed) {
    if (stream) {
        if (!immed && pa_stream_get_state(stream) == PA_STREAM_READY)
                wait_for_operation(pa_stream_drain(stream, NULL, NULL));
        
        pa_stream_unref(stream);
        stream = NULL;
    }

    if (context) {
        pa_context_unref(context);
        context = NULL;
    }

    if (mainloop) {
        pa_mainloop_free(mainloop);
        mainloop = NULL;
    }
}

/** Play the specified data to the polypaudio server */
static int play(void* data, int len, int flags) {
    assert(stream && context);

    if (pa_stream_get_state(stream) != PA_STREAM_READY)
        return -1;

    if (!len)
        wait_for_operation(pa_stream_trigger(stream, NULL, NULL));
    else
        pa_stream_write(stream, data, len, NULL, 0);

    wait_for_completion();

    if (pa_stream_get_state(stream) != PA_STREAM_READY)
        return -1;

    return len;
}

/** Pause the audio stream by corking it on the server */
static void audio_pause(void) {
    assert(stream && context && pa_stream_get_state(stream) == PA_STREAM_READY);
    wait_for_operation(pa_stream_cork(stream, 1, NULL, NULL));
}

/** Resume the audio stream by uncorking it on the server */
static void audio_resume(void) {
    assert(stream && context && pa_stream_get_state(stream) == PA_STREAM_READY);
    wait_for_operation(pa_stream_cork(stream, 0, NULL, NULL));
}

/** Reset the audio stream, i.e. flush the playback buffer on the server side */
static void reset(void) {
    assert(stream && context && pa_stream_get_state(stream) == PA_STREAM_READY);
    wait_for_operation(pa_stream_flush(stream, NULL, NULL));
}

/** Return number of bytes that may be written to the server without blocking */
static int get_space(void) {
    uint32_t l;
    assert(stream && context && pa_stream_get_state(stream) == PA_STREAM_READY);
    
    keep_alive();

    l = pa_stream_writable_size(stream);
    
    return l;
}

/* A temporary latency variable */
/* static pa_usec_t latency = 0; */

/* static void latency_func(struct pa_stream *s, const struct pa_latency_info *l, void *userdata) { */
/*     int negative = 0; */
    
/*     if (!l) { */
/*         mp_msg(MSGT_AO, MSGL_ERR, "AO: [polyp] Invalid sample spec: %s\n", pa_strerror(pa_context_errno(context))); */
/*         return; */
/*     } */

/*     latency = pa_stream_get_latency(s, l, &negative); */

/*     /\* Nor really required *\/ */
/*     if (negative) */
/*         latency = 0; */
/* } */

/** Return the current latency in seconds */
static float get_delay(void) {
    pa_usec_t latency;
    assert(stream && context && pa_stream_get_state(stream) == PA_STREAM_READY);

    /*     latency = 0; */
/*     wait_for_operation(pa_stream_get_latency(stream, latency_func, NULL)); */
    /*     pa_operation_unref(pa_stream_get_latency(stream, latency_func, NULL)); */

    latency = pa_stream_get_interpolated_latency(stream, NULL);
    
    return (float) latency/1000000;
}

/** A temporary variable to store the current volume */
static pa_volume_t volume = PA_VOLUME_NORM;

/** A callback function that is called when the
 * pa_context_get_sink_input_info() operation completes. Saves the
 * volume field of the specified structure to the global variable volume. */
static void info_func(struct pa_context *c, const struct pa_sink_input_info *i, int is_last, void *userdata) {
    if (is_last < 0) {
        mp_msg(MSGT_AO, MSGL_ERR, "AO: [polyp] Failed to get sink input info: %s\n", pa_strerror(pa_context_errno(context)));
        return;
    }

    if (!i)
        return;

    volume = i->volume;
}

/** Issue special libao controls on the device */
static int control(int cmd, void *arg) {
    
    if (!context || !stream)
        return CONTROL_ERROR;
    
    switch (cmd) {

        case AOCONTROL_SET_DEVICE:
            /* Change the playback device */
            sink = (char*)arg;
            return CONTROL_OK;

        case AOCONTROL_GET_DEVICE:
            /* Return the playback device */
            *(char**)arg = sink;
            return CONTROL_OK;
        
        case AOCONTROL_GET_VOLUME: {
            /* Return the current volume of the playback stream */
            ao_control_vol_t *vol = (ao_control_vol_t*) arg;
                
            volume = PA_VOLUME_NORM;
            wait_for_operation(pa_context_get_sink_input_info(context, pa_stream_get_index(stream), info_func, NULL));
            vol->left = vol->right = (int) (pa_volume_to_user(volume)*100);
            return CONTROL_OK;
        }
            
        case AOCONTROL_SET_VOLUME: {
            /* Set the playback volume of the stream */
            const ao_control_vol_t *vol = (ao_control_vol_t*) arg;
            int v = vol->left;
            if (vol->right > v)
                v = vol->left;
            
            wait_for_operation(pa_context_set_sink_input_volume(context, pa_stream_get_index(stream), pa_volume_from_user((double)v/100), NULL, NULL));
            
            return CONTROL_OK;
        }
            
        default:
            /* Unknown CONTROL command */
            return CONTROL_UNKNOWN;
    }
}

