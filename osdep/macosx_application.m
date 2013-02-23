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

#include "talloc.h"

#include "core/mp_fifo.h"
#include "core/input/input.h"
#include "core/input/keycodes.h"

#include "osdep/macosx_application_objc.h"
#include "video/out/osx_common.h"

// 0.0001 seems too much and 0.01 too low, no idea why this works so well
#define COCOA_MAGIC_TIMER_DELAY 0.001

static Application *app;

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

@implementation Application
@synthesize files = _files;
@synthesize argumentsList = _arguments_list;
@synthesize willStopOnOpenEvent = _will_stop_on_open_event;

@synthesize callback = _callback;
@synthesize shouldStopPlayback = _should_stop_playback;
@synthesize context = _context;
@synthesize inputContext = _input_context;
@synthesize keyFIFO = _key_fifo;
@synthesize callbackTimer = _callback_timer;
@synthesize menuItems = _menu_items;

- (id)init
{
    if (self = [super init]) {
        self.menuItems = [[[NSMutableDictionary alloc] init] autorelease];
        self.files = nil;
        self.argumentsList = [[[NSMutableArray alloc] init] autorelease];
        self.willStopOnOpenEvent = NO;
    }

    return self;
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
    [self menuItemWithParent:menu title:@"Quit mpv"
                      action:@selector(stopPlayback) keyEquivalent: @"q"];
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

    [app mainMenuItemWithParent:main_menu child:[self movieMenu]];
    [app mainMenuItemWithParent:main_menu child:[self windowMenu]];
}

#undef _R

- (void)call_callback
{
    if (self.shouldStopPlayback(self.context)) {
        [NSApp stop:nil];
        cocoa_post_fake_event();
    } else {
        self.callback(self.context);
    }
}

- (void)schedule_timer
{
    self.callbackTimer =
        [NSTimer timerWithTimeInterval:COCOA_MAGIC_TIMER_DELAY
                                target:self
                              selector:@selector(call_callback)
                              userInfo:nil
                               repeats:YES];

    [[NSRunLoop currentRunLoop] addTimer:self.callbackTimer
                                forMode:NSDefaultRunLoopMode];

    [[NSRunLoop currentRunLoop] addTimer:self.callbackTimer
                                forMode:NSEventTrackingRunLoopMode];
}

- (void)stopPlayback
{
    mplayer_put_key(app.keyFIFO, MP_KEY_CLOSE_WIN);
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

- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
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
        [NSApp stop:nil];
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
        char *cmd = talloc_asprintf(ctx, "loadfile \"%s\"%s", file, append);
        mp_cmd_t *cmdt = mp_input_parse_cmd(bstr0(cmd), "");
        mp_input_queue_cmd(self.inputContext, cmdt);
    }];
    talloc_free(ctx);
}
@end

void cocoa_register_menu_item_action(MPMenuKey key, void* action)
{
    [app registerSelector:(SEL)action forKey:key];
}

void init_cocoa_application(void)
{
    NSApp = [NSApplication sharedApplication];
    app = [[Application alloc] init];
    [NSApp setDelegate:app];
    [app initialize_menu];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

void terminate_cocoa_application(void)
{
    [NSApp hide:app];
    [NSApp terminate:app];
}

void cocoa_run_runloop(void)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSApp run];
    [pool drain];
}

void cocoa_run_loop_schedule(play_loop_callback callback,
                             should_stop_callback stop_query,
                             void *context,
                             struct input_ctx *input_context,
                             struct mp_fifo *key_fifo)
{
    [NSApp setDelegate:app];
    app.callback            = callback;
    app.context             = context;
    app.shouldStopPlayback  = stop_query;
    app.inputContext        = input_context;
    app.keyFIFO             = key_fifo;
    [app schedule_timer];
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
    app.willStopOnOpenEvent = YES;
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

static bool psn_matches_current_process(char *psn_arg_to_check)
{
    ProcessSerialNumber psn;
    size_t psn_length = 5+10+1+10;
    char psn_arg[psn_length+1];

    GetCurrentProcess(&psn);
    snprintf(psn_arg, 5+10+1+10+1, "-psn_%u_%u",
             psn.highLongOfPSN, psn.lowLongOfPSN);
    psn_arg[psn_length]=0;

    return strcmp(psn_arg, psn_arg_to_check) == 0;
}

void macosx_finder_args_preinit(int *argc, char ***argv)
{
    if (*argc==2 && psn_matches_current_process((*argv)[1])) {
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
