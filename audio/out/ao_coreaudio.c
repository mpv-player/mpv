/*
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

#include <CoreAudio/HostTime.h>

#include "config.h"
#include "ao.h"
#include "internal.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "options/m_option.h"
#include "misc/ring.h"
#include "common/msg.h"
#include "audio/out/ao_coreaudio_properties.h"
#include "audio/out/ao_coreaudio_utils.h"

struct priv {
    AudioDeviceID device;
    AudioUnit audio_unit;

    uint64_t hw_latency_us;
};

bool ca_layout_to_mp_chmap(struct ao *ao, AudioChannelLayout *layout,
                           struct mp_chmap *chmap);

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

    uint32_t frames = 0;
    err = CA_GET_O(p->device, kAudioDevicePropertyLatency, &frames);
    CHECK_CA_ERROR("cannot get device latency");

    uint64_t audiounit_latency_us = audiounit_latency_sec * 1e6;
    uint64_t device_latency_us    = ca_frames_to_us(ao, frames);

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
    AudioBuffer buf = buffer_list->mBuffers[0];

    int64_t end = mp_time_us();
    end += p->hw_latency_us + ca_get_latency(ts) + ca_frames_to_us(ao, frames);
    ao_read_data(ao, &buf.mData, frames, end);
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

static bool init_chmap(struct ao *ao);
static bool init_audiounit(struct ao *ao, AudioStreamBasicDescription asbd);

static bool reinit_device(struct ao *ao) {
    struct priv *p = ao->priv;

    OSStatus err = ca_select_device(ao, ao->device, &p->device);
    CHECK_CA_ERROR("failed to select device");

    char *uid;
    err = CA_GET_STR(p->device, kAudioDevicePropertyDeviceUID, &uid);
    CHECK_CA_ERROR("failed to get device UID");
    ao->detected_device = talloc_steal(ao, uid);

    return true;

coreaudio_error:
    return false;
}

static int init(struct ao *ao)
{
    if (AF_FORMAT_IS_IEC61937(ao->format)) {
        MP_WARN(ao, "detected IEC61937, redirecting to coreaudio_exclusive\n");
        ao->redirect = "coreaudio_exclusive";
        return CONTROL_ERROR;
    }

    if (!reinit_device(ao))
        goto coreaudio_error;

    if (!init_chmap(ao))
        goto coreaudio_error;

    ao->format = af_fmt_from_planar(ao->format);

    AudioStreamBasicDescription asbd;
    ca_fill_asbd(ao, &asbd);

    if (!init_audiounit(ao, asbd))
        goto coreaudio_error;

    return CONTROL_OK;

coreaudio_error:
    return CONTROL_ERROR;
}

static AudioChannelLayout* ca_query_layout(struct ao *ao, void *talloc_ctx)
{
    struct priv *p = ao->priv;
    OSStatus err;
    uint32_t psize;
    AudioChannelLayout *r = NULL;

    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = kAudioDevicePropertyPreferredChannelLayout,
        .mScope    = kAudioDevicePropertyScopeOutput,
        .mElement  = kAudioObjectPropertyElementWildcard,
    };

    err = AudioObjectGetPropertyDataSize(p->device, &p_addr, 0, NULL, &psize);
    CHECK_CA_ERROR("could not get device preferred layout (size)");

    r = talloc_size(talloc_ctx, psize);

    err = AudioObjectGetPropertyData(p->device, &p_addr, 0, NULL, &psize, r);
    CHECK_CA_ERROR("could not get device preferred layout (get)");

coreaudio_error:
    return r;
}

static AudioChannelLayout* ca_query_stereo_layout(struct ao *ao, void *talloc_ctx)
{
    struct priv *p = ao->priv;
    OSStatus err;
    const int nch = 2;
    uint32_t channels[nch];
    AudioChannelLayout *r = NULL;

    AudioObjectPropertyAddress p_addr = (AudioObjectPropertyAddress) {
        .mSelector = kAudioDevicePropertyPreferredChannelsForStereo,
        .mScope    = kAudioDevicePropertyScopeOutput,
        .mElement  = kAudioObjectPropertyElementWildcard,
    };

    uint32_t psize = sizeof(channels);
    err = AudioObjectGetPropertyData(p->device, &p_addr, 0, NULL, &psize, channels);
    CHECK_CA_ERROR("could not get device preferred stereo layout");

    psize = sizeof(AudioChannelLayout) + nch * sizeof(AudioChannelDescription);
    r = talloc_zero_size(talloc_ctx, psize);
    r->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
    r->mNumberChannelDescriptions = nch;

    AudioChannelDescription desc = {0};
    desc.mChannelFlags = kAudioChannelFlags_AllOff;

    for(int i = 0; i < nch; i++) {
        desc.mChannelLabel = channels[i];
        r->mChannelDescriptions[i] = desc;
    }

coreaudio_error:
    return r;
}

static bool init_chmap(struct ao *ao)
{
    struct priv *p = ao->priv;
    void *ta_ctx = talloc_new(NULL);

    struct mp_chmap_sel chmap_sel = {.tmp = p};
    struct mp_chmap chmap = {0};

    AudioChannelLayout *ml = ca_query_layout(ao, ta_ctx);
    if (ml && ca_layout_to_mp_chmap(ao, ml, &chmap))
        mp_chmap_sel_add_map(&chmap_sel, &chmap);

    AudioChannelLayout *sl = ca_query_stereo_layout(ao, ta_ctx);
    if (sl && ca_layout_to_mp_chmap(ao, sl, &chmap))
        mp_chmap_sel_add_map(&chmap_sel, &chmap);

    talloc_free(ta_ctx);

    if (!ao_chmap_sel_adjust(ao, &chmap_sel, &ao->channels)) {
        MP_ERR(ao, "could not select a suitable channel map among the "
                   "hardware supported ones. Make sure to configure your "
                   "output device correctly in 'Audio MIDI Setup.app'\n");
        goto coreaudio_error;
    }

    return true;

coreaudio_error:
    return false;
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
}

static OSStatus hotplug_cb(AudioObjectID id, UInt32 naddr,
                           const AudioObjectPropertyAddress addr[],
                           void *ctx) {
    reinit_device(ctx);
    ao_hotplug_event(ctx);
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
            char *c1 = fourcc_repr(hotplug_properties[i]);
            char *c2 = fourcc_repr(err);
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
            char *c1 = fourcc_repr(hotplug_properties[i]);
            char *c2 = fourcc_repr(err);
            MP_ERR(ao, "failed to set device listener %s (%s)", c1, c2);
        }
    }
}


// Channel Mapping functions
static const int speaker_map[][2] = {
    { kAudioChannelLabel_Left,                 MP_SPEAKER_ID_FL   },
    { kAudioChannelLabel_Right,                MP_SPEAKER_ID_FR   },
    { kAudioChannelLabel_Center,               MP_SPEAKER_ID_FC   },
    { kAudioChannelLabel_LFEScreen,            MP_SPEAKER_ID_LFE  },
    { kAudioChannelLabel_LeftSurround,         MP_SPEAKER_ID_BL   },
    { kAudioChannelLabel_RightSurround,        MP_SPEAKER_ID_BR   },
    { kAudioChannelLabel_LeftCenter,           MP_SPEAKER_ID_FLC  },
    { kAudioChannelLabel_RightCenter,          MP_SPEAKER_ID_FRC  },
    { kAudioChannelLabel_CenterSurround,       MP_SPEAKER_ID_BC   },
    { kAudioChannelLabel_LeftSurroundDirect,   MP_SPEAKER_ID_SL   },
    { kAudioChannelLabel_RightSurroundDirect,  MP_SPEAKER_ID_SR   },
    { kAudioChannelLabel_TopCenterSurround,    MP_SPEAKER_ID_TC   },
    { kAudioChannelLabel_VerticalHeightLeft,   MP_SPEAKER_ID_TFL  },
    { kAudioChannelLabel_VerticalHeightCenter, MP_SPEAKER_ID_TFC  },
    { kAudioChannelLabel_VerticalHeightRight,  MP_SPEAKER_ID_TFR  },
    { kAudioChannelLabel_TopBackLeft,          MP_SPEAKER_ID_TBL  },
    { kAudioChannelLabel_TopBackCenter,        MP_SPEAKER_ID_TBC  },
    { kAudioChannelLabel_TopBackRight,         MP_SPEAKER_ID_TBR  },

    // unofficial extensions
    { kAudioChannelLabel_RearSurroundLeft,     MP_SPEAKER_ID_SDL  },
    { kAudioChannelLabel_RearSurroundRight,    MP_SPEAKER_ID_SDR  },
    { kAudioChannelLabel_LeftWide,             MP_SPEAKER_ID_WL   },
    { kAudioChannelLabel_RightWide,            MP_SPEAKER_ID_WR   },
    { kAudioChannelLabel_LFE2,                 MP_SPEAKER_ID_LFE2 },

    { kAudioChannelLabel_HeadphonesLeft,       MP_SPEAKER_ID_DL   },
    { kAudioChannelLabel_HeadphonesRight,      MP_SPEAKER_ID_DR   },

    { kAudioChannelLabel_Unknown,              MP_SPEAKER_ID_NA  },

    { 0,                                       -1                 },
};

static int ca_label_to_mp_speaker_id(AudioChannelLabel label)
{
    for (int i = 0; speaker_map[i][1] >= 0; i++)
        if (speaker_map[i][0] == label)
            return speaker_map[i][1];
    return -1;
}

static void ca_log_layout(struct ao *ao, int l, AudioChannelLayout *layout)
{
    if (!mp_msg_test(ao->log, l))
        return;

    AudioChannelDescription *descs = layout->mChannelDescriptions;

    mp_msg(ao->log, l, "layout: tag: <%u>, bitmap: <%u>, "
                       "descriptions <%u>\n",
                       (unsigned) layout->mChannelLayoutTag,
                       (unsigned) layout->mChannelBitmap,
                       (unsigned) layout->mNumberChannelDescriptions);

    for (int i = 0; i < layout->mNumberChannelDescriptions; i++) {
        AudioChannelDescription d = descs[i];
        mp_msg(ao->log, l, " - description %d: label <%u, %u>, "
            " flags: <%u>, coords: <%f, %f, %f>\n", i,
            (unsigned) d.mChannelLabel,
            (unsigned) ca_label_to_mp_speaker_id(d.mChannelLabel),
            (unsigned) d.mChannelFlags,
            d.mCoordinates[0],
            d.mCoordinates[1],
            d.mCoordinates[2]);
    }
}

static AudioChannelLayout *ca_layout_to_custom_layout(
    struct ao *ao, void *talloc_ctx, AudioChannelLayout *l)
{
    AudioChannelLayoutTag tag = l->mChannelLayoutTag;
    AudioChannelLayout *r;
    OSStatus err;

    if (tag == kAudioChannelLayoutTag_UseChannelBitmap) {
        uint32_t psize;
        err = AudioFormatGetPropertyInfo(
            kAudioFormatProperty_ChannelLayoutForBitmap,
            sizeof(uint32_t), &l->mChannelBitmap, &psize);
        CHECK_CA_ERROR("failed to convert channel bitmap to descriptions (info)");
        r = talloc_size(NULL, psize);
        err = AudioFormatGetProperty(
            kAudioFormatProperty_ChannelLayoutForBitmap,
            sizeof(uint32_t), &l->mChannelBitmap, &psize, r);
        CHECK_CA_ERROR("failed to convert channel bitmap to descriptions (get)");
    } else if (tag != kAudioChannelLayoutTag_UseChannelDescriptions) {
        uint32_t psize;
        err = AudioFormatGetPropertyInfo(
            kAudioFormatProperty_ChannelLayoutForTag,
            sizeof(AudioChannelLayoutTag), &l->mChannelLayoutTag, &psize);
        r = talloc_size(NULL, psize);
        CHECK_CA_ERROR("failed to convert channel tag to descriptions (info)");
        err = AudioFormatGetProperty(
            kAudioFormatProperty_ChannelLayoutForTag,
            sizeof(AudioChannelLayoutTag), &l->mChannelLayoutTag, &psize, r);
        CHECK_CA_ERROR("failed to convert channel tag to descriptions (get)");
    } else {
        r = l;
    }

    return r;
coreaudio_error:
    return NULL;
}

bool ca_layout_to_mp_chmap(struct ao *ao, AudioChannelLayout *layout,
                           struct mp_chmap *chmap)
{
    void *talloc_ctx = talloc_new(NULL);

    MP_DBG(ao, "input channel layout:\n");
    ca_log_layout(ao, MSGL_DEBUG, layout);

    AudioChannelLayout *l = ca_layout_to_custom_layout(ao, talloc_ctx, layout);
    if (!l)
        goto coreaudio_error;

    MP_VERBOSE(ao, "converted input channel layout:\n");
    ca_log_layout(ao, MSGL_V, l);

    if (l->mNumberChannelDescriptions > MP_NUM_CHANNELS) {
        MP_VERBOSE(ao, "layout has too many descriptions (%u, max: %d)\n",
                   (unsigned) l->mNumberChannelDescriptions, MP_NUM_CHANNELS);
        return false;
    }

    int next_na = MP_SPEAKER_ID_NA;
    for (int n = 0; n < l->mNumberChannelDescriptions; n++) {
        AudioChannelLabel label = l->mChannelDescriptions[n].mChannelLabel;
        int speaker = ca_label_to_mp_speaker_id(label);
        if (speaker < 0) {
            MP_VERBOSE(ao, "channel label=%u unusable to build channel "
                           "bitmap, skipping layout\n", (unsigned) label);
            goto coreaudio_error;
        } else {
            chmap->speaker[n] = speaker;
            chmap->num = n + 1;
        }
    }

    talloc_free(talloc_ctx);
    return chmap->num > 0;
coreaudio_error:
    MP_VERBOSE(ao, "converted input channel layout (failed):\n");
    ca_log_layout(ao, MSGL_V, layout);
    talloc_free(talloc_ctx);
    return false;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_coreaudio = {
    .description    = "CoreAudio AudioUnit",
    .name           = "coreaudio",
    .uninit         = uninit,
    .init           = init,
    .control        = control,
    .pause          = stop,
    .resume         = start,
    .hotplug_init   = hotplug_init,
    .hotplug_uninit = hotplug_uninit,
    .list_devs      = ca_get_device_list,
    .priv_size      = sizeof(struct priv),
};
