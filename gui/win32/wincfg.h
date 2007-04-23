/*
  MPlayer Gui for win32
  Copyright (c) 2003 Sascha Sommer <saschasommer@freenet.de>
  Copyright (c) 2006 Erik Augustson <erik_27can@yahoo.com>
  Copyright (c) 2006 Gianluigi Tiesi <sherpya@netfarm.it>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02111-1307 USA
*/

#ifndef _WINCFG_H
#define _WINCFG_H

extern char *skinName;
extern int sub_window;
extern int console;

extern int gui_save_pos;
extern int gui_main_pos_x;
extern int gui_main_pos_y;
extern int gui_sub_pos_x;
extern int gui_sub_pos_y;

extern int cfg_read(void);
extern int cfg_write(void);

#endif
