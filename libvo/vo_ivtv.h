/*
 * video output for WinTV PVR-150/250/350 (a.k.a IVTV) cards
 * TV-Out through hardware MPEG decoder
 * Based on some old code from ivtv driver authors.
 * See http://ivtvdriver.org/index.php/Main_Page for more details on the
 * cards supported by the ivtv driver.
 *
 * Copyright (C) 2006 Benjamin Zores
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

#ifndef MPLAYER_VO_IVTV_H
#define MPLAYER_VO_IVTV_H

int ivtv_write(const unsigned char *data, int len);

#endif /* MPLAYER_VO_IVTV_H */
