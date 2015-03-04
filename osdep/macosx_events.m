/*
 * Cocoa Application Event Handling
 *
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

// Carbon header is included but Carbon is NOT linked to mpv's binary. This
// file only needs this include to use the keycode definitions in keymap.
#import <Carbon/Carbon.h>

// Media keys definitions
#import <IOKit/hidsystem/ev_keymap.h>
#import <Cocoa/Cocoa.h>

#include "talloc.h"
#include "input/event.h"
#include "input/input.h"
#include "input/keycodes.h"
// doesn't make much sense, but needed to access keymap functionality
#include "video/out/vo.h"

#include "osdep/macosx_compat.h"
#import "osdep/macosx_events_objc.h"

#include "config.h"

@interface EventsResponder ()
{
    struct input_ctx *_inputContext;
    NSCondition *_input_ready;
    CFMachPortRef _mk_tap_port;
#if HAVE_APPLE_REMOTE
    HIDRemote *_remote;
#endif
}

- (BOOL)handleMediaKey:(NSEvent *)event;
- (NSEvent *)handleKey:(NSEvent *)event;
- (void)startEventMonitor;
- (void)startAppleRemote;
- (void)stopAppleRemote;
- (void)startMediaKeys;
- (void)restartMediaKeys;
- (void)stopMediaKeys;
- (int)mapKeyModifiers:(int)cocoaModifiers;
- (int)keyModifierMask:(NSEvent *)event;
@end


#define NSLeftAlternateKeyMask  (0x000020 | NSAlternateKeyMask)
#define NSRightAlternateKeyMask (0x000040 | NSAlternateKeyMask)

static bool LeftAltPressed(int mask)
{
    return (mask & NSLeftAlternateKeyMask) == NSLeftAlternateKeyMask;
}

static bool RightAltPressed(int mask)
{
    return (mask & NSRightAlternateKeyMask) == NSRightAlternateKeyMask;
}

static const struct mp_keymap keymap[] = {
    // special keys
    {kVK_Return, MP_KEY_ENTER}, {kVK_Escape, MP_KEY_ESC},
    {kVK_Delete, MP_KEY_BACKSPACE}, {kVK_Option, MP_KEY_BACKSPACE},
    {kVK_Control, MP_KEY_BACKSPACE}, {kVK_Shift, MP_KEY_BACKSPACE},
    {kVK_Tab, MP_KEY_TAB},

    // cursor keys
    {kVK_UpArrow, MP_KEY_UP}, {kVK_DownArrow, MP_KEY_DOWN},
    {kVK_LeftArrow, MP_KEY_LEFT}, {kVK_RightArrow, MP_KEY_RIGHT},

    // navigation block
    {kVK_Help, MP_KEY_INSERT}, {kVK_ForwardDelete, MP_KEY_DELETE},
    {kVK_Home, MP_KEY_HOME}, {kVK_End, MP_KEY_END},
    {kVK_PageUp, MP_KEY_PAGE_UP}, {kVK_PageDown, MP_KEY_PAGE_DOWN},

    // F-keys
    {kVK_F1, MP_KEY_F + 1}, {kVK_F2, MP_KEY_F + 2}, {kVK_F3, MP_KEY_F + 3},
    {kVK_F4, MP_KEY_F + 4}, {kVK_F5, MP_KEY_F + 5}, {kVK_F6, MP_KEY_F + 6},
    {kVK_F7, MP_KEY_F + 7}, {kVK_F8, MP_KEY_F + 8}, {kVK_F9, MP_KEY_F + 9},
    {kVK_F10, MP_KEY_F + 10}, {kVK_F11, MP_KEY_F + 11}, {kVK_F12, MP_KEY_F + 12},

    // numpad
    {kVK_ANSI_KeypadPlus, '+'}, {kVK_ANSI_KeypadMinus, '-'},
    {kVK_ANSI_KeypadMultiply, '*'}, {kVK_ANSI_KeypadDivide, '/'},
    {kVK_ANSI_KeypadEnter, MP_KEY_KPENTER},
    {kVK_ANSI_KeypadDecimal, MP_KEY_KPDEC},
    {kVK_ANSI_Keypad0, MP_KEY_KP0}, {kVK_ANSI_Keypad1, MP_KEY_KP1},
    {kVK_ANSI_Keypad2, MP_KEY_KP2}, {kVK_ANSI_Keypad3, MP_KEY_KP3},
    {kVK_ANSI_Keypad4, MP_KEY_KP4}, {kVK_ANSI_Keypad5, MP_KEY_KP5},
    {kVK_ANSI_Keypad6, MP_KEY_KP6}, {kVK_ANSI_Keypad7, MP_KEY_KP7},
    {kVK_ANSI_Keypad8, MP_KEY_KP8}, {kVK_ANSI_Keypad9, MP_KEY_KP9},

    {0, 0}
};

static int convert_key(unsigned key, unsigned charcode)
{
    int mpkey = lookup_keymap_table(keymap, key);
    if (mpkey)
        return mpkey;
    return charcode;
}

void cocoa_start_event_monitor(void)
{
    [[EventsResponder sharedInstance] startEventMonitor];
}

void cocoa_init_apple_remote(void)
{
    [[EventsResponder sharedInstance] startAppleRemote];
}

void cocoa_uninit_apple_remote(void)
{
    [[EventsResponder sharedInstance] stopAppleRemote];
}

static int mk_code(NSEvent *event)
{
    return (([event data1] & 0xFFFF0000) >> 16);
}

static int mk_flags(NSEvent *event)
{
    return ([event data1] & 0x0000FFFF);
}

static  int mk_down(NSEvent *event) {
    return (((mk_flags(event) & 0xFF00) >> 8)) == 0xA;
}

static CGEventRef tap_event_callback(CGEventTapProxy proxy, CGEventType type,
                                     CGEventRef event, void *ctx)
{
    EventsResponder *responder = ctx;

    if (type == kCGEventTapDisabledByTimeout) {
        // The Mach Port receiving the taps became unresponsive for some
        // reason, restart listening on it.
        [responder restartMediaKeys];
        return event;
    }

    if (type == kCGEventTapDisabledByUserInput)
        return event;

    NSEvent *nse = [NSEvent eventWithCGEvent:event];

    if ([nse type] != NSSystemDefined || [nse subtype] != 8)
        // This is not a media key
        return event;

    if (mk_down(nse) && [responder handleMediaKey:nse]) {
        // Handled this event, return nil so that it is removed from the
        // global queue.
        return nil;
    } else {
        // Was a media key but we were not interested in it. Leave it in the
        // global queue by returning the original event.
        return event;
    }
}

void cocoa_init_media_keys(void) {
    [[EventsResponder sharedInstance] startMediaKeys];
}

void cocoa_uninit_media_keys(void) {
    [[EventsResponder sharedInstance] stopMediaKeys];
}

void cocoa_put_key(int keycode)
{
    struct input_ctx *inputContext = [EventsResponder sharedInstance].inputContext;
    if (inputContext)
        mp_input_put_key(inputContext, keycode);
}

void cocoa_put_key_event(void *event)
{
    [[EventsResponder sharedInstance] handleKey:event];
}

void cocoa_put_key_with_modifiers(int keycode, int modifiers)
{
    keycode |= [[EventsResponder sharedInstance] mapKeyModifiers:modifiers];
    cocoa_put_key(keycode);
}

void cocoa_set_input_context(struct input_ctx *input_context)
{
    [EventsResponder sharedInstance].inputContext = input_context;
}

@implementation EventsResponder

+ (EventsResponder *)sharedInstance
{
    static EventsResponder *responder = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        responder = [EventsResponder new];
    });
    return responder;
}

- (id)init
{
    self = [super init];
    if (self) {
        _input_ready = [NSCondition new];
    }
    return self;
}

- (void)waitForInputContext
{
    [_input_ready lock];
    while (!self.inputContext)
        [_input_ready wait];
    [_input_ready unlock];
}

- (void)setInputContext:(struct input_ctx *)ctx;
{
    [_input_ready lock];
    _inputContext = ctx;
    [_input_ready signal];
    [_input_ready unlock];
}

- (struct input_ctx *)inputContext
{
    return _inputContext;
}

- (BOOL)useAltGr
{
    if (self.inputContext)
        return mp_input_use_alt_gr(self.inputContext);
    else
        return YES;
}

- (void)startEventMonitor
{
    [NSEvent addLocalMonitorForEventsMatchingMask:NSKeyDownMask|NSKeyUpMask
                                          handler:^(NSEvent *event) {
        BOOL equivalent = [[NSApp mainMenu] performKeyEquivalent:event];
        if (equivalent) {
            return (NSEvent *)nil;
        } else {
            return [self handleKey:event];
        }
    }];
}

- (void)startAppleRemote
{

#if HAVE_APPLE_REMOTE
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_remote = [[HIDRemote alloc] init];
        if (self->_remote) {
            [self->_remote setDelegate:self];
            [self->_remote startRemoteControl:kHIDRemoteModeExclusiveAuto];
        }
    });
#endif

}
- (void)stopAppleRemote
{
#if HAVE_APPLE_REMOTE
    dispatch_async(dispatch_get_main_queue(), ^{
        [self->_remote stopRemoteControl];
        [self->_remote release];
    });
#endif
}
- (void)restartMediaKeys
{
    CGEventTapEnable(self->_mk_tap_port, true);
}
- (void)startMediaKeys
{
    dispatch_async(dispatch_get_main_queue(), ^{
        // Install a Quartz Event Tap. This will notify mpv through the
        // returned Mach Port and cause mpv to execute the `tap_event_callback`
        // function.
        self->_mk_tap_port = CGEventTapCreate(kCGSessionEventTap,
            kCGHeadInsertEventTap,
            kCGEventTapOptionDefault,
            CGEventMaskBit(NX_SYSDEFINED),
            tap_event_callback,
            self);

        assert(self->_mk_tap_port != nil);

        NSMachPort *port = (NSMachPort *)self->_mk_tap_port;
        [[NSRunLoop mainRunLoop] addPort:port forMode:NSRunLoopCommonModes];
    });
}
- (void)stopMediaKeys
{
    dispatch_async(dispatch_get_main_queue(), ^{
        NSMachPort *port = (NSMachPort *)self->_mk_tap_port;
        [[NSRunLoop mainRunLoop] removePort:port forMode:NSRunLoopCommonModes];
        CFRelease(self->_mk_tap_port);
        self->_mk_tap_port = nil;
    });
}

- (BOOL)handleMediaKey:(NSEvent *)event
{
    NSDictionary *keymapd = @{
        @(NX_KEYTYPE_PLAY):    @(MP_KEY_PLAY),
        @(NX_KEYTYPE_REWIND):  @(MP_KEY_PREV),
        @(NX_KEYTYPE_FAST):    @(MP_KEY_NEXT),
    };

    return [self handleKey:mk_code(event)
                  withMask:[self keyModifierMask:event]
                andMapping:keymapd];
}

- (void)hidRemote:(HIDRemote *)remote
    eventWithButton:(HIDRemoteButtonCode)buttonCode
          isPressed:(BOOL)isPressed
 fromHardwareWithAttributes:(NSMutableDictionary *)attributes
{
    if (!isPressed) return;

    NSDictionary *keymapd = @{
        @(kHIDRemoteButtonCodePlay):       @(MP_AR_PLAY),
        @(kHIDRemoteButtonCodePlayHold):   @(MP_AR_PLAY_HOLD),
        @(kHIDRemoteButtonCodeCenter):     @(MP_AR_CENTER),
        @(kHIDRemoteButtonCodeCenterHold): @(MP_AR_CENTER_HOLD),
        @(kHIDRemoteButtonCodeLeft):       @(MP_AR_PREV),
        @(kHIDRemoteButtonCodeLeftHold):   @(MP_AR_PREV_HOLD),
        @(kHIDRemoteButtonCodeRight):      @(MP_AR_NEXT),
        @(kHIDRemoteButtonCodeRightHold):  @(MP_AR_NEXT_HOLD),
        @(kHIDRemoteButtonCodeMenu):       @(MP_AR_MENU),
        @(kHIDRemoteButtonCodeMenuHold):   @(MP_AR_MENU_HOLD),
        @(kHIDRemoteButtonCodeUp):         @(MP_AR_VUP),
        @(kHIDRemoteButtonCodeUpHold):     @(MP_AR_VUP_HOLD),
        @(kHIDRemoteButtonCodeDown):       @(MP_AR_VDOWN),
        @(kHIDRemoteButtonCodeDownHold):   @(MP_AR_VDOWN_HOLD),
    };

    [self handleKey:buttonCode withMask:0 andMapping:keymapd];
}

- (int)mapKeyModifiers:(int)cocoaModifiers
{
    int mask = 0;
    if (cocoaModifiers & NSShiftKeyMask)
        mask |= MP_KEY_MODIFIER_SHIFT;
    if (cocoaModifiers & NSControlKeyMask)
        mask |= MP_KEY_MODIFIER_CTRL;
    if (LeftAltPressed(cocoaModifiers) ||
        (RightAltPressed(cocoaModifiers) && ![self useAltGr]))
        mask |= MP_KEY_MODIFIER_ALT;
    if (cocoaModifiers & NSCommandKeyMask)
        mask |= MP_KEY_MODIFIER_META;
    return mask;
}

- (int)mapTypeModifiers:(NSEventType)type
{
    NSDictionary *map = @{
        @(NSKeyDown) : @(MP_KEY_STATE_DOWN),
        @(NSKeyUp)   : @(MP_KEY_STATE_UP),
    };
    return [map[@(type)] intValue];
}

- (int)keyModifierMask:(NSEvent *)event
{
    return [self mapKeyModifiers:[event modifierFlags]] |
        [self mapTypeModifiers:[event type]];
}

-(BOOL)handleMPKey:(int)key withMask:(int)mask
{
    if (key > 0) {
        cocoa_put_key(key | mask);
        if (mask & MP_KEY_STATE_UP)
            cocoa_put_key(MP_INPUT_RELEASE_ALL);
        return YES;
    } else {
        return NO;
    }
}

-(BOOL)handleKey:(int)key withMask:(int)mask andMapping:(NSDictionary *)mapping
{
    int mpkey = [mapping[@(key)] intValue];
    return [self handleMPKey:mpkey withMask:mask];
}

- (NSEvent*)handleKey:(NSEvent *)event
{
    if ([event isARepeat]) return nil;

    NSString *chars;

    if ([self useAltGr] && RightAltPressed([event modifierFlags]))
        chars = [event characters];
    else
        chars = [event charactersIgnoringModifiers];

    int key = convert_key([event keyCode], *[chars UTF8String]);

    if (key > -1)
        [self handleMPKey:key withMask:[self keyModifierMask:event]];

    return nil;
}

- (void)handleFilesArray:(NSArray *)files
{
    size_t num_files  = [files count];
    char **files_utf8 = talloc_array(NULL, char*, num_files);
    [files enumerateObjectsUsingBlock:^(NSString *p, NSUInteger i, BOOL *_){
        if ([p hasPrefix:@"file:///.file/id="])
            p = [[NSURL URLWithString:p] path];
        char *filename = (char *)[p UTF8String];
        size_t bytes   = [p lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
        files_utf8[i]  = talloc_memdup(files_utf8, filename, bytes + 1);
    }];
    mp_event_drop_files(_inputContext, num_files, files_utf8);
    talloc_free(files_utf8);
}

@end
