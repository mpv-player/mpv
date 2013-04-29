/*
 * KEY code definitions for keys/events not passed by ASCII value
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

#ifndef MPLAYER_KEYCODES_H
#define MPLAYER_KEYCODES_H

#define MP_KEY_BASE (1<<21)

#define MP_KEY_ENTER 13
#define MP_KEY_TAB 9

/* Control keys */
#define MP_KEY_BACKSPACE (MP_KEY_BASE+0)
#define MP_KEY_DELETE (MP_KEY_BASE+1)
#define MP_KEY_INSERT (MP_KEY_BASE+2)
#define MP_KEY_HOME (MP_KEY_BASE+3)
#define MP_KEY_END (MP_KEY_BASE+4)
#define MP_KEY_PAGE_UP (MP_KEY_BASE+5)
#define MP_KEY_PAGE_DOWN (MP_KEY_BASE+6)
#define MP_KEY_ESC (MP_KEY_BASE+7)
#define MP_KEY_PRINT (MP_KEY_BASE+8)

/* Control keys short name */
#define MP_KEY_BS MP_KEY_BACKSPACE
#define MP_KEY_DEL MP_KEY_DELETE
#define MP_KEY_INS MP_KEY_INSERT
#define MP_KEY_PGUP MP_KEY_PAGE_UP
#define MP_KEY_PGDOWN MP_KEY_PAGE_DOWN
#define MP_KEY_PGDWN MP_KEY_PAGE_DOWN

/* Cursor movement */
#define MP_KEY_CRSR (MP_KEY_BASE+0x10)
#define MP_KEY_RIGHT (MP_KEY_CRSR+0)
#define MP_KEY_LEFT (MP_KEY_CRSR+1)
#define MP_KEY_DOWN (MP_KEY_CRSR+2)
#define MP_KEY_UP (MP_KEY_CRSR+3)

/* Multimedia keyboard/remote keys */
#define MP_KEY_MM_BASE (MP_KEY_BASE+0x20)
#define MP_KEY_POWER (MP_KEY_MM_BASE+0)
#define MP_KEY_MENU (MP_KEY_MM_BASE+1)
#define MP_KEY_PLAY (MP_KEY_MM_BASE+2)
#define MP_KEY_PAUSE (MP_KEY_MM_BASE+3)
#define MP_KEY_PLAYPAUSE (MP_KEY_MM_BASE+4)
#define MP_KEY_STOP (MP_KEY_MM_BASE+5)
#define MP_KEY_FORWARD (MP_KEY_MM_BASE+6)
#define MP_KEY_REWIND (MP_KEY_MM_BASE+7)
#define MP_KEY_NEXT (MP_KEY_MM_BASE+8)
#define MP_KEY_PREV (MP_KEY_MM_BASE+9)
#define MP_KEY_VOLUME_UP (MP_KEY_MM_BASE+10)
#define MP_KEY_VOLUME_DOWN (MP_KEY_MM_BASE+11)
#define MP_KEY_MUTE (MP_KEY_MM_BASE+12)

/*  Function keys  */
#define MP_KEY_F (MP_KEY_BASE+0x40)

/* Keypad keys */
#define MP_KEY_KEYPAD (MP_KEY_BASE+0x60)
#define MP_KEY_KP0 (MP_KEY_KEYPAD+0)
#define MP_KEY_KP1 (MP_KEY_KEYPAD+1)
#define MP_KEY_KP2 (MP_KEY_KEYPAD+2)
#define MP_KEY_KP3 (MP_KEY_KEYPAD+3)
#define MP_KEY_KP4 (MP_KEY_KEYPAD+4)
#define MP_KEY_KP5 (MP_KEY_KEYPAD+5)
#define MP_KEY_KP6 (MP_KEY_KEYPAD+6)
#define MP_KEY_KP7 (MP_KEY_KEYPAD+7)
#define MP_KEY_KP8 (MP_KEY_KEYPAD+8)
#define MP_KEY_KP9 (MP_KEY_KEYPAD+9)
#define MP_KEY_KPDEC (MP_KEY_KEYPAD+10)
#define MP_KEY_KPINS (MP_KEY_KEYPAD+11)
#define MP_KEY_KPDEL (MP_KEY_KEYPAD+12)
#define MP_KEY_KPENTER (MP_KEY_KEYPAD+13)


// Joystick input module
#define MP_JOY_BASE   (MP_KEY_BASE+0x70)
#define MP_JOY_AXIS0_PLUS (MP_JOY_BASE+0)
#define MP_JOY_AXIS0_MINUS (MP_JOY_BASE+1)
#define MP_JOY_AXIS1_PLUS (MP_JOY_BASE+2)
#define MP_JOY_AXIS1_MINUS (MP_JOY_BASE+3)
#define MP_JOY_AXIS2_PLUS (MP_JOY_BASE+4)
#define MP_JOY_AXIS2_MINUS (MP_JOY_BASE+5)
#define MP_JOY_AXIS3_PLUS (MP_JOY_BASE+6)
#define MP_JOY_AXIS3_MINUS (MP_JOY_BASE+7)
#define MP_JOY_AXIS4_PLUS (MP_JOY_BASE+8)
#define MP_JOY_AXIS4_MINUS (MP_JOY_BASE+9)
#define MP_JOY_AXIS5_PLUS (MP_JOY_BASE+10)
#define MP_JOY_AXIS5_MINUS (MP_JOY_BASE+11)
#define MP_JOY_AXIS6_PLUS (MP_JOY_BASE+12)
#define MP_JOY_AXIS6_MINUS (MP_JOY_BASE+13)
#define MP_JOY_AXIS7_PLUS (MP_JOY_BASE+14)
#define MP_JOY_AXIS7_MINUS (MP_JOY_BASE+15)
#define MP_JOY_AXIS8_PLUS (MP_JOY_BASE+16)
#define MP_JOY_AXIS8_MINUS (MP_JOY_BASE+17)
#define MP_JOY_AXIS9_PLUS (MP_JOY_BASE+18)
#define MP_JOY_AXIS9_MINUS (MP_JOY_BASE+19)

#define MP_JOY_BTN_BASE ((MP_KEY_BASE+0x90)|MP_NO_REPEAT_KEY)
#define MP_JOY_BTN0 (MP_JOY_BTN_BASE+0)
#define MP_JOY_BTN1 (MP_JOY_BTN_BASE+1)
#define MP_JOY_BTN2 (MP_JOY_BTN_BASE+2)
#define MP_JOY_BTN3 (MP_JOY_BTN_BASE+3)
#define MP_JOY_BTN4 (MP_JOY_BTN_BASE+4)
#define MP_JOY_BTN5 (MP_JOY_BTN_BASE+5)
#define MP_JOY_BTN6 (MP_JOY_BTN_BASE+6)
#define MP_JOY_BTN7 (MP_JOY_BTN_BASE+7)
#define MP_JOY_BTN8 (MP_JOY_BTN_BASE+8)
#define MP_JOY_BTN9 (MP_JOY_BTN_BASE+9)


// Mouse events from VOs
#define MP_MOUSE_BASE ((MP_KEY_BASE+0xA0)|MP_NO_REPEAT_KEY)
#define MP_MOUSE_BTN0 (MP_MOUSE_BASE+0)
#define MP_MOUSE_BTN1 (MP_MOUSE_BASE+1)
#define MP_MOUSE_BTN2 (MP_MOUSE_BASE+2)
#define MP_MOUSE_BTN3 (MP_MOUSE_BASE+3)
#define MP_MOUSE_BTN4 (MP_MOUSE_BASE+4)
#define MP_MOUSE_BTN5 (MP_MOUSE_BASE+5)
#define MP_MOUSE_BTN6 (MP_MOUSE_BASE+6)
#define MP_MOUSE_BTN7 (MP_MOUSE_BASE+7)
#define MP_MOUSE_BTN8 (MP_MOUSE_BASE+8)
#define MP_MOUSE_BTN9 (MP_MOUSE_BASE+9)
#define MP_MOUSE_BTN10 (MP_MOUSE_BASE+10)
#define MP_MOUSE_BTN11 (MP_MOUSE_BASE+11)
#define MP_MOUSE_BTN12 (MP_MOUSE_BASE+12)
#define MP_MOUSE_BTN13 (MP_MOUSE_BASE+13)
#define MP_MOUSE_BTN14 (MP_MOUSE_BASE+14)
#define MP_MOUSE_BTN15 (MP_MOUSE_BASE+15)
#define MP_MOUSE_BTN16 (MP_MOUSE_BASE+16)
#define MP_MOUSE_BTN17 (MP_MOUSE_BASE+17)
#define MP_MOUSE_BTN18 (MP_MOUSE_BASE+18)
#define MP_MOUSE_BTN19 (MP_MOUSE_BASE+19)
#define MP_MOUSE_BTN_END (MP_MOUSE_BASE+20)

#define MP_MOUSE_BASE_DBL ((MP_KEY_BASE+0xC0)|MP_NO_REPEAT_KEY)
#define MP_MOUSE_BTN0_DBL (MP_MOUSE_BASE_DBL+0)
#define MP_MOUSE_BTN1_DBL (MP_MOUSE_BASE_DBL+1)
#define MP_MOUSE_BTN2_DBL (MP_MOUSE_BASE_DBL+2)
#define MP_MOUSE_BTN3_DBL (MP_MOUSE_BASE_DBL+3)
#define MP_MOUSE_BTN4_DBL (MP_MOUSE_BASE_DBL+4)
#define MP_MOUSE_BTN5_DBL (MP_MOUSE_BASE_DBL+5)
#define MP_MOUSE_BTN6_DBL (MP_MOUSE_BASE_DBL+6)
#define MP_MOUSE_BTN7_DBL (MP_MOUSE_BASE_DBL+7)
#define MP_MOUSE_BTN8_DBL (MP_MOUSE_BASE_DBL+8)
#define MP_MOUSE_BTN9_DBL (MP_MOUSE_BASE_DBL+9)
#define MP_MOUSE_BTN10_DBL (MP_MOUSE_BASE_DBL+10)
#define MP_MOUSE_BTN11_DBL (MP_MOUSE_BASE_DBL+11)
#define MP_MOUSE_BTN12_DBL (MP_MOUSE_BASE_DBL+12)
#define MP_MOUSE_BTN13_DBL (MP_MOUSE_BASE_DBL+13)
#define MP_MOUSE_BTN14_DBL (MP_MOUSE_BASE_DBL+14)
#define MP_MOUSE_BTN15_DBL (MP_MOUSE_BASE_DBL+15)
#define MP_MOUSE_BTN16_DBL (MP_MOUSE_BASE_DBL+16)
#define MP_MOUSE_BTN17_DBL (MP_MOUSE_BASE_DBL+17)
#define MP_MOUSE_BTN18_DBL (MP_MOUSE_BASE_DBL+18)
#define MP_MOUSE_BTN19_DBL (MP_MOUSE_BASE_DBL+19)
#define MP_MOUSE_BTN_DBL_END (MP_MOUSE_BASE_DBL+20)

/* Special keys */
#define MP_KEY_INTERN (MP_KEY_BASE+0x1000)
#define MP_KEY_CLOSE_WIN (MP_KEY_INTERN+0)

/* Modifiers added to individual keys */
#define MP_KEY_MODIFIER_SHIFT  (1<<22)
#define MP_KEY_MODIFIER_CTRL   (1<<23)
#define MP_KEY_MODIFIER_ALT    (1<<24)
#define MP_KEY_MODIFIER_META   (1<<25)

#define MP_KEY_MODIFIER_MASK (MP_KEY_MODIFIER_SHIFT | MP_KEY_MODIFIER_CTRL | \
                              MP_KEY_MODIFIER_ALT | MP_KEY_MODIFIER_META)

// Use this when the key shouldn't be auto-repeated (like mouse buttons)
// This is not a modifier, but is part of the keycode itself.
#define MP_NO_REPEAT_KEY (1<<28)

// Flag for key events. Multiple down events are idempotent. Release keys by
// sending the key code without this flag, or by sending MP_INPUT_RELEASE_ALL
// as key code.
#define MP_KEY_STATE_DOWN (1<<29)

#endif /* MPLAYER_KEYCODES_H */
