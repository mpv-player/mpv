/*
 * Cocoa OpenGL common backend bits
 *
 * This file is part of mpv.
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

#ifndef MPLAYER_COCOA_COMMON_H
#define MPLAYER_COCOA_COMMON_H

struct vo;

void *vo_cocoa_glgetaddr(const char *s);

void *vo_cocoa_cgl_context(struct vo *vo);
void *vo_cocoa_cgl_pixel_format(struct vo *vo);

#endif
