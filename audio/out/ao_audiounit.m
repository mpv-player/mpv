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

#include "ao.h"
#include "internal.h"
#include "audio/format.h"
#include "osdep/timer.h"
#include "options/m_option.h"
#include "common/msg.h"
#include "ao_coreaudio_utils.h"
#include "ao_coreaudio_chmap.h"

#import <AudioUnit/AudioUnit.h>
#import <CoreAudio/CoreAudioTypes.h>
#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>
#import <mach/mach_time.h>

struct priv {
    AudioUnit audio_unit;
    double device_latency;
};

static OSStatus au_get_ary(AudioUnit unit, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, void **data, UInt32 *outDataSize)
{
    OSStatus err;

    err = AudioUnitGetPropertyInfo(unit, inID, inScope, inElement, outDataSize, NULL);
    CHECK_CA_ERROR_SILENT_L(coreaudio_error);

    *data = talloc_zero_size(NULL, *outDataSize);

    err = AudioUnitGetProperty(unit, inID, inScope, inElement, *data, outDataSize);
    CHECK_CA_ERROR_SILENT_L(coreaudio_error_free);

    return err;
coreaudio_error_free:
    talloc_free(*data);
coreaudio_error:
    return err;
}

static AudioChannelLayout *convert_layout(AudioChannelLayout *layout, UInt32* size)
{
    AudioChannelLayoutTag tag = layout->mChannelLayoutTag;
    AudioChannelLayout *new_layout;
    if (tag == kAudioChannelLayoutTag_UseChannelDescriptions)
        return layout;
    else if (tag == kAudioChannelLayoutTag_UseChannelBitmap)
        AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForBitmap,
                                   sizeof(UInt32), &layout->mChannelBitmap, size);
    else
        AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForTag,
                                   sizeof(AudioChannelLayoutTag), &tag, size);
    new_layout = talloc_zero_size(NULL, *size);
    if (!new_layout) {
        talloc_free(layout);
        return NULL;
    }
    if (tag == kAudioChannelLayoutTag_UseChannelBitmap)
        AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForBitmap,
                               sizeof(UInt32), &layout->mChannelBitmap, size, new_layout);
    else
        AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag,
                               sizeof(AudioChannelLayoutTag), &tag, size, new_layout);
    new_layout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
    talloc_free(layout);
    return new_layout;
}


static OSStatus render_cb_lpcm(void *ctx, AudioUnitRenderActionFlags *aflags,
                              const AudioTimeStamp *ts, UInt32 bus,
                              UInt32 frames, AudioBufferList *buffer_list)
{
    struct ao *ao = ctx;
    struct priv *p = ao->priv;
    void *planes[MP_NUM_CHANNELS] = {0};

    for (int n = 0; n < ao->num_planes; n++)
        planes[n] = buffer_list->mBuffers[n].mData;

    int64_t end = mp_time_ns();
    end += MP_TIME_S_TO_NS(p->device_latency);
    end += ca_get_latency(ts);
    ao_read_data(ao, planes, frames, end, NULL, true, true);
    return noErr;
}

static bool init_audiounit(struct ao *ao)
{
    AudioStreamBasicDescription asbd;
    OSStatus err;
    uint32_t size;
    AudioChannelLayout *layout = NULL;
    struct priv *p = ao->priv;
    AVAudioSession *instance = AVAudioSession.sharedInstance;
    AVAudioSessionPortDescription *port = nil;
    NSInteger maxChannels = instance.maximumOutputNumberOfChannels;
    NSInteger prefChannels = MIN(maxChannels, ao->channels.num);

    MP_VERBOSE(ao, "max channels: %ld, requested: %d\n", maxChannels, (int)ao->channels.num);

    [instance setCategory:AVAudioSessionCategoryPlayback error:nil];
    [instance setMode:AVAudioSessionModeMoviePlayback error:nil];
    [instance setActive:YES error:nil];
    [instance setPreferredOutputNumberOfChannels:prefChannels error:nil];

    AudioComponentDescription desc = (AudioComponentDescription) {
        .componentType         = kAudioUnitType_Output,
        .componentSubType      = kAudioUnitSubType_RemoteIO,
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

    err = au_get_ary(p->audio_unit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Output,
                     0, (void**)&layout, &size);
    CHECK_CA_ERROR_L(coreaudio_error_audiounit,
                     "unable to retrieve audio unit channel layout");

    MP_VERBOSE(ao, "AU channel layout tag: %x (%x)\n", layout->mChannelLayoutTag, layout->mChannelBitmap);

    layout = convert_layout(layout, &size);
    if (!layout) {
        MP_ERR(ao, "unable to convert channel layout to list format\n");
        goto coreaudio_error_audiounit;
    }

    for (UInt32 i = 0; i < layout->mNumberChannelDescriptions; i++) {
        MP_VERBOSE(ao, "channel map: %i: %u\n", i, layout->mChannelDescriptions[i].mChannelLabel);
    }

    if (af_fmt_is_spdif(ao->format) || instance.outputNumberOfChannels <= 2) {
        ao->channels = (struct mp_chmap)MP_CHMAP_INIT_STEREO;
        MP_VERBOSE(ao, "using stereo output\n");
    } else {
        ao->channels.num = (uint8_t)layout->mNumberChannelDescriptions;
        for (UInt32 i = 0; i < layout->mNumberChannelDescriptions; i++) {
          ao->channels.speaker[i] =
              ca_label_to_mp_speaker_id(layout->mChannelDescriptions[i].mChannelLabel);
        }
        MP_VERBOSE(ao, "using standard channel mapping\n");
    }

    ca_fill_asbd(ao, &asbd);
    size = sizeof(AudioStreamBasicDescription);
    err = AudioUnitSetProperty(p->audio_unit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0, &asbd, size);

    CHECK_CA_ERROR_L(coreaudio_error_audiounit,
                     "unable to set the input format on the audio unit");

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

    talloc_free(layout);

    return true;

coreaudio_error_audiounit:
    AudioUnitUninitialize(p->audio_unit);
coreaudio_error_component:
    AudioComponentInstanceDispose(p->audio_unit);
coreaudio_error:
    talloc_free(layout);
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
    AVAudioSession *instance = AVAudioSession.sharedInstance;

    p->device_latency = [instance outputLatency];

    OSStatus err = AudioOutputUnitStart(p->audio_unit);
    CHECK_CA_WARN("can't start audio unit");
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;
    AudioOutputUnitStop(p->audio_unit);
    AudioUnitUninitialize(p->audio_unit);
    AudioComponentInstanceDispose(p->audio_unit);

    [AVAudioSession.sharedInstance
        setActive:NO
        withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
        error:nil];
}

static int init(struct ao *ao)
{
    if (!init_audiounit(ao))
        goto coreaudio_error;

    return CONTROL_OK;

coreaudio_error:
    return CONTROL_ERROR;
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_audiounit = {
    .description    = "AudioUnit (iOS)",
    .name           = "audiounit",
    .uninit         = uninit,
    .init           = init,
    .reset          = stop,
    .start          = start,
    .priv_size      = sizeof(struct priv),
};
