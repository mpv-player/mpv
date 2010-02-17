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
 *
 *
 * Based on Attila Otvos' teletext patch, Michael Niedermayer's
 * proof-of-concept teletext capture utility and some parts
 * (decode_raw_line_runin,pll_add,pll_reset) of MythTV project.
 * Code for calculating [soc:eoc] is based on aletv of Edgar Toernig.
 *
 * Teletext system is described in
 * ETS 300 706 "Enhanced Teletext specification" : May 1997
 * http://www.themm.net/~mihu/linux/saa7146/specs/ets_300706e01p.pdf
 *
 * Some implementation details:
 * How to port teletext to another tvi_* driver (see tvi_v4l2.c for example):
 *
 * 1. Implement TVI_CONTROL_VBI_INIT (initialize driver-related vbi subsystem,
 *    start grabbing thread)
 *    input data: vbi device name.
 *    (driver should also call TV_VBI_CONTROL_START for common vbi subsystem initialization
 *    with pointer to initialized tt_stream_properties structure.
 *    After ioctl call variable will contain pointer to initialized priv_vbi_t structure.
 *
 * 2. After receiving next chunk of raw vbi data call TV_VBI_CONTROL_DECODE_PAGE
 *    ioctl with pointer to data buffer
 * 3. pass all other VBI related ioctl cmds to teletext_control routine
 *
 * Page displaying process consist of following stages:
 *
 * ---grabbing stage---
 * 0. stream/tvi_*.c: vbi_grabber(...)
 *      getting vbi data from video device
 * ---decoding stage---
 * 1. libmpcodecs/dec_teletext.c: decode_raw_line_runin(...) or decode_raw_line_sine(...)
 *      decode raw vbi data into sliced 45(?) bytes long packets
 * 2. libmpcodecs/dec_teletext.c: decode_pkt0(...), decode_pkt_page(...)
 *      packets processing (header analyzing, storing complete page in cache,
 *      only raw member of tt_char is filled at this stage)
 * 3. libmpcodecs/dec_teletext.c: decode_page(...)
 *      page decoding. filling unicode,gfx,ctl,etc members of tt_char structure
 *      with appropriate values according to teletext control chars, converting
 *      text to utf8.
 * ---rendering stage---
 * 4. libmpcodecs/dec_teletext.c: prepare_visible_page(...)
 *      processing page. adding number of just received by background process
 *      teletext page, adding current time,etc.
 * 5. libvo/sub.c: vo_update_text_teletext(...)
 *      rendering displayable osd with text and graphics
 *
 * TODO:
 *  v4lv1,bktr support
 *  spu rendering
 *  is better quality on poor signal possible ?
 *  link support
 *  greyscale osd
 *  slave command for dumping pages
 *  fix bcd<->dec as suggested my Michael
 *
 *  BUGS:
 *  wrong colors in debug dump
 *  blinking when visible page was just updated
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>

#ifdef HAVE_PTHREADS
// pthreads are needed for async updates from v4l(2)
// FIXME: try to avoid using pthread calls when running only a single
// thread as e.g. with DVB teletext
#include <pthread.h>
#else
#define pthread_mutex_init(m, p)
#define pthread_mutex_destroy(m)
#define pthread_mutex_lock(m)
#define pthread_mutex_unlock(m)
#endif

#include "dec_teletext.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "libmpcodecs/img_format.h"
#include "libavutil/common.h"
#include "input/input.h"
#include "osdep/timer.h"

//#define DEBUG_DUMP 1

/// page magazine entry structure
typedef struct mag_s{
    tt_page* pt;
    int      order;
} mag_t;

typedef struct {
    int             on;            ///< teletext on/off
    int             pagenum;       ///< seek page number
    int             subpagenum;    ///< seek subpage
    int             curr_pagenum;  ///< current page number
    int             pagenumdec;    ///< set page num with dec

    teletext_format tformat;       ///< see teletext_format enum
    teletext_zoom   zoom;          ///< see teletext_zoom enum
    mag_t*          mag;           ///< pages magazine (has 8 entities)
    int             primary_language;      ///< primary character set
    int             secondary_language;    ///< secondary character set
    /// Currently displayed page (with additional info, e.g current time)
    tt_char         display_page[VBI_ROWS*VBI_COLUMNS];
    /// number of raw bytes between two subsequent encoded bits
    int bpb;
    /// clock run-in sequence will be searched in buffer in [soc:eoc] bytes range
    int soc;
    int eoc;
    /// minimum number of raw vbi bytes wich can be decoded into 8 data bits
    int bp8bl;
    /// maximum number of raw vbi bytes wich can be decoded into 8 data bits
    int bp8bh;

    int pll_adj;
    int pll_dir;
    int pll_cnt;
    int pll_err;
    int pll_lerr;
    int pll_fixed;
    /// vbi stream properties (buffer size,bytes per line, etc)
    tt_stream_props* ptsp;
#ifdef HAVE_PTHREADS
    pthread_mutex_t buffer_mutex;
#endif

    tt_page** ptt_cache;
    unsigned char* ptt_cache_first_subpage;
    /// network info
    unsigned char initialpage;
    unsigned int  initialsubpage;
    unsigned int  networkid;
    int           timeoffset; // timeoffset=realoffset*2
    unsigned int  juliandate;
    unsigned int  universaltime;
    unsigned char networkname[21];
    int           cache_reset;
    /// "page changed" flag: 0-unchanged, 1-entire page, 3-only header
    int           page_changed;
    int           last_rendered;
} priv_vbi_t;

static unsigned char fixParity[256];

static const tt_char tt_space={0x20,7,0,0,0,0,0,0,0x20};
static const tt_char tt_error={'?',1,0,0,0,0,0,0,'?'}; // Red '?' on black background
static double si[12];
static double co[12];

#define VBI_FORMAT(priv) (*(priv->ptsp))

#define FIXP_SH 16
#define ONE_FIXP (1<<FIXP_SH)
#define FIXP2INT(a) ((a)>>FIXP_SH)
#define ANY2FIXP(a) ((int)((a)*ONE_FIXP))

static const unsigned char corrHamm48[256]={
  0x01, 0xff, 0x01, 0x01, 0xff, 0x00, 0x01, 0xff,
  0xff, 0x02, 0x01, 0xff, 0x0a, 0xff, 0xff, 0x07,
  0xff, 0x00, 0x01, 0xff, 0x00, 0x00, 0xff, 0x00,
  0x06, 0xff, 0xff, 0x0b, 0xff, 0x00, 0x03, 0xff,
  0xff, 0x0c, 0x01, 0xff, 0x04, 0xff, 0xff, 0x07,
  0x06, 0xff, 0xff, 0x07, 0xff, 0x07, 0x07, 0x07,
  0x06, 0xff, 0xff, 0x05, 0xff, 0x00, 0x0d, 0xff,
  0x06, 0x06, 0x06, 0xff, 0x06, 0xff, 0xff, 0x07,
  0xff, 0x02, 0x01, 0xff, 0x04, 0xff, 0xff, 0x09,
  0x02, 0x02, 0xff, 0x02, 0xff, 0x02, 0x03, 0xff,
  0x08, 0xff, 0xff, 0x05, 0xff, 0x00, 0x03, 0xff,
  0xff, 0x02, 0x03, 0xff, 0x03, 0xff, 0x03, 0x03,
  0x04, 0xff, 0xff, 0x05, 0x04, 0x04, 0x04, 0xff,
  0xff, 0x02, 0x0f, 0xff, 0x04, 0xff, 0xff, 0x07,
  0xff, 0x05, 0x05, 0x05, 0x04, 0xff, 0xff, 0x05,
  0x06, 0xff, 0xff, 0x05, 0xff, 0x0e, 0x03, 0xff,
  0xff, 0x0c, 0x01, 0xff, 0x0a, 0xff, 0xff, 0x09,
  0x0a, 0xff, 0xff, 0x0b, 0x0a, 0x0a, 0x0a, 0xff,
  0x08, 0xff, 0xff, 0x0b, 0xff, 0x00, 0x0d, 0xff,
  0xff, 0x0b, 0x0b, 0x0b, 0x0a, 0xff, 0xff, 0x0b,
  0x0c, 0x0c, 0xff, 0x0c, 0xff, 0x0c, 0x0d, 0xff,
  0xff, 0x0c, 0x0f, 0xff, 0x0a, 0xff, 0xff, 0x07,
  0xff, 0x0c, 0x0d, 0xff, 0x0d, 0xff, 0x0d, 0x0d,
  0x06, 0xff, 0xff, 0x0b, 0xff, 0x0e, 0x0d, 0xff,
  0x08, 0xff, 0xff, 0x09, 0xff, 0x09, 0x09, 0x09,
  0xff, 0x02, 0x0f, 0xff, 0x0a, 0xff, 0xff, 0x09,
  0x08, 0x08, 0x08, 0xff, 0x08, 0xff, 0xff, 0x09,
  0x08, 0xff, 0xff, 0x0b, 0xff, 0x0e, 0x03, 0xff,
  0xff, 0x0c, 0x0f, 0xff, 0x04, 0xff, 0xff, 0x09,
  0x0f, 0xff, 0x0f, 0x0f, 0xff, 0x0e, 0x0f, 0xff,
  0x08, 0xff, 0xff, 0x05, 0xff, 0x0e, 0x0d, 0xff,
  0xff, 0x0e, 0x0f, 0xff, 0x0e, 0x0e, 0xff, 0x0e };


enum {
  LATIN=0,
  CYRILLIC1,
  CYRILLIC2,
  CYRILLIC3,
  GREEK,
  LANGS
};

// conversion table for chars 0x20-0x7F (UTF8)
// TODO: add another languages
static const unsigned int lang_chars[LANGS][0x60]={
 {
  //Latin
  0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
  0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
  0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
  0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
  0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
  0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
  0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
  0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
  0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
  0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
  0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f
 },
 {
  //Cyrillic-1 (Serbian/Croatian)
  0x20,0x21,0x22,0x23,0x24,0x25,0x044b,0x27,
  0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
  0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
  0x0427,0x0410,0x0411,0x0426,0x0414,0x0415,0x0424,0x0413,
  0x0425,0x0418,0x0408,0x041a,0x041b,0x041c,0x041d,0x041e,
  0x041f,0x040c,0x0420,0x0421,0x0422,0x0423,0x0412,0x0403,
  0x0409,0x040a,0x0417,0x040b,0x0416,0x0402,0x0428,0x040f,
  0x0447,0x0430,0x0431,0x0446,0x0434,0x0435,0x0444,0x0433,
  0x0445,0x0438,0x0428,0x043a,0x043b,0x043c,0x043d,0x043e,
  0x043f,0x042c,0x0440,0x0441,0x0442,0x0443,0x0432,0x0423,
  0x0429,0x042a,0x0437,0x042b,0x0436,0x0422,0x0448,0x042f
 },
 {
  //Cyrillic-2 (Russian/Bulgarian)
  0x20,0x21,0x22,0x23,0x24,0x25,0x044b,0x27,
  0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
  0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
  0x042e,0x0410,0x0411,0x0426,0x0414,0x0415,0x0424,0x0413,
  0x0425,0x0418,0x0419,0x041a,0x041b,0x041c,0x041d,0x041e,
  0x041f,0x042f,0x0420,0x0421,0x0422,0x0423,0x0416,0x0412,
  0x042c,0x042a,0x0417,0x0428,0x042d,0x0429,0x0427,0x042b,
  0x044e,0x0430,0x0431,0x0446,0x0434,0x0435,0x0444,0x0433,
  0x0445,0x0438,0x0439,0x043a,0x043b,0x043c,0x043d,0x043e,
  0x043f,0x044f,0x0440,0x0441,0x0442,0x0443,0x0436,0x0432,
  0x044c,0x044a,0x0437,0x0448,0x044d,0x0449,0x0447,0x044b
 },
 {
  //Cyrillic-3 (Ukrainian)
  0x20,0x21,0x22,0x23,0x24,0x25,0xef,0x27,
  0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
  0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
  0x042e,0x0410,0x0411,0x0426,0x0414,0x0415,0x0424,0x0413,
  0x0425,0x0418,0x0419,0x041a,0x041b,0x041c,0x041d,0x041e,
  0x041f,0x042f,0x0420,0x0421,0x0422,0x0423,0x0416,0x0412,
  0x042c,0x49,0x0417,0x0428,0x042d,0x0429,0x0427,0xcf,
  0x044e,0x0430,0x0431,0x0446,0x0434,0x0435,0x0444,0x0433,
  0x0445,0x0438,0x0439,0x043a,0x043b,0x043c,0x043d,0x043e,
  0x043f,0x044f,0x0440,0x0441,0x0442,0x0443,0x0436,0x0432,
  0x044c,0x69,0x0437,0x0448,0x044d,0x0449,0x0447,0xFF
 },
 {
  //Greek
  0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
  0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
  0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
  0x0390,0x0391,0x0392,0x0393,0x0394,0x0395,0x0396,0x0397,
  0x0398,0x0399,0x039a,0x039b,0x039c,0x039d,0x039e,0x039f,
  0x03a0,0x03a1,0x03a2,0x03a3,0x03a4,0x03a5,0x03a6,0x03a7,
  0x03a8,0x03a9,0x03aa,0x03ab,0x03ac,0x03ad,0x03ae,0x03af,
  0x03b0,0x03b1,0x03b2,0x03b3,0x03b4,0x03b5,0x03b6,0x03b7,
  0x03b8,0x03b9,0x03ba,0x03bb,0x03bc,0x03bd,0x03be,0x03bf,
  0x03c0,0x03c1,0x03c2,0x03c3,0x03c4,0x03c5,0x03c6,0x03c7,
  0x03c8,0x03c9,0x03ca,0x03cb,0x03cc,0x03cd,0x03ce,0x03cf
 }
};

/**
 * Latin National Option Sub-Sets
 * see Table 36 of ETS specification for details.
 *
 * 00:  £ $ @ « ½ » ¬ # ­ ¼ ¦ ¾ ÷  English
 * 01:  é ï à ë ê ù î # è â ô û ç  French
 * 02:  # ¤ É Ä Ö Å Ü _ é ä ö å ü  Swedish/Finnish/Hungarian
 * 03:  # ů č ť ž ý í ř é á ě ú š  Czech/Slovak
 * 04:  # $ § Ä Ö Ü ^ _ ° ä ö ü ß  German
 * 05:  ç $ ¡ á é í ó ú ¿ ü ñ è à  Portuguese/Spanish
 * 06:  £ $ é ° ç » ¬ # ù à ò è ì  Italian
 *
 */
static const unsigned int latin_subchars[8][13]={
  // English
  {0xa3,0x24,0x40,0xab,0xbd,0xbb,0xac,0x23,0xad,0xbc,0xa6,0xbe,0xf7},
  // French
  {0xe9,0xef,0xe0,0xeb,0xea,0xf9,0xee,0x23,0xe8,0xe2,0xf4,0xfb,0xe7},
  // Swedish/Finnish/Hungarian
  {0x23,0xa4,0xc9,0xc4,0xd6,0xc5,0xdc,0x5f,0xe9,0xe4,0xf6,0xe5,0xfc},
  // Czech/Slovak
  {0x23,0x16f,0x10d,0x165,0x17e,0xfd,0xed,0x159,0xe9,0xe1,0x11b,0xfa,0x161},
  // German
  {0x23,0x24,0xa7,0xc4,0xd6,0xdc,0x5e,0x5f,0xb0,0xe4,0xf6,0xfc,0xdf},
  // Portuguese/Spanish
  {0xe7,0x24,0xa1,0xe1,0xe9,0xed,0xf3,0xfa,0xbf,0xfc,0xf1,0xe8,0xe0},
  // Italian
  {0xa3,0x24,0xe9,0xb0,0xe7,0xbb,0xac,0x23,0xf9,0xe0,0xf2,0xe8,0xec},
  // Reserved
  {0x23,0x24,0x40,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,0x7b,0x7c,0x7d,0x7e}
};

/**
 * List of supported languages.
 *
 * lang_code bits for primary Language:
 * bits 7-4 corresponds to bits 14-11 of 28 packet's first triplet
 * bits 3-1 corresponds to bits C12-C14 of packet 0 (lang)
 *
 * lang_code bits for secondary Language:
 * bits 7-5 corresponds to bits 3-1 of 28 packet's second triplet
 * bits 4,2 corresponds to bits 18,16 of 28 packet's first triplet
 * bits 3,1 corresponds to bits 15,17 of 28 packet's first triplet
 *
 * For details see Tables 32 and 33 of specification (subclause 15.2)
 */
struct {
    unsigned char lang_code;
    unsigned char charset;
    const char* lang_name;
} const tt_languages[]=
{
  { 0x01, LATIN,     "French"},
  { 0x02, LATIN,     "Swedish/Finnish/Hungarian"},
  { 0x03, LATIN,     "Czech/Slovak"},
  { 0x04, LATIN,     "German"},
  { 0x05, LATIN,     "Portuguese/Spanish"},
  { 0x06, LATIN,     "Italian"},

  { 0x08, LATIN,     "Polish"},
  { 0x09, LATIN,     "French"},
  { 0x0a, LATIN,     "Swedish/Finnish/Hungarian"},
  { 0x0b, LATIN,     "Czech/Slovak"},
  { 0x0c, LATIN,     "German"},
  { 0x0e, LATIN,     "Italian"},

  { 0x10, LATIN,     "English"},
  { 0x11, LATIN,     "French"},
  { 0x12, LATIN,     "Swedish/Finnish/Hungarian"},
  { 0x13, LATIN,     "Turkish"},
  { 0x14, LATIN,     "German"},
  { 0x15, LATIN,     "Portuguese/Spanish"},
  { 0x16, LATIN,     "Italian"},

  { 0x1d, LATIN,     "Serbian/Croatian/Slovenian (Latin)"},

  { 0x20, CYRILLIC1, "Serbian/Croatian (Cyrillic)"},
  { 0x21, CYRILLIC2, "Russian, Bulgarian"},
  { 0x22, LATIN,     "Estonian"},
  { 0x23, LATIN,     "Czech/Slovak"},
  { 0x24, LATIN,     "German"},
  { 0x25, CYRILLIC3, "Ukrainian"},
  { 0x26, LATIN,     "Lettish/Lithuanian"},

  { 0x33, LATIN,     "Turkish"},
  { 0x37, GREEK,     "Greek"},

  { 0x40, LATIN,     "English"},
  { 0x41, LATIN,     "French"},
//  { 0x47, ARABIC,    "Arabic"},

//  { 0x55, HEBREW,    "Hebrew"},
//  { 0x57, ARABIC,    "Arabic"},

  { 0x00, LATIN,     "English"},
};

/**
 * \brief 24/18 Hamming code decoding
 * \param data bytes with hamming code (array must be at least 3 bytes long)
 * \return -1 if multiple bit error occured, D1-DI data bits - otherwise
 *
 * \note Bits must be correctly ordered, that is for 24/18 (lowest bit first)
 * P1 P2 D1 P3 D2 D3 D4 P4  D5 D6 D7 D8 D9 DA DB P5  DC DD DE DF DG DH DI P6
 */
static int corrHamm24(unsigned char *data){
    unsigned char syndrom=0;
    int cw=data[0] | (data[1]<<8) | (data[2]<<16);
    int i;

    for(i=0;i<23;i++)
        syndrom^=((cw>>i)&1)*(i+33);

    syndrom^=(cw>>11)&32;

    if(syndrom&31){
        if(syndrom < 32 || syndrom > 55)
            return -1;
        cw ^= 1<<((syndrom&31)-1);
   }

   return (cw&4)>>2 |
          (cw&0x70)>>3 |
          (cw&0x3f00)>>4 |
          (cw&0x3f0000)>>5;
}

/**
 * \brief converts language bits to charset index
 * \param lang language bits
 * \return charset index in lang_chars array
 */
static int lang2charset (int lang){
    int i;
    for(i=0;tt_languages[i].lang_code;i++)
        if(tt_languages[i].lang_code==lang)
            break;

    return tt_languages[i].charset;
}

/**
 * \brief convert chars from curent teletext codepage into MPlayer charset
 * \param p raw teletext char to decode
 * \param charset index on lang_chars
 * \param lang index in substitution array (latin charset only)
 * \return UTF8 char
 *
 * \remarks
 * routine will analyze raw member of given tt_char structure and
 * fill unicode member of the same struct with appropriate utf8 code.
 */
static unsigned int conv2uni(unsigned int p,int charset,int lang)
{

    if(p<0x80 && p>=0x20){
        if(charset==LATIN){
            lang&=7;
            if (p>=0x23 && p<=0x24){
                return latin_subchars[lang][p-0x23];
            }else if (p==0x40){
                return latin_subchars[lang][2];
            }else if (p>=0x5b && p<=0x60){
                return latin_subchars[lang][p-0x5b+3];
            }else if (p>=0x7b && p<=0x7e){
                return latin_subchars[lang][p-0x7b+9];
            }
        }
        return lang_chars[charset][p-0x20];
    }else
        return 0x20;
}

static void init_vbi_consts(priv_vbi_t* priv){
    int i,j;
    double ang;
    for(i=0; i<256; i++){
        j=i&0x7F;
        j^= j+j;
        j^= j<<2;
        j^= j<<4;
        fixParity[i]= i ^ (j&0x80) ^ 0x80;
    }

    for(i=0,ang=0; i<12; i++,ang+=M_PI/priv->bpb){
        si[i]= sin(ang);
        co[i]= cos(ang);
    }

    priv->bpb=(priv->ptsp->sampling_rate/6937500.0)*ONE_FIXP+0.5;
    priv->soc=FFMAX(9.2e-6*priv->ptsp->sampling_rate-priv->ptsp->offset, 0);
    priv->eoc=FFMIN(12.9e-6*priv->ptsp->sampling_rate-priv->ptsp->offset,
                    priv->ptsp->samples_per_line-43*8*priv->bpb/ONE_FIXP);
    if (priv->eoc - priv->soc<16*priv->bpb/ONE_FIXP){ // invalid [soc:eoc]
        priv->soc=0;
        priv->eoc=92;
    }
    priv->bp8bl=0.97*8*priv->bpb/ONE_FIXP; // -3% tolerance
    priv->bp8bh=1.03*8*priv->bpb/ONE_FIXP; // +3% tolerance
}
/**
 * \brief calculate increased/decreased by given value page number
 * \param curr  current page number in hexadecimal for
 * \param direction decimal value (can be negative) to add to value
 *        of curr parameter
 * \return new page number in hexadecimal form
 *
 * VBI page numbers are represented in special hexadecimal form, e.g.
 * page with number 123 (as seen by user) internally has number 0x123.
 * and equation 0x123+8 should be equal to 0x131 instead of regular 0x12b.
 *
 *
 * Page numbers 0xYYY (where Y is not belongs to (0..9).
 * Page number belongs to [0x000,0x799] or [0x100:0x899] (first 0 can be
 * treated as '8')
 */
static int steppage(int p, int direction, int skip_hidden)
{
    if(skip_hidden)
        p=(p&15)+((p>>4)&15)*10+(p>>8)*100;
    p+=direction;
    if(skip_hidden){
        p=(p+800)%800;
        p=(p%10)+((p/10)%10)*16+(p/100)*256;
    }

    return p&0x7ff;
}

/*
------------------------------------------------------------------
   Cache stuff
------------------------------------------------------------------
*/

/**
 * \brief add/update entry in cache
 * \param priv private data structure
 * \param pg page to store in cache
 * \param line line to update (value below 0 means update entire page)
 */
static void put_to_cache(priv_vbi_t* priv,tt_page* pg,int line){
    tt_page* pgc; //page in cache
    int i,j,count;

    if(line<0){
        i=0;
        count=VBI_ROWS*VBI_COLUMNS;
    }else if(line<VBI_ROWS){
        i=line*VBI_COLUMNS;
        count=(line+1)*VBI_COLUMNS;
    }else
        return;

    pthread_mutex_lock(&(priv->buffer_mutex));

    if(!priv->ptt_cache[pg->pagenum]){
        priv->ptt_cache[pg->pagenum]=calloc(1,sizeof(tt_page));
        pgc=priv->ptt_cache[pg->pagenum];
    }else{
        pgc=priv->ptt_cache[pg->pagenum];
        while(pgc->next_subpage && pgc->subpagenum!=pg->subpagenum)
            pgc=pgc->next_subpage;

        if(pgc->subpagenum!=pg->subpagenum){
            pgc->next_subpage=calloc(1,sizeof(tt_page));
            pgc=pgc->next_subpage;
        }
    }
    pgc->pagenum=pg->pagenum;
    pgc->subpagenum=pg->subpagenum;
    pgc->primary_lang=pg->primary_lang;
    pgc->secondary_lang=pg->secondary_lang;
    pgc->flags=pg->flags;
    for(j=0;j<6;++j)
        pgc->links[j]=pg->links[j];
    //instead of copying entire page into cache, copy only undamaged
    //symbols into cache
    for(;i<count;i++){
        if(!(pg->raw[i]&0x80))
            pgc->raw[i]=pg->raw[i];
        else
            mp_msg(MSGT_TELETEXT,MSGL_DBG3,"char error. pg:%x, c[%d]=0x%x\n",
                pg->pagenum,i,pg->raw[i]);
    }
    pgc->active=1;
    pthread_mutex_unlock(&(priv->buffer_mutex));
}

/**
 * \brief get any subpage number of given page
 * \param priv private data structure
 * \param pagenum page number to search subpages in
 *
 * \return subpage number of first found subpage which belongs to
 * given page number
 *
 * \note page itself is subpage too (and usually has subpage number 0)
 */
static inline int get_subpagenum_from_cache(priv_vbi_t* priv, int pagenum){
    if (!priv->ptt_cache[pagenum])
        return 0x3f7f;
    else
        return priv->ptt_cache[pagenum]->subpagenum;
}

/**
 * \brief get page from cache by it page and subpage number
 * \param priv private data structure
 * \param pagenum page number
 * \param subpagenum subpage number
 *
 * \return pointer to tt_page structure if requested page is found
 * and NULL otherwise
 */
static inline tt_page* get_from_cache(priv_vbi_t* priv, int pagenum,int subpagenum){
    tt_page* tp=priv->ptt_cache[pagenum];

    while(tp && tp->subpagenum!=subpagenum)
        tp=tp->next_subpage;
    return tp;
}

/**
 * \brief clears cache
 * \param priv private data structure
 *
 * Deletes all tt_page structures from cache and frees allocated memory.
 * Only zero-filled array of pointers remains in memory
 */
static void clear_cache(priv_vbi_t* priv){
    int i;
    tt_page* tp;

    /*
      Skip next 5 buffers to avoid mixing teletext pages from different
      channels during channel switch
    */
    priv->cache_reset=5;
    for(i=0;i<VBI_MAX_PAGES;i++){
        while(priv->ptt_cache[i]){
            tp=priv->ptt_cache[i];
            priv->ptt_cache[i]=tp->next_subpage;
            free(tp);
        }
    }
    priv->initialsubpage=priv->networkid=0;
    priv->timeoffset=0;
    priv->juliandate=priv->universaltime=0;
    memset(priv->networkname,0,21);
}

/**
 * \brief cache initialization
 * \param priv private data structure
 *
 * \note Has to be called before any cache operations!
 */
static void init_cache(priv_vbi_t* priv){
    priv->ptt_cache=calloc(VBI_MAX_PAGES,sizeof(tt_page*));
}

/**
 * \brief destroys cache
 * \param priv private data structure
 *
 * Frees all memory allocated for cache (including array of pointers).
 * It is safe to call this routine multiple times
 */
static void destroy_cache(priv_vbi_t* priv){
    if(priv->ptt_cache){
        clear_cache(priv);
        free(priv->ptt_cache);
        priv->ptt_cache=NULL;
    }
}

/*
------------------------------------------------------------------
   Decoder stuff
------------------------------------------------------------------
*/
/**
 * \brief converts raw teletext page into useful format (1st rendering stage)
 * \param pg page to decode
 * \param raw raw data to decode page from
 * \param primary_lang primary language code
 * \param secondary_lang secondary language code
*
 * Routine fills tt_char structure of each teletext_page character with proper
 * info about foreground and background colors, character
 * type (graphics/control/text).
 */
static void decode_page(tt_char* p,unsigned char* raw,int primary_lang,int secondary_lang,int flags)
{
    int row,col;
    int prim_charset=lang2charset(primary_lang);
    int sec_charset=lang2charset(secondary_lang);

    for(row=0;row<VBI_ROWS;row++)   {
        int prim_lang=1;
        int gfx=0;
        int fg_color=7;
        int bg_color=0;
        int separated=0;
        int conceal=0;
        int hold=0;
        int flash=0;
        int box=0;

        tt_char tt_held=tt_space;
        for(col=0;col<VBI_COLUMNS;col++){
            int i=row*VBI_COLUMNS+col;
            int c=raw[i];
            p[i].raw=c;
            if(c&0x80){ //damaged char
                p[i]=tt_error;
                continue;
            }
            if((flags&TT_PGFL_SUBTITLE) || (flags&TT_PGFL_NEWFLASH))
                p[i].hidden=!box;
            else
                p[i].hidden=0;
            p[i].gfx=gfx?(separated?2:1):0;
            p[i].lng=prim_lang;
            p[i].ctl=(c&0x60)==0?1:0;
            p[i].fg=fg_color;
            p[i].bg=bg_color;
            p[i].flh=flash;

            if ((c&0x60)==0){ //control chars
                if(c>=0x08 && c<=0x09){//Flash/Steady
                    flash=c==0x08;
                    p[i].flh=flash;
                    if(c==0x09){
                        p[i].fg=fg_color;
                        p[i].bg=bg_color;
                    }
                }else if(c>=0x0a && c<=0x0b){
                    box=c&1;
                }else if(c>=0x0c && c<=0x0f){
                }else if (c<=0x17){ //colors
                    fg_color=c&0x0f;
                    gfx=c>>4;
                    conceal=0;
                    if(!gfx) hold=0;
                }else if (c<=0x18){
                    conceal=1;
                }else if (c<=0x1a){ //Contiguous/Separated gfx
                    separated=!(c&1);
                }else if (c<=0x1b){
                    prim_lang=!prim_lang;
                }else if (c<=0x1d){
                    bg_color=(c&1)?fg_color:0;
                    p[i].bg=bg_color;
                }else{ //Hold/Release Graphics
                    hold=!(c&1);
                }
                p[i].ctl=1;
                if(hold || c==0x1f){
                    p[i]=tt_held;
                    p[i].fg=fg_color;
                    p[i].bg=bg_color;
                }else
                    p[i].unicode=p[i].gfx?0:' ';
                continue;
            }

            if(conceal){
                p[i].gfx=0;
                p[i].unicode=' ';
            }else if(gfx){
                p[i].unicode=c-0x20;
                if (p[i].unicode>0x3f) p[i].unicode-=0x20;
                tt_held=p[i];
            }else{
                if(p[i].lng){
                    p[i].unicode=conv2uni(c,prim_charset,primary_lang&7);
                }else{
                    p[i].unicode=conv2uni(c,sec_charset,secondary_lang&7);
                }
            }
            p[i].fg=fg_color;
            p[i].bg=bg_color;
        }
    }
}

/**
 * \brief prepares current page for displaying
 * \param priv_vbi private data structure
 *
 * Routine adds some useful info (time and page number of page, grabbed by
 * background thread to top line of current page). Displays "No teletext"
 * string if no vbi data available.
 */
#define PRINT_HEX(dp,i,h) dp[i].unicode=((h)&0xf)>9?'A'+((h)&0xf)-10:'0'+((h)&0xf)
static void prepare_visible_page(priv_vbi_t* priv){
    tt_page *pg,*curr_pg;
    unsigned char *p;
    int i;

    pthread_mutex_lock(&(priv->buffer_mutex));
    mp_msg(MSGT_TELETEXT,MSGL_DBG3,"dec_teletext: prepare_visible_page pg:0x%x, sub:0x%x\n",
        priv->pagenum,priv->subpagenum);
    if(priv->subpagenum==0x3f7f) //no page yet
        priv->subpagenum=get_subpagenum_from_cache(priv,priv->pagenum);

    pg=get_from_cache(priv,priv->pagenum,priv->subpagenum);
    mp_dbg(MSGT_TELETEXT,MSGL_DBG3,"dec_teletext: prepare_vibible_page2 pg:0x%x, sub:0x%x\n",
        priv->pagenum,priv->subpagenum);

    curr_pg=get_from_cache(priv,priv->curr_pagenum,
        get_subpagenum_from_cache(priv,priv->curr_pagenum));
    if (!pg && !curr_pg){
        p=MSGTR_TV_NoTeletext;
        for(i=0;i<VBI_COLUMNS && *p;i++){
            GET_UTF8(priv->display_page[i].unicode,*p++,break;);
        }
        for(;i<VBI_ROWS*VBI_COLUMNS;i++)
            priv->display_page[i]=tt_space;
        pthread_mutex_unlock(&(priv->buffer_mutex));
        return;
    }

    if (!pg || !pg->active){
        for(i=0;i<VBI_ROWS*VBI_COLUMNS;i++){
            priv->display_page[i]=tt_space;
        }
    }else{
        decode_page(priv->display_page,pg->raw,pg->primary_lang,pg->secondary_lang,pg->flags);
        mp_msg(MSGT_TELETEXT,MSGL_DBG3,"page #%x was decoded!\n",pg->pagenum);
    }

    PRINT_HEX(priv->display_page,0,(priv->curr_pagenum&0x700)?priv->curr_pagenum>>8:8);
    PRINT_HEX(priv->display_page,1,priv->curr_pagenum>>4);
    PRINT_HEX(priv->display_page,2,priv->curr_pagenum);
    priv->display_page[3].unicode=' ';
    priv->display_page[4].unicode=' ';
    switch(priv->pagenumdec>>12){
    case 1:
        priv->display_page[5].unicode='_';
        priv->display_page[6].unicode='_';
        PRINT_HEX(priv->display_page,7,priv->pagenumdec);
        break;
    case 2:
        priv->display_page[5].unicode='_';
        PRINT_HEX(priv->display_page,6,priv->pagenumdec>>4);
        PRINT_HEX(priv->display_page,7,priv->pagenumdec);
    break;
    default:
        PRINT_HEX(priv->display_page,5,(priv->pagenum&0x700)?priv->pagenum>>8:8);
        PRINT_HEX(priv->display_page,6,priv->pagenum>>4);
        PRINT_HEX(priv->display_page,7,priv->pagenum);
    }
    if(priv->subpagenum!=0x3f7f){
        priv->display_page[8].unicode='.';
        PRINT_HEX(priv->display_page,9,priv->subpagenum>>4);
        PRINT_HEX(priv->display_page,10,priv->subpagenum);
    }else{
        priv->display_page[8].unicode=' ';
        priv->display_page[9].unicode=' ';
        priv->display_page[10].unicode=' ';
    }
    priv->display_page[11].unicode=' ';
    for(i=VBI_COLUMNS;i>VBI_TIME_LINEPOS ||
            ((curr_pg->raw[i]&0x60) && curr_pg->raw[i]!=0x20 && i>11);
            --i)
        if(curr_pg->raw[i]&0x60)
            priv->display_page[i].unicode=curr_pg->raw[i];
        else
            priv->display_page[i].unicode=' ';
    pthread_mutex_unlock(&(priv->buffer_mutex));
}
/*
------------------------------------------------------------------
   Renderer stuff
------------------------------------------------------------------
*/
#ifdef DEBUG_DUMP
/**
 * \brief renders teletext page into given file
 * \param pt page to render
 * \param f opened file descriptor
 * \param pagenum which page to render
 * \param colored use colors not implemented yet)
 *
 * Text will be UTF8 encoded
 */
static void render2text(tt_page* pt,FILE* f,int colored){
    int i,j;
    unsigned int u;
    unsigned char buf[8];
    unsigned char tmp;
    int pos;
    tt_char dp[VBI_ROWS*VBI_COLUMNS];
    int color=0;
    int bkg=0;
    int c1,b1;
    if(!pt)
        return;
    fprintf(f,"+========================================+\n");
    fprintf(f,"| lang:%d pagenum:0x%x subpagenum:%d flags:0x%x|\n",
    pt->lang,
    pt->pagenum,
    pt->subpagenum,
    0);
    fprintf(f,"+----------------------------------------+\n");

    decode_page(dp,pt->raw,pt->primary_lang,pt->secondary_lang,pt->flags);
    for(i=0;i<VBI_ROWS;i++){
        fprintf(f,"|");
        if(colored) fprintf(f,"\033[40m");
        for(j=0;j<VBI_COLUMNS;j++)
        {
             u=dp[i*VBI_COLUMNS+j].unicode;
              if(dp[i*VBI_COLUMNS+j].fg <= 7)
                c1=30+dp[i*VBI_COLUMNS+j].fg;
              else
                c1=38;
              if(dp[i*VBI_COLUMNS+j].bg <= 7)
                  b1=40+dp[i*VBI_COLUMNS+j].bg;
              else
                b1=40;
            if (b1!=bkg  && colored){
                fprintf(f,"\033[%dm",b1);
                bkg=b1;
            }
            if(c1!=color && colored){
                fprintf(f,"\033[%dm",c1);
                color=c1;
            }
            if(dp[i*VBI_COLUMNS+j].gfx){
                fprintf(f,"*");
            }else{
                pos=0;
                PUT_UTF8(u,tmp,if(pos<7) buf[pos++]=tmp;);
                buf[pos]='\0';
                fprintf(f,"%s",buf);
            }
        }

        if (colored) fprintf(f,"\033[0m");
        color=-1;bkg=-1;
        fprintf(f,"|\n");
    }
    //for debug
    fprintf(f,"+====================raw=================+\n");
    for(i=0;i<VBI_ROWS;i++){
        for(j=0;j<VBI_COLUMNS;j++)
            fprintf(f,"%02x ",dp[i*VBI_COLUMNS+j].raw);
        fprintf(f,"\n");
    }
    fprintf(f,"+====================lng=================+\n");
    for(i=0;i<VBI_ROWS;i++){
        for(j=0;j<VBI_COLUMNS;j++)
            fprintf(f,"%02x ",dp[i*VBI_COLUMNS+j].lng);
        fprintf(f,"\n");
    }
    fprintf(f,"+========================================+\n");
}

/**
 * \brief dump page into pgXXX.txt file in vurrent directory
 * \param pt page to dump
 *
 * \note XXX in filename is page number
 * \note use only for debug purposes
 */
static void dump_page(tt_page* pt)
{
    FILE*f;
    char name[100];
    snprintf(name,99,"pg%x.txt",pt->pagenum);
    f=fopen(name,"wb");
    render2text(pt,f,1);
    fclose(f);
}
#endif //DEBUG_DUMP


/**
 * \brief checks whether page is ready and copies it into cache array if so
 * \param priv private data structure
 * \param magAddr page's magazine address (0-7)
 */
static void store_in_cache(priv_vbi_t* priv, int magAddr, int line){
    mp_msg(MSGT_TELETEXT,MSGL_DBG2,"store_in_cache(%d): pagenum:%x\n",
        priv->mag[magAddr].order,
        priv->mag[magAddr].pt->pagenum);

    put_to_cache(priv,priv->mag[magAddr].pt,line);
    priv->curr_pagenum=priv->mag[magAddr].pt->pagenum;

#ifdef DEBUG_DUMP
    dump_page(get_from_cache(priv,
        priv->mag[magAddr].pt->pagenum,
        priv->mag[magAddr].pt->subpagenum));
#endif
}


/*
------------------------------------------------------------------
  Grabber stuff
------------------------------------------------------------------
*/
#define PLL_SAMPLES 4
#define PLL_ERROR   4
#define PLL_ADJUST  4

/**
 * \brief adjust current phase for better signal decoding
 * \param n count of bytes processed (?)
 * \param err count of error bytes (?)
 *
 * \remarks code was got from MythTV project
 */
static void pll_add(priv_vbi_t* priv,int n,int err){
    if(priv->pll_fixed)
        return;
    if(err>PLL_ERROR*2/3)
        err=PLL_ERROR*2/3;
    priv->pll_err+=err;
    priv->pll_cnt+=n;
    if(priv->pll_cnt<PLL_SAMPLES)
        return;
    if(priv->pll_err>PLL_ERROR)
    {
        if(priv->pll_err>priv->pll_lerr)
            priv->pll_dir= -priv->pll_dir;
        priv->pll_lerr=priv->pll_err;
        priv->pll_adj+=priv->pll_dir;
        if (priv->pll_adj<-PLL_ADJUST || priv->pll_adj>PLL_ADJUST)
        {
            priv->pll_adj=0;
            priv->pll_dir=-1;
            priv->pll_lerr=0;
        }
        mp_msg(MSGT_TELETEXT,MSGL_DBG3,"vbi: pll_adj=%2d\n",priv->pll_adj);
    }
    priv->pll_cnt=0;
    priv->pll_err=0;
}

/**
 * \brief reset error correction
 * \param priv private data structure
 * \param fine_tune shift value for adjusting
 *
 * \remarks code was got from MythTV project
 */
static void pll_reset(priv_vbi_t* priv,int fine_tune){
    priv->pll_fixed=fine_tune >= -PLL_ADJUST && fine_tune <= PLL_ADJUST;

    priv->pll_err=0;
    priv->pll_lerr=0;
    priv->pll_cnt=0;
    priv->pll_dir=-1;
    priv->pll_adj=0;
    if(priv->pll_fixed)
        priv->pll_adj=fine_tune;
    if(priv->pll_fixed)
        mp_msg(MSGT_TELETEXT,MSGL_DBG3,"pll_reset (fixed@%2d)\n",priv->pll_adj);
    else
        mp_msg(MSGT_TELETEXT,MSGL_DBG3,"pll_reset (auto)\n");

}
/**
 * \brief decode packet 0 (teletext page header)
 * \param priv private data structure
 * \param data raw teletext data (with not applied hamm correction yet)
 * \param magAddr teletext page's magazine address
 *
 * \remarks
 * data buffer was shifted by 6 and now contains:
 *  0..1 page number
 *  2..5 sub-code
 *  6..7 control codes
 *  8..39 display data
 *
 *  only first 8 bytes protected by Hamm 8/4 code
 */
static int decode_pkt0(priv_vbi_t* priv,unsigned char* data,int magAddr)
{
    int d[8];
    int i,err;

    if (magAddr<0 || magAddr>7)
        return 0;
    for(i=0;i<8;i++){
        d[i]= corrHamm48[ data[i] ];
        if(d[i]&0x80){
            pll_add(priv,2,4);

            if(priv->mag[magAddr].pt)
                  free(priv->mag[magAddr].pt);
            priv->mag[magAddr].pt=NULL;
            priv->mag[magAddr].order=0;
            return 0;
        }
    }
    if (!priv->mag[magAddr].pt)
        priv->mag[magAddr].pt= malloc(sizeof(tt_page));

    if(priv->primary_language)
        priv->mag[magAddr].pt->primary_lang=priv->primary_language;
    else
        priv->mag[magAddr].pt->primary_lang= (d[7]>>1)&7;
    priv->mag[magAddr].pt->secondary_lang=priv->secondary_language;
    priv->mag[magAddr].pt->subpagenum=(d[2]|(d[3]<<4)|(d[4]<<8)|(d[5]<<12))&0x3f7f;
    priv->mag[magAddr].pt->pagenum=(magAddr<<8) | d[0] | (d[1]<<4);
    priv->mag[magAddr].pt->flags=((d[7]&1)<<7) | ((d[3]&8)<<3) | ((d[5]&12)<<2) | d[6];

    memset(priv->mag[magAddr].pt->raw, 0x00, VBI_COLUMNS*VBI_ROWS);
    priv->mag[magAddr].order=0;

    for(i=0;i<8;i++){
        priv->mag[magAddr].pt->raw[i]=0x20;
    }
    err=0;
    for(i=8; i<VBI_COLUMNS; i++){
        data[i]= fixParity[data[i]];
        priv->mag[magAddr].pt->raw[i]=data[i];
        if(data[i]&0x80) //Error
            err++;
        pll_add(priv,1,err);
    }

    store_in_cache(priv,magAddr,0);

    return 1;
}

/**
 * \brief decode teletext 8/30 Format 1 packet
 * \param priv private data structure
 * \param data raw teletext data (with not applied hamm correction yet)
 * \param magAddr teletext page's magazine address
 *
 * \remarks
 * packet contains:
 * 0      designation code
 * 1..2   initial page
 * 3..6   initial subpage & magazine address
 * 7..8   network id
 * 9      time offset
 * 10..12 julian date
 * 13..15 universal time
 * 20..40 network name
 *
 * First 7 bytes are protected by Hamm 8/4 code.
 * Bytes 20-40 has odd parity check.
 *
 * See subcaluse 9.8.1 of specification for details
 */
static int decode_pkt30(priv_vbi_t* priv,unsigned char* data,int magAddr)
{
    int d[8];
    int i,err;

    for(i=0;i<7;i++){
        d[i]= corrHamm48[ data[i] ];
        if(d[i]&0x80){
            pll_add(priv,2,4);
            return 0;
        }
        d[i]&=0xf;
    }

    err=0;
    for(i=20; i<40; i++){
        data[i]= fixParity[data[i]];
        if(data[i]&0x80)//Unrecoverable error
            err++;
        pll_add(priv,1,err);
    }
    if (err) return 0;

    if (d[0]&0xe) //This is not 8/30 Format 1 packet
        return 1;

    priv->initialpage=d[1] | d[2]<<4 | (d[6]&0xc)<<7 | (d[4]&1)<<8;
    priv->initialsubpage=d[3] | d[4]<<4 | d[5]<<8 | d[6]<<12;
    priv->networkid=data[7]<<8 | data[8];

    priv->timeoffset=(data[9]>>1)&0xf;
    if(data[9]&0x40)
        priv->timeoffset=-priv->timeoffset;

    priv->juliandate=(data[10]&0xf)<<16 | data[11]<<8 | data[12];
    priv->juliandate-=0x11111;

    priv->universaltime=data[13]<<16 | data[14]<<8 | data[15];
    priv->universaltime-=0x111111;

    snprintf(priv->networkname,21,"%s",data+20);

    return 1;
}

/**
 * \brief decode packets 1..24 (teletext page header)
 * \param priv private data structure
 * \param data raw teletext data
 * \param magAddr teletext page's magazine address
 * \param rowAddr teletext page's row number
 *
 * \remarks
 * data buffer was shifted by 6 and now contains 40 bytes of display data:
 * this type of packet is not proptected by Hamm 8/4 code
 */
static void decode_pkt_page(priv_vbi_t* priv,unsigned char*data,int magAddr,int rowAddr){
    int i,err;
    if (!priv->mag[magAddr].pt)
        return;

    priv->mag[magAddr].order=rowAddr;

    err=0;
    for(i=0; i<VBI_COLUMNS; i++){
        data[i]= fixParity[ data[i] ];
        priv->mag[magAddr].pt->raw[i+rowAddr*VBI_COLUMNS]=data[i];
        if( data[i]&0x80) //HammError
            err++;
    }
    pll_add(priv,1,err);

    store_in_cache(priv,magAddr,rowAddr);
}

/**
 * \brief decode packets 27 (teletext links)
 * \param priv private data structure
 * \param data raw teletext data
 * \param magAddr teletext page's magazine address
 */
static int decode_pkt27(priv_vbi_t* priv,unsigned char* data,int magAddr){
    int i,hpg;

    if (!priv->mag[magAddr].pt)
        return 0;
    for(i=0;i<38;++i)
        if ((data[i] = corrHamm48[ data[i] ]) & 0x80){
            pll_add(priv,2,4);
            return 0;
        }

    /*
      Not a X/27/0 Format 1 packet or
      flag "show links on row 24" is not set.
    */
    if (data[0] || !(data[37] & 8))
        return 1;
    for(i=0;i<6;++i) {
        hpg = (magAddr<<8) ^ ((data[4+i*6]&0x8)<<5 | (data[6+i*6]&0xc)<<7);
        if (!hpg) hpg=0x800;
        priv->mag[magAddr].pt->links[i].pagenum = (data[1+i*6] & 0xf) |
                ((data[2+i*6] & 0xf) << 4) | hpg;
        priv->mag[magAddr].pt->links[i].subpagenum = ((data[3+i*6] & 0xf) |
                (data[4+i*6] & 0xf) << 4 | (data[5+i*6] & 0xf) << 8 |
                (data[6+i*6] & 0xf) << 12) & 0x3f7f;
    }
    put_to_cache(priv,priv->mag[magAddr].pt,-1);
    return 1;
}

/**
 * \brief Decode teletext X/28/0 Format 1 packet
 * \param priv private data structure
 * \param data raw teletext data
 *
 * Primary G0 charset is transmitted in bits 14-8 of Triplet 1
 * See Table 32 of specification for details.
 *
 * Secondary G0 charset is transmitted in bits 3-1 of Triplet 2 and
 * bits 18-15 of Triplet 1
 * See Table 33 of specification for details.
 *
 */
static void decode_pkt28(priv_vbi_t* priv,unsigned char*data){
    int d;
    int t1,t2;
    d=corrHamm48[ data[0] ];
    if(d) return; //this is not X/28/0 Format 1 packet or error occured

    t1=corrHamm24(data+1);
    t2=corrHamm24(data+4);
    if (t1<0 || t2<0){
        pll_add(priv,1,4);
        return;
    }

    priv->primary_language=(t1>>7)&0x7f;
    priv->secondary_language=((t2<<4) | (t1>>14))&0x7f;
    if (priv->secondary_language==0x7f)
        //No secondary language required
        priv->secondary_language=priv->primary_language;
    else // Swapping bits 1 and 3
        priv->secondary_language=(priv->secondary_language&0x7a) |
                                (priv->secondary_language&4)>>2 |
                                (priv->secondary_language&1)<<2;

    mp_msg(MSGT_TELETEXT,MSGL_DBG2,"pkt28: language: primary=%02x secondary=0x%02x\n",
        priv->primary_language,priv->secondary_language);
}

/**
 * \brief decodes raw vbi data (signal amplitudes) into sequence of bytes
 * \param priv private data structure
 * \param buf raw vbi data (one line of frame)
 * \param data output buffer for decoded bytes (at least 45 bytes long)
 *
 * Used XawTV's algorithm. Signal phase is calculated with help of starting clock
 * run-in sequence (min/max values and bit distance values are calculated)
 */
static int decode_raw_line_runin(priv_vbi_t* priv,unsigned char* buf,unsigned char* data){
    const int magic= 0x27; // reversed 1110010
    int dt[256],hi[6],lo[6];
    int i,x,r;
    int decoded;
    int sync;
    unsigned char min,max;
    int thr=0; //threshold

    //stubs
    int soc=priv->soc;
    int eoc=priv->eoc;

    for(i=soc;i<eoc;i++)
        dt[i]=buf[i+priv->bpb/ONE_FIXP]-buf[i];    // amplifies the edges best.
    /* set barrier */
    for (i=eoc; i<eoc+16; i+=2)
        dt[i]=100, dt[i+1]=-100;

    /* find 6 rising and falling edges */
    for (i=soc, x=0; x<6; ++x)
    {
        while (dt[i]<32)
            i++;
        hi[x]=i;
        while (dt[i]>-32)
            i++;
        lo[x]=i;
    }
    if (i>=eoc)
    {
        return 0;      // not enough periods found
    }
    i=hi[5]-hi[1]; // length of 4 periods (8 bits)
    if (i<priv->bp8bl || i>priv->bp8bh)
    {
        mp_msg(MSGT_TELETEXT,MSGL_DBG3,"vbi: wrong freq %d (%d,%d)\n",
            i,priv->bp8bl,priv->bp8bh);
        return 0;      // bad frequency
    }
    /* AGC and sync-reference */
    min=255, max=0, sync=0;
    for (i=hi[4]; i<hi[5]; ++i)
        if (buf[i]>max)
            max=buf[i], sync=i;
    for (i=lo[4]; i<lo[5]; ++i)
        if (buf[i]<min)
            min=buf[i];
    thr=(min+max)/2;

    buf+=sync;
    // searching for '11'
    for(i=priv->pll_adj*priv->bpb/10;i<16*priv->bpb;i+=priv->bpb)
        if(buf[FIXP2INT(i)]>thr && buf[FIXP2INT(i+priv->bpb)]>thr)
            break;
    r=0;
    for(decoded=1; decoded<= (VBI_COLUMNS+3)<<3;decoded++){
        r>>=1;
        if(buf[FIXP2INT(i)]>thr) r|=0x80;
        if(!(decoded & 0x07)){
            data[(decoded>>3) - 1]=r;
            r=0;
        }
        i+=priv->bpb;
    }
    if(data[0]!=magic)
        return 0; //magic not found

    //stub
    for(i=0;i<43;i++){
        data[i]=data[i+1];
    }
    mp_msg(MSGT_TELETEXT,MSGL_DBG3,"thr:%d sync:%d ",thr,sync);

    return 1;
}

#if 0
//See comment in vbi_decode for a reason of commenting out this routine.

/**
 * \brief decodes raw vbi data (signal amplitudes) into sequence of bytes
 * \param priv private data structure
 * \param buf raw vbi data (one line of frame)
 * \param data output buffer for decoded bytes (at least 45 bytes long)
 *
 * Used Michael Niedermayer's algorithm.
 * Signal phase is calculated using correlation between given samples data and
 * pure sine
 */
static int decode_raw_line_sine(priv_vbi_t* priv,unsigned char* buf,unsigned char* data){
    int i,x,r,amp,xFixp;
    int avg=0;
    double sin_sum=0, cos_sum=0;

    for(x=0; x< FIXP2INT(10*priv->bpb); x++)
      avg+=buf[x];

    avg/=FIXP2INT(10*priv->bpb);

    for(x=0; x<12; x++){
      amp= buf[x<<1];
      sin_sum+= si[x]*(amp-avg);
      cos_sum+= co[x]*(amp-avg);
    }
    //this is always zero. Why ?
    xFixp= atan(sin_sum/cos_sum)*priv->bpb/M_PI;

    //Without this line the result is full of errors
    //and routine is unable to find magic sequence
    buf+=FIXP2INT(10*priv->bpb);

    r=0;
    for(x=FIXP2INT(xFixp);x<70;x=FIXP2INT(xFixp)){
      r=(r<<1) & 0xFFFF;
      if(buf[x]>avg) r|=1;
      xFixp+=priv->bpb;
      if(r==0xAAE4) break;
    }

    //this is not teletext
    if (r!=0xaae4) return 0;

    //Decode remaining 45-2(clock run-in)-1(framing code)=42 bytes
    for(i=1; i<=(42<<3); i++){
      r>>=1;
      x=FIXP2INT(xFixp);
      if(buf[x]> avg)
          r|=0x80;

      if(!(i & 0x07)){
          data[(i>>3)-1]=r;
          r=0;
      }
      xFixp+=priv->bpb;
    }

    return 1;
}
#endif

/**
 * \brief decodes one vbi line from one video frame
 * \param priv private data structure
 * \param data buffer with raw vbi data in it
 */
static void vbi_decode_line(priv_vbi_t *priv, unsigned char *data) {
    int d0,d1,magAddr,pkt;

    d0= corrHamm48[ data[0] ];
    d1= corrHamm48[ data[1] ];

    if(d0&0x80 || d1&0x80){
        pll_add(priv,2,4);
        mp_msg(MSGT_TELETEXT,MSGL_V,"vbi_decode_line: HammErr\n");

        return; //hamError
    }
    magAddr=d0 & 0x7;
    pkt=(d0>>3)|(d1<<1);
    mp_msg(MSGT_TELETEXT,MSGL_DBG3,"vbi_decode_line:%x %x (mag:%x, pkt:%d)\n",
            d0,d1,magAddr,pkt);
    if(!pkt){
        decode_pkt0(priv,data+2,magAddr); //skip MRGA
    }else if(pkt>0 && pkt<VBI_ROWS){
        if(!priv->mag[magAddr].pt)
            return;
        decode_pkt_page(priv,data+2,magAddr,pkt);//skip MRGA
    }else if(pkt==27) {
        decode_pkt27(priv,data+2,magAddr);
    }else if(pkt==28){
        decode_pkt28(priv,data+2);
    }else if(pkt==30){
        decode_pkt30(priv,data+2,magAddr);
    } else {
        mp_msg(MSGT_TELETEXT,MSGL_DBG3,"unsupported packet:%d\n",pkt);
    }
}

/**
 * \brief decodes all vbi lines from one video frame
 * \param priv private data structure
 * \param buf buffer with raw vbi data in it
 *
 * \note buffer size have to be at least priv->ptsp->bufsize bytes
 */
static void vbi_decode(priv_vbi_t* priv,unsigned char*buf){
    unsigned char data[64];
    unsigned char* linep;
    int i=0;
    mp_msg(MSGT_TELETEXT,MSGL_DBG3,"vbi: vbi_decode\n");
    for(linep=buf; !priv->cache_reset && linep<buf+priv->ptsp->bufsize; linep+=priv->ptsp->samples_per_line,i++){
#if 0
        /*
          This routine is alternative implementation of raw VBI data decoding.
          Unfortunately, it detects only about 20% of incoming data,
          but Michael says that this algorithm is better, and he wants to fix it.
        */
        if(decode_raw_line_sine(priv,linep,data)<=0){
#endif
        if(decode_raw_line_runin(priv,linep,data)<=0){
             continue; //this is not valid teletext line
        }
        vbi_decode_line(priv, data);
    }
    if (priv->cache_reset){
        pthread_mutex_lock(&(priv->buffer_mutex));
        priv->cache_reset--;
        pthread_mutex_unlock(&(priv->buffer_mutex));
    }

}

/**
 * \brief decodes a vbi line from a DVB teletext stream
 * \param priv private data structure
 * \param buf buffer with DVB teletext data
 *
 * No locking is done since this is only called from a single-threaded context
 */
static void vbi_decode_dvb(priv_vbi_t *priv, const uint8_t buf[44]){
    int i;
    uint8_t data[42];

    mp_msg(MSGT_TELETEXT,MSGL_DBG3, "vbi: vbi_decode_dvb\n");

    /* Reverse bit order, skipping the first two bytes (field parity, line
       offset and framing code). */
    for (i = 0; i < sizeof(data); i++)
        data[i] = av_reverse[buf[2 + i]];

    vbi_decode_line(priv, data);
    if (priv->cache_reset)
        priv->cache_reset--;
}

/*
---------------------------------------------------------------------------------
    Public routines
---------------------------------------------------------------------------------
*/

/**
 * \brief toggles teletext page displaying format
 * \param priv_vbi private data structure
 * \param flag new format
 * \return
 *   VBI_CONTROL_TRUE is success,
 *   VBI_CONTROL_FALSE otherwise
 *
 * flag:
 * 0 - opaque
 * 1 - transparent
 * 2 - opaque  with black foreground color (only in bw mode)
 * 3 - transparent  with black foreground color (only in bw mode)
 */
static int teletext_set_format(priv_vbi_t * priv, teletext_format flag)
{
    flag&=3;

    mp_msg(MSGT_TELETEXT,MSGL_DBG3,"teletext_set_format_is called. mode:%d\n",flag);
    pthread_mutex_lock(&(priv->buffer_mutex));

    priv->tformat=flag;

    priv->pagenumdec=0;

    pthread_mutex_unlock(&(priv->buffer_mutex));
    return VBI_CONTROL_TRUE;
}

/**
 * \brief append just entered digit to editing page number
 * \param priv_vbi private data structure
 * \param dec decimal digit to append
 *
 *  dec:
 *   '0'..'9' append digit
 *    '-' remove last digit (backspace emulation)
 *
 * This routine allows user to jump to arbitrary page.
 * It implements simple page number editing algorithm.
 *
 * Subsystem can be on one of two modes: normal and page number edit mode.
 * Zero value of priv->pagenumdec means normal mode
 * Non-zero value means page number edit mode and equals to packed
 * decimal number of already entered part of page number.
 *
 * How this works.
 * Let's assume that current mode is normal (pagenumdec is zero), teletext page
 * 100 are displayed as usual. topmost left corner of page contains page number.
 * Then vbi_add_dec is sequentially called (through slave
 * command of course) with 1,4,-,2,3 * values of dec parameter.
 *
 * +-----+------------+------------------+
 * | dec | pagenumdec | displayed number |
 * +-----+------------+------------------+
 * |     | 0x000      | 100              |
 * +-----+------------+------------------+
 * | 1   | 0x001      | __1              |
 * +-----+------------+------------------+
 * | 4   | 0x014      | _14              |
 * +-----+------------+------------------+
 * | -   | 0x001      | __1              |
 * +-----+------------+------------------+
 * | 2   | 0x012      | _12              |
 * +-----+------------+------------------+
 * | 3   | 0x123      | 123              |
 * +-----+------------+------------------+
 * |     | 0x000      | 123              |
 * +-----+------------+------------------+
 *
 * pagenumdec will automatically receive zero value after third digit of page
 * number is entered and current page will be switched to another one with
 * entered page number.
 */
static void vbi_add_dec(priv_vbi_t * priv, char *dec)
{
    int count, shift;
    if (!dec)
        return;
    if (!priv->on)
        return;
    if ((*dec<'0' || *dec>'9') && *dec!='-')
        return;
    if (!priv->pagenumdec) //first digit cannot be '0','9' or '-'
        if(*dec=='-' || *dec=='0' || *dec=='9')
            return;
    pthread_mutex_lock(&(priv->buffer_mutex));
    count=(priv->pagenumdec>>12)&0xf;
    if (*dec=='-') {
        count--;
        if (count)
            priv->pagenumdec=((priv->pagenumdec>>4)&0xfff)|(count<<12);
        else
            priv->pagenumdec=0;
    } else {
        shift = count * 4;
        count++;
        priv->pagenumdec=
            (((priv->pagenumdec)<<4|(*dec-'0'))&0xfff)|(count<<12);
        if (count==3) {
            priv->pagenum=priv->pagenumdec&0x7ff;
            priv->subpagenum=get_subpagenum_from_cache(priv,priv->pagenum);
            priv->pagenumdec=0;
        }
    }
    pthread_mutex_unlock(&(priv->buffer_mutex));
}


/**
 * \brief Teletext control routine
 * \param priv_vbi private data structure
 * \param cmd command
 * \param arg command parameter (has to be not null)
 */
int teletext_control(void* p, int cmd, void *arg)
{
    int fine_tune=99;
    priv_vbi_t* priv=(priv_vbi_t*)p;
    tt_page* pgc;

    if (!priv && cmd!=TV_VBI_CONTROL_START)
        return VBI_CONTROL_FALSE;
    if (!arg && cmd!=TV_VBI_CONTROL_STOP && cmd!=TV_VBI_CONTROL_MARK_UNCHANGED)
        return VBI_CONTROL_FALSE;

    switch (cmd) {
    case TV_VBI_CONTROL_RESET:
    {
        int i;
        struct tt_param* tt_param=arg;
        pthread_mutex_lock(&(priv->buffer_mutex));
        priv->pagenumdec=0;
        clear_cache(priv);
        priv->pagenum=steppage(0,tt_param->page&0x7ff,1);
        priv->tformat=tt_param->format;
        priv->subpagenum=0x3f7f;
        pll_reset(priv,fine_tune);
        if(tt_param->lang==-1){
            mp_msg(MSGT_TELETEXT,MSGL_INFO,MSGTR_TV_TTSupportedLanguages);
            for(i=0; tt_languages[i].lang_code; i++){
                mp_msg(MSGT_TELETEXT,MSGL_INFO,"  %3d  %s\n",
                    tt_languages[i].lang_code, tt_languages[i].lang_name);
            }
            mp_msg(MSGT_TELETEXT,MSGL_INFO,"  %3d  %s\n",
                tt_languages[i].lang_code, tt_languages[i].lang_name);
        }else{
            for(i=0; tt_languages[i].lang_code; i++){
                if(tt_languages[i].lang_code==tt_param->lang)
                    break;
            }
            if (priv->primary_language!=tt_languages[i].lang_code){
                mp_msg(MSGT_TELETEXT,MSGL_INFO,MSGTR_TV_TTSelectedLanguage,
                    tt_languages[i].lang_name);
                priv->primary_language=tt_languages[i].lang_code;
            }
        }
        priv->page_changed=1;
        pthread_mutex_unlock(&(priv->buffer_mutex));
        return VBI_CONTROL_TRUE;
    }
    case TV_VBI_CONTROL_START:
    {
        int i;
        tt_stream_props* ptsp=*(tt_stream_props**)arg;

        if(!ptsp)
            return VBI_CONTROL_FALSE;

        priv=calloc(1,sizeof(priv_vbi_t));

        priv->ptsp=malloc(sizeof(tt_stream_props));
        memcpy(priv->ptsp,ptsp,sizeof(tt_stream_props));
        *(priv_vbi_t**)arg=priv;

        priv->subpagenum=0x3f7f;
        pthread_mutex_init(&priv->buffer_mutex, NULL);
        priv->pagenumdec=0;
        for(i=0;i<VBI_ROWS*VBI_COLUMNS;i++)
            priv->display_page[i]=tt_space;

        priv->mag=calloc(8,sizeof(mag_t));
        init_cache(priv);
        init_vbi_consts(priv);
        pll_reset(priv,fine_tune);
        priv->page_changed=1;
        return VBI_CONTROL_TRUE;
    }
    case TV_VBI_CONTROL_STOP:
    {
        if(priv->mag)
            free(priv->mag);
        if(priv->ptsp)
            free(priv->ptsp);
        destroy_cache(priv);
        priv->page_changed=1;
        pthread_mutex_destroy(&priv->buffer_mutex);
        free(priv);
        return VBI_CONTROL_TRUE;
    }
    case TV_VBI_CONTROL_SET_MODE:
        priv->on=(*(int*)arg%2);
        priv->page_changed=1;
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_GET_MODE:
        *(int*)arg=priv->on;
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_SET_FORMAT:
        priv->page_changed=1;
        return teletext_set_format(priv, *(int *) arg);
    case TV_VBI_CONTROL_GET_FORMAT:
        pthread_mutex_lock(&(priv->buffer_mutex));
        *(int*)arg=priv->tformat;
        pthread_mutex_unlock(&(priv->buffer_mutex));
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_GET_HALF_PAGE:
        if(!priv->on)
            return VBI_CONTROL_FALSE;
        *(int *)arg=priv->zoom;
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_SET_HALF_PAGE:
    {
        int val=*(int*)arg;
        val%=3;
        if(val<0)
            val+=3;
        pthread_mutex_lock(&(priv->buffer_mutex));
        priv->zoom=val;
        priv->page_changed=1;
        pthread_mutex_unlock(&(priv->buffer_mutex));
        return VBI_CONTROL_TRUE;
    }
    case TV_VBI_CONTROL_GO_LINK:
    {
        int val=*(int *) arg;
        if(val<1 || val>6)
            return VBI_CONTROL_FALSE;
        pthread_mutex_lock(&(priv->buffer_mutex));
        if (!(pgc = priv->ptt_cache[priv->pagenum])) {
            pthread_mutex_unlock(&(priv->buffer_mutex));
            return VBI_CONTROL_FALSE;
        }
        if (!pgc->links[val-1].pagenum || pgc->links[val-1].pagenum>0x7ff) {
            pthread_mutex_unlock(&(priv->buffer_mutex));
            return VBI_CONTROL_FALSE;
        }
        priv->pagenum=pgc->links[val-1].pagenum;
        if(pgc->links[val-1].subpagenum!=0x3f7f)
            priv->subpagenum=pgc->links[val-1].subpagenum;
        else
            priv->subpagenum=get_subpagenum_from_cache(priv,priv->pagenum);
        priv->page_changed=1;
        pthread_mutex_unlock(&(priv->buffer_mutex));
        return VBI_CONTROL_TRUE;
    }
    case TV_VBI_CONTROL_SET_PAGE:
    {
        int val=*(int *) arg;
        if(val<100 || val>0x899)
            return VBI_CONTROL_FALSE;
        pthread_mutex_lock(&(priv->buffer_mutex));
        priv->pagenum=val&0x7ff;
        priv->subpagenum=get_subpagenum_from_cache(priv,priv->pagenum);
        priv->pagenumdec=0;
        priv->page_changed=1;
        pthread_mutex_unlock(&(priv->buffer_mutex));
        return VBI_CONTROL_TRUE;
    }
    case TV_VBI_CONTROL_STEP_PAGE:
    {
        int direction=*(int *) arg;
        pthread_mutex_lock(&(priv->buffer_mutex));
        priv->pagenum=steppage(priv->pagenum, direction,1);
        priv->subpagenum=get_subpagenum_from_cache(priv,priv->pagenum);
        priv->pagenumdec=0;
        priv->page_changed=1;
        pthread_mutex_unlock(&(priv->buffer_mutex));
        return VBI_CONTROL_TRUE;
    }
    case TV_VBI_CONTROL_GET_PAGE:
        *(int*)arg=((priv->pagenum+0x700)&0x7ff)+0x100;
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_SET_SUBPAGE:
        pthread_mutex_lock(&(priv->buffer_mutex));
        priv->pagenumdec=0;
        priv->subpagenum=*(int*)arg;
        if(priv->subpagenum<0)
            priv->subpagenum=0x3f7f;
        if(priv->subpagenum>=VBI_MAX_SUBPAGES)
            priv->subpagenum=VBI_MAX_SUBPAGES-1;
        priv->page_changed=1;
        pthread_mutex_unlock(&(priv->buffer_mutex));
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_GET_SUBPAGE:
        *(int*)arg=priv->subpagenum;
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_ADD_DEC:
        vbi_add_dec(priv, *(char **) arg);
        priv->page_changed=1;
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_DECODE_PAGE:
        vbi_decode(priv,*(unsigned char**)arg);
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_DECODE_DVB:
        vbi_decode_dvb(priv, arg);
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_GET_VBIPAGE:
        if(!priv->on)
            return VBI_CONTROL_FALSE;
        prepare_visible_page(priv);
        *(void **)arg=priv->display_page;
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_GET_NETWORKNAME:
        *(void **)arg=priv->networkname;
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_MARK_UNCHANGED:
        priv->page_changed=0;
        priv->last_rendered=GetTimerMS();
        return VBI_CONTROL_TRUE;
    case TV_VBI_CONTROL_IS_CHANGED:
        if(GetTimerMS()-priv->last_rendered> 250)  //forcing page update every 1/4 sec
            priv->page_changed=3; //mark that header update is enough
        *(int*)arg=priv->page_changed;
        return VBI_CONTROL_TRUE;
    }
    return VBI_CONTROL_UNKNOWN;
}
