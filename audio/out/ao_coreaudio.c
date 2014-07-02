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

    // options
    int opt_device_id;
    int opt_list;
};

bool ca_layout_to_mp_chmap(struct ao *ao, AudioChannelLayout *layout,
                           struct mp_chmap *chmap);

static int64_t ca_get_latency(const AudioTimeStamp *ts)
{
    uint64_t out = AudioConvertHostTimeToNanos(ts->mHostTime);
    uint64_t now = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());

    if (now > out)
        return 0;

    return (out - now) * 1e-3;
}

static OSStatus render_cb_lpcm(void *ctx, AudioUnitRenderActionFlags *aflags,
                              const AudioTimeStamp *ts, UInt32 bus,
                              UInt32 frames, AudioBufferList *buffer_list)
{
    struct ao *ao   = ctx;
    AudioBuffer buf = buffer_list->mBuffers[0];

    const int64_t playback_us = frames / (float) ao->samplerate * 1e6;
    const int64_t latency_us  = ca_get_latency(ts);

    const int64_t end = mp_time_us() + playback_us + latency_us;
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
    }
    return CONTROL_UNKNOWN;
}

static bool init_chmap(struct ao *ao);
static bool init_audiounit(struct ao *ao, AudioStreamBasicDescription asbd);

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (p->opt_list) ca_print_device_list(ao);

    ao->per_application_mixer = true;
    ao->no_persistent_volume  = true;

    OSStatus err = ca_select_device(ao, p->opt_device_id, &p->device);
    CHECK_CA_ERROR("failed to select device");

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

static bool init_chmap(struct ao *ao)
{
    struct priv *p = ao->priv;
    OSStatus err;
    AudioChannelLayout *layouts;
    size_t n_layouts;

    err = CA_GET_ARY_O(p->device,
                       kAudioDevicePropertyPreferredChannelLayout,
                       &layouts, &n_layouts);
    CHECK_CA_ERROR("could not get audio device prefered layouts");

    struct mp_chmap_sel chmap_sel = {0};
    for (int i = 0; i < n_layouts; i++) {
        struct mp_chmap chmap = {0};
        if (ca_layout_to_mp_chmap(ao, &layouts[i], &chmap))
            mp_chmap_sel_add_map(&chmap_sel, &chmap);
    }

    talloc_free(layouts);

    if (ao->channels.num < 3) {
        struct mp_chmap chmap;
        mp_chmap_from_channels(&chmap, ao->channels.num);
        mp_chmap_sel_add_map(&chmap_sel, &chmap);
    }

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
        .componentSubType      = (p->opt_device_id < 0) ?
                                    kAudioUnitSubType_DefaultOutput :
                                    kAudioUnitSubType_HALOutput,
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

    { kAudioChannelLabel_Unknown,              -1 },
};

static int ca_label_to_mp_speaker_id(AudioChannelLabel label)
{
    for (int i = 0; speaker_map[i][1] >= 0; i++)
        if (speaker_map[i][0] == label)
            return speaker_map[i][1];
    return -1;
}

static void ca_log_layout(struct ao *ao, AudioChannelLayout *layout)
{
    if (!mp_msg_test(ao->log, MSGL_V))
        return;

    AudioChannelDescription *descs = layout->mChannelDescriptions;

    MP_VERBOSE(ao, "layout: tag: <%d>, bitmap: <%d>, "
                   "descriptions <%d>\n",
                   layout->mChannelLayoutTag,
                   layout->mChannelBitmap,
                   layout->mNumberChannelDescriptions);

    for (int i = 0; i < layout->mNumberChannelDescriptions; i++) {
        AudioChannelDescription d = descs[i];
        MP_VERBOSE(ao, " - description %d: label <%d, %d>, flags: <%u>, "
                       "coords: <%f, %f, %f>\n", i,
                       d.mChannelLabel,
                       ca_label_to_mp_speaker_id(d.mChannelLabel),
                       d.mChannelFlags,
                       d.mCoordinates[0],
                       d.mCoordinates[1],
                       d.mCoordinates[2]);
    }
}

bool ca_layout_to_mp_chmap(struct ao *ao, AudioChannelLayout *layout,
                           struct mp_chmap *chmap)
{
    AudioChannelLayoutTag tag  = layout->mChannelLayoutTag;
    uint32_t layout_size       = sizeof(layout);
    OSStatus err;

    if (tag == kAudioChannelLayoutTag_UseChannelBitmap) {
        err = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForBitmap,
                                     sizeof(uint32_t),
                                     &layout->mChannelBitmap,
                                     &layout_size,
                                     layout);
        CHECK_CA_ERROR("failed to convert channel bitmap to descriptions");
    } else if (tag != kAudioChannelLayoutTag_UseChannelDescriptions) {
        err = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag,
                                     sizeof(AudioChannelLayoutTag),
                                     &layout->mChannelLayoutTag,
                                     &layout_size,
                                     layout);
        CHECK_CA_ERROR("failed to convert channel tag to descriptions");
    }

    ca_log_layout(ao, layout);

    // If the channel layout uses channel descriptions, from my
    // experiments there are there three possibile cases:
    // * The description has a label kAudioChannelLabel_Unknown:
    //   Can't do anything about this (looks like non surround
    //   layouts are like this).
    // * The description uses positional information: this in
    //   theory could be used but one would have to map spatial
    //   positions to labels which is not really feasible.
    // * The description has a well known label which can be mapped
    //   to the waveextensible definition: this is the kind of
    //   descriptions we process here.

    for (int n = 0; n < layout->mNumberChannelDescriptions; n++) {
        AudioChannelLabel label = layout->mChannelDescriptions[n].mChannelLabel;
        uint8_t speaker = ca_label_to_mp_speaker_id(label);
        if (label == kAudioChannelLabel_Unknown)
            continue;
        if (speaker < 0) {
            MP_VERBOSE(ao, "channel label=%d unusable to build channel "
                           "bitmap, skipping layout\n", label);
        } else {
            chmap->speaker[n] = speaker;
            chmap->num = n + 1;
        }
    }

    return chmap->num > 0;
coreaudio_error:
    ca_log_layout(ao, layout);
    return false;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_coreaudio = {
    .description = "CoreAudio AudioUnit",
    .name      = "coreaudio",
    .uninit    = uninit,
    .init      = init,
    .control   = control,
    .pause     = stop,
    .resume    = start,
    .priv_size = sizeof(struct priv),
    .options = (const struct m_option[]) {
        OPT_INT("device_id", opt_device_id, 0, OPTDEF_INT(-1)),
        OPT_FLAG("list", opt_list, 0),
        {0}
    },
};
