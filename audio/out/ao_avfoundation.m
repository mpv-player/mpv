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
#include "audio/aframe.h"
#include "audio/format.h"
#include "audio/out/ao_coreaudio_chmap.h"
#include "audio/out/ao_coreaudio_utils.h"
#include "common/common.h"
#include "common/msg.h"
#include "internal.h"
#include "osdep/timer.h"
#include "ta/ta_talloc.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#import <CoreAudioTypes/CoreAudioTypes.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreMedia/CoreMedia.h>


@interface AVObserver : NSObject {
    struct ao *ao;
}
- (void)handleRestartNotification:(NSNotification*)notification;
@end

struct priv {
    AVSampleBufferAudioRenderer *renderer;
    AVSampleBufferRenderSynchronizer *synchronizer;
    dispatch_queue_t queue;
    CMAudioFormatDescriptionRef format_description;
    AVObserver *observer;
    int64_t end_time_av;
};

static void free_block(void *refcon, void *doomedMemoryBlock, size_t sizeInBytes)
{
    struct mp_aframe *aframe = refcon;
    talloc_free(aframe);
}

static int64_t CMTimeGetNanoseconds(CMTime time)
{
    time = CMTimeConvertScale(time, 1000000000, kCMTimeRoundingMethod_Default);
    return time.value;
}

static CMTime CMTimeFromNanoseconds(int64_t time)
{
    return CMTimeMake(time, 1000000000);
}

static void feed(struct ao *ao)
{
    struct priv *p = ao->priv;
    int samplerate = ao->samplerate;
    int sstride = ao->sstride;

    CMBlockBufferRef block_buffer = NULL;
    CMSampleBufferRef sample_buffer = NULL;
    struct mp_aframe *aframe = NULL;
    OSStatus err;

    int64_t cur_time_av = CMTimeGetNanoseconds([p->synchronizer currentTime]);
    int64_t cur_time_mp = mp_time_ns();
    int64_t end_time_av = MPMAX(p->end_time_av, cur_time_av);
    bool eof;
    aframe = ao_read_frame(ao, end_time_av - cur_time_av + cur_time_mp, &eof, true);
    int sample_count = aframe ? mp_aframe_get_size(aframe) : 0;
    if (eof) {
        [p->renderer stopRequestingMediaData];
        ao_stop_streaming(ao);
    }
    if (sample_count == 0) {
        goto finish;
    }

    CMBlockBufferCustomBlockSource block_source = {
        .version = kCMBlockBufferCustomBlockSourceVersion,
        .AllocateBlock = NULL,
        .FreeBlock = free_block,
        .refCon = aframe,
    };

    if ((err = CMBlockBufferCreateWithMemoryBlock(
        NULL,
        mp_aframe_get_data_ro(aframe)[0],
        sample_count * sstride,
        NULL,
        &block_source,
        0,
        sample_count * sstride,
        0,
        &block_buffer
    )) != noErr) {
        MP_FATAL(ao, "failed to create block buffer\n");
        MP_VERBOSE(ao, "CMBlockBufferCreateWithMemoryBlock returned %d\n", err);
        goto error;
    }
    aframe = NULL;

    CMSampleTimingInfo sample_timing_into[] = {(CMSampleTimingInfo) {
        .duration = CMTimeMake(1, samplerate),
        .presentationTimeStamp = CMTimeFromNanoseconds(end_time_av),
        .decodeTimeStamp = kCMTimeInvalid
    }};
    size_t sample_size_array[] = {sstride};
    if ((err = CMSampleBufferCreateReady(
        NULL,
        block_buffer,
        p->format_description,
        sample_count,
        1,
        sample_timing_into,
        1,
        sample_size_array,
        &sample_buffer
    )) != noErr) {
        MP_FATAL(ao, "failed to create sample buffer\n");
        MP_VERBOSE(ao, "CMSampleBufferCreateReady returned %d\n", err);
        goto error;
    }

    [p->renderer enqueueSampleBuffer:sample_buffer];

    int64_t time_delta = CMTimeGetNanoseconds(CMTimeMake(sample_count, samplerate));
    p->end_time_av = end_time_av + time_delta;

    goto finish;

error:
    ao_request_reload(ao);
finish:
    talloc_free(aframe);
    if (block_buffer) CFRelease(block_buffer);
    if (sample_buffer) CFRelease(sample_buffer);
}

static void start(struct ao *ao)
{
    struct priv *p = ao->priv;

    p->end_time_av = -1;
    [p->synchronizer setRate:1];
    [p->renderer requestMediaDataWhenReadyOnQueue:p->queue usingBlock:^{
        feed(ao);
    }];
}

static void stop(struct ao *ao)
{
    struct priv *p = ao->priv;

    dispatch_sync(p->queue, ^{
        [p->renderer stopRequestingMediaData];
        [p->renderer flush];
        [p->synchronizer setRate:0];
    });
}

static bool set_pause(struct ao *ao, bool paused)
{
    struct priv *p = ao->priv;

    if (paused) {
        [p->synchronizer setRate:0];
    } else {
        [p->synchronizer setRate:1];
    }

    return true;
}

static int control(struct ao *ao, enum aocontrol cmd, void *arg)
{
    struct priv *p = ao->priv;

    switch (cmd) {
    case AOCONTROL_GET_MUTE:
        *(bool*)arg = [p->renderer isMuted];
        return CONTROL_OK;
    case AOCONTROL_GET_VOLUME:
        *(float*)arg = [p->renderer volume] * 100;
        return CONTROL_OK;
    case AOCONTROL_SET_MUTE:
        [p->renderer setMuted:*(bool*)arg];
        return CONTROL_OK;
    case AOCONTROL_SET_VOLUME:
        [p->renderer setVolume:*(float*)arg / 100];
        return CONTROL_OK;
    default:
        return CONTROL_UNKNOWN;
    }
}

@implementation AVObserver
- (instancetype)initWithAO:(struct ao*)_ao {
    self = [super init];
    if (self) {
        ao = _ao;
    }
    return self;
}
- (void)handleRestartNotification:(NSNotification*)notification {
    char *name = cfstr_get_cstr((CFStringRef)notification.name);
    MP_WARN(ao, "restarting due to system notification; this will cause desync\n");
    MP_VERBOSE(ao, "notification name: %s\n", name);
    talloc_free(name);
    stop(ao);
    start(ao);
}
@end

static int init(struct ao *ao)
{
    struct priv *p = ao->priv;
    AudioChannelLayout *layout = NULL;

#if TARGET_OS_IPHONE
    AVAudioSession *instance = AVAudioSession.sharedInstance;
    NSInteger maxChannels = instance.maximumOutputNumberOfChannels;
    NSInteger prefChannels = MIN(maxChannels, ao->channels.num);
    [instance setCategory:AVAudioSessionCategoryPlayback error:nil];
    [instance setMode:AVAudioSessionModeMoviePlayback error:nil];
    [instance setActive:YES error:nil];
    [instance setPreferredOutputNumberOfChannels:prefChannels error:nil];
#endif

    if ((p->renderer = [[AVSampleBufferAudioRenderer alloc] init]) == nil) {
        MP_FATAL(ao, "failed to create audio renderer\n");
        MP_VERBOSE(ao, "AVSampleBufferAudioRenderer failed to initialize\n");
        goto error;
    }
    if ((p->synchronizer = [[AVSampleBufferRenderSynchronizer alloc] init]) == nil) {
        MP_FATAL(ao, "failed to create rendering synchronizer\n");
        MP_VERBOSE(ao, "AVSampleBufferRenderSynchronizer failed to initialize\n");
        goto error;
    }
    if ((p->queue = dispatch_queue_create(
        "avfoundation event",
        dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INTERACTIVE, 0)
    )) == NULL) {
        MP_FATAL(ao, "failed to create dispatch queue\n");
        MP_VERBOSE(ao, "dispatch_queue_create failed\n");
        goto error;
    }

    if (ao->device && ao->device[0]) {
        [p->renderer setAudioOutputDeviceUniqueID:(NSString*)cfstr_from_cstr(ao->device)];
    }

    [p->synchronizer addRenderer:p->renderer];
#if HAVE_MACOS_11_3_FEATURES
    if (@available(tvOS 14.5, iOS 14.5, macOS 11.3, *)) {
        [p->synchronizer setDelaysRateChangeUntilHasSufficientMediaData:NO];
    }
#endif

    if (af_fmt_is_spdif(ao->format)) {
        MP_FATAL(ao, "avfoundation does not support SPDIF\n");
#if HAVE_COREAUDIO
        MP_FATAL(ao, "please use coreaudio_exclusive instead\n");
#endif
        goto error;
    }

    // AVSampleBufferAudioRenderer only supports interleaved formats
    ao->format = af_fmt_from_planar(ao->format);
    if (af_fmt_is_planar(ao->format)) {
        MP_FATAL(ao, "planar audio formats are unsupported\n");
        goto error;
    }

    AudioStreamBasicDescription asbd;
    ca_fill_asbd(ao, &asbd);
    size_t layout_size = sizeof(AudioChannelLayout)
                         + (ao->channels.num - 1) * sizeof(AudioChannelDescription);
    layout = talloc_size(ao, layout_size);
    layout->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
    layout->mNumberChannelDescriptions = ao->channels.num;
    for (int i = 0; i < ao->channels.num; ++i) {
        AudioChannelDescription *desc = layout->mChannelDescriptions + i;
        desc->mChannelFlags = kAudioChannelFlags_AllOff;
        desc->mChannelLabel = mp_speaker_id_to_ca_label(ao->channels.speaker[i]);
    }

    void *talloc_ctx = talloc_new(NULL);
    AudioChannelLayout *std_layout = ca_find_standard_layout(talloc_ctx, layout);
    memmove(layout, std_layout, sizeof(AudioChannelLayout));
    talloc_free(talloc_ctx);
    ca_log_layout(ao, MSGL_V, layout);

    OSStatus err;
    if ((err = CMAudioFormatDescriptionCreate(
        NULL,
        &asbd,
        layout_size,
        layout,
        0,
        NULL,
        NULL,
        &p->format_description
    )) != noErr) {
        MP_FATAL(ao, "failed to create audio format description\n");
        MP_VERBOSE(ao, "CMAudioFormatDescriptionCreate returned %d\n", err);
        goto error;
    }
    talloc_free(layout);
    layout = NULL;

    // AVSampleBufferAudioRenderer read ahead aggressively
    ao->device_buffer = ao->samplerate * 2;

    p->observer = [[AVObserver alloc] initWithAO:ao];
    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
#if HAVE_MACOS_12_FEATURES
    if (@available(tvOS 15.0, iOS 15.0, macOS 12.0, *)) {
        [center addObserver:p->observer selector:@selector(handleRestartNotification:) name:AVSampleBufferAudioRendererOutputConfigurationDidChangeNotification object:p->renderer];
    }
#endif
    [center addObserver:p->observer selector:@selector(handleRestartNotification:) name:AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification object:p->renderer];

    return CONTROL_OK;

error:
    talloc_free(layout);
    if (p->renderer) [p->renderer release];
    if (p->synchronizer) [p->synchronizer release];
    if (p->queue) dispatch_release(p->queue);
    if (p->format_description) CFRelease(p->format_description);

#if TARGET_OS_IPHONE
    [AVAudioSession.sharedInstance setActive:NO
        withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
        error:nil
    ];
#endif

    return CONTROL_ERROR;
}

static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;

    stop(ao);

    [p->renderer release];
    [p->synchronizer release];
    dispatch_release(p->queue);
    CFRelease(p->format_description);

    [[NSNotificationCenter defaultCenter] removeObserver:p->observer];
    [p->observer release];

#if TARGET_OS_IPHONE
    [AVAudioSession.sharedInstance setActive:NO
        withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
        error:nil
    ];
#endif
}

#define OPT_BASE_STRUCT struct priv

const struct ao_driver audio_out_avfoundation = {
    .description    = "AVFoundation AVSampleBufferAudioRenderer",
    .name           = "avfoundation",
    .uninit         = uninit,
    .init           = init,
    .control        = control,
    .reset          = stop,
    .start          = start,
    .set_pause      = set_pause,
    .list_devs      = ca_get_device_list,
    .priv_size      = sizeof(struct priv),
};
