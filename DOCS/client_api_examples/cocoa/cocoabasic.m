// Plays a video from the command line in a window provided by mpv.
// You likely want to play the video in your own window instead,
// but that's not quite ready yet.
// You may need a basic Info.plist and MainMenu.xib to make this work.

// Build with: clang -o cocoabasic cocoabasic.m `pkg-config --libs --cflags mpv`

#include <mpv/client.h>

#include <stdio.h>
#include <stdlib.h>

static inline void check_error(int status)
{
    if (status < 0) {
        printf("mpv API error: %s\n", mpv_error_string(status));
        exit(1);
    }
}

#import <Cocoa/Cocoa.h>

@interface CocoaWindow : NSWindow
@end

@implementation CocoaWindow
- (BOOL)canBecomeMainWindow { return YES; }
- (BOOL)canBecomeKeyWindow { return YES; }
@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
{
    mpv_handle *mpv;
    dispatch_queue_t queue;
    NSWindow *w;
    NSView *wrapper;
}
@end

static void wakeup(void *);

@implementation AppDelegate

- (void)createWindow {

    int mask = NSTitledWindowMask|NSClosableWindowMask|
               NSMiniaturizableWindowMask|NSResizableWindowMask;

    self->w = [[CocoaWindow alloc]
        initWithContentRect:NSMakeRect(0,0, 1280, 720)
                  styleMask:mask
                    backing:NSBackingStoreBuffered
                      defer:NO];

    [self->w setTitle:@"cocoabasic example"];
    [self->w makeMainWindow];
    [self->w makeKeyAndOrderFront:nil];

    NSRect frame = [[self->w contentView] bounds];
    self->wrapper = [[NSView alloc] initWithFrame:frame];
    [self->wrapper setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    [[self->w contentView] addSubview:self->wrapper];
    [self->wrapper release];

    NSMenu *m = [[NSMenu alloc] initWithTitle:@"AMainMenu"];
    NSMenuItem *item = [m addItemWithTitle:@"Apple" action:nil keyEquivalent:@""];
    NSMenu *sm = [[NSMenu alloc] initWithTitle:@"Apple"];
    [m setSubmenu:sm forItem:item];
    [sm addItemWithTitle: @"mpv_command('stop')" action:@selector(mpv_stop) keyEquivalent:@""];
    [sm addItemWithTitle: @"mpv_command('quit')" action:@selector(mpv_quit) keyEquivalent:@""];
    [sm addItemWithTitle: @"quit" action:@selector(terminate:) keyEquivalent:@"q"];
    [NSApp setMenu:m];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void) applicationDidFinishLaunching:(NSNotification *)notification {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    atexit_b(^{
        // Because activation policy has just been set to behave like a real
        // application, that policy must be reset on exit to prevent, among
        // other things, the menubar created here from remaining on screen.
        [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
    });

    // Read filename
    NSArray *args = [NSProcessInfo processInfo].arguments;
    if (args.count < 2) {
        NSLog(@"Expected filename on command line");
        exit(1);
    }
    NSString *filename = args[1];

    [self createWindow];

    // Deal with MPV in the background.
    queue = dispatch_queue_create("mpv", DISPATCH_QUEUE_SERIAL);
    dispatch_async(queue, ^{

        mpv = mpv_create();
        if (!mpv) {
            printf("failed creating context\n");
            exit(1);
        }

        int64_t wid = (intptr_t) self->wrapper;
        check_error(mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid));

        // Maybe set some options here, like default key bindings.
        // NOTE: Interaction with the window seems to be broken for now.
        check_error(mpv_set_option_string(mpv, "input-default-bindings", "yes"));

        // for testing!
        check_error(mpv_set_option_string(mpv, "input-media-keys", "yes"));
        check_error(mpv_set_option_string(mpv, "input-cursor", "no"));
        check_error(mpv_set_option_string(mpv, "input-vo-keyboard", "yes"));

        // request important errors
        check_error(mpv_request_log_messages(mpv, "warn"));

        check_error(mpv_initialize(mpv));

        // Register to be woken up whenever mpv generates new events.
        mpv_set_wakeup_callback(mpv, wakeup, (__bridge void *) self);

        // Load the indicated file
        const char *cmd[] = {"loadfile", filename.UTF8String, NULL};
        check_error(mpv_command(mpv, cmd));
    });
}

- (void) handleEvent:(mpv_event *)event
{
    switch (event->event_id) {
    case MPV_EVENT_SHUTDOWN: {
        mpv_detach_destroy(mpv);
        mpv = NULL;
        printf("event: shutdown\n");
        break;
    }

    case MPV_EVENT_LOG_MESSAGE: {
        struct mpv_event_log_message *msg = (struct mpv_event_log_message *)event->data;
        printf("[%s] %s: %s", msg->prefix, msg->level, msg->text);
    }

    case MPV_EVENT_VIDEO_RECONFIG: {
        dispatch_async(dispatch_get_main_queue(), ^{
            NSArray *subviews = [self->wrapper subviews];
            if ([subviews count] > 0) {
                // mpv's events view
                NSView *eview = [self->wrapper subviews][0];
                [self->w makeFirstResponder:eview];
            }
        });
    }

    default:
        printf("event: %s\n", mpv_event_name(event->event_id));
    }
}

- (void) readEvents
{
    dispatch_async(queue, ^{
        while (mpv) {
            mpv_event *event = mpv_wait_event(mpv, 0);
            if (event->event_id == MPV_EVENT_NONE)
                break;
            [self handleEvent:event];
        }
    });
}

static void wakeup(void *context) {
    AppDelegate *a = (__bridge AppDelegate *) context;
    [a readEvents];
}

// Ostensibly, mpv's window would be hooked up to this.
- (BOOL) windowShouldClose:(id)sender
{
    return NO;
}

- (void) mpv_stop
{
    if (mpv) {
        const char *args[] = {"stop", NULL};
        mpv_command(mpv, args);
    }
}

- (void) mpv_quit
{
    if (mpv) {
        const char *args[] = {"quit", NULL};
        mpv_command(mpv, args);
    }
}
@end

// Delete this if you already have a main.m.
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *delegate = [AppDelegate new];
        app.delegate = delegate;
        [app run];
    }
    return EXIT_SUCCESS;
}
