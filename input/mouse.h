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

#ifndef MPLAYER_MOUSE_H
#define MPLAYER_MOUSE_H

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

#endif /* MPLAYER_MOUSE_H */
