#include "ao.h"
#include "audio/format.h"
#include "common/common.h"
#include "common/msg.h"
#include "internal.h"
#include "osdep/timer.h"
#include "stream/stream.h"
#include "options/m_option.h"

#include <aaudio/AAudio.h>
#include <android/api-level.h>
#include <bits/get_device_api_level_inlines.h>
#include <errno.h>
#include <linux/time.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

struct priv {
  AAudioStream *stream;
  int32_t x_run_count;

  int32_t device_id;
  int32_t buffer_capacity;
  aaudio_performance_mode_t performance_mode;
};

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

  AAudioStreamBuilder *builder = NULL;
  aaudio_result_t result;
  if ((result = AAudio_createStreamBuilder(&builder)) < 0) {
    MP_ERR(ao, "Failed to create stream builder: %" PRId32 "\n", result);
    return -1;
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
  AAudioStreamBuilder_setErrorCallback(builder, error_callback, ao);
  AAudioStreamBuilder_setPerformanceMode(builder, p->performance_mode);
  AAudioStreamBuilder_setBufferCapacityInFrames(builder, p->buffer_capacity);

  if ((result = AAudioStreamBuilder_openStream(builder, &p->stream)) < 0) {
    MP_ERR(ao, "Failed to open stream: %" PRId32 "\n", result);
    goto error;
  }

  ao->device_buffer = AAudioStream_getBufferCapacityInFrames(p->stream);
  
  return 1;
error:
  AAudioStreamBuilder_delete(builder);
  return -1;
}

static void start(struct ao *ao) {
  struct priv *p = ao->priv;
  aaudio_result_t result = AAudioStream_requestStart(p->stream);

  if (result < 0) {
    MP_ERR(ao, "Failed to start stream: %" PRId32 "\n", result);
  }
}

static bool set_pause(struct ao *ao, bool paused) {
  struct priv *p = ao->priv;
  aaudio_result_t result;
  if (paused) {
    if ((result = AAudioStream_requestPause(p->stream)) < 0) {
      MP_ERR(ao, "Failed to pause stream: %" PRId32 "\n", result);
      goto error;
    }
  } else {
    if ((result = AAudioStream_requestStart(p->stream)) < 0) {
      MP_ERR(ao, "Failed to start stream: %" PRId32 "\n", result);
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
    MP_ERR(ao, "Failed to stop stream: %" PRId32 "\n", result);
  }
}

static bool audio_write(struct ao *ao, void **data, int samples) {
  struct priv *p = ao->priv;
  aaudio_result_t result = AAudioStream_write(p->stream, 
    data[0], samples, INT64_MAX);
  
  if(result < 0) {
    MP_ERR(ao, "Failed to write data: %" PRId32 "\n", result);
    return false;
  }

  return true;
}
static void get_state(struct ao *ao, struct mp_pcm_state *state)
{
  struct priv *p = ao->priv;

  state->free_samples = MPCLAMP(
    AAudioStream_getBufferSizeInFrames(p->stream), 
    0, ao->device_buffer
  );
  state->queued_samples = ao->device_buffer - state->free_samples;

  int64_t frame_pos;
  int64_t time_ns;
  aaudio_result_t result = AAudioStream_getTimestamp(p->stream, CLOCK_MONOTONIC, &frame_pos, &time_ns);

  if(result >= 0)
  {
    state->delay = (double)(AAudioStream_getFramesWritten(p->stream) - frame_pos) / ao->samplerate;
  }

  int32_t x_run_count = AAudioStream_getXRunCount(p->stream);
  if(x_run_count > p->x_run_count)
  {
    state->playing = false;
  }
  else
  {
    aaudio_stream_state_t stream_state = AAudioStream_getState(p->stream);
    switch (stream_state) {
      case AAUDIO_STREAM_STATE_STARTING:
      case AAUDIO_STREAM_STATE_STARTED:
      case AAUDIO_STREAM_STATE_PAUSED:
      case AAUDIO_STREAM_STATE_PAUSING:
      case AAUDIO_STREAM_STATE_OPEN:
        state->playing = true;
    }
  }

  p->x_run_count = x_run_count;
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
    .write = audio_write,
    .get_state = get_state,
    .priv_size = sizeof(struct priv),
    .priv_defaults = &(const struct priv) {
      .device_id = AAUDIO_UNSPECIFIED,
      .buffer_capacity = AAUDIO_UNSPECIFIED,
      .performance_mode = AAUDIO_PERFORMANCE_MODE_NONE
    },
    .options_prefix = "aaudio",
    .options = (const struct m_option[]) {
        {"device-id", 
              OPT_CHOICE(device_id, {"auto", AAUDIO_UNSPECIFIED}),
              M_RANGE(1, 96000)},
        {"buffer-capacity", 
              OPT_CHOICE(buffer_capacity, {"auto", AAUDIO_UNSPECIFIED}),
              M_RANGE(1, 96000)},
        {"performance-mode", OPT_CHOICE(performance_mode, 
          {"none", AAUDIO_PERFORMANCE_MODE_NONE},
          {"low-latency", AAUDIO_PERFORMANCE_MODE_LOW_LATENCY},
          {"power-saving", AAUDIO_PERFORMANCE_MODE_POWER_SAVING}
        )},
        {0}
    },
};