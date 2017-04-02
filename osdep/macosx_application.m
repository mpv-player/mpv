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

#include <stdio.h>
#include <pthread.h>
#include "config.h"
#include "mpv_talloc.h"

#include "common/msg.h"
#include "input/input.h"
#include "player/client.h"

#import "osdep/macosx_application_objc.h"
#include "osdep/macosx_compat.h"
#import "osdep/macosx_events_objc.h"
#include "osdep/threads.h"
#include "osdep/main-fn.h"

#if HAVE_MACOS_TOUCHBAR
#import "osdep/macosx_touchbar.h"
#endif

#define MPV_PROTOCOL @"mpv://"

// Whether the NSApplication singleton was created. If this is false, we are
// running in libmpv mode, and cocoa_main() was never called.
static bool application_instantiated;

static pthread_t playback_thread_id;

@interface Application ()
{
    EventsResponder *_eventsResponder;
}

- (NSMenuItem *)menuItemWithParent:(NSMenu *)parent
                             title:(NSString *)title
                            action:(SEL)selector
                     keyEquivalent:(NSString*)key;

- (NSMenuItem *)mainMenuItemWithParent:(NSMenu *)parent
                                 child:(NSMenu *)child;
- (void)registerMenuItem:(NSMenuItem*)menuItem forKey:(MPMenuKey)key;
- (NSMenu *)appleMenuWithMainMenu:(NSMenu *)mainMenu;
- (NSMenu *)videoMenu;
- (NSMenu *)windowMenu;
@end

@interface NSApplication (NiblessAdditions)
- (void)setAppleMenu:(NSMenu *)aMenu;
@end

static Application *mpv_shared_app(void)
{
    return (Application *)[Application sharedApplication];
}

static void terminate_cocoa_application(void)
{
    [NSApp hide:NSApp];
    [NSApp terminate:NSApp];
}

@implementation Application
@synthesize menuItems = _menu_items;
@synthesize openCount = _open_count;

- (void)sendEvent:(NSEvent *)event
{
    if (![_eventsResponder processKeyEvent:event])
        [super sendEvent:event];
    [_eventsResponder wakeup];
}

- (id)init
{
    if (self = [super init]) {
        self.menuItems = [[[NSMutableDictionary alloc] init] autorelease];
        _eventsResponder = [EventsResponder sharedInstance];

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
    TouchBar *tBar = [[TouchBar alloc] init];
    [tBar setApp:self];
    tBar.delegate = tBar;
    tBar.customizationIdentifier = customID;
    tBar.defaultItemIdentifiers = @[play, previousItem, nextItem, seekBar];
    tBar.customizationAllowedItemIdentifiers = @[play, seekBar, previousItem,
        nextItem, previousChapter, nextChapter, cycleAudio, cycleSubtitle,
        currentPosition, timeLeft];
    return tBar;
}

- (void)toggleTouchBarMenu
{
    [NSApp toggleTouchBarCustomizationPalette:self];
}
#endif

- (void)processEvent:(struct mpv_event *)event
{
#if HAVE_MACOS_TOUCHBAR
    if ([self respondsToSelector:@selector(touchBar)])
        [(TouchBar *)self.touchBar processEvent:event];
#endif
}

- (void)queueCommand:(char *)cmd
{
    [_eventsResponder queueCommand:cmd];
}

#define _R(P, T, E, K) \
    { \
        NSMenuItem *tmp = [self menuItemWithParent:(P) title:(T) \
                                            action:nil keyEquivalent:(E)]; \
        [self registerMenuItem:tmp forKey:(K)]; \
    }

- (NSMenu *)appleMenuWithMainMenu:(NSMenu *)mainMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Apple Menu"];
    [self mainMenuItemWithParent:mainMenu child:menu];
    [self menuItemWithParent:menu title:@"Hide mpv"
                      action:@selector(hide:) keyEquivalent: @"h"];
    [menu addItem:[NSMenuItem separatorItem]];
    [self menuItemWithParent:menu title:@"Quit mpv"
                      action:@selector(stopPlayback) keyEquivalent: @"q"];
    return [menu autorelease];
}

- (NSMenu *)videoMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Video"];
    _R(menu, @"Half Size",   @"0", MPM_H_SIZE)
    _R(menu, @"Normal Size", @"1", MPM_N_SIZE)
    _R(menu, @"Double Size", @"2", MPM_D_SIZE)
    return [menu autorelease];
}

- (NSMenu *)windowMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Window"];
    _R(menu, @"Minimize", @"m", MPM_MINIMIZE)
    _R(menu, @"Zoom",     @"z", MPM_ZOOM)

#if HAVE_MACOS_TOUCHBAR
    if ([self respondsToSelector:@selector(touchBar)]) {
        [menu addItem:[NSMenuItem separatorItem]];
        [self menuItemWithParent:menu title:@"Customize Touch Bar…"
                          action:@selector(toggleTouchBarMenu) keyEquivalent: @""];
    }
#endif

    return [menu autorelease];
}

- (void)initialize_menu
{
    NSMenu *main_menu = [[NSMenu new] autorelease];
    [NSApp setMainMenu:main_menu];
    [NSApp setAppleMenu:[self appleMenuWithMainMenu:main_menu]];

    [NSApp mainMenuItemWithParent:main_menu child:[self videoMenu]];
    [NSApp mainMenuItemWithParent:main_menu child:[self windowMenu]];
}

#undef _R

- (void)stopPlayback
{
    [self stopMPV:"quit"];
}

- (void)stopPlaybackAndRememberPosition
{
    [self stopMPV:"quit-watch-later"];
}

- (void)stopMPV:(char *)cmd
{
    if (![_eventsResponder queueCommand:cmd])
        terminate_cocoa_application();
}

- (void)registerMenuItem:(NSMenuItem*)menuItem forKey:(MPMenuKey)key
{
    [self.menuItems setObject:menuItem forKey:[NSNumber numberWithInt:key]];
}

- (void)registerSelector:(SEL)action forKey:(MPMenuKey)key
{
    NSNumber *boxedKey = [NSNumber numberWithInt:key];
    NSMenuItem *item   = [self.menuItems objectForKey:boxedKey];
    if (item) {
        [item setAction:action];
    }
}

- (NSMenuItem *)menuItemWithParent:(NSMenu *)parent
                             title:(NSString *)title
                            action:(SEL)action
                     keyEquivalent:(NSString*)key
{

    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title
                                                  action:action
                                           keyEquivalent:key];
    [parent addItem:item];
    return [item autorelease];
}

- (NSMenuItem *)mainMenuItemWithParent:(NSMenu *)parent
                                 child:(NSMenu *)child
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@""
                                                  action:nil
                                           keyEquivalent:@""];
    [item setSubmenu:child];
    [parent addItem:item];
    return [item autorelease];
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
    [self stopPlayback];
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
    [_eventsResponder handleFilesArray:@[url]];
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
    if (mpv_shared_app().openCount > 0) {
        mpv_shared_app().openCount--;
        return;
    }
    SEL cmpsel = @selector(localizedStandardCompare:);
    NSArray *files = [filenames sortedArrayUsingSelector:cmpsel];
    [_eventsResponder handleFilesArray:files];
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

static void *playback_thread(void *ctx_obj)
{
    mpthread_set_name("playback core (OSX)");
    @autoreleasepool {
        struct playback_thread_ctx *ctx = (struct playback_thread_ctx*) ctx_obj;
        int r = mpv_main(*ctx->argc, *ctx->argv);
        terminate_cocoa_application();
        // normally never reached - unless the cocoa mainloop hasn't started yet
        exit(r);
    }
}

void cocoa_register_menu_item_action(MPMenuKey key, void* action)
{
    if (application_instantiated)
        [NSApp registerSelector:(SEL)action forKey:key];
}

static void init_cocoa_application(bool regular)
{
    NSApp = mpv_shared_app();
    [NSApp setDelegate:NSApp];
    [NSApp initialize_menu];

    // Will be set to Regular from cocoa_common during UI creation so that we
    // don't create an icon when playing audio only files.
    [NSApp setActivationPolicy: regular ?
        NSApplicationActivationPolicyRegular :
        NSApplicationActivationPolicyAccessory];

    atexit_b(^{
        // Because activation policy has just been set to behave like a real
        // application, that policy must be reset on exit to prevent, among
        // other things, the menubar created here from remaining on screen.
        [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
    });
}

static void macosx_redirect_output_to_logfile(const char *filename)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSString *log_path = [NSHomeDirectory() stringByAppendingPathComponent:
        [@"Library/Logs/" stringByAppendingFormat:@"%s.log", filename]];
    freopen([log_path fileSystemRepresentation], "a", stdout);
    freopen([log_path fileSystemRepresentation], "a", stderr);
    [pool release];
}

static bool bundle_started_from_finder()
{
    NSDictionary *env = [[NSProcessInfo processInfo] environment];
    NSString *is_bundle = [env objectForKey:@"MPVBUNDLE"];

    return is_bundle ? [is_bundle boolValue] : false;
}

int cocoa_main(int argc, char *argv[])
{
    @autoreleasepool {
        application_instantiated = true;
        [[EventsResponder sharedInstance] setIsApplication:YES];

        struct playback_thread_ctx ctx = {0};
        ctx.argc     = &argc;
        ctx.argv     = &argv;

        if (bundle_started_from_finder()) {
            macosx_redirect_output_to_logfile("mpv");
            init_cocoa_application(true);
        } else {
            for (int i = 1; i < argc; i++)
                if (argv[i][0] != '-')
                    mpv_shared_app().openCount++;
            init_cocoa_application(false);
        }

        pthread_create(&playback_thread_id, NULL, playback_thread, &ctx);
        [[EventsResponder sharedInstance] waitForInputContext];
        cocoa_run_runloop();

        // This should never be reached: cocoa_run_runloop blocks until the
        // process is quit
        fprintf(stderr, "There was either a problem "
                "initializing Cocoa or the Runloop was stopped unexpectedly. "
                "Please report this issues to a developer.\n");
        pthread_join(playback_thread_id, NULL);
        return 1;
    }
}

