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
 * with mpv; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <pthread.h>
#include "talloc.h"

#include "mpvcore/mp_msg.h"
#include "mpvcore/input/input.h"
#include "mpvcore/input/keycodes.h"

#include "osdep/macosx_application_objc.h"
#include "osdep/macosx_compat.h"

#define MPV_PROTOCOL @"mpv://"

static pthread_t playback_thread_id;

@interface Application (PrivateMethods)
- (NSMenuItem *)menuItemWithParent:(NSMenu *)parent
                             title:(NSString *)title
                            action:(SEL)selector
                     keyEquivalent:(NSString*)key;

- (NSMenuItem *)mainMenuItemWithParent:(NSMenu *)parent
                                 child:(NSMenu *)child;
- (void)registerMenuItem:(NSMenuItem*)menuItem forKey:(MPMenuKey)key;
- (NSMenu *)appleMenuWithMainMenu:(NSMenu *)mainMenu;
- (NSMenu *)movieMenu;
- (NSMenu *)windowMenu;
- (void)handleFiles;
@end

@interface NSApplication (NiblessAdditions)
- (void)setAppleMenu:(NSMenu *)aMenu;
@end

Application *mpv_shared_app(void)
{
    return (Application *)[Application sharedApplication];
}

static NSString *escape_loadfile_name(NSString *input)
{
    NSArray *mappings = @[
        @{ @"in": @"\\", @"out": @"\\\\" },
        @{ @"in": @"\"", @"out": @"\\\"" },
    ];

    for (NSDictionary *mapping in mappings) {
        input = [input stringByReplacingOccurrencesOfString:mapping[@"in"]
                                                 withString:mapping[@"out"]];
    }

    return input;
}

@implementation Application
@synthesize files = _files;
@synthesize argumentsList = _arguments_list;
@synthesize willStopOnOpenEvent = _will_stop_on_open_event;

@synthesize inputContext = _input_context;
@synthesize eventsResponder = _events_responder;
@synthesize menuItems = _menu_items;

- (void)sendEvent:(NSEvent *)event
{
    [super sendEvent:event];

    if (self.inputContext)
        mp_input_wakeup(self.inputContext);
}

- (id)init
{
    if (self = [super init]) {
        self.menuItems = [[[NSMutableDictionary alloc] init] autorelease];
        self.files = nil;
        self.argumentsList = [[[NSMutableArray alloc] init] autorelease];
        self.eventsResponder = [[[EventsResponder alloc] init] autorelease];
        self.willStopOnOpenEvent = NO;

        [NSEvent addLocalMonitorForEventsMatchingMask:NSKeyDownMask|NSKeyUpMask
                                              handler:^(NSEvent *event) {
            return [self.eventsResponder handleKey:event];
        }];

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
    [super dealloc];
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
    [self menuItemWithParent:menu title:@"Quit mpv"
                      action:@selector(stopPlayback) keyEquivalent: @"q"];
    [self menuItemWithParent:menu title:@"Quit mpv & remember position"
                      action:@selector(stopPlaybackAndRememberPosition)
               keyEquivalent: @"Q"];
    return [menu autorelease];
}

- (NSMenu *)movieMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Movie"];
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
    return [menu autorelease];
}

- (void)initialize_menu
{
    NSMenu *main_menu = [[NSMenu new] autorelease];
    [NSApp setMainMenu:main_menu];
    [NSApp setAppleMenu:[self appleMenuWithMainMenu:main_menu]];

    [NSApp mainMenuItemWithParent:main_menu child:[self movieMenu]];
    [NSApp mainMenuItemWithParent:main_menu child:[self windowMenu]];
}

#undef _R

- (void)stopPlayback
{
    [self stopMPV:"quit"];
}

- (void)stopPlaybackAndRememberPosition
{
    [self stopMPV:"quit_watch_later"];
}

- (void)stopMPV:(char *)cmd
{
    if (self.inputContext) {
        mp_cmd_t *cmdt = mp_input_parse_cmd(bstr0(cmd), "");
        mp_input_queue_cmd(self.inputContext, cmdt);
    } else {
        terminate_cocoa_application();
    }
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

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)theApp {
    return NSTerminateNow;
}

- (void)handleQuitEvent:(NSAppleEventDescriptor*)e
         withReplyEvent:(NSAppleEventDescriptor*)r
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

    self.files = @[url];

    if (self.willStopOnOpenEvent) {
        self.willStopOnOpenEvent = NO;
        cocoa_stop_runloop();
    } else {
        [self handleFiles];
    }
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
    Application *app = mpv_shared_app();
    NSMutableArray *filesToOpen = [[[NSMutableArray alloc] init] autorelease];

    [filenames enumerateObjectsUsingBlock:^(id obj, NSUInteger i, BOOL *_) {
        NSInteger place = [app.argumentsList indexOfObject:obj];
        if (place == NSNotFound) {
            // Proper new event ^_^
            [filesToOpen addObject:obj];
        } else {
            // This file was already opened from the CLI. Cocoa is trying to
            // open it again using events. Ignore it!
            [app.argumentsList removeObjectAtIndex:place];
        }
    }];

    self.files = [filesToOpen sortedArrayUsingSelector:@selector(compare:)];
    if (self.willStopOnOpenEvent) {
        self.willStopOnOpenEvent = NO;
        cocoa_stop_runloop();
    } else {
        [self handleFiles];
    }
}

- (void)handleFiles
{
    void *ctx = talloc_new(NULL);
    [self.files enumerateObjectsUsingBlock:^(id obj, NSUInteger i, BOOL *_){
        const char *file = [escape_loadfile_name(obj) UTF8String];
        const char *append = (i == 0) ? "" : " append";
        char *cmd = talloc_asprintf(ctx, "raw loadfile \"%s\"%s", file, append);
        mp_cmd_t *cmdt = mp_input_parse_cmd(bstr0(cmd), "");
        mp_input_queue_cmd(self.inputContext, cmdt);
    }];
    talloc_free(ctx);
}
@end

struct playback_thread_ctx {
    mpv_main_fn mpv_main;
    int  *argc;
    char ***argv;
};

static void *playback_thread(void *ctx_obj)
{
    @autoreleasepool {
        struct playback_thread_ctx *ctx = (struct playback_thread_ctx*) ctx_obj;
        ctx->mpv_main(*ctx->argc, *ctx->argv);
        cocoa_stop_runloop();
        pthread_exit(NULL);
    }
}

int cocoa_main(mpv_main_fn mpv_main, int argc, char *argv[])
{
    @autoreleasepool {
        struct playback_thread_ctx ctx = {0};
        ctx.mpv_main = mpv_main;
        ctx.argc     = &argc;
        ctx.argv     = &argv;

        init_cocoa_application();
        macosx_finder_args_preinit(&argc, &argv);
        pthread_create(&playback_thread_id, NULL, playback_thread, &ctx);
        cocoa_run_runloop();

        // This should never be reached: cocoa_run_runloop blocks until the
        // process is quit
        mp_msg(MSGT_CPLAYER, MSGL_ERR, "There was either a problem "
               "initializing Cocoa or the Runloop was stopped unexpectedly. "
               "Please report this issues to a developer.\n");
        pthread_join(playback_thread_id, NULL);
        return 1;
    }
}

void cocoa_register_menu_item_action(MPMenuKey key, void* action)
{
    [NSApp registerSelector:(SEL)action forKey:key];
}

void init_cocoa_application(void)
{
    NSApp = mpv_shared_app();
    [NSApp setDelegate:NSApp];
    [NSApp initialize_menu];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    atexit_b(^{
        // Because activation policy has just been set to behave like a real
        // application, that policy must be reset on exit to prevent, among
        // other things, the menubar created here from remaining on screen.
        [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
    });
}

void terminate_cocoa_application(void)
{
    [NSApp hide:NSApp];
    [NSApp terminate:NSApp];
}

void cocoa_run_runloop()
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSApp run];
    [pool drain];
}

void cocoa_stop_runloop(void)
{
    [NSApp performSelectorOnMainThread:@selector(stop:)
                            withObject:nil
                         waitUntilDone:true];
    cocoa_post_fake_event();
}

void cocoa_set_input_context(struct input_ctx *input_context)
{
    mpv_shared_app().inputContext = input_context;
}

void cocoa_post_fake_event(void)
{
    NSEvent* event = [NSEvent otherEventWithType:NSApplicationDefined
                                        location:NSMakePoint(0,0)
                                   modifierFlags:0
                                       timestamp:0.0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0];
    [NSApp postEvent:event atStart:NO];
}

static void macosx_wait_fileopen_events()
{
    mpv_shared_app().willStopOnOpenEvent = YES;
    cocoa_run_runloop(); // block until done
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

static void get_system_version(int* major, int* minor, int* bugfix)
{
    static dispatch_once_t once_token;
    static int s_major  = 0;
    static int s_minor  = 0;
    static int s_bugfix = 0;
    dispatch_once(&once_token, ^{
        NSString *version_plist =
            @"/System/Library/CoreServices/SystemVersion.plist";
        NSString *version_string =
            [NSDictionary dictionaryWithContentsOfFile:version_plist]
                [@"ProductVersion"];
        NSArray* versions = [version_string componentsSeparatedByString:@"."];
        int count = [versions count];
        if (count >= 1)
            s_major = [versions[0] intValue];
        if (count >= 2)
            s_minor = [versions[1] intValue];
        if (count >= 3)
            s_bugfix = [versions[2] intValue];
    });
    *major  = s_major;
    *minor  = s_minor;
    *bugfix = s_bugfix;
}

static bool is_psn_argument(char *psn_arg_to_check)
{
    NSString *psn_arg = [NSString stringWithUTF8String:psn_arg_to_check];
    return [psn_arg hasPrefix:@"-psn_"];
}

static bool bundle_started_from_finder(int argc, char **argv)
{
    bool bundle_detected = [[NSBundle mainBundle] bundleIdentifier];
    int major, minor, bugfix;
    get_system_version(&major, &minor, &bugfix);
    if ((major == 10) && (minor >= 9)) {
        return bundle_detected && argc==1;
    } else {
        return bundle_detected && argc==2 && is_psn_argument(argv[1]);
    }
}

void macosx_finder_args_preinit(int *argc, char ***argv)
{
    Application *app = mpv_shared_app();

    if (bundle_started_from_finder(*argc, *argv)) {
        macosx_redirect_output_to_logfile("mpv");
        macosx_wait_fileopen_events();

        char **cocoa_argv = talloc_zero_array(NULL, char*, [app.files count] + 2);
        cocoa_argv[0]     = "mpv";
        cocoa_argv[1]     = "--quiet";
        int  cocoa_argc   = 2;

        for (NSString *filename in app.files) {
            cocoa_argv[cocoa_argc] = (char*)[filename UTF8String];
            cocoa_argc++;
        }

        *argc = cocoa_argc;
        *argv = cocoa_argv;
    } else {
        for (int i = 0; i < *argc; i++ ) {
            NSString *arg = [NSString stringWithUTF8String:(*argv)[i]];
            [app.argumentsList addObject:arg];
        }
    }
}
