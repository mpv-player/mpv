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

#include <stdio.h>
#include <pthread.h>
#include "talloc.h"

#include "common/msg.h"
#include "input/input.h"
#include "input/event.h"
#include "input/keycodes.h"

#include "osdep/macosx_application_objc.h"
#include "osdep/macosx_compat.h"

#define MPV_PROTOCOL @"mpv://"

static pthread_t playback_thread_id;

// Give the cocoa thread and explicit state WRT the mpv thread.
// The app state passes monotonically through these states.
enum ApplicationState {
    ApplicationInitialState = 0, // Before cocoa_main
    ApplicationMPVInitState,     // Waiting for MPV to call init_cocoa_application.
    ApplicationInitializedState, // cocoa_main can call [NSApp run] now.
    ApplicationLaunchingState,   // cocoa_main has called/will call [NSApp run].
    ApplicationRunningState,     // applicationDidFinishLaunching was received
    ApplicationMPVTermState,     // terminate_cocoa_application called, waiting for cocoa_exit
    ApplicationExitingState,     // Exit is imminent.
} application_state = 0;

static pthread_mutex_t app_state_mutex;
static pthread_cond_t  app_state_cond;

// Caller should almost certainly be holding app_state_mutex.
static void stepAppState(enum ApplicationState currentState,
                         enum ApplicationState targetState)
{
    assert(application_state == currentState);
    application_state = targetState;
    pthread_cond_broadcast(&app_state_cond);
}

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
@end

@interface NSApplication (NiblessAdditions)
- (void)setAppleMenu:(NSMenu *)aMenu;
@end

Application *mpv_shared_app(void)
{
    return (Application *)[Application sharedApplication];
}

@implementation Application

- (void)sendEvent:(NSEvent *)event
{
    [super sendEvent:event];

    if (self.inputContext)
        mp_input_wakeup(self.inputContext);
}

- (id)init
{
    if (self = [super init]) {
        self.menuItems = [NSMutableDictionary dictionary];
        self.eventsResponder = [EventsResponder new];

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

    self.eventsResponder = nil;
    self.menuItems = nil;
    [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    pthread_mutex_lock(&app_state_mutex);
    stepAppState(ApplicationLaunchingState, ApplicationRunningState);
    pthread_mutex_unlock(&app_state_mutex);
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
        mp_cmd_t *cmdt = mp_input_parse_cmd(self.inputContext, bstr0(cmd), "");
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

    [self handleFilesArray:@[url]];
}

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
    if(application_state >= ApplicationMPVTermState)
        return;

    // Cocoa likes to re-send events for CLI arguments. Ignore them!
    if(!self.bundleStartedFromFinder && application_state < ApplicationRunningState)
        return;

    NSArray *files = [filenames sortedArrayUsingSelector:@selector(localizedStandardCompare:)];
    [self handleFilesArray:files];
}

- (void)handleFilesArray:(NSArray *)files
{
    if(!self.inputContext)
        return;
    size_t num_files  = [files count];
    char **files_utf8 = talloc_array(NULL, char*, num_files);
    [files enumerateObjectsUsingBlock:^(id obj, NSUInteger i, BOOL *_){
        char *filename = (char *)[obj UTF8String];
        size_t bytes   = [obj lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
        files_utf8[i]  = talloc_memdup(files_utf8, filename, bytes + 1);
    }];
    mp_event_drop_files(self.inputContext, num_files, files_utf8);
    talloc_free(files_utf8);
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
        int status = ctx->mpv_main(*ctx->argc, *ctx->argv);
        cocoa_exit(status);
    }
}

static void macosx_finder_args_preinit(int *argc, char ***argv);
int cocoa_main(mpv_main_fn mpv_main, int argc, char *argv[])
{
    pthread_mutex_init(&app_state_mutex, NULL);
    pthread_cond_init(&app_state_cond, NULL);

    @autoreleasepool {
        NSApp = mpv_shared_app();
        [NSApp setDelegate:NSApp];
        [NSApp initialize_menu];

        macosx_finder_args_preinit(&argc, &argv);
    }

    struct playback_thread_ctx ctx = {
        .mpv_main = mpv_main,
        .argc     = &argc,
        .argv     = &argv,
    };

    stepAppState(ApplicationInitialState, ApplicationMPVInitState);
    pthread_create(&playback_thread_id, NULL, playback_thread, &ctx);

    pthread_mutex_lock(&app_state_mutex);
    while (application_state < ApplicationInitializedState)
        pthread_cond_wait(&app_state_cond, &app_state_mutex);

    if(application_state == ApplicationExitingState) {
        // cocoa_exit was called before we've even really started
        pthread_mutex_unlock(&app_state_mutex);
        pthread_join(playback_thread_id, NULL);

        // This is unreachable, since the playback thread that we're joining
        // with has called cocoa_exit, which at this point in time* certainly
        // results in an exit() call.
        // * i.e. before the "stepAppState()" below has been run.
        fprintf(stderr, "The main Cocoa thread expected to be terminated and wasn't.\n"
                        "Please report this issue to a developer.\n");
        exit(1);
    }

    stepAppState(ApplicationInitializedState, ApplicationLaunchingState);
    pthread_mutex_unlock(&app_state_mutex);

    @autoreleasepool {
        [NSApp run];
    }

    fprintf(stderr, "The Cocoa runloop was stopped unexpectedly.\n"
                    "Please report this issue to a developer.\n");
    exit(1);
}

static const char macosx_icon[] =
#include "osdep/macosx_icon.inc"
;

static void set_application_icon()
{
    NSData *icon_data = [NSData dataWithBytesNoCopy:(void *)macosx_icon
                                             length:sizeof(macosx_icon)
                                       freeWhenDone:NO];
    NSImage *icon = [[NSImage alloc] initWithData:icon_data];
    [NSApp setApplicationIconImage:icon];
    [icon release];
}

// Map a struct MPOpts app_style to a NSApplicationActivationPolicy
static NSApplicationActivationPolicy activation_policy_from_app_style(int app_style) {
    switch(app_style) {
        case -1: return NSApplicationActivationPolicyProhibited; // --app-style=disabled
        case  1: return NSApplicationActivationPolicyRegular;    // --app-style=normal
        case  2: return NSApplicationActivationPolicyAccessory;  // --app-style=accessory
        default:
            fprintf(stderr, "init_cocoa_application() found an unknown option value for opts->app_style.\n"
                            "Please report this issue to a developer.\n");
            // Fall through to auto case
        case 0: // --app-style=auto
            return NSApplicationActivationPolicyRegular;
    }
}

void init_cocoa_application(const struct MPOpts *opts, struct input_ctx *inputCtx)
{
    // *opts does not persist for the whole cocoa lifetime, so the opts pointer
    // must not be kept past the scope of this function! Any information from it
    // that is needed later should be copied out.
    @autoreleasepool {
        mpv_shared_app().inputContext = inputCtx;

        NSApplicationActivationPolicy policy = activation_policy_from_app_style(opts->app_style);
        if ([NSApp activationPolicy] != policy) {
            [NSApp setActivationPolicy: policy];

            // The icon needs to be set only when a command-line launched mpv switches
            // from ActivationPolicyProhibited to ActivationPolicyRegular
            if (policy == NSApplicationActivationPolicyRegular)
                set_application_icon();
        }
    }
    pthread_mutex_lock(&app_state_mutex);
    stepAppState(ApplicationMPVInitState, ApplicationInitializedState);
    pthread_mutex_unlock(&app_state_mutex);
}

void terminate_cocoa_application()
{
    pthread_mutex_lock(&app_state_mutex);
    if (application_state < ApplicationInitializedState) {
        pthread_mutex_unlock(&app_state_mutex);
        return;
    }

    // If terminate is called right after init, the cocoa app may not have started yet.
    while (application_state < ApplicationRunningState)
        pthread_cond_wait(&app_state_cond, &app_state_mutex);

    @autoreleasepool {
        Application *app = mpv_shared_app();
        app.inputContext = NULL;
        [app hide:nil];
    }

    stepAppState(ApplicationRunningState, ApplicationMPVTermState);
    pthread_mutex_unlock(&app_state_mutex);
}

void cocoa_exit(int status)
{
    if (application_state == ApplicationInitialState) {
        exit(status);
    }

    pthread_mutex_lock(&app_state_mutex);
    if (application_state < ApplicationLaunchingState) {
        application_state = ApplicationExitingState;
        pthread_cond_broadcast(&app_state_cond);
        pthread_mutex_unlock(&app_state_mutex);
        exit(status);
    }

    if (application_state < ApplicationMPVTermState) {
        pthread_mutex_unlock(&app_state_mutex);
        terminate_cocoa_application();
        pthread_mutex_lock(&app_state_mutex);
    }

    if (application_state < ApplicationExitingState) {
        stepAppState(ApplicationMPVTermState, ApplicationExitingState);
    }
    pthread_mutex_unlock(&app_state_mutex);

    @autoreleasepool {
        Application *app = mpv_shared_app();
        if (app.bundleStartedFromFinder) {
            [app terminate:app];
            pthread_exit(NULL);
        }
    }

    exit(status);
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
    static int s_major  = 0;
    static int s_minor  = 0;
    static int s_bugfix = 0;
    if (!s_major && !s_minor && !s_bugfix) {
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
    }
    *major  = s_major;
    *minor  = s_minor;
    *bugfix = s_bugfix;
}

static bool is_psn_argument(char *psn_arg_to_check)
{
    return strncmp(psn_arg_to_check, "-psn_", 5) == 0;
}

static bool bundle_started_from_finder(int argc, char **argv)
{
    bool bundle_detected = [[NSBundle mainBundle] bundleIdentifier];
    int major, minor, bugfix;
    get_system_version(&major, &minor, &bugfix);
    bool without_psn = bundle_detected && argc==1;
    bool with_psn    = bundle_detected && argc==2 && is_psn_argument(argv[1]);

    if ((major == 10) && (minor >= 9)) {
        // Looks like opening quarantined files from the finder inserts the
        // -psn argument while normal files do not. Hurr.
        return with_psn || without_psn;
    } else {
        return with_psn;
    }
}

static void macosx_finder_args_preinit(int *argc, char ***argv)
{
    Application *app = mpv_shared_app();

    if (!bundle_started_from_finder(*argc, *argv)) {
        app.bundleStartedFromFinder = NO;
    } else {
        app.bundleStartedFromFinder = YES;
        macosx_redirect_output_to_logfile("mpv");

        static char *cocoa_argv[] = {
            "mpv",
            "--quiet",
            "--keep-open",
            "--idle",
        };

        *argc = sizeof(cocoa_argv)/sizeof(*cocoa_argv);
        *argv = cocoa_argv;
    }
}

void cocoa_register_menu_item_action(MPMenuKey key, void* action)
{
    [NSApp registerSelector:(SEL)action forKey:key];
}
