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

// For appleir.c which includes another header with KEY_ENTER etc defines
#ifndef AR_DEFINES_ONLY

#define KEY_ENTER 13
#define KEY_TAB 9

#define KEY_BASE 0x100

/*  Function keys  */
#define KEY_F (KEY_BASE+64)

/* Control keys */
#define KEY_CTRL (KEY_BASE)
#define KEY_BACKSPACE (KEY_CTRL+0)
#define KEY_DELETE (KEY_CTRL+1)
#define KEY_INSERT (KEY_CTRL+2)
#define KEY_HOME (KEY_CTRL+3)
#define KEY_END (KEY_CTRL+4)
#define KEY_PAGE_UP (KEY_CTRL+5)
#define KEY_PAGE_DOWN (KEY_CTRL+6)
#define KEY_ESC (KEY_CTRL+7)

/* Control keys short name */
#define KEY_BS KEY_BACKSPACE
#define KEY_DEL KEY_DELETE
#define KEY_INS KEY_INSERT
#define KEY_PGUP KEY_PAGE_UP
#define KEY_PGDOWN KEY_PAGE_DOWN
#define KEY_PGDWN KEY_PAGE_DOWN

/* Cursor movement */
#define KEY_CRSR (KEY_BASE+16)
#define KEY_RIGHT (KEY_CRSR+0)
#define KEY_LEFT (KEY_CRSR+1)
#define KEY_DOWN (KEY_CRSR+2)
#define KEY_UP (KEY_CRSR+3)

/* Multimedia keyboard/remote keys */
#define KEY_MM_BASE (0x100+384)
#define KEY_POWER (KEY_MM_BASE+0)
#define KEY_MENU (KEY_MM_BASE+1)
#define KEY_PLAY (KEY_MM_BASE+2)
#define KEY_PAUSE (KEY_MM_BASE+3)
#define KEY_PLAYPAUSE (KEY_MM_BASE+4)
#define KEY_STOP (KEY_MM_BASE+5)
#define KEY_FORWARD (KEY_MM_BASE+6)
#define KEY_REWIND (KEY_MM_BASE+7)
#define KEY_NEXT (KEY_MM_BASE+8)
#define KEY_PREV (KEY_MM_BASE+9)
#define KEY_VOLUME_UP (KEY_MM_BASE+10)
#define KEY_VOLUME_DOWN (KEY_MM_BASE+11)
#define KEY_MUTE (KEY_MM_BASE+12)

/* Keypad keys */
#define KEY_KEYPAD (KEY_BASE+32)
#define KEY_KP0 (KEY_KEYPAD+0)
#define KEY_KP1 (KEY_KEYPAD+1)
#define KEY_KP2 (KEY_KEYPAD+2)
#define KEY_KP3 (KEY_KEYPAD+3)
#define KEY_KP4 (KEY_KEYPAD+4)
#define KEY_KP5 (KEY_KEYPAD+5)
#define KEY_KP6 (KEY_KEYPAD+6)
#define KEY_KP7 (KEY_KEYPAD+7)
#define KEY_KP8 (KEY_KEYPAD+8)
#define KEY_KP9 (KEY_KEYPAD+9)
#define KEY_KPDEC (KEY_KEYPAD+10)
#define KEY_KPINS (KEY_KEYPAD+11)
#define KEY_KPDEL (KEY_KEYPAD+12)
#define KEY_KPENTER (KEY_KEYPAD+13)


// Joystick input module
#define JOY_BASE   (0x100+128)
#define JOY_AXIS0_PLUS (JOY_BASE+0)
#define JOY_AXIS0_MINUS (JOY_BASE+1)
#define JOY_AXIS1_PLUS (JOY_BASE+2)
#define JOY_AXIS1_MINUS (JOY_BASE+3)
#define JOY_AXIS2_PLUS (JOY_BASE+4)
#define JOY_AXIS2_MINUS (JOY_BASE+5)
#define JOY_AXIS3_PLUS (JOY_BASE+6)
#define JOY_AXIS3_MINUS (JOY_BASE+7)
#define JOY_AXIS4_PLUS (JOY_BASE+8)
#define JOY_AXIS4_MINUS (JOY_BASE+9)
#define JOY_AXIS5_PLUS (JOY_BASE+10)
#define JOY_AXIS5_MINUS (JOY_BASE+11)
#define JOY_AXIS6_PLUS (JOY_BASE+12)
#define JOY_AXIS6_MINUS (JOY_BASE+13)
#define JOY_AXIS7_PLUS (JOY_BASE+14)
#define JOY_AXIS7_MINUS (JOY_BASE+15)
#define JOY_AXIS8_PLUS (JOY_BASE+16)
#define JOY_AXIS8_MINUS (JOY_BASE+17)
#define JOY_AXIS9_PLUS (JOY_BASE+18)
#define JOY_AXIS9_MINUS (JOY_BASE+19)

#define JOY_BTN_BASE ((0x100+148)|MP_NO_REPEAT_KEY)
#define JOY_BTN0 (JOY_BTN_BASE+0)
#define JOY_BTN1 (JOY_BTN_BASE+1)
#define JOY_BTN2 (JOY_BTN_BASE+2)
#define JOY_BTN3 (JOY_BTN_BASE+3)
#define JOY_BTN4 (JOY_BTN_BASE+4)
#define JOY_BTN5 (JOY_BTN_BASE+5)
#define JOY_BTN6 (JOY_BTN_BASE+6)
#define JOY_BTN7 (JOY_BTN_BASE+7)
#define JOY_BTN8 (JOY_BTN_BASE+8)
#define JOY_BTN9 (JOY_BTN_BASE+9)


// Mouse events from VOs
#define MOUSE_BASE ((0x100+256)|MP_NO_REPEAT_KEY)
#define MOUSE_BTN0 (MOUSE_BASE+0)
#define MOUSE_BTN1 (MOUSE_BASE+1)
#define MOUSE_BTN2 (MOUSE_BASE+2)
#define MOUSE_BTN3 (MOUSE_BASE+3)
#define MOUSE_BTN4 (MOUSE_BASE+4)
#define MOUSE_BTN5 (MOUSE_BASE+5)
#define MOUSE_BTN6 (MOUSE_BASE+6)
#define MOUSE_BTN7 (MOUSE_BASE+7)
#define MOUSE_BTN8 (MOUSE_BASE+8)
#define MOUSE_BTN9 (MOUSE_BASE+9)
#define MOUSE_BTN10 (MOUSE_BASE+10)
#define MOUSE_BTN11 (MOUSE_BASE+11)
#define MOUSE_BTN12 (MOUSE_BASE+12)
#define MOUSE_BTN13 (MOUSE_BASE+13)
#define MOUSE_BTN14 (MOUSE_BASE+14)
#define MOUSE_BTN15 (MOUSE_BASE+15)
#define MOUSE_BTN16 (MOUSE_BASE+16)
#define MOUSE_BTN17 (MOUSE_BASE+17)
#define MOUSE_BTN18 (MOUSE_BASE+18)
#define MOUSE_BTN19 (MOUSE_BASE+19)
#define MOUSE_BTN_END (MOUSE_BASE+20)

#define MOUSE_BASE_DBL (0x300|MP_NO_REPEAT_KEY)
#define MOUSE_BTN0_DBL (MOUSE_BASE_DBL+0)
#define MOUSE_BTN1_DBL (MOUSE_BASE_DBL+1)
#define MOUSE_BTN2_DBL (MOUSE_BASE_DBL+2)
#define MOUSE_BTN3_DBL (MOUSE_BASE_DBL+3)
#define MOUSE_BTN4_DBL (MOUSE_BASE_DBL+4)
#define MOUSE_BTN5_DBL (MOUSE_BASE_DBL+5)
#define MOUSE_BTN6_DBL (MOUSE_BASE_DBL+6)
#define MOUSE_BTN7_DBL (MOUSE_BASE_DBL+7)
#define MOUSE_BTN8_DBL (MOUSE_BASE_DBL+8)
#define MOUSE_BTN9_DBL (MOUSE_BASE_DBL+9)
#define MOUSE_BTN10_DBL (MOUSE_BASE_DBL+10)
#define MOUSE_BTN11_DBL (MOUSE_BASE_DBL+11)
#define MOUSE_BTN12_DBL (MOUSE_BASE_DBL+12)
#define MOUSE_BTN13_DBL (MOUSE_BASE_DBL+13)
#define MOUSE_BTN14_DBL (MOUSE_BASE_DBL+14)
#define MOUSE_BTN15_DBL (MOUSE_BASE_DBL+15)
#define MOUSE_BTN16_DBL (MOUSE_BASE_DBL+16)
#define MOUSE_BTN17_DBL (MOUSE_BASE_DBL+17)
#define MOUSE_BTN18_DBL (MOUSE_BASE_DBL+18)
#define MOUSE_BTN19_DBL (MOUSE_BASE_DBL+19)
#define MOUSE_BTN_DBL_END (MOUSE_BASE_DBL+20)


#endif // AR_DEFINES_ONLY

// Apple Remote input module
#define AR_BASE      0x500
#define AR_PLAY      (AR_BASE + 0)
#define AR_PLAY_HOLD (AR_BASE + 1)
#define AR_NEXT      (AR_BASE + 2)
#define AR_NEXT_HOLD (AR_BASE + 3)
#define AR_PREV      (AR_BASE + 4)
#define AR_PREV_HOLD (AR_BASE + 5)
#define AR_MENU      (AR_BASE + 6)
#define AR_MENU_HOLD (AR_BASE + 7)
#define AR_VUP       (AR_BASE + 8)
#define AR_VDOWN     (AR_BASE + 9)

#ifndef AR_DEFINES_ONLY


/* Special keys */
#define KEY_INTERN (0x1000)
#define KEY_CLOSE_WIN (KEY_INTERN+0)

/* Modifiers added to individual keys */
#define KEY_MODIFIER_SHIFT  0x2000
#define KEY_MODIFIER_CTRL   0x4000
#define KEY_MODIFIER_ALT    0x8000
#define KEY_MODIFIER_META  0x10000

#endif // AR_DEFINES_ONLY

// Use this when the key shouldn't be auto-repeated (like mouse buttons)
#define MP_NO_REPEAT_KEY (1<<28)

#define MP_KEY_DOWN (1<<29)

#endif /* MPLAYER_KEYCODES_H */
