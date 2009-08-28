// only to get keycode definitions from HIToolbox/Events.h
#include "config.h"

#include <Carbon/Carbon.h>
#include "osx_common.h"
#include "video_out.h"
#include "osdep/keycodes.h"

static const struct keymap keymap[] = {
    // special keys
    {0x34, KEY_ENTER}, // Enter key on some iBooks?
    {kVK_Return, KEY_ENTER},
    {kVK_Escape, KEY_ESC},
    {kVK_Delete, KEY_BACKSPACE}, {kVK_Option, KEY_BACKSPACE}, {kVK_Control, KEY_BACKSPACE}, {kVK_Shift, KEY_BACKSPACE},
    {kVK_Tab, KEY_TAB},

    // cursor keys
    {kVK_UpArrow, KEY_UP}, {kVK_DownArrow, KEY_DOWN}, {kVK_LeftArrow, KEY_LEFT}, {kVK_RightArrow, KEY_RIGHT},

    // navigation block
    {kVK_Help, KEY_INSERT}, {kVK_ForwardDelete, KEY_DELETE}, {kVK_Home, KEY_HOME},
    {kVK_End, KEY_END}, {kVK_PageUp, KEY_PAGE_UP}, {kVK_PageUp, KEY_PAGE_DOWN},

    // F-keys
    {kVK_F1, KEY_F + 1}, {kVK_F2, KEY_F + 2}, {kVK_F3, KEY_F + 3}, {kVK_F4, KEY_F + 4},
    {kVK_F5, KEY_F + 5}, {kVK_F6, KEY_F + 6}, {kVK_F7, KEY_F + 7}, {kVK_F8, KEY_F + 8},
    {kVK_F9, KEY_F + 9}, {kVK_F10, KEY_F + 10}, {kVK_F11, KEY_F + 11}, {kVK_F12, KEY_F + 12},

    // numpad
    {kVK_ANSI_KeypadPlus, '+'}, {kVK_ANSI_KeypadMinus, '-'}, {kVK_ANSI_KeypadMultiply, '*'},
    {kVK_ANSI_KeypadDivide, '/'}, {kVK_ANSI_KeypadEnter, KEY_KPENTER}, {kVK_ANSI_KeypadDecimal, KEY_KPDEC},
    {kVK_ANSI_Keypad0, KEY_KP0}, {kVK_ANSI_Keypad1, KEY_KP1}, {kVK_ANSI_Keypad2, KEY_KP2}, {kVK_ANSI_Keypad3, KEY_KP3},
    {kVK_ANSI_Keypad4, KEY_KP4}, {kVK_ANSI_Keypad5, KEY_KP5}, {kVK_ANSI_Keypad6, KEY_KP6}, {kVK_ANSI_Keypad7, KEY_KP7},
    {kVK_ANSI_Keypad8, KEY_KP8}, {kVK_ANSI_Keypad9, KEY_KP9},

    {0, 0}
};

int convert_key(unsigned key, unsigned charcode)
{
    int mpkey = lookup_keymap_table(keymap, key);
    if (mpkey)
        return mpkey;
    return charcode;
}
