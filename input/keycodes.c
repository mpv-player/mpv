/*
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

#include <stddef.h>
#include <string.h>
#include <strings.h>

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
  { MP_KEY_KPENTER, "KP_ENTER" },
  { MP_MOUSE_BTN0, "MOUSE_BTN0" },
  { MP_MOUSE_BTN1, "MOUSE_BTN1" },
  { MP_MOUSE_BTN2, "MOUSE_BTN2" },
  { MP_MOUSE_BTN3, "MOUSE_BTN3" },
  { MP_MOUSE_BTN4, "MOUSE_BTN4" },
  { MP_MOUSE_BTN5, "MOUSE_BTN5" },
  { MP_MOUSE_BTN6, "MOUSE_BTN6" },
  { MP_MOUSE_BTN7, "MOUSE_BTN7" },
  { MP_MOUSE_BTN8, "MOUSE_BTN8" },
  { MP_MOUSE_BTN9, "MOUSE_BTN9" },
  { MP_MOUSE_BTN10, "MOUSE_BTN10" },
  { MP_MOUSE_BTN11, "MOUSE_BTN11" },
  { MP_MOUSE_BTN12, "MOUSE_BTN12" },
  { MP_MOUSE_BTN13, "MOUSE_BTN13" },
  { MP_MOUSE_BTN14, "MOUSE_BTN14" },
  { MP_MOUSE_BTN15, "MOUSE_BTN15" },
  { MP_MOUSE_BTN16, "MOUSE_BTN16" },
  { MP_MOUSE_BTN17, "MOUSE_BTN17" },
  { MP_MOUSE_BTN18, "MOUSE_BTN18" },
  { MP_MOUSE_BTN19, "MOUSE_BTN19" },
  { MP_MOUSE_BTN0_DBL, "MOUSE_BTN0_DBL" },
  { MP_MOUSE_BTN1_DBL, "MOUSE_BTN1_DBL" },
  { MP_MOUSE_BTN2_DBL, "MOUSE_BTN2_DBL" },
  { MP_MOUSE_BTN3_DBL, "MOUSE_BTN3_DBL" },
  { MP_MOUSE_BTN4_DBL, "MOUSE_BTN4_DBL" },
  { MP_MOUSE_BTN5_DBL, "MOUSE_BTN5_DBL" },
  { MP_MOUSE_BTN6_DBL, "MOUSE_BTN6_DBL" },
  { MP_MOUSE_BTN7_DBL, "MOUSE_BTN7_DBL" },
  { MP_MOUSE_BTN8_DBL, "MOUSE_BTN8_DBL" },
  { MP_MOUSE_BTN9_DBL, "MOUSE_BTN9_DBL" },
  { MP_MOUSE_BTN10_DBL, "MOUSE_BTN10_DBL" },
  { MP_MOUSE_BTN11_DBL, "MOUSE_BTN11_DBL" },
  { MP_MOUSE_BTN12_DBL, "MOUSE_BTN12_DBL" },
  { MP_MOUSE_BTN13_DBL, "MOUSE_BTN13_DBL" },
  { MP_MOUSE_BTN14_DBL, "MOUSE_BTN14_DBL" },
  { MP_MOUSE_BTN15_DBL, "MOUSE_BTN15_DBL" },
  { MP_MOUSE_BTN16_DBL, "MOUSE_BTN16_DBL" },
  { MP_MOUSE_BTN17_DBL, "MOUSE_BTN17_DBL" },
  { MP_MOUSE_BTN18_DBL, "MOUSE_BTN18_DBL" },
  { MP_MOUSE_BTN19_DBL, "MOUSE_BTN19_DBL" },

  { MP_AR_PLAY,         "AR_PLAY" },
  { MP_AR_PLAY_HOLD,    "AR_PLAY_HOLD" },
  { MP_AR_CENTER,       "AR_CENTER" },
  { MP_AR_CENTER_HOLD,  "AR_CENTER_HOLD" },
  { MP_AR_NEXT,         "AR_NEXT" },
  { MP_AR_NEXT_HOLD,    "AR_NEXT_HOLD" },
  { MP_AR_PREV,         "AR_PREV" },
  { MP_AR_PREV_HOLD,    "AR_PREV_HOLD" },
  { MP_AR_MENU,         "AR_MENU" },
  { MP_AR_MENU_HOLD,    "AR_MENU_HOLD" },
  { MP_AR_VUP,          "AR_VUP" },
  { MP_AR_VUP_HOLD,     "AR_VUP_HOLD" },
  { MP_AR_VDOWN,        "AR_VDOWN" },
  { MP_AR_VDOWN_HOLD,   "AR_VDOWN_HOLD" },

  { MP_AXIS_UP,         "AXIS_UP" },
  { MP_AXIS_DOWN,       "AXIS_DOWN" },
  { MP_AXIS_LEFT,       "AXIS_LEFT" },
  { MP_AXIS_RIGHT,      "AXIS_RIGHT" },

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

  // These are kept for backward compatibility
  { MP_KEY_PAUSE,   "XF86_PAUSE" },
  { MP_KEY_STOP,    "XF86_STOP" },
  { MP_KEY_PREV,    "XF86_PREV" },
  { MP_KEY_NEXT,    "XF86_NEXT" },

  { MP_KEY_CLOSE_WIN,   "CLOSE_WIN" },
  { MP_KEY_MOUSE_MOVE,  "MOUSE_MOVE" },
  { MP_KEY_MOUSE_LEAVE, "MOUSE_LEAVE" },
  { MP_KEY_MOUSE_ENTER, "MOUSE_ENTER" },

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

    if (bstr_startswith0(bname, "0x"))
        return mp_normalize_keycode(strtol(name, NULL, 16) + modifiers);

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

    // printable, and valid unicode range
    if (key >= 32 && key <= 0x10FFFF) {
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
