// Plays a video from the command line in a window provided by mpv.
// You likely want to play the video in your own window instead,
// but that's not quite ready yet.
// You may need a basic Info.plist and MainMenu.xib to make this work.

#include "../../libmpv/client.h"
#include "shared.h"

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
{
    mpv_handle *mpv;
    dispatch_queue_t queue;
}
@end

static void wakeup(void *);

@implementation AppDelegate

- (void) applicationDidFinishLaunching:(NSNotification *)notification {
    // Read filename
    NSArray *args = [NSProcessInfo processInfo].arguments;
    if (args.count < 2) {
        NSLog(@"Expected filename on command line");
        exit(1);
    }
    NSString *filename = args[1];

    // Deal with MPV in the background.
    queue = dispatch_queue_create("mpv", DISPATCH_QUEUE_SERIAL);
    dispatch_async(queue, ^{

        mpv = mpv_create();
        if (!mpv) {
            printf("failed creating context\n");
            exit(1);
        }

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
        NSApplication *app = [NSApplication new];
        AppDelegate *delegate = [AppDelegate new];
        app.delegate = delegate;
        [app run];
    }
    return EXIT_SUCCESS;
}
