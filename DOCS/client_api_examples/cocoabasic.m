// Plays a video from the command line in a window provided by mpv.
// You likely want to play the video in your own window instead,
// but that's not quite ready yet.
// You may need a basic Info.plist and MainMenu.xib to make this work.

#include "../../libmpv/client.h"
#include "shared.h"

#import <Cocoa/Cocoa.h>

#define EMBED_VIEW 1

#if EMBED_VIEW
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
}
@end
#endif

static void wakeup(void *);

#if EMBED_VIEW
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
    [self->w makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}
#endif

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

#if EMBED_VIEW
    [self createWindow];
#endif

    // Deal with MPV in the background.
    queue = dispatch_queue_create("mpv", DISPATCH_QUEUE_SERIAL);
    dispatch_async(queue, ^{

        mpv = mpv_create();
        if (!mpv) {
            printf("failed creating context\n");
            exit(1);
        }

#if EMBED_VIEW
        int64_t wid = (intptr_t) self->w;
        check_error(mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid));
#endif

        // Maybe set some options here, like default key bindings.
        // NOTE: Interaction with the window seems to be broken for now.
        check_error(mpv_set_option_string(mpv, "input-default-bindings", "yes"));

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
        case MPV_EVENT_SHUTDOWN:
            // Clean up and shut down.
            mpv_terminate_destroy(mpv);
            mpv = NULL;
            dispatch_async(dispatch_get_main_queue(), ^{
                [[NSApplication sharedApplication] terminate:nil];
            });
            break;

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
    [self shutdown];
    if (self->w)
        [self->w release];
    return YES;
}

- (void) shutdown
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
