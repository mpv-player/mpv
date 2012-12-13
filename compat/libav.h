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

#if (LIBAVUTIL_VERSION_MICRO < 100) || (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 53, 100))
#define AV_CODEC_ID_SUBRIP CODEC_ID_TEXT
#endif

#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(51, 27, 0)
#define av_get_packed_sample_fmt(x) (x)
#endif

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(54, 28, 0)
#define avcodec_free_frame av_freep
#endif

// For Libav 0.9
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(54, 2, 100)
#define AV_DISPOSITION_ATTACHED_PIC      0x0400
#endif

#endif /* MPV_LIBAV_COMPAT_H */
