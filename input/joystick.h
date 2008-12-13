/*
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

#ifndef MPLAYER_JOYSTICK_H
#define MPLAYER_JOYSTICK_H

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

int mp_input_joystick_init(char* dev);

int mp_input_joystick_read(int fd);

#endif /* MPLAYER_JOYSTICK_H */
