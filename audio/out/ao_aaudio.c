#include "ao.h"
#include "audio/format.h"
#include "common/msg.h"
#include "internal.h"
#include "osdep/timer.h"
#include "stream/stream.h"
#include "options/m_option.h"

#include <aaudio/AAudio.h>
#include <android/api-level.h>
#include <bits/get_device_api_level_inlines.h>
#include <errno.h>
#include <stdint.h>

struct priv {
  AAudioStream *stream;
  pthread_mutex_t buffer_lock;

  int32_t device_id;
  int32_t frames_per_callback;
  aaudio_performance_mode_t performance_mode;
};

static aaudio_data_callback_result_t data_callback(AAudioStream *stream,
                                                   void *context, void *data,
                                                   int32_t num_frames) {
  struct ao* ao = context;
  struct priv* p = ao->priv;

  pthread_mutex_lock(&p->buffer_lock);
  ao_read_data(ao, &data, num_frames,
               mp_time_us() +
                   1000000LL * ((double)num_frames / (double)ao->samplerate));
  pthread_mutex_unlock(&p->buffer_lock);

  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void error_callback(AAudioStream *stream, void *context, aaudio_result_t error) {
  struct ao* ao = context;
  switch (error) {
    case AAUDIO_ERROR_DISCONNECTED:
      MP_ERR(ao, "Device disconnected, trying to reload...");
    break;
    default:
      MP_ERR(ao, "Unkown error %" PRId32 ", trying to reload...", error);
  }

  ao_request_reload(ao);
}

static void uninit(struct ao *ao) {
  struct priv *p = ao->priv;

  if(p->stream != NULL)
    AAudioStream_close(p->stream);
}

static int init(struct ao *ao) {
  struct priv *p = ao->priv;

  int r = pthread_mutex_init(&p->buffer_lock, NULL);
  if (r) {
      MP_ERR(ao, "Failed to initialize the mutex: %d\n", r);
      goto error;
  }

  AAudioStreamBuilder *builder = NULL;
  aaudio_result_t result;
  if ((result = AAudio_createStreamBuilder(&builder)) < 0) {
    MP_ERR(ao, "Failed to create stream builder: %" PRId32, result);
    goto error;
  }

  aaudio_format_t format = AAUDIO_UNSPECIFIED;

  int api_level = android_get_device_api_level();

  // See: https://google.github.io/oboe/namespaceoboe.html#a92afc593e856571aacbfd02e57075df6
  // Not yet in the NDK headers
  // if(af_fmt_is_spdif(ao->format) && api_level >= 34)
  // {
  //     dataFormat = AAUDIO_FORMAT_IEC61937; // In the documentation but
  //     somehow not in the headers
  // } else
  if (af_fmt_is_float(ao->format)) {
    ao->format = AF_FORMAT_FLOAT;
    format = AAUDIO_FORMAT_PCM_FLOAT;
  } else if (af_fmt_is_int(ao->format)) {
    int bytes = af_fmt_to_bytes(ao->format);
    if (bytes > 2) {
      if (api_level >= 31) {
        ao->format = AF_FORMAT_S32;
        format = AAUDIO_FORMAT_PCM_I32;
      } else {
        ao->format = AF_FORMAT_FLOAT;
        format = AAUDIO_FORMAT_PCM_FLOAT;
      }
    } else {
      ao->format = AF_FORMAT_S16;
      format = AAUDIO_FORMAT_PCM_I16;
    }
  }

  AAudioStreamBuilder_setDeviceId(builder, p->device_id);
  AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
  AAudioStreamBuilder_setSharingMode(builder,
                                     (ao->init_flags & AO_INIT_EXCLUSIVE)
                                         ? AAUDIO_SHARING_MODE_EXCLUSIVE
                                         : AAUDIO_SHARING_MODE_SHARED);
  AAudioStreamBuilder_setFormat(builder, format);
  AAudioStreamBuilder_setChannelCount(builder, ao->channels.num);
  AAudioStreamBuilder_setSampleRate(builder, ao->samplerate);
  AAudioStreamBuilder_setDataCallback(builder, data_callback, ao);
  AAudioStreamBuilder_setFramesPerDataCallback(builder, p->frames_per_callback);
  AAudioStreamBuilder_setErrorCallback(builder, error_callback, ao);
  AAudioStreamBuilder_setPerformanceMode(builder, p->performance_mode);

  if ((result = AAudioStreamBuilder_openStream(builder, &p->stream)) < 0) {
    MP_ERR(ao, "Failed to open stream: %" PRId32, result);
    goto error;
  }
  AAudioStreamBuilder_delete(builder);

  return 1;
error:
  uninit(ao);
  return -1;
}

static void start(struct ao *ao) {
  struct priv *p = ao->priv;
  aaudio_result_t result = AAudioStream_requestStart(p->stream);

  if (result < 0) {
    MP_ERR(ao, "Failed to start stream: %" PRId32, result);
  }
}

static bool set_pause(struct ao *ao, bool paused) {
  struct priv *p = ao->priv;
  aaudio_result_t result;
  if (paused) {
    if ((result = AAudioStream_requestPause(p->stream)) < 0) {
      MP_ERR(ao, "Failed to pause stream: %" PRId32, result);
      goto error;
    }
  } else {
    if ((result = AAudioStream_requestStart(p->stream)) < 0) {
      MP_ERR(ao, "Failed to start stream: %" PRId32, result);
      goto error;
    }
  }

  return true;
error:
  return false;
}

static void reset(struct ao *ao) {
  struct priv *p = ao->priv;
  aaudio_result_t result;

  if ((result = AAudioStream_requestStop(p->stream)) < 0) {
    MP_ERR(ao, "Failed to stop stream: %" PRId32, result);
  }
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_aaudio = {
    .description = "AAudio audio output",
    .name = "aaudio",
    .init = init,
    .uninit = uninit,
    .start = start,
    .reset = reset,
    .set_pause = set_pause,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
      .device_id = AAUDIO_UNSPECIFIED,
      .frames_per_callback = AAUDIO_UNSPECIFIED,
      .performance_mode = AAUDIO_PERFORMANCE_MODE_NONE
    },
    .options_prefix = "aaudio",
    .options = (const struct m_option[]) {
        {"device-id", 
              OPT_CHOICE(device_id, {"auto", AAUDIO_UNSPECIFIED}),
              M_RANGE(1, 96000)},
        {"frames-per-callback", 
              OPT_CHOICE(frames_per_callback, {"auto", AAUDIO_UNSPECIFIED}),
              M_RANGE(1, 96000)},
        {"performance-mode", OPT_CHOICE(performance_mode, 
          {"none", AAUDIO_PERFORMANCE_MODE_NONE},
          {"low-latency", AAUDIO_PERFORMANCE_MODE_LOW_LATENCY},
          {"power-saving", AAUDIO_PERFORMANCE_MODE_POWER_SAVING}
        )},
        {0}
    },
};