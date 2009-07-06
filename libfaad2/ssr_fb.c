/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2004 M. Bakker, Ahead Software AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: ssr_fb.c,v 1.13 2004/09/04 14:56:29 menno Exp $
**/

#include "common.h"
#include "structs.h"

#ifdef SSR_DEC

#include <string.h>
#include <stdlib.h>
#include "syntax.h"
#include "filtbank.h"
#include "mdct.h"
#include "ssr_fb.h"
#include "ssr_win.h"

fb_info *ssr_filter_bank_init(uint16_t frame_len)
{
    uint16_t nshort = frame_len/8;

    fb_info *fb = (fb_info*)faad_malloc(sizeof(fb_info));
    memset(fb, 0, sizeof(fb_info));

    /* normal */
    fb->mdct256 = faad_mdct_init(2*nshort);
    fb->mdct2048 = faad_mdct_init(2*frame_len);

    fb->long_window[0]  = sine_long_256;
    fb->short_window[0] = sine_short_32;
    fb->long_window[1]  = kbd_long_256;
    fb->short_window[1] = kbd_short_32;

    return fb;
}

void ssr_filter_bank_end(fb_info *fb)
{
    faad_mdct_end(fb->mdct256);
    faad_mdct_end(fb->mdct2048);

    if (fb) faad_free(fb);
}

static INLINE void imdct_ssr(fb_info *fb, real_t *in_data,
                             real_t *out_data, uint16_t len)
{
    mdct_info *mdct;

    switch (len)
    {
    case 512:
        mdct = fb->mdct2048;
        break;
    case 64:
        mdct = fb->mdct256;
        break;
    }

    faad_imdct(mdct, in_data, out_data);
}

/* NON-overlapping inverse filterbank for use with SSR */
void ssr_ifilter_bank(fb_info *fb, uint8_t window_sequence, uint8_t window_shape,
                      uint8_t window_shape_prev, real_t *freq_in,
                      real_t *time_out, uint16_t frame_len)
{
    int16_t i;
    real_t *transf_buf;

    real_t *window_long;
    real_t *window_long_prev;
    real_t *window_short;
    real_t *window_short_prev;

    uint16_t nlong = frame_len;
    uint16_t nshort = frame_len/8;
    uint16_t trans = nshort/2;

    uint16_t nflat_ls = (nlong-nshort)/2;

    transf_buf = (real_t*)faad_malloc(2*nlong*sizeof(real_t));

    window_long       = fb->long_window[window_shape];
    window_long_prev  = fb->long_window[window_shape_prev];
    window_short      = fb->short_window[window_shape];
    window_short_prev = fb->short_window[window_shape_prev];

    switch (window_sequence)
    {
    case ONLY_LONG_SEQUENCE:
        imdct_ssr(fb, freq_in, transf_buf, 2*nlong);
        for (i = nlong-1; i >= 0; i--)
        {
            time_out[i] = MUL_R_C(transf_buf[i],window_long_prev[i]);
            time_out[nlong+i] = MUL_R_C(transf_buf[nlong+i],window_long[nlong-1-i]);
        }
        break;

    case LONG_START_SEQUENCE:
        imdct_ssr(fb, freq_in, transf_buf, 2*nlong);
        for (i = 0; i < nlong; i++)
            time_out[i] = MUL_R_C(transf_buf[i],window_long_prev[i]);
        for (i = 0; i < nflat_ls; i++)
            time_out[nlong+i] = transf_buf[nlong+i];
        for (i = 0; i < nshort; i++)
            time_out[nlong+nflat_ls+i] = MUL_R_C(transf_buf[nlong+nflat_ls+i],window_short[nshort-i-1]);
        for (i = 0; i < nflat_ls; i++)
            time_out[nlong+nflat_ls+nshort+i] = 0;
        break;

    case EIGHT_SHORT_SEQUENCE:
        imdct_ssr(fb, freq_in+0*nshort, transf_buf+2*nshort*0, 2*nshort);
        imdct_ssr(fb, freq_in+1*nshort, transf_buf+2*nshort*1, 2*nshort);
        imdct_ssr(fb, freq_in+2*nshort, transf_buf+2*nshort*2, 2*nshort);
        imdct_ssr(fb, freq_in+3*nshort, transf_buf+2*nshort*3, 2*nshort);
        imdct_ssr(fb, freq_in+4*nshort, transf_buf+2*nshort*4, 2*nshort);
        imdct_ssr(fb, freq_in+5*nshort, transf_buf+2*nshort*5, 2*nshort);
        imdct_ssr(fb, freq_in+6*nshort, transf_buf+2*nshort*6, 2*nshort);
        imdct_ssr(fb, freq_in+7*nshort, transf_buf+2*nshort*7, 2*nshort);
        for(i = nshort-1; i >= 0; i--)
        {
            time_out[i+0*nshort] = MUL_R_C(transf_buf[nshort*0+i],window_short_prev[i]);
            time_out[i+1*nshort] = MUL_R_C(transf_buf[nshort*1+i],window_short[i]);
            time_out[i+2*nshort] = MUL_R_C(transf_buf[nshort*2+i],window_short_prev[i]);
            time_out[i+3*nshort] = MUL_R_C(transf_buf[nshort*3+i],window_short[i]);
            time_out[i+4*nshort] = MUL_R_C(transf_buf[nshort*4+i],window_short_prev[i]);
            time_out[i+5*nshort] = MUL_R_C(transf_buf[nshort*5+i],window_short[i]);
            time_out[i+6*nshort] = MUL_R_C(transf_buf[nshort*6+i],window_short_prev[i]);
            time_out[i+7*nshort] = MUL_R_C(transf_buf[nshort*7+i],window_short[i]);
            time_out[i+8*nshort] = MUL_R_C(transf_buf[nshort*8+i],window_short_prev[i]);
            time_out[i+9*nshort] = MUL_R_C(transf_buf[nshort*9+i],window_short[i]);
            time_out[i+10*nshort] = MUL_R_C(transf_buf[nshort*10+i],window_short_prev[i]);
            time_out[i+11*nshort] = MUL_R_C(transf_buf[nshort*11+i],window_short[i]);
            time_out[i+12*nshort] = MUL_R_C(transf_buf[nshort*12+i],window_short_prev[i]);
            time_out[i+13*nshort] = MUL_R_C(transf_buf[nshort*13+i],window_short[i]);
            time_out[i+14*nshort] = MUL_R_C(transf_buf[nshort*14+i],window_short_prev[i]);
            time_out[i+15*nshort] = MUL_R_C(transf_buf[nshort*15+i],window_short[i]);
        }
        break;

    case LONG_STOP_SEQUENCE:
        imdct_ssr(fb, freq_in, transf_buf, 2*nlong);
        for (i = 0; i < nflat_ls; i++)
            time_out[i] = 0;
        for (i = 0; i < nshort; i++)
            time_out[nflat_ls+i] = MUL_R_C(transf_buf[nflat_ls+i],window_short_prev[i]);
        for (i = 0; i < nflat_ls; i++)
            time_out[nflat_ls+nshort+i] = transf_buf[nflat_ls+nshort+i];
        for (i = 0; i < nlong; i++)
            time_out[nlong+i] = MUL_R_C(transf_buf[nlong+i],window_long[nlong-1-i]);
		break;
    }

    faad_free(transf_buf);
}


#endif
