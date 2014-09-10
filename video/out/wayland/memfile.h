/*
 * This file is part of mpv video player.
 * Copyright © 2014 Alexander Preisinger <alexander.preisinger@gmail.com>
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MPLAYER_WAYLAND_MEMFILE_H
#define MPLAYER_WAYLAND_MEMFILE_H

// create file decsriptor to memory space without filesystem representation
// truncates to size immediately
int memfile_create(off_t size);

#endif /* MPLAYER_WAYLAND_MEMFILE_H */
