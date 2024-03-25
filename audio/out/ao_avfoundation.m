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

#import <AVFoundation/AVFoundation.h>

#include "config.h"
#include "osdep/timer.h"
#include "ao.h"
#include "internal.h"
#include "audio/format.h"
#include "options/m_option.h"
#include "common/msg.h"

#include "ao_coreaudio_utils.h"


#if TARGET_OS_IPHONE
#define HAVE_AVAUDIOSESSION
#endif

#define OPT_BASE_STRUCT struct priv
struct priv {
    dispatch_queue_t queue;
    CMFormatDescriptionRef desc;
    AVSampleBufferAudioRenderer *renderer;
    AVSampleBufferRenderSynchronizer *synchronizer;
    int64_t enqueued;
    id autoFlushObserver;
    id outputChangedObserver;
    CMSampleBufferRef lastSampleBuf;
};


static bool enqueue_frames(struct ao *ao)
{
    struct priv *p = ao->priv;

    int frames = ao->buffer / ao->sstride;
    OSStatus err;

    CMSampleBufferRef sBuf = NULL;
    CMBlockBufferRef bBuf = NULL;

    int64_t end = mp_time_us();
    int64_t playhead = CMTimeGetSeconds([p->synchronizer currentTime]) * 1e6;
    end += ca_frames_to_us(ao, frames);
    end += MPMAX(0, ca_frames_to_us(ao, p->enqueued) - playhead);

    void *buf = calloc(1, ao->buffer);
    int samples = ao_read_data(ao, &buf, frames, end);

    if (samples <= 0) {
        free(buf);
        return false;
    }

    int bufsize = samples * ao->sstride;
    err = CMBlockBufferCreateWithMemoryBlock(NULL,
                                             buf,
                                             bufsize,
                                             0,
                                             NULL,
                                             0,
                                             bufsize,
                                             0,
                                             &bBuf);
    CHECK_CA_WARN("failed to create CMBlockBuffer");

    p->enqueued += samples;
    CMSampleTimingInfo timing = {
        CMTimeMake(1, ao->samplerate),
        CMTimeMake(p->enqueued, ao->samplerate),
        kCMTimeInvalid
    };
    err = CMSampleBufferCreate(NULL,
                               bBuf,
                               true,
                               NULL,
                               NULL,
                               p->desc,
                               samples,
                               1,
                               &timing,
                               0,
                               NULL,
                               &sBuf);
    CHECK_CA_WARN("failed to create CMSampleBuffer");

    [p->renderer enqueueSampleBuffer: sBuf];

    if (p->lastSampleBuf) {
        CFRelease(p->lastSampleBuf);
    }

    p->lastSampleBuf = sBuf;
    CFRelease(bBuf);
    return true;
}


static bool init_renderer(struct ao *ao)
{
    struct priv *p = ao->priv;

    p->renderer = [[AVSampleBufferAudioRenderer alloc] init];
    [p->synchronizer addRenderer: p->renderer];

    OSStatus err;

#ifdef HAVE_AVAUDIOSESSION
    AVAudioSession *instance = AVAudioSession.sharedInstance;
    NSInteger prefChannels = MPMIN(instance.maximumOutputNumberOfChannels, ao->channels.num);
    [instance setPreferredOutputNumberOfChannels: prefChannels error: nil];
    [instance setActive: YES error: nil];
#endif

    AudioStreamBasicDescription asbd;
    ca_fill_asbd(ao, &asbd);
    AudioChannelLayout acl = {};

    if (ao->channels.num == 1) {
        acl.mChannelLayoutTag = kAudioChannelLayoutTag_Mono;
    } else if (ao->channels.num == 2) {
        acl.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
    } else {
        acl.mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelBitmap;
        for (int i = 0; i < ao->channels.num; ++i) {
            acl.mChannelBitmap |= 1 << ao->channels.speaker[i];
        }
    }

    err = CMAudioFormatDescriptionCreate(NULL,
                                         &asbd,
                                         sizeof(acl),
                                         &acl,
                                         0, NULL,
                                         NULL,
                                         &p->desc);
    CHECK_CA_ERROR_L(coreaudio_error, "unable to create format description");

    return true;

coreaudio_error:
    return false;
}


static void reset(struct ao *ao)
{
    struct priv *p = ao->priv;

    [p->synchronizer setRate: 0.0f time: CMTimeMake(0, ao->samplerate)];
    [p->renderer stopRequestingMediaData];
    [p->renderer flush];
    p->enqueued = 0;
}


static void start(struct ao *ao)
{
    struct priv *p = ao->priv;

    if (!init_renderer(ao))
        return;

    [p->renderer requestMediaDataWhenReadyOnQueue: p->queue usingBlock: ^{
        while ([p->renderer isReadyForMoreMediaData] && enqueue_frames(ao))
            ;
    }];
    [p->synchronizer setRate: 1.0f];
}


// currently not called by mpv on pull based drivers
static bool set_pause(struct ao *ao, bool paused)
{
    struct priv *p = ao->priv;
    [p->synchronizer setRate: paused ? 0.0f : 1.0f];
    return true;
}


static void uninit(struct ao *ao)
{
    struct priv *p = ao->priv;
    p->renderer = nil;
    p->synchronizer = nil;
    if (p->desc) {
        CFRelease(p->desc);
        p->desc = NULL;
    }
    [NSNotificationCenter.defaultCenter removeObserver: p->autoFlushObserver];
    [NSNotificationCenter.defaultCenter removeObserver: p->outputChangedObserver];

#ifdef HAVE_AVAUDIOSESSION
    [AVAudioSession.sharedInstance
        setActive:NO
        withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
        error:nil];
#endif
}


static int init(struct ao *ao)
{
    ao->device_buffer = af_fmt_to_bytes(ao->format) * ao->channels.num * (ao->samplerate / 5);

    struct priv *p = ao->priv;

    if (@available(macos 10.14, ios 12.0, tvos 12.0, watchos 5.0, *)) {
        // supported, fall through
    } else {
        MP_FATAL(ao, "unsupported on this OS version\n");
        return CONTROL_ERROR;
    }

    p->queue = dispatch_queue_create("org.mpv.audiorenderer", NULL);

    if (@available(macos 10.13, ios 11.0, tvos 11.0, watchos 4.0, *)) {
        p->autoFlushObserver = [NSNotificationCenter.defaultCenter
            addObserverForName: AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification
                        object: p->renderer
                         queue: nil
                    usingBlock: ^(NSNotification *notification) {
            NSValue *flushTime = [notification.userInfo objectForKey: AVSampleBufferAudioRendererFlushTimeKey];
            MP_DBG(ao, "AVSBAR flushed at: %f\n", CMTimeGetSeconds(flushTime.CMTimeValue));
            [p->renderer enqueueSampleBuffer: p->lastSampleBuf];
            [p->synchronizer setRate: 1.0f time: flushTime.CMTimeValue];
        }];
    }

    if (@available(macos 12.0, ios 15.0, tvos 15.0, watchos 8.0, *)) {
        p->outputChangedObserver = [NSNotificationCenter.defaultCenter
            addObserverForName: AVSampleBufferAudioRendererOutputConfigurationDidChangeNotification
                        object: p->renderer
                         queue: nil
                    usingBlock: ^(NSNotification *notification) {
            MP_DBG(ao, "AVSBAR output config changed\n");
            ao_request_reload(ao);
        }];
    }

    p->synchronizer = [[AVSampleBufferRenderSynchronizer alloc] init];

    ao->format = af_fmt_from_planar(ao->format);

    return CONTROL_OK;
}



const struct ao_driver audio_out_avfoundation = {
    .description    = "AVFoundation AVSampleBufferAudioRenderer (macOS/iOS)",
    .name           = "avfoundation",
    .uninit         = uninit,
    .init           = init,
    .reset          = reset,
    .start          = start,
    .set_pause      = set_pause,
    .priv_size      = sizeof(struct priv),
};
