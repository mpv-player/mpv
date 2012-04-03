/*
 * Cocoa Event Handling
 *
 * This file is part of mplayer2.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Implementation details:
 * This file deals with custom event polling on MacOSX. When mplayer2 is paused
 * it will asynchronously poll for events using select. This works correctly on
 * Linux with X11 since the events are notified through the file descriptors
 * where mplayer2 is listening on. On the other hand, the OSX window server
 * notifies the processes for events using mach ports.
 *
 * The code below uses functionality from Cocoa that abstracts the async polling
 * of events from the window server. When a Cocoa event comes in, the polling is
 * interrupted and the event is dealt with in the next vo_check_events.
 *
 * To keep the select fd polling code working, that functionality is executed
 * from another thread. Whoever finishes polling before the given time, be it
 * Cocoa or the original select code, notifies the other for an immediate wake.
 */

#include "cocoa_events.h"
#include "libvo/cocoa_common.h"
#include "talloc.h"

#import <Cocoa/Cocoa.h>
#include <dispatch/dispatch.h>

// Bogus event subtype to wake the Cocoa code from polling state
#define MP_EVENT_SUBTYPE_WAKE_EVENTLOOP 100

// This is the threshold in milliseconds below which the Cocoa polling is not
// executed. There is some overhead caused by the synchronization between
// threads. Even if in practice it isn't noticeable, we try to avoid the useless
// waste of resources.
#define MP_ASYNC_THRESHOLD 50

struct priv {
    dispatch_queue_t select_queue;
    bool is_runloop_polling;
    void (*read_all_fd_events)(struct input_ctx *ictx, int time);
};

static struct priv *p;

static void cocoa_wait_events(int mssleeptime)
{
    NSTimeInterval sleeptime = mssleeptime / 1000.0;
    NSEvent *event;
    p->is_runloop_polling = YES;
    event = [NSApp nextEventMatchingMask:NSAnyEventMask
           untilDate:[NSDate dateWithTimeIntervalSinceNow:sleeptime]
           inMode:NSEventTrackingRunLoopMode dequeue:NO];

    // dequeue the next event if it is a fake to wake the cocoa polling
    if (event && [event type] == NSApplicationDefined &&
                 [event subtype] == MP_EVENT_SUBTYPE_WAKE_EVENTLOOP) {
        [NSApp nextEventMatchingMask:NSAnyEventMask untilDate:nil
               inMode:NSEventTrackingRunLoopMode dequeue:YES];
    }
    p->is_runloop_polling = NO;
}

static void cocoa_wake_runloop()
{
    if (p->is_runloop_polling) {
        NSAutoreleasePool *pool = [NSAutoreleasePool new];
        NSEvent *event;

        /* Post an event so we'll wake the run loop that is async polling */
        event = [NSEvent otherEventWithType: NSApplicationDefined
                                   location: NSZeroPoint
                              modifierFlags: 0
                                  timestamp: 0
                               windowNumber: 0
                                    context: nil
                                    subtype: MP_EVENT_SUBTYPE_WAKE_EVENTLOOP
                                      data1: 0
                                      data2: 0];

        [NSApp postEvent:event atStart:NO];
        [pool release];
    }
}

void cocoa_events_init(struct input_ctx *ictx,
    void (*read_all_fd_events)(struct input_ctx *ictx, int time))
{
    NSApplicationLoad();
    p = talloc_ptrtype(NULL, p);
    *p = (struct priv){
        .is_runloop_polling = NO,
        .read_all_fd_events = read_all_fd_events,
        .select_queue = dispatch_queue_create("org.mplayer2.select_queue",
                                              NULL),
    };
}

void cocoa_events_uninit(void)
{
    talloc_free(p);
}

void cocoa_events_read_all_events(struct input_ctx *ictx, int time)
{
    // don't bother delegating the select to the async queue if the blocking
    // time is really low or if we are not running a GUI
    if (time > MP_ASYNC_THRESHOLD && vo_cocoa_gui_running()) {
        dispatch_async(p->select_queue, ^{
            p->read_all_fd_events(ictx, time);
            cocoa_wake_runloop();
        });

        cocoa_wait_events(time);
        mp_input_wakeup(ictx);

        // wait for the async queue to get empty.
        dispatch_sync(p->select_queue, ^{});
    } else {
        p->read_all_fd_events(ictx, time);
    }
}
