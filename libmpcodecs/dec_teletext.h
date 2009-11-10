/*
 * Teletext support
 *
 * Copyright (C) 2007 Vladimir Voroshilov <voroshil@gmail.com>
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

#ifndef MPLAYER_DEC_TELETEXT_H
#define MPLAYER_DEC_TELETEXT_H

struct tt_param {
    char *device;  ///< teletext device
    int format;    ///< teletext display format
    int page;      ///< start teletext page
    int lang;      ///< primary language code
};

#define VBI_CONTROL_FALSE              0
#define VBI_CONTROL_TRUE               1
#define VBI_CONTROL_UNKNOWN           -1

int teletext_control(void* p, int cmd, void *arg);

/*
  TELETEXT controls (through teletext_control() )
   NOTE:
    _SET_ should be _GET_ +1
   _STEP_ should be _GET_ +2
*/
#define TV_VBI_CONTROL_GET_MODE        0x510   ///< get current mode teletext
#define TV_VBI_CONTROL_SET_MODE        0x511   ///< on/off grab teletext

#define TV_VBI_CONTROL_GET_PAGE        0x513   ///< get grabbed teletext page
#define TV_VBI_CONTROL_SET_PAGE        0x514   ///< set grab teletext page number
#define TV_VBI_CONTROL_STEP_PAGE       0x515   ///< step grab teletext page number

#define TV_VBI_CONTROL_GET_SUBPAGE     0x516   ///< get grabbed teletext page
#define TV_VBI_CONTROL_SET_SUBPAGE     0x517   ///< set grab teletext page number

#define TV_VBI_CONTROL_GET_FORMAT      0x519   ///< get teletext format
#define TV_VBI_CONTROL_SET_FORMAT      0x51a   ///< set teletext format

#define TV_VBI_CONTROL_GET_HALF_PAGE   0x51c   ///< get current half page
#define TV_VBI_CONTROL_SET_HALF_PAGE   0x51d   ///< switch half page

#define TV_VBI_CONTROL_IS_CHANGED      0x540   ///< teletext page is changed
#define TV_VBI_CONTROL_MARK_UNCHANGED  0x541   ///< teletext page is changed

#define TV_VBI_CONTROL_ADD_DEC         0x550   ///< add page number with dec
#define TV_VBI_CONTROL_GO_LINK         0x551   ///< go link (1..6) NYI
#define TV_VBI_CONTROL_GET_VBIPAGE     0x552   ///< get vbi_image for grabbed teletext page
#define TV_VBI_CONTROL_RESET           0x553   ///< vbi reset
#define TV_VBI_CONTROL_START           0x554   ///< vbi start
#define TV_VBI_CONTROL_STOP            0x555   ///< vbi stop
#define TV_VBI_CONTROL_DECODE_PAGE     0x556   ///< decode vbi page
#define TV_VBI_CONTROL_GET_NETWORKNAME 0x557   ///< get current network name
#define TV_VBI_CONTROL_DECODE_DVB      0x558   ///< decode DVB teletext

#define VBI_TFORMAT_TEXT    0               ///< text mode
#define VBI_TFORMAT_BW      1               ///< black&white mode
#define VBI_TFORMAT_GRAY    2               ///< grayscale mode
#define VBI_TFORMAT_COLOR   3               ///< color mode (require color_spu patch!)

#define VBI_MAX_PAGES      0x800            ///< max sub pages number
#define VBI_MAX_SUBPAGES   64               ///< max sub pages number

#define VBI_ROWS    25                      ///< teletext page height in rows
#define VBI_COLUMNS 40                      ///< teletext page width in chars
#define VBI_TIME_LINEPOS    26              ///< time line pos in page header

typedef
enum{
    TT_FORMAT_OPAQUE=0,       ///< opaque
    TT_FORMAT_TRANSPARENT,    ///< transparent
    TT_FORMAT_OPAQUE_INV,     ///< opaque with inverted colors
    TT_FORMAT_TRANSPARENT_INV ///< transparent with inverted colors
} teletext_format;

typedef
enum{
    TT_ZOOM_NORMAL=0,
    TT_ZOOM_TOP_HALF,
    TT_ZOOM_BOTTOM_HALF
} teletext_zoom;

typedef struct tt_char_s{
    unsigned int unicode; ///< unicode (utf8) character
    unsigned char fg;  ///< foreground color
    unsigned char bg;  ///< background color
    unsigned char gfx; ///< 0-no gfx, 1-solid gfx, 2-separated gfx
    unsigned char flh; ///< 0-no flash, 1-flash
    unsigned char hidden; ///< char is hidden (for subtitle pages)
    unsigned char ctl; ///< control character
    unsigned char lng; ///< lang: 0-secondary language,1-primary language
    unsigned char raw; ///< raw character (as received from device)
} tt_char;

typedef struct tt_link_s{
    int pagenum;          ///< page number
    int subpagenum;       ///< subpage number
} tt_link_t;

typedef struct tt_page_s{
    int pagenum;          ///< page number
    int subpagenum;       ///< subpage number
    unsigned char primary_lang;   ///< primary language code
    unsigned char secondary_lang; ///< secondary language code
    unsigned char active; ///< page is complete and ready for rendering
    unsigned char flags;  ///< page flags
    unsigned char raw[VBI_ROWS*VBI_COLUMNS]; ///< page data
    struct tt_page_s* next_subpage;
    struct tt_link_s links[6];
}  tt_page;

#define TT_PGFL_SUPPRESS_HEADER  0x01
#define TT_PGFL_UPDATE_INDICATOR 0x02
#define TT_PGFL_INTERRUPTED_SEQ  0x04
#define TT_PGFL_INHIBIT_DISPLAY  0x08
#define TT_PGFL_NEWFLASH         0x10
#define TT_PGFL_SUBTITLE         0x20
#define TT_PGFL_ERASE_PAGE       0x40
#define TT_PGFL_MAGAZINE_SERIAL  0x80

typedef struct tt_stream_props_s{
    int sampling_rate;
    int samples_per_line;
    int offset;
    int count[2];     ///< number of lines in first and second fields
    int interlaced;   ///< vbi data are interlaced
    int bufsize;      ///< required buffer size
} tt_stream_props;

#endif /* MPLAYER_DEC_TELETEXT_H */
