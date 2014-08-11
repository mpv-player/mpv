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
}
@end

@implementation AppDelegate

- (void) applicationDidFinishLaunching:(NSNotification *)notification {
    // Read filename
    NSArray *args = [NSProcessInfo processInfo].arguments;
    if (args.count < 2) {
        NSLog(@"Expected filename on command line");
        exit(1);
    }
    NSString *filename = args[1];

    // Run MPV loop on its own queue
    dispatch_async(dispatch_queue_create("mpv", DISPATCH_QUEUE_SERIAL), ^{

        // Set up MPV
        mpv = mpv_create();
        if (!mpv) {
            printf("failed creating context\n");
            exit(1);
        }
        check_error(mpv_initialize(mpv));

        const char *cmd[] = {"loadfile", filename.UTF8String, NULL};
        check_error(mpv_command(mpv, cmd));

        // Listen for events
        mpv_event *event;
        do {
            event = mpv_wait_event(mpv, -1);
            switch (event->event_id) {
                case MPV_EVENT_NONE:
                    break;
                default:
                    printf("event: %s\n", mpv_event_name(event->event_id));
            }
        } while (event->event_id != MPV_EVENT_SHUTDOWN);

        // Clean up and shut down
        mpv_terminate_destroy(mpv);
        mpv = nil;
        dispatch_async(dispatch_get_main_queue(), ^{
            [[NSApplication sharedApplication] terminate:nil];
        });
    });
}

// Ostensibly, mpv's window would be hooked up to this.
- (BOOL) windowShouldClose:(id)sender
{
    if (mpv) {
        const char *args[] = {"quit", NULL};
        mpv_command(mpv, args);
    }
    return YES;
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
