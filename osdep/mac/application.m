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

#include <stdio.h>
#include "config.h"
#include "mpv_talloc.h"

#include "common/msg.h"
#include "input/input.h"
#include "player/client.h"
#include "options/m_config.h"
#include "options/options.h"

#import "osdep/mac/application_objc.h"
#include "osdep/threads.h"
#include "osdep/main-fn.h"

#if HAVE_SWIFT
#include "osdep/mac/swift.h"
#endif

#define MPV_PROTOCOL @"mpv://"

// Whether the NSApplication singleton was created. If this is false, we are
// running in libmpv mode, and cocoa_main() was never called.
static bool application_instantiated;

static mp_thread playback_thread_id;

@interface Application ()
{
    AppHub *_appHub;
}

@end

static Application *mpv_shared_app(void)
{
    return (Application *)[Application sharedApplication];
}

static void terminate_cocoa_application(void)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSApp hide:NSApp];
        [NSApp terminate:NSApp];
    });
}

@implementation Application
@synthesize menuBar = _menu_bar;
@synthesize openCount = _open_count;
@synthesize cocoaCB = _cocoa_cb;

- (void)sendEvent:(NSEvent *)event
{
    if ([self modalWindow] || ![_appHub.input processKeyWithEvent:event])
        [super sendEvent:event];
    [_appHub.input wakeup];
}

- (id)init
{
    if (self = [super init]) {
        _appHub = [AppHub shared];

        NSAppleEventManager *em = [NSAppleEventManager sharedAppleEventManager];
        [em setEventHandler:self
                andSelector:@selector(getUrl:withReplyEvent:)
              forEventClass:kInternetEventClass
                 andEventID:kAEGetURL];
    }

    return self;
}

- (void)dealloc
{
    NSAppleEventManager *em = [NSAppleEventManager sharedAppleEventManager];
    [em removeEventHandlerForEventClass:kInternetEventClass
                             andEventID:kAEGetURL];
    [em removeEventHandlerForEventClass:kCoreEventClass
                             andEventID:kAEQuitApplication];
    [super dealloc];
}

#if HAVE_MACOS_TOUCHBAR
- (NSTouchBar *)makeTouchBar
{
    return [[AppHub shared] touchBar];
}
#endif

- (void)initCocoaCb:(struct mpv_handle *)ctx
{
#if HAVE_MACOS_COCOA_CB
    if (!_cocoa_cb) {
        mpv_handle *mpv = mpv_create_client(ctx, "cocoacb");
        [NSApp setCocoaCB:[[CocoaCB alloc] init:mpv]];
    }
#endif
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
    NSAppleEventManager *em = [NSAppleEventManager sharedAppleEventManager];
    [em setEventHandler:self
            andSelector:@selector(handleQuitEvent:withReplyEvent:)
          forEventClass:kCoreEventClass
             andEventID:kAEQuitApplication];
}

- (void)handleQuitEvent:(NSAppleEventDescriptor *)event
         withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    if (![_appHub.input command:@"quit"])
        terminate_cocoa_application();
}

- (void)getUrl:(NSAppleEventDescriptor *)event
    withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    NSString *url =
        [[event paramDescriptorForKeyword:keyDirectObject] stringValue];

    url = [url stringByReplacingOccurrencesOfString:MPV_PROTOCOL
                withString:@""
                   options:NSAnchoredSearch
                     range:NSMakeRange(0, [MPV_PROTOCOL length])];

    url = [url stringByRemovingPercentEncoding];
    [_appHub.input openWithFiles:@[url]];
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
    if (mpv_shared_app().openCount > 0) {
        mpv_shared_app().openCount--;
        return;
    }

    SEL cmpsel = @selector(localizedStandardCompare:);
    NSArray *files = [filenames sortedArrayUsingSelector:cmpsel];
    [_appHub.input openWithFiles:files];
}
@end

struct playback_thread_ctx {
    int  *argc;
    char ***argv;
};

static void cocoa_run_runloop(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSApp run];
    [pool drain];
}

static MP_THREAD_VOID playback_thread(void *ctx_obj)
{
    mp_thread_set_name("core/playback");
    @autoreleasepool {
        struct playback_thread_ctx *ctx = (struct playback_thread_ctx*) ctx_obj;
        int r = mpv_main(*ctx->argc, *ctx->argv);
        terminate_cocoa_application();
        // normally never reached - unless the cocoa mainloop hasn't started yet
        exit(r);
    }
}

static void init_cocoa_application(bool regular)
{
    NSApp = mpv_shared_app();
    [NSApp setDelegate:NSApp];
    [NSApp setMenuBar:[[MenuBar alloc] init]];

    // Will be set to Regular from cocoa_common during UI creation so that we
    // don't create an icon when playing audio only files.
    [NSApp setActivationPolicy: regular ?
        NSApplicationActivationPolicyRegular :
        NSApplicationActivationPolicyAccessory];

    atexit_b(^{
        // Because activation policy has just been set to behave like a real
        // application, that policy must be reset on exit to prevent, among
        // other things, the menubar created here from remaining on screen.
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
        });
    });
}

static bool bundle_started_from_finder()
{
    NSString* bundle = [[[NSProcessInfo processInfo] environment] objectForKey:@"MPVBUNDLE"];
    return [bundle isEqual:@"true"];
}

static bool is_psn_argument(char *arg_to_check)
{
    NSString *arg = [NSString stringWithUTF8String:arg_to_check];
    return [arg hasPrefix:@"-psn_"];
}

static void setup_bundle(int *argc, char *argv[])
{
    if (*argc > 1 && is_psn_argument(argv[1])) {
        *argc = 1;
        argv[1] = NULL;
    }

    NSDictionary *env = [[NSProcessInfo processInfo] environment];
    NSString *path_bundle = [env objectForKey:@"PATH"];
    NSString *path_new = [NSString stringWithFormat:@"%@:%@:%@:%@:%@",
                                                    path_bundle,
                                                    @"/usr/local/bin",
                                                    @"/usr/local/sbin",
                                                    @"/opt/local/bin",
                                                    @"/opt/local/sbin"];
    setenv("PATH", [path_new UTF8String], 1);
}

int cocoa_main(int argc, char *argv[])
{
    @autoreleasepool {
        application_instantiated = true;

        struct playback_thread_ctx ctx = {0};
        ctx.argc     = &argc;
        ctx.argv     = &argv;

        if (bundle_started_from_finder()) {
            setup_bundle(&argc, argv);
            init_cocoa_application(true);
        } else {
            for (int i = 1; i < argc; i++)
                if (argv[i][0] != '-')
                    mpv_shared_app().openCount++;
            init_cocoa_application(false);
        }

        mp_thread_create(&playback_thread_id, playback_thread, &ctx);
        [[AppHub shared].input wait];
        cocoa_run_runloop();

        // This should never be reached: cocoa_run_runloop blocks until the
        // process is quit
        fprintf(stderr, "There was either a problem "
                "initializing Cocoa or the Runloop was stopped unexpectedly. "
                "Please report this issues to a developer.\n");
        mp_thread_join(playback_thread_id);
        return 1;
    }
}
