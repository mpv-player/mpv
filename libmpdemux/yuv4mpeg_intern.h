/*
 *  yuv4mpeg_intern.h:  Internal constants for "new" YUV4MPEG streams
 *
 *  Copyright (C) 2001 Andrew Stevens <andrew.stevens@philips.com>
 *
 *  This file is part of the lavtools package (mjpeg.sourceforge.net).  
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __YUV4MPEG_INTERN_H__
#define __YUV4MPEG_INTERN_H__


#define Y4M_MAGIC "YUV4MPEG2"
#define Y4M_FRAME_MAGIC "FRAME"

#define Y4M_DELIM " "  /* single-character(space) separating tagged fields */

#define Y4M_LINE_MAX 256   /* max number of characters in a header line
                               (including the '\n', but not the '\0') */


/* standard framerate ratios */
#define Y4M_FPS_UNKNOWN    { 0, 0 }
#define Y4M_FPS_NTSC_FILM  { 24000, 1001 }
#define Y4M_FPS_FILM       { 24, 1 }
#define Y4M_FPS_PAL        { 25, 1 }
#define Y4M_FPS_NTSC       { 30000, 1001 }
#define Y4M_FPS_30         { 30, 1 }
#define Y4M_FPS_PAL_FIELD  { 50, 1 }
#define Y4M_FPS_NTSC_FIELD { 60000, 1001 }
#define Y4M_FPS_60         { 60, 1 }

/* standard sample/pixel aspect ratios */
#define Y4M_SAR_UNKNOWN        {   0, 0  }
#define Y4M_SAR_SQUARE         {   1, 1  }
#define Y4M_SAR_NTSC_CCIR601   {  10, 11 }
#define Y4M_SAR_NTSC_16_9      {  40, 33 }
#define Y4M_SAR_NTSC_SVCD_4_3  {  15, 11 }
#define Y4M_SAR_NTSC_SVCD_16_9 {  20, 11 }
#define Y4M_SAR_PAL_CCIR601    {  59, 54 }
#define Y4M_SAR_PAL_16_9       { 118, 81 }
#define Y4M_SAR_PAL_SVCD_4_3   {  59, 36 }
#define Y4M_SAR_PAL_SVCD_16_9  {  59, 27 }

#define Y4M_SAR_MPEG1_1 Y4M_SAR_SQUARE
#define Y4M_SAR_MPEG1_2  { 10000, 6735 }
#define Y4M_SAR_MPEG1_3  { 10000, 7031 } /* Anamorphic 16:9 PAL */
#define Y4M_SAR_MPEG1_4  { 10000, 7615 }
#define Y4M_SAR_MPEG1_5  { 10000, 8055 }
#define Y4M_SAR_MPEG1_6  { 10000, 8437 } /* Anamorphic 16:9 NTSC */
#define Y4M_SAR_MPEG1_7  { 10000, 8935 } 
#define Y4M_SAR_MPEG1_8  { 10000, 9375 } /* PAL/SECAM 4:3 */
#define Y4M_SAR_MPEG1_9  { 10000, 9815 }
#define Y4M_SAR_MPEG1_10 { 10000, 10255 }
#define Y4M_SAR_MPEG1_11 { 10000, 10695 }
#define Y4M_SAR_MPEG1_12 { 10000, 11250 } /* NTSC 4:3 */
#define Y4M_SAR_MPEG1_13 { 10000, 11575 }
#define Y4M_SAR_MPEG1_14 { 10000, 12015 }

#define Y4M_DAR_MPEG2_1 { 1, 1}
#define Y4M_DAR_MPEG2_2 { 4, 3 }
#define Y4M_DAR_MPEG2_3 { 16, 9 }
#define Y4M_DAR_MPEG2_4 { 221, 100 }

#endif /* __YUV4MPEG_INTERN_H__ */
