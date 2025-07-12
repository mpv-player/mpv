/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "misc/bstr.h"
#include "common/common.h"
#include "common/msg.h"

#include "keycodes.h"

struct key_name {
    int key;
    char *name;
};

/// The names of the keys as used in input.conf
/// If you add some new keys, you also need to add them here

static const struct key_name key_names[] = {
    { ' ', "SPACE" },
    { '#', "SHARP" },
    { 0x3000, "IDEOGRAPHIC_SPACE" },
    { MP_KEY_ENTER, "ENTER" },
    { MP_KEY_TAB, "TAB" },
    { MP_KEY_BACKSPACE, "BS" },
    { MP_KEY_DELETE, "DEL" },
    { MP_KEY_INSERT, "INS" },
    { MP_KEY_HOME, "HOME" },
    { MP_KEY_END, "END" },
    { MP_KEY_PAGE_UP, "PGUP" },
    { MP_KEY_PAGE_DOWN, "PGDWN" },
    { MP_KEY_ESC, "ESC" },
    { MP_KEY_PRINT, "PRINT" },
    { MP_KEY_RIGHT, "RIGHT" },
    { MP_KEY_LEFT, "LEFT" },
    { MP_KEY_DOWN, "DOWN" },
    { MP_KEY_UP, "UP" },
    { MP_KEY_F+1, "F1" },
    { MP_KEY_F+2, "F2" },
    { MP_KEY_F+3, "F3" },
    { MP_KEY_F+4, "F4" },
    { MP_KEY_F+5, "F5" },
    { MP_KEY_F+6, "F6" },
    { MP_KEY_F+7, "F7" },
    { MP_KEY_F+8, "F8" },
    { MP_KEY_F+9, "F9" },
    { MP_KEY_F+10, "F10" },
    { MP_KEY_F+11, "F11" },
    { MP_KEY_F+12, "F12" },
    { MP_KEY_F+13, "F13" },
    { MP_KEY_F+14, "F14" },
    { MP_KEY_F+15, "F15" },
    { MP_KEY_F+16, "F16" },
    { MP_KEY_F+17, "F17" },
    { MP_KEY_F+18, "F18" },
    { MP_KEY_F+19, "F19" },
    { MP_KEY_F+20, "F20" },
    { MP_KEY_F+21, "F21" },
    { MP_KEY_F+22, "F22" },
    { MP_KEY_F+23, "F23" },
    { MP_KEY_F+24, "F24" },
    { MP_KEY_KP0, "KP0" },
    { MP_KEY_KP1, "KP1" },
    { MP_KEY_KP2, "KP2" },
    { MP_KEY_KP3, "KP3" },
    { MP_KEY_KP4, "KP4" },
    { MP_KEY_KP5, "KP5" },
    { MP_KEY_KP6, "KP6" },
    { MP_KEY_KP7, "KP7" },
    { MP_KEY_KP8, "KP8" },
    { MP_KEY_KP9, "KP9" },
    { MP_KEY_KPDEL, "KP_DEL" },
    { MP_KEY_KPDEC, "KP_DEC" },
    { MP_KEY_KPINS, "KP_INS" },
    { MP_KEY_KPHOME, "KP_HOME" },
    { MP_KEY_KPEND, "KP_END" },
    { MP_KEY_KPPGUP, "KP_PGUP" },
    { MP_KEY_KPPGDOWN, "KP_PGDWN" },
    { MP_KEY_KPRIGHT, "KP_RIGHT" },
    { MP_KEY_KPBEGIN, "KP_BEGIN" },
    { MP_KEY_KPLEFT, "KP_LEFT" },
    { MP_KEY_KPDOWN, "KP_DOWN" },
    { MP_KEY_KPUP, "KP_UP" },
    { MP_KEY_KPENTER, "KP_ENTER" },
    { MP_KEY_KPADD, "KP_ADD" },
    { MP_KEY_KPSUBTRACT, "KP_SUBTRACT" },
    { MP_KEY_KPMULTIPLY, "KP_MULTIPLY" },
    { MP_KEY_KPDIVIDE, "KP_DIVIDE" },
    { MP_MBTN_LEFT, "MBTN_LEFT" },
    { MP_MBTN_MID, "MBTN_MID" },
    { MP_MBTN_RIGHT, "MBTN_RIGHT" },
    { MP_WHEEL_UP, "WHEEL_UP" },
    { MP_WHEEL_DOWN, "WHEEL_DOWN" },
    { MP_WHEEL_LEFT, "WHEEL_LEFT" },
    { MP_WHEEL_RIGHT, "WHEEL_RIGHT" },
    { MP_MBTN_BACK, "MBTN_BACK" },
    { MP_MBTN_FORWARD, "MBTN_FORWARD" },
    { MP_MBTN9, "MBTN9" },
    { MP_MBTN10, "MBTN10" },
    { MP_MBTN11, "MBTN11" },
    { MP_MBTN12, "MBTN12" },
    { MP_MBTN13, "MBTN13" },
    { MP_MBTN14, "MBTN14" },
    { MP_MBTN15, "MBTN15" },
    { MP_MBTN16, "MBTN16" },
    { MP_MBTN17, "MBTN17" },
    { MP_MBTN18, "MBTN18" },
    { MP_MBTN19, "MBTN19" },
    { MP_MBTN_LEFT_DBL, "MBTN_LEFT_DBL" },
    { MP_MBTN_MID_DBL, "MBTN_MID_DBL" },
    { MP_MBTN_RIGHT_DBL, "MBTN_RIGHT_DBL" },

    { MP_KEY_TABLET_TOOL_TIP, "TABLET_TOOL_TIP" },
    { MP_KEY_TABLET_TOOL_STYLUS_BTN1, "TABLET_TOOL_STYLUS_BTN1" },
    { MP_KEY_TABLET_TOOL_STYLUS_BTN2, "TABLET_TOOL_STYLUS_BTN2" },
    { MP_KEY_TABLET_TOOL_STYLUS_BTN3, "TABLET_TOOL_STYLUS_BTN3" },

    { MP_KEY_GAMEPAD_ACTION_DOWN, "GAMEPAD_ACTION_DOWN" },
    { MP_KEY_GAMEPAD_ACTION_RIGHT, "GAMEPAD_ACTION_RIGHT" },
    { MP_KEY_GAMEPAD_ACTION_LEFT, "GAMEPAD_ACTION_LEFT" },
    { MP_KEY_GAMEPAD_ACTION_UP, "GAMEPAD_ACTION_UP" },
    { MP_KEY_GAMEPAD_BACK, "GAMEPAD_BACK" },
    { MP_KEY_GAMEPAD_MENU, "GAMEPAD_MENU" },
    { MP_KEY_GAMEPAD_START, "GAMEPAD_START" },
    { MP_KEY_GAMEPAD_LEFT_SHOULDER, "GAMEPAD_LEFT_SHOULDER" },
    { MP_KEY_GAMEPAD_RIGHT_SHOULDER, "GAMEPAD_RIGHT_SHOULDER" },
    { MP_KEY_GAMEPAD_LEFT_TRIGGER, "GAMEPAD_LEFT_TRIGGER" },
    { MP_KEY_GAMEPAD_RIGHT_TRIGGER, "GAMEPAD_RIGHT_TRIGGER" },
    { MP_KEY_GAMEPAD_LEFT_STICK, "GAMEPAD_LEFT_STICK" },
    { MP_KEY_GAMEPAD_RIGHT_STICK, "GAMEPAD_RIGHT_STICK" },
    { MP_KEY_GAMEPAD_DPAD_UP, "GAMEPAD_DPAD_UP" },
    { MP_KEY_GAMEPAD_DPAD_DOWN, "GAMEPAD_DPAD_DOWN" },
    { MP_KEY_GAMEPAD_DPAD_LEFT, "GAMEPAD_DPAD_LEFT" },
    { MP_KEY_GAMEPAD_DPAD_RIGHT, "GAMEPAD_DPAD_RIGHT" },
    { MP_KEY_GAMEPAD_LEFT_STICK_UP, "GAMEPAD_LEFT_STICK_UP" },
    { MP_KEY_GAMEPAD_LEFT_STICK_DOWN, "GAMEPAD_LEFT_STICK_DOWN" },
    { MP_KEY_GAMEPAD_LEFT_STICK_LEFT, "GAMEPAD_LEFT_STICK_LEFT" },
    { MP_KEY_GAMEPAD_LEFT_STICK_RIGHT, "GAMEPAD_LEFT_STICK_RIGHT" },
    { MP_KEY_GAMEPAD_RIGHT_STICK_UP, "GAMEPAD_RIGHT_STICK_UP" },
    { MP_KEY_GAMEPAD_RIGHT_STICK_DOWN, "GAMEPAD_RIGHT_STICK_DOWN" },
    { MP_KEY_GAMEPAD_RIGHT_STICK_LEFT, "GAMEPAD_RIGHT_STICK_LEFT" },
    { MP_KEY_GAMEPAD_RIGHT_STICK_RIGHT, "GAMEPAD_RIGHT_STICK_RIGHT" },

    { MP_KEY_POWER,       "POWER" },
    { MP_KEY_MENU,        "MENU" },
    { MP_KEY_PLAY,        "PLAY" },
    { MP_KEY_PAUSE,       "PAUSE" },
    { MP_KEY_PLAYPAUSE,   "PLAYPAUSE" },
    { MP_KEY_STOP,        "STOP" },
    { MP_KEY_FORWARD,     "FORWARD" },
    { MP_KEY_REWIND,      "REWIND" },
    { MP_KEY_NEXT,        "NEXT" },
    { MP_KEY_PREV,        "PREV" },
    { MP_KEY_VOLUME_UP,   "VOLUME_UP" },
    { MP_KEY_VOLUME_DOWN, "VOLUME_DOWN" },
    { MP_KEY_MUTE,        "MUTE" },
    { MP_KEY_HOMEPAGE,    "HOMEPAGE" },
    { MP_KEY_WWW,         "WWW" },
    { MP_KEY_MAIL,        "MAIL" },
    { MP_KEY_FAVORITES,   "FAVORITES" },
    { MP_KEY_SEARCH,      "SEARCH" },
    { MP_KEY_SLEEP,       "SLEEP" },
    { MP_KEY_CANCEL,      "CANCEL" },
    { MP_KEY_RECORD,      "RECORD" },
    { MP_KEY_CHANNEL_UP,  "CHANNEL_UP" },
    { MP_KEY_CHANNEL_DOWN,"CHANNEL_DOWN" },
    { MP_KEY_PLAYONLY,    "PLAYONLY" },
    { MP_KEY_PAUSEONLY,   "PAUSEONLY" },
    { MP_KEY_GO_BACK,     "GO_BACK" },
    { MP_KEY_GO_FORWARD,  "GO_FORWARD" },
    { MP_KEY_TOOLS,       "TOOLS" },
    { MP_KEY_ZOOMIN,      "ZOOMIN" },
    { MP_KEY_ZOOMOUT,     "ZOOMOUT" },

    // These are kept for backward compatibility
    { MP_KEY_PAUSE,   "XF86_PAUSE" },
    { MP_KEY_STOP,    "XF86_STOP" },
    { MP_KEY_PREV,    "XF86_PREV" },
    { MP_KEY_NEXT,    "XF86_NEXT" },

    // Deprecated numeric aliases for the mouse buttons
    { MP_MBTN_LEFT, "MOUSE_BTN0" },
    { MP_MBTN_MID, "MOUSE_BTN1" },
    { MP_MBTN_RIGHT, "MOUSE_BTN2" },
    { MP_WHEEL_UP, "MOUSE_BTN3" },
    { MP_WHEEL_DOWN, "MOUSE_BTN4" },
    { MP_WHEEL_LEFT, "MOUSE_BTN5" },
    { MP_WHEEL_RIGHT, "MOUSE_BTN6" },
    { MP_MBTN_BACK, "MOUSE_BTN7" },
    { MP_MBTN_FORWARD, "MOUSE_BTN8" },
    { MP_MBTN9, "MOUSE_BTN9" },
    { MP_MBTN10, "MOUSE_BTN10" },
    { MP_MBTN11, "MOUSE_BTN11" },
    { MP_MBTN12, "MOUSE_BTN12" },
    { MP_MBTN13, "MOUSE_BTN13" },
    { MP_MBTN14, "MOUSE_BTN14" },
    { MP_MBTN15, "MOUSE_BTN15" },
    { MP_MBTN16, "MOUSE_BTN16" },
    { MP_MBTN17, "MOUSE_BTN17" },
    { MP_MBTN18, "MOUSE_BTN18" },
    { MP_MBTN19, "MOUSE_BTN19" },
    { MP_MBTN_LEFT_DBL, "MOUSE_BTN0_DBL" },
    { MP_MBTN_MID_DBL, "MOUSE_BTN1_DBL" },
    { MP_MBTN_RIGHT_DBL, "MOUSE_BTN2_DBL" },
    { MP_WHEEL_UP, "AXIS_UP" },
    { MP_WHEEL_DOWN, "AXIS_DOWN" },
    { MP_WHEEL_LEFT, "AXIS_LEFT" },
    { MP_WHEEL_RIGHT, "AXIS_RIGHT" },

    { MP_KEY_CLOSE_WIN,   "CLOSE_WIN" },
    { MP_KEY_MOUSE_MOVE,  "MOUSE_MOVE" },
    { MP_KEY_MOUSE_LEAVE, "MOUSE_LEAVE" },
    { MP_KEY_MOUSE_ENTER, "MOUSE_ENTER" },

    { MP_KEY_UNMAPPED,    "UNMAPPED" },
    { MP_KEY_ANY_UNICODE, "ANY_UNICODE" },

    { 0, NULL }
};

static const struct key_name modifier_names[] = {
    { MP_KEY_MODIFIER_SHIFT, "Shift" },
    { MP_KEY_MODIFIER_CTRL,  "Ctrl" },
    { MP_KEY_MODIFIER_ALT,   "Alt" },
    { MP_KEY_MODIFIER_META,  "Meta" },
    { 0 }
};

int mp_input_get_key_from_name(const char *name)
{
    int modifiers = 0;
    const char *p;
    while ((p = strchr(name, '+'))) {
        for (const struct key_name *m = modifier_names; m->name; m++)
            if (!bstrcasecmp(bstr0(m->name),
                             (struct bstr){(char *)name, p - name})) {
                modifiers |= m->key;
                goto found;
            }
        if (!strcmp(name, "+"))
            return '+' + modifiers;
        return -1;
found:
        name = p + 1;
    }

    struct bstr bname = bstr0(name);

    struct bstr rest;
    int code = bstr_decode_utf8(bname, &rest);
    if (code >= 0 && rest.len == 0)
        return mp_normalize_keycode(code + modifiers);

    if (bstr_startswith0(bname, "0x")) {
        char *end;
        long long val = strtoll(name, &end, 16);
        if (name == end || val > INT_MAX || val < INT_MIN)
            return -1;
        long long keycode = val + modifiers;
        if (keycode > INT_MAX || keycode < INT_MIN)
            return -1;
        return mp_normalize_keycode(keycode);
    }

    for (int i = 0; key_names[i].name != NULL; i++) {
        if (strcasecmp(key_names[i].name, name) == 0)
            return mp_normalize_keycode(key_names[i].key + modifiers);
    }

    return -1;
}

static void mp_input_append_key_name(bstr *buf, int key)
{
    for (int i = 0; modifier_names[i].name; i++) {
        if (modifier_names[i].key & key) {
            bstr_xappend_asprintf(NULL, buf, "%s+", modifier_names[i].name);
            key -= modifier_names[i].key;
        }
    }
    for (int i = 0; key_names[i].name != NULL; i++) {
        if (key_names[i].key == key) {
            bstr_xappend_asprintf(NULL, buf, "%s", key_names[i].name);
            return;
        }
    }

    if (MP_KEY_IS_UNICODE(key)) {
        mp_append_utf8_bstr(NULL, buf, key);
        return;
    }

    // Print the hex key code
    bstr_xappend_asprintf(NULL, buf, "0x%x", key);
}

char *mp_input_get_key_name(int key)
{
    bstr dst = {0};
    mp_input_append_key_name(&dst, key);
    return dst.start;
}

char *mp_input_get_key_combo_name(const int *keys, int max)
{
    bstr dst = {0};
    while (max > 0) {
        mp_input_append_key_name(&dst, *keys);
        if (--max && *++keys)
            bstr_xappend(NULL, &dst, bstr0("-"));
        else
            break;
    }
    return dst.start;
}

int mp_input_get_keys_from_string(char *name, int max_num_keys,
                                  int *out_num_keys, int *keys)
{
    char *end, *ptr;
    int n = 0;

    ptr = name;
    n = 0;
    for (end = strchr(ptr, '-'); ; end = strchr(ptr, '-')) {
        if (end && end[1] != '\0') {
            if (end[1] == '-')
                end = &end[1];
            end[0] = '\0';
        }
        keys[n] = mp_input_get_key_from_name(ptr);
        if (keys[n] < 0)
            return 0;
        n++;
        if (end && end[1] != '\0' && n < max_num_keys)
            ptr = &end[1];
        else
            break;
    }
    *out_num_keys = n;
    return 1;
}

void mp_print_key_list(struct mp_log *out)
{
    mp_info(out, "\n");
    for (int i = 0; key_names[i].name != NULL; i++)
        mp_info(out, "%s\n", key_names[i].name);
}

char **mp_get_key_list(void)
{
    char **list = NULL;
    int num = 0;
    for (int i = 0; key_names[i].name != NULL; i++)
        MP_TARRAY_APPEND(NULL, list, num, talloc_strdup(NULL, key_names[i].name));
    MP_TARRAY_APPEND(NULL, list, num, NULL);
    return list;
}

int mp_normalize_keycode(int keycode)
{
    if (keycode <= 0)
        return keycode;
    int code = keycode & ~MP_KEY_MODIFIER_MASK;
    int mod = keycode & MP_KEY_MODIFIER_MASK;
    /* On normal keyboards shift changes the character code of non-special
     * keys, so don't count the modifier separately for those. In other words
     * we want to have "a" and "A" instead of "a" and "Shift+A"; but a separate
     * shift modifier is still kept for special keys like arrow keys. */
    if (code >= 32 && code < MP_KEY_BASE) {
        /* Still try to support ASCII case-modifications properly. For example,
         * we want to change "Shift+a" to "A", not "a". Doing this for unicode
         * in general would require huge lookup tables, or a libc with proper
         * unicode support, so we don't do that. */
        if (code >= 'a' && code <= 'z' && (mod & MP_KEY_MODIFIER_SHIFT))
            code &= 0x5F;
        mod &= ~MP_KEY_MODIFIER_SHIFT;
    }
    return code | mod;
}
