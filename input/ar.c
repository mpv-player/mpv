/*
 * Apple Remote input interface
 *
 * Copyright (C) 2007 Zoltan Ponekker <pontscho at kac.poliod.hu>
 *
 * (modified a bit by Ulion <ulion2002 at gmail.com>)
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include <Carbon/Carbon.h>

#include <sys/types.h>
#include <unistd.h>

#include "input.h"
#include "ar.h"

extern int slave_mode;

extern const double NSAppKitVersionNumber;

typedef struct cookie_keycode_map {
    char *cookies;
    int seq_len;
    int keycode;
} cookie_keycode_map_t;

/* On tiger, 5 always follows 6; on leopard, 18 always follows 19.
 * On leopard, there seems to be no cookie value of 5 or 6.
 * Following is the shortened cookie sequence list
 * keycode      cookies_on_tiger cookies_on_leopard *down_state
 * AR_PREV_HOLD 14+6+3+2         31+19+3+2          yes
 * AR_NEXT_HOLD 14+6+4+2         31+19+4+2          yes
 * AR_MENU_HOLD 14+6+14+6        31+19+31+19
 * AR_VUP       14+12+11+6       31+29+28+19        yes
 * AR_VDOWN     14+13+11+6       31+30+28+19        yes
 * AR_MENU      14+7+6+14+7+6    31+20+19+31+20+19
 * AR_PLAY      14+8+6+14+8+6    31+21+19+31+21+19
 * AR_NEXT      14+9+6+14+9+6    31+22+19+31+22+19
 * AR_PREV      14+10+6+14+10+6  31+23+19+31+23+19
 * AR_PLAY_HOLD 18+14+6+18+14+6  35+31+19+35+31+19
 *
 * *down_state: A button with this feature has a pressed event and
 * a released event, with which we can trace the state of the button.
 * A button without this feature will only return one release event.
 *
 * hidden keys currently not implemented:
 * hold for 5 secs
 * MENU_NEXT_HOLD  15+14+6+15+14+6
 * MENU_PREV_HOLD  16+14+6+16+14+6
 * MENU_VUP_HOLD   20+14+6+20+14+6
 * MENU_VDOWN_HOLD 19+14+6+19+14+6
 *
 * It seems that pressing 'menu' and 'play' on the Apple Remote for
 * 5 seconds will trigger the make-pair function of the remote.
 * MENU_PLAY_HOLD  21+15+14+6+15+14+6
 */

static const cookie_keycode_map_t ar_codes_tiger[] = {
    { "\x0E\x06\x03\x02",         4, AR_PREV_HOLD     },
    { "\x0E\x06\x04\x02",         4, AR_NEXT_HOLD     },
    { "\x0E\x06\x0E\x06",         4, AR_MENU_HOLD     },
    { "\x0E\x0C\x0B\x06",         4, AR_VUP           },
    { "\x0E\x0D\x0B\x06",         4, AR_VDOWN         },
    { "\x0E\x07\x06\x0E\x07\x06", 6, AR_MENU          },
    { "\x0E\x08\x06\x0E\x08\x06", 6, AR_PLAY          },
    { "\x0E\x09\x06\x0E\x09\x06", 6, AR_NEXT          },
    { "\x0E\x0A\x06\x0E\x0A\x06", 6, AR_PREV          },
    { "\x12\x0E\x06\x12\x0E\x06", 6, AR_PLAY_HOLD     },
    { NULL,                       0, MP_INPUT_NOTHING },
};

static const cookie_keycode_map_t ar_codes_leopard[] = {
    { "\x1F\x13\x03\x02",         4, AR_PREV_HOLD     },
    { "\x1F\x13\x04\x02",         4, AR_NEXT_HOLD     },
    { "\x1F\x13\x1F\x13",         4, AR_MENU_HOLD     },
    { "\x1F\x1D\x1C\x13",         4, AR_VUP           },
    { "\x1F\x1E\x1C\x13",         4, AR_VDOWN         },
    { "\x1F\x14\x13\x1F\x14\x13", 6, AR_MENU          },
    { "\x1F\x15\x13\x1F\x15\x13", 6, AR_PLAY          },
    { "\x1F\x16\x13\x1F\x16\x13", 6, AR_NEXT          },
    { "\x1F\x17\x13\x1F\x17\x13", 6, AR_PREV          },
    { "\x23\x1F\x13\x23\x1F\x13", 6, AR_PLAY_HOLD     },
    { NULL,                       0, MP_INPUT_NOTHING },
};

static int is_leopard;
static const cookie_keycode_map_t *ar_codes;

static IOHIDQueueInterface **queue;
static IOHIDDeviceInterface **hidDeviceInterface = NULL;
static int initialized = 0;
static int hidDeviceIsOpen;

/* Maximum number of elements in queue before oldest elements
   in queue begin to be lost. Set to 16 to hold at least 2 events. */
static const int MAX_QUEUE_SIZE = 16;


static int FindHIDDevices(mach_port_t masterPort,
                          io_iterator_t *hidObjectIterator)
{
    CFMutableDictionaryRef hidMatchDictionary;
    IOReturn ioReturnValue;

    // Set up a matching dictionary to search the I/O Registry
    // by class name for all HID class devices.
    hidMatchDictionary = IOServiceMatching("AppleIRController");

    // Now search I/O Registry for matching devices.
    ioReturnValue = IOServiceGetMatchingServices(masterPort,
                                                 hidMatchDictionary,
                                                 hidObjectIterator);

    // If search is unsuccessful, print message and hang.
    if (ioReturnValue != kIOReturnSuccess ||
            !IOIteratorIsValid(*hidObjectIterator)) {
        return -1;
    }
    return 0;
}

static int getHIDCookies(IOHIDDeviceInterface122 **handle,
                         IOHIDElementCookie **cookies,
                         int *nr_cookies)
{
    CFTypeRef object;
    long number;
    CFArrayRef elements;
    CFDictionaryRef element;
    CFIndex i;

    *nr_cookies = 0;

    if (!handle || !(*handle))
        return -1;

    // Copy all elements, since we're grabbing most of the elements
    // for this device anyway, and thus, it's faster to iterate them
    // ourselves. When grabbing only one or two elements, a matching
    // dictionary should be passed in here instead of NULL.
    if (((*handle)->copyMatchingElements(handle, NULL, &elements)) != kIOReturnSuccess)
        return -1;

    // No elements, still a valid result.
    if (CFArrayGetCount(elements)==0)
        return 0;

    *cookies = calloc(CFArrayGetCount(elements), sizeof(IOHIDElementCookie));
    if (*cookies == NULL)
        return -1;

    for (i=0; i<CFArrayGetCount(elements); i++) {
        element = CFArrayGetValueAtIndex(elements, i);

        // Get cookie.
        object = CFDictionaryGetValue(element, CFSTR(kIOHIDElementCookieKey));
        if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
            continue;
        if (!CFNumberGetValue((CFNumberRef)object, kCFNumberLongType, &number))
            continue;
        (*cookies)[(*nr_cookies)++] = (IOHIDElementCookie)number;
    }

    return 0;
}

static int CreateHIDDeviceInterface(io_object_t hidDevice,
                                    IOHIDDeviceInterface ***hidDeviceInterface)
{
    io_name_t className;
    IOCFPlugInInterface **plugInInterface = NULL;
    SInt32 score = 0;

    if (IOObjectGetClass(hidDevice, className) != kIOReturnSuccess)
        return -1;

    if (IOCreatePlugInInterfaceForService(hidDevice,
                                          kIOHIDDeviceUserClientTypeID,
                                          kIOCFPlugInInterfaceID,
                                          &plugInInterface,
                                          &score) != kIOReturnSuccess)
        return -1;

    // Call a method of the intermediate plugin to create the device interface
    if ((*plugInInterface)->QueryInterface(plugInInterface,
                                   CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID),
                                   (LPVOID)hidDeviceInterface) != S_OK
            || *hidDeviceInterface == NULL || **hidDeviceInterface == NULL) {
        (*plugInInterface)->Release(plugInInterface);
        return -1;
    }

    (*plugInInterface)->Release(plugInInterface);

    return 0;
}

int mp_input_ar_init(void)
{
    io_iterator_t hidObjectIterator;
    io_object_t hidDevice;
    int i;
    IOHIDElementCookie *cookies = NULL;
    int nr_cookies = 0;

    if (initialized)
        mp_input_ar_close(-1);

    if (floor(NSAppKitVersionNumber) <= 824 /* NSAppKitVersionNumber10_4 */) {
        ar_codes = &ar_codes_tiger[0];
        is_leopard = 0;
    }
    else {
        ar_codes = &ar_codes_leopard[0];
        is_leopard = 1;
    }

    if (FindHIDDevices(kIOMasterPortDefault, &hidObjectIterator))
        return -1;

    // Multiple controls could be found, we only use the first usable one.
    while ((hidDevice = IOIteratorNext(hidObjectIterator))) {
        if (CreateHIDDeviceInterface(hidDevice, &hidDeviceInterface) < 0) {
            hidDeviceInterface = NULL;
            IOObjectRelease(hidDevice);
            continue;
        }
        if (getHIDCookies((IOHIDDeviceInterface122 **)hidDeviceInterface,
                          &cookies,
                          &nr_cookies) < 0) {
            (*hidDeviceInterface)->Release(hidDeviceInterface);
            hidDeviceInterface = NULL;
            IOObjectRelease(hidDevice);
            continue;
        }
        IOObjectRelease(hidDevice);
        break;
    }
    if (hidDeviceInterface == NULL)
        goto mp_input_ar_init_error;

    // Open the device.
    if ((*hidDeviceInterface)->open(hidDeviceInterface,
                              kIOHIDOptionsTypeSeizeDevice) != kIOReturnSuccess)
        goto mp_input_ar_init_error;
    hidDeviceIsOpen = 1;

    if ((queue = (*hidDeviceInterface)->allocQueue(hidDeviceInterface)) == NULL
            || *queue == NULL)
        goto mp_input_ar_init_error;

    // Create the queue.
    (*queue)->create(queue, 0, MAX_QUEUE_SIZE);

    // Add elements to the queue to make the queue work.
    // On tiger, it's a sequence from 1 to 21,
    // maybe it's the range of cookie values.
    for (i = 0;i < nr_cookies;i++)
        (*queue)->addElement(queue, cookies[i], 0);

    // not used anymore
    if (cookies != NULL)
        free(cookies);

    // Start data delivery to the queue.
    (*queue)->start(queue);

    // not useful anymore
    IOObjectRelease(hidObjectIterator);

    initialized = 1;
    return 0;

mp_input_ar_init_error:
    if (cookies != NULL)
        free(cookies);
    if (hidDeviceInterface != NULL) {
        if (*hidDeviceInterface != NULL) {
            (*hidDeviceInterface)->close(hidDeviceInterface);
            (*hidDeviceInterface)->Release(hidDeviceInterface);
        }
        hidDeviceInterface = NULL;
    }
    IOObjectRelease(hidObjectIterator);
    return -1;
}

int is_mplayer_front()
{
    ProcessSerialNumber myProc, frProc;
    Boolean sameProc;
    pid_t parentPID;

    if (GetFrontProcess(&frProc) == noErr
            && GetCurrentProcess(&myProc) == noErr
            && SameProcess(&frProc, &myProc, &sameProc) == noErr) {
        if (sameProc)
            return 1;
        // If MPlayer is running in slave mode, also check parent process.
        if (slave_mode && GetProcessPID(&frProc, &parentPID) == noErr)
            return parentPID==getppid();
    }
    return 0;
}

int mp_input_ar_read(int fd)
{
    int i, down = 0;
    int ret = MP_INPUT_NOTHING;
    AbsoluteTime zeroTime = {0,0};
    IOHIDEventStruct event;
    static int prev_event = 0;
    IOReturn result = kIOReturnSuccess;

    const cookie_keycode_map_t *ar_code;
    int cookie_nr = 0;
    char cookie_queue[MAX_QUEUE_SIZE];
    int value_queue[MAX_QUEUE_SIZE];

    if (initialized == 0)
        return MP_INPUT_NOTHING;

    while ((result = (*queue)->getNextEvent(queue, &event, zeroTime, 0)) == kIOReturnSuccess) {
#ifdef TEST
        printf(" - event cookie: %d, value: %d, long value: %d\n",
              (int)event.elementCookie, (int)event.value, (int)event.longValue);
#endif
        // Shorten cookie sequence by removing cookies value 5 and 18,
        // since 5 always follows 6 (on tiger), 18 follows 19 (on leopard).
        if ((int)event.elementCookie == 5
                || ((int)event.elementCookie == 18 && is_leopard))
            continue;
        // Check valid cookie range.
        if ((int)event.elementCookie > 35 || (int)event.elementCookie < 2) {
            cookie_nr = 0;
            continue;
        }
        cookie_queue[cookie_nr] = (char)(int)event.elementCookie;
        value_queue[cookie_nr++] = event.value;
        // 4 cookies are necessary to make up a valid sequence.
        if (cookie_nr>=4) {
            // Find matching sequence.
            ar_code = ar_codes;
            while (ar_code->cookies != NULL &&
                    (cookie_nr < ar_code->seq_len ||
                     0 != memcmp(ar_code->cookies,
                                 &cookie_queue[cookie_nr-ar_code->seq_len],
                                 ar_code->seq_len)))
                ++ar_code;
            if (ar_code->cookies != NULL) {
                ret = ar_code->keycode;
                switch (ret) {
                    // For these 4 keys, the remote can keep a hold state.
                    case AR_VUP:
                    case AR_VDOWN:
                    case AR_NEXT_HOLD:
                    case AR_PREV_HOLD:
                        for (i = cookie_nr-ar_code->seq_len; i < cookie_nr; ++i) {
                            if (value_queue[i]) {
                                down = MP_KEY_DOWN;
                                break;
                            }
                        }
                        break;
                    default:
                        down = 0;
                }
            }
        }
    }

    if (!is_mplayer_front()) {
        if (hidDeviceIsOpen) {
            (*hidDeviceInterface)->close(hidDeviceInterface);
            hidDeviceIsOpen = 0;

            // Read out all pending events.
            while (result == kIOReturnSuccess)
                result = (*queue)->getNextEvent(queue, &event, zeroTime, 0);
        }
        return MP_INPUT_NOTHING;
    }
    // If we are switched from running as a foreground process to a
    // background process and back again, re-open the device to make
    // sure we are not affected by the system or other applications
    // using the Apple Remote.
    else if (!hidDeviceIsOpen) {
        if ((*hidDeviceInterface)->open(hidDeviceInterface,
                              kIOHIDOptionsTypeSeizeDevice) == kIOReturnSuccess)
            hidDeviceIsOpen = 1;
    }

    if (ret > 0)
        prev_event = ret;
    return ret | down;
}

void mp_input_ar_close(int fd)
{
    if (initialized == 0)
        return;

    // Close the device.
    (*hidDeviceInterface)->close(hidDeviceInterface);

    // Stop data delivery to queue.
    (*queue)->stop(queue);
    // Dispose of queue.
    (*queue)->dispose(queue);
    // Release the queue we allocated.
    (*queue)->Release(queue);

    // Release the interface.
    (*hidDeviceInterface)->Release(hidDeviceInterface);

    initialized = 0;
}

#ifdef TEST
int main(void)
{
    int ret;

    if (mp_input_ar_init() < 0) {
        printf("Unable to initialize Apple Remote.\n");
        return 1;
    }

    while (1) {
        switch ((ret = mp_input_ar_read(0)) & ~MP_KEY_DOWN) {
            case AR_PLAY:       printf(" - AR_PLAY."); break;
            case AR_PLAY_HOLD:  printf(" - AR_PLAY_HOLD."); break;
            case AR_NEXT:       printf(" - AR_NEXT."); break;
            case AR_NEXT_HOLD:  printf(" - AR_NEXT_HOLD."); break;
            case AR_PREV:       printf(" - AR_PREV."); break;
            case AR_PREV_HOLD:  printf(" - AR_PREV_HOLD."); break;
            case AR_MENU:       printf(" - AR_MENU."); break;
            case AR_MENU_HOLD:  printf(" - AR_MENU_HOLD."); break;
            case AR_VUP:        printf(" - AR_VUP."); break;
            case AR_VDOWN:      printf(" - AR_VDOWN."); break;
        }
        if ((ret > 0 )&&(ret & MP_KEY_DOWN))
            printf(" [hold]");
        if (ret > 0)
            printf("\n");
    }

    mp_input_ar_close(0);
    return 0;
}
#endif
