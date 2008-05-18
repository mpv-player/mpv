/*
 * Apple Remote input interface
 *
 * Copyright (C) 2007 Zoltan Ponekker <pontscho at kac.poliod.hu>
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

#ifndef MPLAYER_AR_H
#define MPLAYER_AR_H

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

/* MacOSX Driver */
int mp_input_ar_init(void);
int mp_input_ar_read(int fd);
void mp_input_ar_close(int fd);

/* Linux Driver */
int mp_input_appleir_init(char* dev);
int mp_input_appleir_read(int fd);

#endif /* MPLAYER_AR_H */
