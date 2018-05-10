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

#ifndef MPLAYER_KEYCODES_H
#define MPLAYER_KEYCODES_H

// Keys in the range [0, MP_KEY_BASE) follow unicode.
// Special keys come after this.
#define MP_KEY_BASE (1<<21)

#define MP_KEY_ENTER 13
#define MP_KEY_TAB 9

/* Control keys */
#define MP_KEY_BACKSPACE        (MP_KEY_BASE+0)
#define MP_KEY_DELETE           (MP_KEY_BASE+1)
#define MP_KEY_INSERT           (MP_KEY_BASE+2)
#define MP_KEY_HOME             (MP_KEY_BASE+3)
#define MP_KEY_END              (MP_KEY_BASE+4)
#define MP_KEY_PAGE_UP          (MP_KEY_BASE+5)
#define MP_KEY_PAGE_DOWN        (MP_KEY_BASE+6)
#define MP_KEY_ESC              (MP_KEY_BASE+7)
#define MP_KEY_PRINT            (MP_KEY_BASE+8)

/* Control keys short name */
#define MP_KEY_BS       MP_KEY_BACKSPACE
#define MP_KEY_DEL      MP_KEY_DELETE
#define MP_KEY_INS      MP_KEY_INSERT
#define MP_KEY_PGUP     MP_KEY_PAGE_UP
#define MP_KEY_PGDOWN   MP_KEY_PAGE_DOWN
#define MP_KEY_PGDWN    MP_KEY_PAGE_DOWN

/* Cursor movement */
#define MP_KEY_CRSR     (MP_KEY_BASE+0x10)
#define MP_KEY_RIGHT    (MP_KEY_CRSR+0)
#define MP_KEY_LEFT     (MP_KEY_CRSR+1)
#define MP_KEY_DOWN     (MP_KEY_CRSR+2)
#define MP_KEY_UP       (MP_KEY_CRSR+3)

/* Multimedia/internet keyboard/remote keys */
#define MP_KEY_MM_BASE          (MP_KEY_BASE+0x20)
#define MP_KEY_POWER            (MP_KEY_MM_BASE+0)
#define MP_KEY_MENU             (MP_KEY_MM_BASE+1)
#define MP_KEY_PLAY             (MP_KEY_MM_BASE+2)
#define MP_KEY_PAUSE            (MP_KEY_MM_BASE+3)
#define MP_KEY_PLAYPAUSE        (MP_KEY_MM_BASE+4)
#define MP_KEY_STOP             (MP_KEY_MM_BASE+5)
#define MP_KEY_FORWARD          (MP_KEY_MM_BASE+6)
#define MP_KEY_REWIND           (MP_KEY_MM_BASE+7)
#define MP_KEY_NEXT             (MP_KEY_MM_BASE+8)
#define MP_KEY_PREV             (MP_KEY_MM_BASE+9)
#define MP_KEY_VOLUME_UP        (MP_KEY_MM_BASE+10)
#define MP_KEY_VOLUME_DOWN      (MP_KEY_MM_BASE+11)
#define MP_KEY_MUTE             (MP_KEY_MM_BASE+12)
#define MP_KEY_HOMEPAGE         (MP_KEY_MM_BASE+13)
#define MP_KEY_WWW              (MP_KEY_MM_BASE+14)
#define MP_KEY_MAIL             (MP_KEY_MM_BASE+15)
#define MP_KEY_FAVORITES        (MP_KEY_MM_BASE+16)
#define MP_KEY_SEARCH           (MP_KEY_MM_BASE+17)
#define MP_KEY_SLEEP            (MP_KEY_MM_BASE+18)
#define MP_KEY_CANCEL           (MP_KEY_MM_BASE+19)
#define MP_KEY_RECORD           (MP_KEY_MM_BASE+20)
#define MP_KEY_CHANNEL_UP       (MP_KEY_MM_BASE+21)
#define MP_KEY_CHANNEL_DOWN     (MP_KEY_MM_BASE+22)

/*  Function keys  */
#define MP_KEY_F (MP_KEY_BASE+0x40)

/* Keypad keys */
#define MP_KEY_KEYPAD   (MP_KEY_BASE+0x60)
#define MP_KEY_KP0      (MP_KEY_KEYPAD+0)
#define MP_KEY_KP1      (MP_KEY_KEYPAD+1)
#define MP_KEY_KP2      (MP_KEY_KEYPAD+2)
#define MP_KEY_KP3      (MP_KEY_KEYPAD+3)
#define MP_KEY_KP4      (MP_KEY_KEYPAD+4)
#define MP_KEY_KP5      (MP_KEY_KEYPAD+5)
#define MP_KEY_KP6      (MP_KEY_KEYPAD+6)
#define MP_KEY_KP7      (MP_KEY_KEYPAD+7)
#define MP_KEY_KP8      (MP_KEY_KEYPAD+8)
#define MP_KEY_KP9      (MP_KEY_KEYPAD+9)
#define MP_KEY_KPDEC    (MP_KEY_KEYPAD+10)
#define MP_KEY_KPINS    (MP_KEY_KEYPAD+11)
#define MP_KEY_KPDEL    (MP_KEY_KEYPAD+12)
#define MP_KEY_KPENTER  (MP_KEY_KEYPAD+13)

// Mouse events from VOs
#define MP_MBTN_BASE     ((MP_KEY_BASE+0xA0)|MP_NO_REPEAT_KEY|MP_KEY_EMIT_ON_UP)
#define MP_MBTN_LEFT     (MP_MBTN_BASE+0)
#define MP_MBTN_MID      (MP_MBTN_BASE+1)
#define MP_MBTN_RIGHT    (MP_MBTN_BASE+2)
#define MP_WHEEL_UP      (MP_MBTN_BASE+3)
#define MP_WHEEL_DOWN    (MP_MBTN_BASE+4)
#define MP_WHEEL_LEFT    (MP_MBTN_BASE+5)
#define MP_WHEEL_RIGHT   (MP_MBTN_BASE+6)
#define MP_MBTN_BACK     (MP_MBTN_BASE+7)
#define MP_MBTN_FORWARD  (MP_MBTN_BASE+8)
#define MP_MBTN9         (MP_MBTN_BASE+9)
#define MP_MBTN10        (MP_MBTN_BASE+10)
#define MP_MBTN11        (MP_MBTN_BASE+11)
#define MP_MBTN12        (MP_MBTN_BASE+12)
#define MP_MBTN13        (MP_MBTN_BASE+13)
#define MP_MBTN14        (MP_MBTN_BASE+14)
#define MP_MBTN15        (MP_MBTN_BASE+15)
#define MP_MBTN16        (MP_MBTN_BASE+16)
#define MP_MBTN17        (MP_MBTN_BASE+17)
#define MP_MBTN18        (MP_MBTN_BASE+18)
#define MP_MBTN19        (MP_MBTN_BASE+19)
#define MP_MBTN_END      (MP_MBTN_BASE+20)

#define MP_KEY_IS_MOUSE_BTN_SINGLE(code) \
    ((code) >= MP_MBTN_BASE && (code) < MP_MBTN_END)
#define MP_KEY_IS_WHEEL(code) \
    ((code) >= MP_WHEEL_UP && (code) <= MP_WHEEL_RIGHT)

#define MP_MBTN_DBL_BASE   ((MP_KEY_BASE+0xC0)|MP_NO_REPEAT_KEY)
#define MP_MBTN_LEFT_DBL   (MP_MBTN_DBL_BASE+0)
#define MP_MBTN_MID_DBL    (MP_MBTN_DBL_BASE+1)
#define MP_MBTN_RIGHT_DBL  (MP_MBTN_DBL_BASE+2)
#define MP_MBTN_DBL_END    (MP_MBTN_DBL_BASE+20)

#define MP_KEY_IS_MOUSE_BTN_DBL(code) \
    ((code) >= MP_MBTN_DBL_BASE && (code) < MP_MBTN_DBL_END)

#define MP_KEY_MOUSE_BTN_COUNT (MP_MBTN_END - MP_MBTN_BASE)

// Apple Remote input module
#define MP_AR_BASE        (MP_KEY_BASE+0xE0)
#define MP_AR_PLAY        (MP_AR_BASE + 0)
#define MP_AR_PLAY_HOLD   (MP_AR_BASE + 1)
#define MP_AR_CENTER      (MP_AR_BASE + 2)
#define MP_AR_CENTER_HOLD (MP_AR_BASE + 3)
#define MP_AR_NEXT        (MP_AR_BASE + 4)
#define MP_AR_NEXT_HOLD   (MP_AR_BASE + 5)
#define MP_AR_PREV        (MP_AR_BASE + 6)
#define MP_AR_PREV_HOLD   (MP_AR_BASE + 7)
#define MP_AR_MENU        (MP_AR_BASE + 8)
#define MP_AR_MENU_HOLD   (MP_AR_BASE + 9)
#define MP_AR_VUP         (MP_AR_BASE + 10)
#define MP_AR_VUP_HOLD    (MP_AR_BASE + 11)
#define MP_AR_VDOWN       (MP_AR_BASE + 12)
#define MP_AR_VDOWN_HOLD  (MP_AR_BASE + 13)

// Reserved area. Can be used for keys that have no explicit names assigned,
// but should be mappable by the user anyway.
#define MP_KEY_UNKNOWN_RESERVED_START (MP_KEY_BASE+0x10000)
#define MP_KEY_UNKNOWN_RESERVED_LAST (MP_KEY_BASE+0x20000-1)

/* Special keys */
#define MP_KEY_INTERN (MP_KEY_BASE+0x20000)
#define MP_KEY_CLOSE_WIN (MP_KEY_INTERN+0)
// Generated by input.c (VOs use mp_input_set_mouse_pos())
#define MP_KEY_MOUSE_MOVE ((MP_KEY_INTERN+1)|MP_NO_REPEAT_KEY)
#define MP_KEY_MOUSE_LEAVE ((MP_KEY_INTERN+2)|MP_NO_REPEAT_KEY)
#define MP_KEY_MOUSE_ENTER ((MP_KEY_INTERN+3)|MP_NO_REPEAT_KEY)

#define MP_KEY_IS_MOUSE_CLICK(code) \
    (MP_KEY_IS_MOUSE_BTN_SINGLE(code) || MP_KEY_IS_MOUSE_BTN_DBL(code))

#define MP_KEY_IS_MOUSE_MOVE(code) \
    ((code) == MP_KEY_MOUSE_MOVE || (code) == MP_KEY_MOUSE_ENTER || \
     (code) == MP_KEY_MOUSE_LEAVE)

// Whether to dispatch the key binding by current mouse position.
#define MP_KEY_DEPENDS_ON_MOUSE_POS(code) \
    (MP_KEY_IS_MOUSE_CLICK(code) || (code) == MP_KEY_MOUSE_MOVE)

#define MP_KEY_IS_MOUSE(code) \
    (MP_KEY_IS_MOUSE_CLICK(code) || MP_KEY_IS_MOUSE_MOVE(code))

// No input source should generate this.
#define MP_KEY_UNMAPPED (MP_KEY_INTERN+4)

// Emit a command even on key-up (normally key-up is ignored). This means by
// default they binding will be triggered on key-up instead of key-down.
// This is a fixed part of the keycode, not a modifier than can change.
#define MP_KEY_EMIT_ON_UP      (1u<<22)

// Use this when the key shouldn't be auto-repeated (like mouse buttons)
// Also means both key-down key-up events produce emit bound commands.
// This is a fixed part of the keycode, not a modifier than can change.
#define MP_NO_REPEAT_KEY       (1u<<23)

/* Modifiers added to individual keys */
#define MP_KEY_MODIFIER_SHIFT  (1u<<24)
#define MP_KEY_MODIFIER_CTRL   (1u<<25)
#define MP_KEY_MODIFIER_ALT    (1u<<26)
#define MP_KEY_MODIFIER_META   (1u<<27)

// Flag for key events. Multiple down events are idempotent. Release keys by
// sending the key code with KEY_STATE_UP set, or by sending
// MP_INPUT_RELEASE_ALL as key code.
#define MP_KEY_STATE_DOWN      (1u<<28)

// Flag for key events. Releases a key previously held down with
// MP_KEY_STATE_DOWN. Do not send redundant UP events and do not forget to
// release keys at all with UP. If input is unreliable, use MP_INPUT_RELEASE_ALL
// or don't use MP_KEY_STATE_DOWN in the first place.
#define MP_KEY_STATE_UP        (1u<<29)

#define MP_KEY_MODIFIER_MASK (MP_KEY_MODIFIER_SHIFT | MP_KEY_MODIFIER_CTRL | \
                              MP_KEY_MODIFIER_ALT | MP_KEY_MODIFIER_META | \
                              MP_KEY_STATE_DOWN | MP_KEY_STATE_UP)

// Makes adjustments like turning "shift+z" into "Z"
int mp_normalize_keycode(int keycode);

// Get input key from its name.
int mp_input_get_key_from_name(const char *name);

// Return given key (plus modifiers) as talloc'ed name.
char *mp_input_get_key_name(int key);

// Combination of multiple keys to string.
char *mp_input_get_key_combo_name(const int *keys, int max);

// String containing combination of multiple string to keys.
int mp_input_get_keys_from_string(char *str, int max_num_keys,
                                  int *out_num_keys, int *keys);

struct mp_log;
void mp_print_key_list(struct mp_log *out);

#endif /* MPLAYER_KEYCODES_H */
