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
#include "core/input/input.h"
#include "core/mp_fifo.h"
// doesn't make much sense, but needed to access keymap functionality
#include "video/out/vo.h"
#include "osdep/macosx_events.h"
#import  "osdep/macosx_application_objc.h"

#define NSLeftAlternateKeyMask  (0x000020 | NSAlternateKeyMask)
#define NSRightAlternateKeyMask (0x000040 | NSAlternateKeyMask)

static bool LeftAltPressed(NSEvent *event)
{
    return ([event modifierFlags] & NSLeftAlternateKeyMask) ==
            NSLeftAlternateKeyMask;
}

static bool RightAltPressed(NSEvent *event)
{
    return ([event modifierFlags] & NSRightAlternateKeyMask) ==
            NSRightAlternateKeyMask;
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

void cocoa_start_apple_remote(void)
{
    Application *app = mpv_shared_app();
    [app.eventsResponder startAppleRemote];
}

void cocoa_stop_apple_remote(void)
{
    Application *app = mpv_shared_app();
    [app.eventsResponder stopAppleRemote];
}

void cocoa_check_events(void)
{
    Application *app = mpv_shared_app();
    int key;
    while ((key = [app.iqueue pop]) >= 0)
        mplayer_put_key(app.keyFIFO, key);
}

void cocoa_put_key(int keycode)
{
    [mpv_shared_app().iqueue push:keycode];
}

@implementation EventsResponder
- (void)startAppleRemote
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self.remote = [[[HIDRemote alloc] init] autorelease];
        if (self.remote) {
            [self.remote setDelegate:self];
            [self.remote startRemoteControl:kHIDRemoteModeExclusiveAuto];
        }
    });

}
- (void)stopAppleRemote
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.remote stopRemoteControl];
    });
}
- (NSArray *) keyEquivalents
{
    return @[@"h", @"q", @"Q", @"0", @"1", @"2"];
}
- (void)handleMediaKey:(int)key
{
    switch (key) {
        case NX_KEYTYPE_PLAY:
            cocoa_put_key(MP_KEY_PLAY);
            break;

        case NX_KEYTYPE_FAST:
            cocoa_put_key(MP_KEY_NEXT);
            break;

        case NX_KEYTYPE_REWIND:
            cocoa_put_key(MP_KEY_PREV);
            break;
    }
}
- (NSEvent*)handleKeyDown:(NSEvent *)event
{
    NSString *chars;

    if (RightAltPressed(event))
        chars = [event characters];
    else
        chars = [event charactersIgnoringModifiers];

    int key = convert_key([event keyCode], *[chars UTF8String]);

    if (key > -1) {
        if ([event modifierFlags] & NSShiftKeyMask)
            key |= MP_KEY_MODIFIER_SHIFT;
        if ([event modifierFlags] & NSControlKeyMask)
            key |= MP_KEY_MODIFIER_CTRL;
        if (LeftAltPressed(event))
            key |= MP_KEY_MODIFIER_ALT;
        if ([event modifierFlags] & NSCommandKeyMask) {
            // propagate the event in case this is a menu key equivalent
            for(NSString *c in [self keyEquivalents])
                if ([chars isEqualToString:c])
                    return event;

            key |= MP_KEY_MODIFIER_META;
        }

        cocoa_put_key(key);
    }

    return nil;
}
- (void)hidRemote:(HIDRemote *)remote
    eventWithButton:(HIDRemoteButtonCode)buttonCode
          isPressed:(BOOL)isPressed
 fromHardwareWithAttributes:(NSMutableDictionary *)attributes
{
    if (!isPressed) return;

    NSDictionary *keymap = @{
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

    int key = [keymap[@(buttonCode)] intValue];
    if (key > 0)
        cocoa_put_key(key);
}
@end
