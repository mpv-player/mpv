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

#include <CoreAudio/HostTime.h>

#include "config.h"
#include "ao.h"
#include "internal.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "options/m_option.h"
#include "misc/ring.h"
#include "common/msg.h"
#include "ao_coreaudio_chmap.h"
#include "ao_coreaudio_properties.h"
#include "ao_coreaudio_utils.h"

struct priv {
    AudioDeviceID device;
    AudioUnit audio_unit;

    uint64_t hw_latency_us;

    AudioStreamBasicDescription original_asbd;
    AudioStreamID original_asbd_stream;

    int change_physical_format;
};

static int64_t ca_get_hardware_latency(struct ao *ao) {
    struct priv *p = ao->priv;

    double audiounit_latency_sec = 0.0;
    uint32_t size = sizeof(audiounit_latency_sec);
    OSStatus err = AudioUnitGetProperty(
            p->audio_unit,
            kAudioUnitProperty_Latency,
            kAudioUnitScope_Global,
            0,
            &audiounit_latency_sec,
            &size);
    CHECK_CA_ERROR("cannot get audio unit latency");

    uint64_t audiounit_latency_us = audiounit_latency_sec * 1e6;
    uint64_t device_latency_us    = ca_get_device_latency_us(ao, p->device);

    MP_VERBOSE(ao, "audiounit latency [us]: %lld\n", audiounit_latency_us);
    MP_VERBOSE(ao, "device latency [us]: %lld\n", device_latency_us);

    return audiounit_latency_us + device_latency_us;

coreaudio_error:
    return 0;
}

static OSStatus render_cb_lpcm(void *ctx, AudioUnitRenderActionFlags *aflags,
                              const AudioTimeStamp *ts, UInt32 bus,
                              UInt32 frames, AudioBufferList *buffer_list)
{
    struct ao *ao   = ctx;
    struct priv *p  = ao->priv;
    void *planes[MP_NUM_CHANNELS] = {0};

    for (int n = 0; n < ao->num_planes; n++)
        planes[n] = buffer_list->mBuffers[n].mData;

    int64_t end = mp_time_us();
    end += p->hw_latency_us + ca_get_latency(ts) + ca_frames_to_us(ao, frames);
    ao_read_data(ao, planes, frames, end);
    return noErr;
}

static int get_volume(struct ao *ao, struct ao_control_vol *vol) {
    struct priv *p = ao->priv;
    float auvol;
    OSStatus err =
        AudioUnitGetParameter(p->audio_unit, kHALOutputParam_Volume,
                              kAudioUnitScope_Global, 0, &auvol);

    CHECK_CA_ERROR("could not get HAL output volume");
    vol->left = vol->right = auvol * 100.0;
    return CONTROL_TRUE;
coreaudio_error:
    return CONTROL_ERROR;
}

static int set_volume(struct ao *ao, struct ao_control_vol *vol) {
    struct priv *p = ao->priv;
    float auvol = (vol->left + vol->right) / 200.0;
    OSStatus err =
        AudioUnitSetParameter(p->audio_unit, kHALOutputParam_Volume,
                              kAudioUnitScope_Global, 0, auvol, 0);
    CHECK_CA_ERROR("could not set HAL output volume");
    return CONTROL_TRUE;
coreaudio_error:
    return CONTROL_ERROR;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    switch (cmd) {
    case AOCONTROL_GET_VOLUME:
        return get_volume(ao, arg);
    case AOCONTROL_SET_VOLUME:
        return set_volume(ao, arg);
    case AOCONTROL_HAS_SOFT_VOLUME:
        return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

static bool init_audiounit(struct ao *ao, AudioStreamBasicDescription asbd);
static void init_physical_format(struct ao *ao);

static bool reinit_device(struct ao *ao) {
    struct priv *p = ao->priv;

    OSStatus err = ca_select_device(ao, ao->device, &p->device);
    CHECK_CA_ERROR("failed to select device");

    return true;

coreaudio_error:
    return false;
}

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (!af_fmt_is_pcm(ao->format) || (ao->init_flags & AO_INIT_EXCLUSIVE)) {
        MP_VERBOSE(ao, "redirecting to coreaudio_exclusive\n");
        ao->redirect = "coreaudio_exclusive";
        return CONTROL_ERROR;
    }

    if (!reinit_device(ao))
        goto coreaudio_error;

    if (p->change_physical_format)
        init_physical_format(ao);

    if (!ca_init_chmap(ao, p->device))
        goto coreaudio_error;

    AudioStreamBasicDescription asbd;
    ca_fill_asbd(ao, &asbd);

    if (!init_audiounit(ao, asbd))
        goto coreaudio_error;

    return CONTROL_OK;

coreaudio_error:
    return CONTROL_ERROR;
}

static void init_physical_format(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSErr err;

    void *tmp = talloc_new(NULL);

    AudioStreamBasicDescription asbd;
    ca_fill_asbd(ao, &asbd);

    AudioStreamID *streams;
    size_t n_streams;

    err = CA_GET_ARY_O(p->device, kAudioDevicePropertyStreams,
                       &streams, &n_streams);
    CHECK_CA_ERROR("could not get number of streams");

    talloc_steal(tmp, streams);

    MP_VERBOSE(ao, "Found %zd substream(s).\n", n_streams);

    for (int i = 0; i < n_streams; i++) {
        AudioStreamRangedDescription *formats;
        size_t n_formats;

        MP_VERBOSE(ao, "Looking at formats in substream %d...\n", i);

        err = CA_GET_ARY(streams[i], kAudioStreamPropertyAvailablePhysicalFormats,
                         &formats, &n_formats);

        if (!CHECK_CA_WARN("could not get number of stream formats"))
            continue; // try next one

        talloc_steal(tmp, formats);

        uint32_t direction;
        err = CA_GET(streams[i], kAudioStreamPropertyDirection, &direction);
        CHECK_CA_ERROR("could not get stream direction");
        if (direction != 0) {
            MP_VERBOSE(ao, "Not an output stream.\n");
            continue;
        }

        AudioStreamBasicDescription best_asbd = {0};

        for (int j = 0; j < n_formats; j++) {
            AudioStreamBasicDescription *stream_asbd = &formats[j].mFormat;

            ca_print_asbd(ao, "- ", stream_asbd);

            if (!best_asbd.mFormatID || ca_asbd_is_better(&asbd, &best_asbd,
                                                          stream_asbd))
                best_asbd = *stream_asbd;
        }

        if (best_asbd.mFormatID) {
            p->original_asbd_stream = streams[i];
            err = CA_GET(p->original_asbd_stream,
                         kAudioStreamPropertyPhysicalFormat,
                         &p->original_asbd);
            CHECK_CA_WARN("could not get current physical stream format");

            if (ca_asbd_equals(&p->original_asbd, &best_asbd)) {
                MP_VERBOSE(ao, "Requested format already set, not changing.\n");
                p->original_asbd.mFormatID = 0;
                break;
            }

            if (!ca_change_physical_format_sync(ao, streams[i], best_asbd))
                p->original_asbd = (AudioStreamBasicDescription){0};
            break;
        }
    }

coreaudio_error:
    talloc_free(tmp);
    return;
}

static bool init_audiounit(struct ao *ao, AudioStreamBasicDescription asbd)
{
    OSStatus err;
    uint32_t size;
    struct priv *p = ao->priv;

    AudioComponentDescription desc = (AudioComponentDescription) {
        .componentType         = kAudioUnitType_Output,
        .componentSubType      = (ao->device) ?
                                    kAudioUnitSubType_HALOutput :
                                    kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags        = 0,
        .componentFlagsMask    = 0,
    };

    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    if (comp == NULL) {
        MP_ERR(ao, "unable to find audio component\n");
        goto coreaudio_error;
    }

    err = AudioComponentInstanceNew(comp, &(p->audio_unit));
    CHECK_CA_ERROR("unable to open audio component");

    err = AudioUnitInitialize(p->audio_unit);
    CHECK_CA_ERROR_L(coreaudio_error_component,
                     "unable to initialize audio unit");

    size = sizeof(AudioStreamBasicDescription);
    err = AudioUnitSetProperty(p->audio_unit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0, &asbd, size);

    CHECK_CA_ERROR_L(coreaudio_error_audiounit,
                     "unable to set the input format on the audio unit");

    err = AudioUnitSetProperty(p->audio_unit,
                               kAudioOutputUnitProperty_CurrentDevice,
                               kAudioUnitScope_Global, 0, &p->device,
                               sizeof(p->device));
    CHECK_CA_ERROR_L(coreaudio_error_audiounit,
                     "can't link audio unit to selected device");

    p->hw_latency_us = ca_get_hardware_latency(ao);

    AURenderCallbackStruct render_cb = (AURenderCallbackStruct) {
        .inputProc       = render_cb_lpcm,
        .inputProcRefCon = ao,
    };

    err = AudioUnitSetProperty(p->audio_unit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input, 0, &render_cb,
                               sizeof(AURenderCallbackStruct));

    CHECK_CA_ERROR_L(coreaudio_error_audiounit,
                     "unable to set render callback on audio unit");

    return true;

coreaudio_error_audiounit:
    AudioUnitUninitialize(p->audio_unit);
coreaudio_error_component:
    AudioComponentInstanceDispose(p->audio_unit);
coreaudio_error:
    return false;
}

static void stop(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSStatus err = AudioOutputUnitStop(p->audio_unit);
    CHECK_CA_WARN("can't stop audio unit");
}

static void start(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSStatus err = AudioOutputUnitStart(p->audio_unit);
    CHECK_CA_WARN("can't start audio unit");
}


static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;
    AudioOutputUnitStop(p->audio_unit);
    AudioUnitUninitialize(p->audio_unit);
    AudioComponentInstanceDispose(p->audio_unit);

    if (p->original_asbd.mFormatID) {
        OSStatus err = CA_SET(p->original_asbd_stream,
                              kAudioStreamPropertyPhysicalFormat,
                              &p->original_asbd);
        CHECK_CA_WARN("could not restore physical stream format");
    }
}

static OSStatus hotplug_cb(AudioObjectID id, UInt32 naddr,
                           const AudioObjectPropertyAddress addr[],
                           void *ctx)
{
    struct ao *ao = ctx;
    MP_VERBOSE(ao, "Handling potential hotplug event...\n");
    reinit_device(ao);
    ao_hotplug_event(ao);
    return noErr;
}

static uint32_t hotplug_properties[] = {
    kAudioHardwarePropertyDevices,
    kAudioHardwarePropertyDefaultOutputDevice
};

static int hotplug_init(struct ao *ao)
{
    if (!reinit_device(ao))
        goto coreaudio_error;

    OSStatus err = noErr;
    for (int i = 0; i < MP_ARRAY_SIZE(hotplug_properties); i++) {
        AudioObjectPropertyAddress addr = {
            hotplug_properties[i],
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };
        err = AudioObjectAddPropertyListener(
            kAudioObjectSystemObject, &addr, hotplug_cb, (void *)ao);
        if (err != noErr) {
            char *c1 = mp_tag_str(hotplug_properties[i]);
            char *c2 = mp_tag_str(err);
            MP_ERR(ao, "failed to set device listener %s (%s)", c1, c2);
            goto coreaudio_error;
        }
    }

    return 0;

coreaudio_error:
    return -1;
}

static void hotplug_uninit(struct ao *ao)
{
    OSStatus err = noErr;
    for (int i = 0; i < MP_ARRAY_SIZE(hotplug_properties); i++) {
        AudioObjectPropertyAddress addr = {
            hotplug_properties[i],
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };
        err = AudioObjectRemovePropertyListener(
            kAudioObjectSystemObject, &addr, hotplug_cb, (void *)ao);
        if (err != noErr) {
            char *c1 = mp_tag_str(hotplug_properties[i]);
            char *c2 = mp_tag_str(err);
            MP_ERR(ao, "failed to set device listener %s (%s)", c1, c2);
        }
    }
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_coreaudio = {
    .description    = "CoreAudio AudioUnit",
    .name           = "coreaudio",
    .uninit         = uninit,
    .init           = init,
    .control        = control,
    .reset          = stop,
    .resume         = start,
    .hotplug_init   = hotplug_init,
    .hotplug_uninit = hotplug_uninit,
    .list_devs      = ca_get_device_list,
    .priv_size      = sizeof(struct priv),
    .options = (const struct m_option[]){
        OPT_FLAG("change-physical-format", change_physical_format, 0),
        {0}
    },
    .options_prefix = "coreaudio",
};
