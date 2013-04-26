/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
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

#ifndef MPV_LIBAV_COMPAT_H
#define MPV_LIBAV_COMPAT_H

#include <libavutil/avutil.h>
#include <libavutil/cpu.h>
#include <libavcodec/version.h>
#include <libavformat/version.h>

#ifndef AV_CPU_FLAG_MMX2
#define AV_CPU_FLAG_MMX2 AV_CPU_FLAG_MMXEXT
#endif

#endif /* MPV_LIBAV_COMPAT_H */
