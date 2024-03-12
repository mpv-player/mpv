/*
 * Cocoa Application Event Handling
 *
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

#import <Cocoa/Cocoa.h>

#include "mpv_talloc.h"
#include "input/event.h"
#include "input/input.h"
#include "player/client.h"

#import "osdep/mac/events_objc.h"
#import "osdep/mac/application_objc.h"

#include "config.h"

#if HAVE_SWIFT
#include "osdep/mac/swift.h"
#endif

@interface EventsResponder ()
{
    struct mpv_handle *_ctx;
    BOOL _is_application;
}

- (BOOL)setMpvHandle:(struct mpv_handle *)ctx;
- (void)initCocoaCb;
- (void)readEvents;
- (void)startMediaKeys;
- (void)stopMediaKeys;
@end

void cocoa_init_media_keys(void)
{
    [[EventsResponder sharedInstance] startMediaKeys];
}

void cocoa_uninit_media_keys(void)
{
    [[EventsResponder sharedInstance] stopMediaKeys];
}

void cocoa_set_input_context(struct input_ctx *input_context)
{
    [[EventsResponder sharedInstance].inputHelper signalWithInput:input_context];
}

static void wakeup(void *context)
{
    [[EventsResponder sharedInstance] readEvents];
}

void cocoa_set_mpv_handle(struct mpv_handle *ctx)
{
    if ([[EventsResponder sharedInstance] setMpvHandle:ctx]) {
        mpv_observe_property(ctx, 0, "duration", MPV_FORMAT_DOUBLE);
        mpv_observe_property(ctx, 0, "time-pos", MPV_FORMAT_DOUBLE);
        mpv_observe_property(ctx, 0, "speed", MPV_FORMAT_DOUBLE);
        mpv_observe_property(ctx, 0, "pause", MPV_FORMAT_FLAG);
        mpv_observe_property(ctx, 0, "media-title", MPV_FORMAT_STRING);
        mpv_observe_property(ctx, 0, "chapter-metadata/title", MPV_FORMAT_STRING);
        mpv_observe_property(ctx, 0, "metadata/by-key/album", MPV_FORMAT_STRING);
        mpv_observe_property(ctx, 0, "metadata/by-key/artist", MPV_FORMAT_STRING);
        mpv_set_wakeup_callback(ctx, wakeup, NULL);
    }
}

void cocoa_init_cocoa_cb(void)
{
    [[EventsResponder sharedInstance] initCocoaCb];
}

@implementation EventsResponder

@synthesize remoteCommandCenter = _remoteCommandCenter;
@synthesize inputHelper = _inputHelper;

+ (EventsResponder *)sharedInstance
{
    static EventsResponder *responder = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        responder = [EventsResponder new];
        responder.inputHelper = [[InputHelper alloc] init: nil :nil];
    });
    return responder;
}

- (void)setIsApplication:(BOOL)isApplication
{
    _is_application = isApplication;
}

- (BOOL)setMpvHandle:(struct mpv_handle *)ctx
{
    if (_is_application) {
        _ctx = ctx;
        return YES;
    }

    mpv_destroy(ctx);
    return NO;
}

- (void)initCocoaCb
{
    if (_is_application) {
        dispatch_sync(dispatch_get_main_queue(), ^{
            [NSApp initCocoaCb:_ctx];
        });
    }
}

- (void)readEvents
{
    dispatch_async(dispatch_get_main_queue(), ^{
        while (_ctx) {
            mpv_event *event = mpv_wait_event(_ctx, 0);
            if (event->event_id == MPV_EVENT_NONE)
                break;
            [self processEvent:event];
        }
    });
}

-(void)processEvent:(struct mpv_event *)event
{
    if(_is_application) {
        [NSApp processEvent:event];
    }

    if (_remoteCommandCenter) {
        [_remoteCommandCenter processEvent:event];
    }

    switch (event->event_id) {
    case MPV_EVENT_SHUTDOWN: {
#if HAVE_MACOS_COCOA_CB
        if ([(Application *)NSApp cocoaCB].isShuttingDown) {
            _ctx = nil;
            return;
        }
#endif
        mpv_destroy(_ctx);
        _ctx = nil;
        break;
    }
    default:
        break;
    }
}

- (void)startMediaKeys
{
#if HAVE_MACOS_MEDIA_PLAYER
    if (_remoteCommandCenter == nil) {
        _remoteCommandCenter = [[RemoteCommandCenter alloc] init];
    }
#endif

    [_remoteCommandCenter start];
}

- (void)stopMediaKeys
{
    [_remoteCommandCenter stop];
}

@end
