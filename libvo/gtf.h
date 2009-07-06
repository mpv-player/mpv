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

#ifndef MPLAYER_GTF_H
#define MPLAYER_GTF_H

#include <vbe.h>

#define GTF_VF 0
#define GTF_HF 1
#define GTF_PF 2


typedef struct {
    double	Vsync_need;	   /* Number of lines for vert sync (default 3) */
    double	min_Vsync_BP;	   /* Minimum vertical sync + back porch (us) (default 550)*/
    double	min_front_porch;   /* Minimum front porch in lines (default 1)	*/
    double	char_cell_granularity;  /* Character cell granularity in pixels (default 8) */
    double	margin_width;	   /* Top/ bottom MARGIN size as % of height (%) (default 1.8) */
    double	sync_width;	  /* Sync width percent of line period ( default 8) */
    double  c;		/* Blanking formula offset (default 40)*/
    double  j;		/* Blanking formula scaling factor weight (default 20)*/
    double  k;		/* Blanking formula scaling factor (default 128)*/
    double  m;		/* Blanking formula gradient (default 600)*/
    } GTF_constants;

//#ifndef __VESA_VBELIB_INCLUDED__
//    struct VesaCRTCInfoBlock {
//    unsigned short hTotal;     /* Horizontal total in pixels */
//    unsigned short hSyncStart; /* Horizontal sync start in pixels */
//    unsigned short hSyncEnd;   /* Horizontal sync end in pixels */
//    unsigned short vTotal;     /* Vertical total in lines */
//    unsigned short vSyncStart; /* Vertical sync start in lines */
//    unsigned short vSyncEnd;   /* Vertical sync end in lines */
//    unsigned char  Flags;      /* Flags (Interlaced, Double Scan etc) */
//    unsigned long  PixelClock; /* Pixel clock in units of Hz */
//    unsigned short RefreshRate;/* Refresh rate in units of 0.01 Hz*/
//    unsigned char  Reserved[40];/* remainder of CRTCInfoBlock*/
//}__attribute__ ((packed));

//#define VESA_CRTC_DOUBLESCAN 0x01
//#define VESA_CRTC_INTERLACED 0x02
//#define VESA_CRTC_HSYNC_NEG  0x04
//#define VESA_CRTC_VSYNC_NEG  0x08

//#endif

void GTF_calcTimings(double X,double Y,double freq, int type,
                     int want_margins, int want_interlace,struct VesaCRTCInfoBlock *result);

#endif /* MPLAYER_GTF_H */
