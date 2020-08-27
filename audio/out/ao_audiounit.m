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

#include "config.h"
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

static OSStatus render_cb_lpcm(void *ctx, AudioUnitRenderActionFlags *aflags,
                              const AudioTimeStamp *ts, UInt32 bus,
                              UInt32 frames, AudioBufferList *buffer_list)
{
    struct ao *ao = ctx;
    struct priv *p = ao->priv;
    void *planes[MP_NUM_CHANNELS] = {0};

    for (int n = 0; n < ao->num_planes; n++)
        planes[n] = buffer_list->mBuffers[n].mData;

    int64_t end = mp_time_us();
    end += p->device_latency * 1e6;
    end += ca_get_latency(ts) + ca_frames_to_us(ao, frames);
    ao_read_data(ao, planes, frames, end);
    return noErr;
}

static bool init_audiounit(struct ao *ao)
{
    AudioStreamBasicDescription asbd;
    OSStatus err;
    uint32_t size;
    struct priv *p = ao->priv;
    AVAudioSession *instance = AVAudioSession.sharedInstance;
    AVAudioSessionPortDescription *port = nil;
    NSInteger maxChannels = instance.maximumOutputNumberOfChannels;
    NSInteger prefChannels = MIN(maxChannels, ao->channels.num);

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

    if (af_fmt_is_spdif(ao->format) || instance.outputNumberOfChannels <= 2) {
        ao->channels = (struct mp_chmap)MP_CHMAP_INIT_STEREO;
    } else {
        port = instance.currentRoute.outputs.firstObject;
        if (port.channels.count == 2 &&
            port.channels[0].channelLabel == kAudioChannelLabel_Unknown) {
            // Special case when using an HDMI adapter. The iOS device will
            // perform SPDIF conversion for us, so send all available channels
            // using the AC3 mapping.
            ao->channels = (struct mp_chmap)MP_CHMAP6(FL, FC, FR, SL, SR, LFE);
        } else {
            ao->channels.num = (uint8_t)port.channels.count;
            for (AVAudioSessionChannelDescription *ch in port.channels) {
              ao->channels.speaker[ch.channelNumber - 1] =
                  ca_label_to_mp_speaker_id(ch.channelLabel);
            }
        }
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
    AVAudioSession *instance = AVAudioSession.sharedInstance;

    p->device_latency = [instance outputLatency] + [instance IOBufferDuration];

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
