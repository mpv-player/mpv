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

#include "config.h"
#include "video/img_format.h"
#include "stdio.h"
#include "compat/mpbswap.h"

#include <string.h>

const char *vo_format_name(int format)
{
    const char *name = mp_imgfmt_to_name(format);
    if (name)
        return name;
    static char unknown_format[20];
    snprintf(unknown_format, 20, "Unknown 0x%04x", format);
    return unknown_format;
}

int mp_get_chroma_shift(int format, int *x_shift, int *y_shift,
                        int *component_bits)
{
    int xs = 0, ys = 0;
    int bpp;
    int err = 0;
    int bits = 8;
    if ((format & 0xff0000f0) == 0x34000050)
        format = bswap_32(format);
    if ((format & 0xf00000ff) == 0x50000034) {
        switch (format >> 24) {
        case 0x50:
            break;
        case 0x51:
            bits = 16;
            break;
        case 0x55:
            bits = 14;
            break;
        case 0x54:
            bits = 12;
            break;
        case 0x52:
            bits = 10;
            break;
        case 0x53:
            bits = 9;
            break;
        default:
            err = 1;
            break;
        }
        switch (format & 0x00ffffff) {
        case 0x00343434: // 444
            xs = 0;
            ys = 0;
            break;
        case 0x00323234: // 422
            xs = 1;
            ys = 0;
            break;
        case 0x00303234: // 420
            xs = 1;
            ys = 1;
            break;
        case 0x00313134: // 411
            xs = 2;
            ys = 0;
            break;
        case 0x00303434: // 440
            xs = 0;
            ys = 1;
            break;
        default:
            err = 1;
            break;
        }
    } else
        switch (format) {
        case IMGFMT_420A:
        case IMGFMT_I420:
        case IMGFMT_IYUV:
        case IMGFMT_YV12:
            xs = 1;
            ys = 1;
            break;
        case IMGFMT_IF09:
        case IMGFMT_YVU9:
            xs = 2;
            ys = 2;
            break;
        case IMGFMT_Y8:
        case IMGFMT_Y800:
            xs = 31;
            ys = 31;
            break;
        case IMGFMT_Y16BE:
        case IMGFMT_Y16LE:
            bits = 16;
            xs = 31;
            ys = 31;
            break;
        default:
            err = 1;
            break;
        }
    if (x_shift)
        *x_shift = xs;
    if (y_shift)
        *y_shift = ys;
    if (component_bits)
        *component_bits = bits;
    bpp = 8 + ((16 >> xs) >> ys);
    if (format == IMGFMT_420A)
        bpp += 8;
    bpp *= (bits + 7) >> 3;
    return err ? 0 : bpp;
}

struct mp_imgfmt_entry mp_imgfmt_list[] = {
    {"444p16le", IMGFMT_444P16_LE},
    {"444p16be", IMGFMT_444P16_BE},
    {"444p14le", IMGFMT_444P14_LE},
    {"444p14be", IMGFMT_444P14_BE},
    {"444p12le", IMGFMT_444P12_LE},
    {"444p12be", IMGFMT_444P12_BE},
    {"444p10le", IMGFMT_444P10_LE},
    {"444p10be", IMGFMT_444P10_BE},
    {"444p9le", IMGFMT_444P9_LE},
    {"444p9be", IMGFMT_444P9_BE},
    {"422p16le", IMGFMT_422P16_LE},
    {"422p16be", IMGFMT_422P16_BE},
    {"422p14le", IMGFMT_422P14_LE},
    {"422p14be", IMGFMT_422P14_BE},
    {"422p12le", IMGFMT_422P12_LE},
    {"422p12be", IMGFMT_422P12_BE},
    {"422p10le", IMGFMT_422P10_LE},
    {"422p10be", IMGFMT_422P10_BE},
    {"422p9le",  IMGFMT_422P9_LE},
    {"422p9be",  IMGFMT_422P9_BE},
    {"420p16le", IMGFMT_420P16_LE},
    {"420p16be", IMGFMT_420P16_BE},
    {"420p14le", IMGFMT_420P14_LE},
    {"420p14be", IMGFMT_420P14_BE},
    {"420p12le", IMGFMT_420P12_LE},
    {"420p12be", IMGFMT_420P12_BE},
    {"420p10le", IMGFMT_420P10_LE},
    {"420p10be", IMGFMT_420P10_BE},
    {"420p9le", IMGFMT_420P9_LE},
    {"420p9be", IMGFMT_420P9_BE},
    {"444p16", IMGFMT_444P16},
    {"444p14", IMGFMT_444P14},
    {"444p12", IMGFMT_444P12},
    {"444p10", IMGFMT_444P10},
    {"444p9", IMGFMT_444P9},
    {"422p16", IMGFMT_422P16},
    {"422p14", IMGFMT_422P14},
    {"422p12", IMGFMT_422P12},
    {"422p10", IMGFMT_422P10},
    {"420p14", IMGFMT_420P14},
    {"420p12", IMGFMT_420P12},
    {"420p10", IMGFMT_420P10},
    {"420p9", IMGFMT_420P9},
    {"420p16", IMGFMT_420P16},
    {"420a", IMGFMT_420A},
    {"444p", IMGFMT_444P},
    {"422p", IMGFMT_422P},
    {"411p", IMGFMT_411P},
    {"440p", IMGFMT_440P},
    {"yuy2", IMGFMT_YUY2},
    {"yvyu", IMGFMT_YVYU},
    {"uyvy", IMGFMT_UYVY},
    {"yvu9", IMGFMT_YVU9},
    {"if09", IMGFMT_IF09},
    {"yv12", IMGFMT_YV12},
    {"i420", IMGFMT_I420},
    {"iyuv", IMGFMT_IYUV},
    {"clpl", IMGFMT_CLPL},
    {"hm12", IMGFMT_HM12},
    {"y800", IMGFMT_Y800},
    {"y8", IMGFMT_Y8},
    {"y16ne", IMGFMT_Y16},
    {"y16le", IMGFMT_Y16LE},
    {"y16be", IMGFMT_Y16BE},
    {"nv12", IMGFMT_NV12},
    {"nv21", IMGFMT_NV21},
    {"bgr24", IMGFMT_BGR24},
    {"bgr32", IMGFMT_BGR32},
    {"bgr16", IMGFMT_BGR16},
    {"bgr15", IMGFMT_BGR15},
    {"bgr12", IMGFMT_BGR12},
    {"bgr8", IMGFMT_BGR8},
    {"bgr4", IMGFMT_BGR4},
    {"bg4b", IMGFMT_BG4B},
    {"bgr1", IMGFMT_BGR1},
    {"rgb48be", IMGFMT_RGB48BE},
    {"rgb48le", IMGFMT_RGB48LE},
    {"rgb48ne", IMGFMT_RGB48NE},
    {"rgb24", IMGFMT_RGB24},
    {"rgb32", IMGFMT_RGB32},
    {"rgb16", IMGFMT_RGB16},
    {"rgb15", IMGFMT_RGB15},
    {"rgb12", IMGFMT_RGB12},
    {"rgb8", IMGFMT_RGB8},
    {"rgb4", IMGFMT_RGB4},
    {"rg4b", IMGFMT_RG4B},
    {"rgb1", IMGFMT_RGB1},
    {"rgba", IMGFMT_RGBA},
    {"argb", IMGFMT_ARGB},
    {"bgra", IMGFMT_BGRA},
    {"abgr", IMGFMT_ABGR},
    {"bgr0", IMGFMT_BGR0},
    {"gbrp", IMGFMT_GBRP},
    {"mjpeg", IMGFMT_MJPEG},
    {"mjpg", IMGFMT_MJPEG},
    {"vdpau_h264", IMGFMT_VDPAU_H264},
    {"vdpau_mpeg1", IMGFMT_VDPAU_MPEG1},
    {"vdpau_mpeg2", IMGFMT_VDPAU_MPEG2},
    {"vdpau_mpeg4", IMGFMT_VDPAU_MPEG4},
    {"vdpau_wmv3", IMGFMT_VDPAU_WMV3},
    {"vdpau_vc1", IMGFMT_VDPAU_VC1},
    {0}
};

unsigned int mp_imgfmt_from_name(bstr name, bool allow_hwaccel)
{
    if (bstr_startswith0(name, "0x")) {
        bstr rest;
        unsigned int fmt = bstrtoll(name, &rest, 16);
        if (rest.len == 0)
            return fmt;
    }
    for(struct mp_imgfmt_entry *p = mp_imgfmt_list; p->name; ++p) {
        if(!bstrcasecmp0(name, p->name)) {
            if (!allow_hwaccel && IMGFMT_IS_HWACCEL(p->fmt))
                return 0;
            return p->fmt;
        }
    }
    return 0;
}

const char *mp_imgfmt_to_name(unsigned int fmt)
{
    struct mp_imgfmt_entry *p = mp_imgfmt_list;
    for(; p->name; ++p) {
        if(p->fmt == fmt)
            return p->name;
    }
    return NULL;
}
