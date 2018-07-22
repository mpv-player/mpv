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

#ifndef MPV_MACOSX_APPLICATION
#define MPV_MACOSX_APPLICATION

#include "osdep/macosx_menubar.h"

struct macos_opts {
    int macos_title_bar_style;
    int macos_fs_animation_duration;
    int cocoa_cb_sw_renderer;
};

// multithreaded wrapper for mpv_main
int cocoa_main(int argc, char *argv[]);
void cocoa_register_menu_item_action(MPMenuKey key, void* action);

#endif /* MPV_MACOSX_APPLICATION */
