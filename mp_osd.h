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

#ifndef MPLAYER_MP_OSD_H
#define MPLAYER_MP_OSD_H

#define OSD_MSG_TV_CHANNEL              0
#define OSD_MSG_TEXT                    1
#define OSD_MSG_SUB_DELAY               2
#define OSD_MSG_SPEED                   3
#define OSD_MSG_OSD_STATUS              4
#define OSD_MSG_BAR                     5
#define OSD_MSG_PAUSE                   6
#define OSD_MSG_RADIO_CHANNEL           7
/// Base id for messages generated from the commmand to property bridge.
#define OSD_MSG_PROPERTY                0x100
#define OSD_MSG_SUB_BASE                0x1000

#define MAX_OSD_LEVEL 3
#define MAX_TERM_OSD_LEVEL 1

// These appear in options list
extern int osd_duration;
extern int term_osd;

void set_osd_bar(int type,const char* name,double min,double max,double val);
void set_osd_msg(int id, int level, int time, const char* fmt, ...);
void rm_osd_msg(int id);

#endif /* MPLAYER_MP_OSD_H */
