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
** $Id: filtbank.c,v 1.2 2003/10/03 22:22:27 alex Exp $
**/

#include "common.h"
#include "structs.h"

#include <stdlib.h>
#include <string.h>
#ifdef _WIN32_WCE
#define assert(x)
#else
#include <assert.h>
#endif

#include "filtbank.h"
#include "decoder.h"
#include "syntax.h"
#include "kbd_win.h"
#include "sine_win.h"
#include "mdct.h"


fb_info *filter_bank_init(uint16_t frame_len)
{
    uint16_t nshort = frame_len/8;
#ifdef LD_DEC
    uint16_t frame_len_ld = frame_len/2;
#endif

    fb_info *fb = (fb_info*)faad_malloc(sizeof(fb_info));
    memset(fb, 0, sizeof(fb_info));

    /* normal */
    fb->mdct256 = faad_mdct_init(2*nshort);
    fb->mdct2048 = faad_mdct_init(2*frame_len);
#ifdef LD_DEC
    /* LD */
    fb->mdct1024 = faad_mdct_init(2*frame_len_ld);
#endif

#ifdef ALLOW_SMALL_FRAMELENGTH
    if (frame_len == 1024)
    {
#endif
        fb->long_window[0]  = sine_long_1024;
        fb->short_window[0] = sine_short_128;
        fb->long_window[1]  = kbd_long_1024;
        fb->short_window[1] = kbd_short_128;
#ifdef LD_DEC
        fb->ld_window[0] = sine_mid_512;
        fb->ld_window[1] = ld_mid_512;
#endif
#ifdef ALLOW_SMALL_FRAMELENGTH
    } else /* (frame_len == 960) */ {
        fb->long_window[0]  = sine_long_960;
        fb->short_window[0] = sine_short_120;
        fb->long_window[1]  = kbd_long_960;
        fb->short_window[1] = kbd_short_120;
#ifdef LD_DEC
        fb->ld_window[0] = sine_mid_480;
        fb->ld_window[1] = ld_mid_480;
#endif
    }
#endif

#ifdef USE_SSE
    if (cpu_has_sse())
    {
        fb->if_func = ifilter_bank_sse;
    } else {
        fb->if_func = ifilter_bank;
    }
#endif

    return fb;
}

void filter_bank_end(fb_info *fb)
{
    if (fb != NULL)
    {
#ifdef PROFILE
        printf("FB:                 %I64d cycles\n", fb->cycles);
#endif

        faad_mdct_end(fb->mdct256);
        faad_mdct_end(fb->mdct2048);
#ifdef LD_DEC
        faad_mdct_end(fb->mdct1024);
#endif

        faad_free(fb);
    }
}

static INLINE void imdct_long(fb_info *fb, real_t *in_data, real_t *out_data, uint16_t len)
{
#ifdef LD_DEC
    mdct_info *mdct = NULL;

    switch (len)
    {
    case 2048:
    case 1920:
        mdct = fb->mdct2048;
        break;
    case 1024:
    case 960:
        mdct = fb->mdct1024;
        break;
    }

    faad_imdct(mdct, in_data, out_data);
#else
    faad_imdct(fb->mdct2048, in_data, out_data);
#endif
}

#ifdef USE_SSE
static INLINE void imdct_long_sse(fb_info *fb, real_t *in_data, real_t *out_data, uint16_t len)
{
#ifdef LD_DEC
    mdct_info *mdct = NULL;

    switch (len)
    {
    case 2048:
    case 1920:
        mdct = fb->mdct2048;
        break;
    case 1024:
    case 960:
        mdct = fb->mdct1024;
        break;
    }

    faad_imdct_sse(mdct, in_data, out_data);
#else
    faad_imdct_sse(fb->mdct2048, in_data, out_data);
#endif
}
#endif

#ifdef LTP_DEC
static INLINE void mdct(fb_info *fb, real_t *in_data, real_t *out_data, uint16_t len)
{
    mdct_info *mdct = NULL;

    switch (len)
    {
    case 2048:
    case 1920:
        mdct = fb->mdct2048;
        break;
    case 256:
    case 240:
        mdct = fb->mdct256;
        break;
#ifdef LD_DEC
    case 1024:
    case 960:
        mdct = fb->mdct1024;
        break;
#endif
    }

    faad_mdct(mdct, in_data, out_data);
}
#endif

void ifilter_bank(fb_info *fb, uint8_t window_sequence, uint8_t window_shape,
                  uint8_t window_shape_prev, real_t *freq_in,
                  real_t *time_out, real_t *overlap,
                  uint8_t object_type, uint16_t frame_len)
{
    int16_t i;
    ALIGN real_t transf_buf[2*1024] = {0};

    const real_t *window_long = NULL;
    const real_t *window_long_prev = NULL;
    const real_t *window_short = NULL;
    const real_t *window_short_prev = NULL;

    uint16_t nlong = frame_len;
    uint16_t nshort = frame_len/8;
    uint16_t trans = nshort/2;

    uint16_t nflat_ls = (nlong-nshort)/2;

#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif

#ifdef LD_DEC
    if (object_type == LD)
    {
        window_long       = fb->ld_window[window_shape];
        window_long_prev  = fb->ld_window[window_shape_prev];
    } else {
#endif
        window_long       = fb->long_window[window_shape];
        window_long_prev  = fb->long_window[window_shape_prev];
        window_short      = fb->short_window[window_shape];
        window_short_prev = fb->short_window[window_shape_prev];
#ifdef LD_DEC
    }
#endif


    switch (window_sequence)
    {
    case ONLY_LONG_SEQUENCE:
        imdct_long(fb, freq_in, transf_buf, 2*nlong);
        for (i = 0; i < nlong; i+=4)
        {
            time_out[i]   = overlap[i]   + MUL_F(transf_buf[i],window_long_prev[i]);
            time_out[i+1] = overlap[i+1] + MUL_F(transf_buf[i+1],window_long_prev[i+1]);
            time_out[i+2] = overlap[i+2] + MUL_F(transf_buf[i+2],window_long_prev[i+2]);
            time_out[i+3] = overlap[i+3] + MUL_F(transf_buf[i+3],window_long_prev[i+3]);
        }
        for (i = 0; i < nlong; i+=4)
        {
            overlap[i]   = MUL_F(transf_buf[nlong+i],window_long[nlong-1-i]);
            overlap[i+1] = MUL_F(transf_buf[nlong+i+1],window_long[nlong-2-i]);
            overlap[i+2] = MUL_F(transf_buf[nlong+i+2],window_long[nlong-3-i]);
            overlap[i+3] = MUL_F(transf_buf[nlong+i+3],window_long[nlong-4-i]);
        }
        break;

    case LONG_START_SEQUENCE:
        imdct_long(fb, freq_in, transf_buf, 2*nlong);
        for (i = 0; i < nlong; i+=4)
        {
            time_out[i]   = overlap[i]   + MUL_F(transf_buf[i],window_long_prev[i]);
            time_out[i+1] = overlap[i+1] + MUL_F(transf_buf[i+1],window_long_prev[i+1]);
            time_out[i+2] = overlap[i+2] + MUL_F(transf_buf[i+2],window_long_prev[i+2]);
            time_out[i+3] = overlap[i+3] + MUL_F(transf_buf[i+3],window_long_prev[i+3]);
        }
        for (i = 0; i < nflat_ls; i++)
            overlap[i] = transf_buf[nlong+i];
        for (i = 0; i < nshort; i++)
            overlap[nflat_ls+i] = MUL_F(transf_buf[nlong+nflat_ls+i],window_short[nshort-i-1]);
        for (i = 0; i < nflat_ls; i++)
            overlap[nflat_ls+nshort+i] = 0;
        break;

    case EIGHT_SHORT_SEQUENCE:
        faad_imdct(fb->mdct256, freq_in+0*nshort, transf_buf+2*nshort*0);
        faad_imdct(fb->mdct256, freq_in+1*nshort, transf_buf+2*nshort*1);
        faad_imdct(fb->mdct256, freq_in+2*nshort, transf_buf+2*nshort*2);
        faad_imdct(fb->mdct256, freq_in+3*nshort, transf_buf+2*nshort*3);
        faad_imdct(fb->mdct256, freq_in+4*nshort, transf_buf+2*nshort*4);
        faad_imdct(fb->mdct256, freq_in+5*nshort, transf_buf+2*nshort*5);
        faad_imdct(fb->mdct256, freq_in+6*nshort, transf_buf+2*nshort*6);
        faad_imdct(fb->mdct256, freq_in+7*nshort, transf_buf+2*nshort*7);
        for (i = 0; i < nflat_ls; i++)
            time_out[i] = overlap[i];
        for(i = 0; i < nshort; i++)
        {
            time_out[nflat_ls+         i] = overlap[nflat_ls+         i] + MUL_F(transf_buf[nshort*0+i],window_short_prev[i]);
            time_out[nflat_ls+1*nshort+i] = overlap[nflat_ls+nshort*1+i] + MUL_F(transf_buf[nshort*1+i],window_short[nshort-1-i]) + MUL_F(transf_buf[nshort*2+i],window_short[i]);
            time_out[nflat_ls+2*nshort+i] = overlap[nflat_ls+nshort*2+i] + MUL_F(transf_buf[nshort*3+i],window_short[nshort-1-i]) + MUL_F(transf_buf[nshort*4+i],window_short[i]);
            time_out[nflat_ls+3*nshort+i] = overlap[nflat_ls+nshort*3+i] + MUL_F(transf_buf[nshort*5+i],window_short[nshort-1-i]) + MUL_F(transf_buf[nshort*6+i],window_short[i]);
            if (i < trans)
                time_out[nflat_ls+4*nshort+i] = overlap[nflat_ls+nshort*4+i] + MUL_F(transf_buf[nshort*7+i],window_short[nshort-1-i]) + MUL_F(transf_buf[nshort*8+i],window_short[i]);
        }
        for(i = 0; i < nshort; i++)
        {
            if (i >= trans)
                overlap[nflat_ls+4*nshort+i-nlong] = MUL_F(transf_buf[nshort*7+i],window_short[nshort-1-i]) + MUL_F(transf_buf[nshort*8+i],window_short[i]);
            overlap[nflat_ls+5*nshort+i-nlong] = MUL_F(transf_buf[nshort*9+i],window_short[nshort-1-i]) + MUL_F(transf_buf[nshort*10+i],window_short[i]);
            overlap[nflat_ls+6*nshort+i-nlong] = MUL_F(transf_buf[nshort*11+i],window_short[nshort-1-i]) + MUL_F(transf_buf[nshort*12+i],window_short[i]);
            overlap[nflat_ls+7*nshort+i-nlong] = MUL_F(transf_buf[nshort*13+i],window_short[nshort-1-i]) + MUL_F(transf_buf[nshort*14+i],window_short[i]);
            overlap[nflat_ls+8*nshort+i-nlong] = MUL_F(transf_buf[nshort*15+i],window_short[nshort-1-i]);
        }
        for (i = 0; i < nflat_ls; i++)
            overlap[nflat_ls+nshort+i] = 0;
        break;

    case LONG_STOP_SEQUENCE:
        imdct_long(fb, freq_in, transf_buf, 2*nlong);
        for (i = 0; i < nflat_ls; i++)
            time_out[i] = overlap[i];
        for (i = 0; i < nshort; i++)
            time_out[nflat_ls+i] = overlap[nflat_ls+i] + MUL_F(transf_buf[nflat_ls+i],window_short_prev[i]);
        for (i = 0; i < nflat_ls; i++)
            time_out[nflat_ls+nshort+i] = overlap[nflat_ls+nshort+i] + transf_buf[nflat_ls+nshort+i];
        for (i = 0; i < nlong; i++)
            overlap[i] = MUL_F(transf_buf[nlong+i],window_long[nlong-1-i]);
		break;
    }

#ifdef PROFILE
    count = faad_get_ts() - count;
    fb->cycles += count;
#endif
}

#ifdef USE_SSE
void ifilter_bank_sse(fb_info *fb, uint8_t window_sequence, uint8_t window_shape,
                      uint8_t window_shape_prev, real_t *freq_in,
                      real_t *time_out, uint8_t object_type, uint16_t frame_len)
{
    int16_t i;
    ALIGN real_t transf_buf[2*1024] = {0};

    const real_t *window_long = NULL;
    const real_t *window_long_prev = NULL;
    const real_t *window_short = NULL;
    const real_t *window_short_prev = NULL;

    uint16_t nlong = frame_len;
    uint16_t nshort = frame_len/8;
    uint16_t trans = nshort/2;

    uint16_t nflat_ls = (nlong-nshort)/2;

#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif

#ifdef LD_DEC
    if (object_type == LD)
    {
        window_long       = fb->ld_window[window_shape];
        window_long_prev  = fb->ld_window[window_shape_prev];
    } else {
#endif
        window_long       = fb->long_window[window_shape];
        window_long_prev  = fb->long_window[window_shape_prev];
        window_short      = fb->short_window[window_shape];
        window_short_prev = fb->short_window[window_shape_prev];
#ifdef LD_DEC
    }
#endif

    switch (window_sequence)
    {
    case ONLY_LONG_SEQUENCE:
        imdct_long_sse(fb, freq_in, transf_buf, 2*nlong);
        for (i = 0; i < nlong; i+=4)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8;

            m1 = _mm_load_ps(&transf_buf[i]);
            m2 = _mm_load_ps(&window_long_prev[i]);
            m6 = _mm_load_ps(&window_long[nlong-4-i]);
            m3 = _mm_load_ps(&time_out[nlong+i]);
            m5 = _mm_load_ps(&transf_buf[nlong+i]);

            m4 = _mm_mul_ps(m1, m2);
            m7 = _mm_shuffle_ps(m6, m6, _MM_SHUFFLE(0, 1, 2, 3));

            m4 = _mm_add_ps(m4, m3);
            m8 = _mm_mul_ps(m5, m7);

            _mm_store_ps(&time_out[i], m4);
            _mm_store_ps(&time_out[nlong+i], m8);
        }
        break;

    case LONG_START_SEQUENCE:
        imdct_long_sse(fb, freq_in, transf_buf, 2*nlong);
        for (i = 0; i < nlong; i+=4)
        {
            __m128 m1 = _mm_load_ps(&transf_buf[i]);
            __m128 m2 = _mm_load_ps(&window_long_prev[i]);
            __m128 m3 = _mm_load_ps(&time_out[nlong+i]);

            __m128 m4 = _mm_mul_ps(m1, m2);
            m4 = _mm_add_ps(m4, m3);

            _mm_store_ps(&time_out[i], m4);
        }
        for (i = 0; i < nflat_ls; i+=4)
        {
            __m128 m1 = _mm_load_ps(&transf_buf[nlong+i]);
            _mm_store_ps(&time_out[nlong+i], m1);
        }
        for (i = 0; i < nshort; i+=4)
        {
            __m128 m1 = _mm_load_ps(&transf_buf[nlong+nflat_ls+i]);
            __m128 m2 = _mm_load_ps(&window_short[nshort-4-i]);
            __m128 m3, m4;

            m3 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(0, 1, 2, 3));

            m4 = _mm_mul_ps(m1, m3);

            _mm_store_ps(&time_out[nlong+nflat_ls+i], m4);
        }
        for (i = 0; i < nflat_ls; i+=4)
        {
            __m128 m1 = _mm_setzero_ps();
            _mm_store_ps(&time_out[nlong+nflat_ls+nshort+i], m1);
        }
        break;

    case EIGHT_SHORT_SEQUENCE:
        faad_imdct_sse(fb->mdct256, &freq_in[0*nshort], &transf_buf[2*nshort*0]);
        faad_imdct_sse(fb->mdct256, &freq_in[1*nshort], &transf_buf[2*nshort*1]);
        faad_imdct_sse(fb->mdct256, &freq_in[2*nshort], &transf_buf[2*nshort*2]);
        faad_imdct_sse(fb->mdct256, &freq_in[3*nshort], &transf_buf[2*nshort*3]);
        faad_imdct_sse(fb->mdct256, &freq_in[4*nshort], &transf_buf[2*nshort*4]);
        faad_imdct_sse(fb->mdct256, &freq_in[5*nshort], &transf_buf[2*nshort*5]);
        faad_imdct_sse(fb->mdct256, &freq_in[6*nshort], &transf_buf[2*nshort*6]);
        faad_imdct_sse(fb->mdct256, &freq_in[7*nshort], &transf_buf[2*nshort*7]);
        for (i = 0; i < nflat_ls; i+=4)
        {
            __m128 m1 = _mm_load_ps(&time_out[nlong+i]);
            _mm_store_ps(&time_out[i], m1);
        }
        for (i = 0; i < nshort; i+=4)
        {
            __m128 m1 = _mm_load_ps(&transf_buf[nshort*0+i]);
            __m128 m2 = _mm_load_ps(&window_short_prev[i]);
            __m128 m3 = _mm_load_ps(&time_out[nlong+nflat_ls+i]);

            __m128 m4 = _mm_mul_ps(m1, m2);
            m4 = _mm_add_ps(m4, m3);

            _mm_store_ps(&time_out[nflat_ls+i], m4);
        }
        for (i = 0; i < nshort; i+=4)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8;
            m1 = _mm_load_ps(&transf_buf[nshort*1+i]);
            m2 = _mm_load_ps(&window_short[nshort-4-i]);
            m3 = _mm_load_ps(&time_out[nlong+nflat_ls+nshort*1+i]);
            m6 = _mm_load_ps(&transf_buf[nshort*2+i]);
            m7 = _mm_load_ps(&window_short[i]);

            m5 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(0, 1, 2, 3));

            m4 = _mm_mul_ps(m1, m5);
            m8 = _mm_mul_ps(m6, m7);
            m4 = _mm_add_ps(m4, m3);
            m4 = _mm_add_ps(m4, m8);

            _mm_store_ps(&time_out[nflat_ls+1*nshort+i], m4);
        }
        for (i = 0; i < nshort; i+=4)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8;
            m1 = _mm_load_ps(&transf_buf[nshort*3+i]);
            m2 = _mm_load_ps(&window_short[nshort-4-i]);
            m3 = _mm_load_ps(&time_out[nlong+nflat_ls+nshort*2+i]);
            m6 = _mm_load_ps(&transf_buf[nshort*4+i]);
            m7 = _mm_load_ps(&window_short[i]);

            m5 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(0, 1, 2, 3));

            m4 = _mm_mul_ps(m1, m5);
            m8 = _mm_mul_ps(m6, m7);
            m4 = _mm_add_ps(m4, m3);
            m4 = _mm_add_ps(m4, m8);

            _mm_store_ps(&time_out[nflat_ls+2*nshort+i], m4);
        }
        for (i = 0; i < nshort; i+=4)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8;
            m1 = _mm_load_ps(&transf_buf[nshort*5+i]);
            m2 = _mm_load_ps(&window_short[nshort-4-i]);
            m3 = _mm_load_ps(&time_out[nlong+nflat_ls+nshort*3+i]);
            m6 = _mm_load_ps(&transf_buf[nshort*6+i]);
            m7 = _mm_load_ps(&window_short[i]);

            m5 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(0, 1, 2, 3));

            m4 = _mm_mul_ps(m1, m5);
            m8 = _mm_mul_ps(m6, m7);
            m4 = _mm_add_ps(m4, m3);
            m4 = _mm_add_ps(m4, m8);

            _mm_store_ps(&time_out[nflat_ls+3*nshort+i], m4);
        }
        for(i = 0; i < trans; i+=4)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8;
            m1 = _mm_load_ps(&transf_buf[nshort*7+i]);
            m2 = _mm_load_ps(&window_short[nshort-4-i]);
            m3 = _mm_load_ps(&time_out[nlong+nflat_ls+nshort*4+i]);
            m6 = _mm_load_ps(&transf_buf[nshort*8+i]);
            m7 = _mm_load_ps(&window_short[i]);

            m5 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(0, 1, 2, 3));

            m4 = _mm_mul_ps(m1, m5);
            m8 = _mm_mul_ps(m6, m7);
            m4 = _mm_add_ps(m4, m3);
            m4 = _mm_add_ps(m4, m8);

            _mm_store_ps(&time_out[nflat_ls+4*nshort+i], m4);
        }
        for (i = trans; i < nshort; i+=4)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8;
            m1 = _mm_load_ps(&transf_buf[nshort*7+i]);
            m2 = _mm_load_ps(&window_short[nshort-4-i]);
            m6 = _mm_load_ps(&transf_buf[nshort*8+i]);
            m7 = _mm_load_ps(&window_short[i]);

            m5 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(0, 1, 2, 3));

            m4 = _mm_mul_ps(m1, m5);
            m8 = _mm_mul_ps(m6, m7);
            m3 = _mm_add_ps(m4, m8);

            _mm_store_ps(&time_out[nflat_ls+4*nshort+i], m3);
        }
        for (i = 0; i < nshort; i+=4)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8;
            m1 = _mm_load_ps(&transf_buf[nshort*9+i]);
            m2 = _mm_load_ps(&window_short[nshort-4-i]);
            m6 = _mm_load_ps(&transf_buf[nshort*10+i]);
            m7 = _mm_load_ps(&window_short[i]);

            m5 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(0, 1, 2, 3));

            m4 = _mm_mul_ps(m1, m5);
            m8 = _mm_mul_ps(m6, m7);
            m3 = _mm_add_ps(m4, m8);

            _mm_store_ps(&time_out[nflat_ls+5*nshort+i], m3);
        }
        for (i = 0; i < nshort; i+=4)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8;
            m1 = _mm_load_ps(&transf_buf[nshort*11+i]);
            m2 = _mm_load_ps(&window_short[nshort-4-i]);
            m6 = _mm_load_ps(&transf_buf[nshort*12+i]);
            m7 = _mm_load_ps(&window_short[i]);

            m5 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(0, 1, 2, 3));

            m4 = _mm_mul_ps(m1, m5);
            m8 = _mm_mul_ps(m6, m7);
            m3 = _mm_add_ps(m4, m8);

            _mm_store_ps(&time_out[nflat_ls+6*nshort+i], m3);
        }
        for (i = 0; i < nshort; i+=4)
        {
            __m128 m1, m2, m3, m4, m5, m6, m7, m8;
            m1 = _mm_load_ps(&transf_buf[nshort*13+i]);
            m2 = _mm_load_ps(&window_short[nshort-4-i]);
            m6 = _mm_load_ps(&transf_buf[nshort*14+i]);
            m7 = _mm_load_ps(&window_short[i]);

            m5 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(0, 1, 2, 3));

            m4 = _mm_mul_ps(m1, m5);
            m8 = _mm_mul_ps(m6, m7);
            m3 = _mm_add_ps(m4, m8);

            _mm_store_ps(&time_out[nflat_ls+7*nshort+i], m3);
        }
        for (i = 0; i < nshort; i+=4)
        {
            __m128 m1, m2, m3, m5;
            m1 = _mm_load_ps(&transf_buf[nshort*15+i]);
            m2 = _mm_load_ps(&window_short[nshort-4-i]);

            m5 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(0, 1, 2, 3));

            m3 = _mm_mul_ps(m1, m5);

            _mm_store_ps(&time_out[nflat_ls+8*nshort+i], m3);
        }
        for (i = 0; i < nflat_ls; i+=4)
        {
            __m128 m1 = _mm_setzero_ps();
            _mm_store_ps(&time_out[nlong+nflat_ls+nshort+i], m1);
        }
        break;

    case LONG_STOP_SEQUENCE:
        imdct_long_sse(fb, freq_in, transf_buf, 2*nlong);
        for (i = 0; i < nflat_ls; i+=4)
        {
            __m128 m1 = _mm_load_ps(&time_out[nlong+i]);
            _mm_store_ps(&time_out[i], m1);
        }
        for (i = 0; i < nshort; i+=4)
        {
            __m128 m1 = _mm_load_ps(&transf_buf[nflat_ls+i]);
            __m128 m2 = _mm_load_ps(&window_short_prev[i]);
            __m128 m3 = _mm_load_ps(&time_out[nlong+nflat_ls+i]);

            __m128 m4 = _mm_mul_ps(m1, m2);
            m4 = _mm_add_ps(m4, m3);

            _mm_store_ps(&time_out[nflat_ls+i], m4);
        }
        for (i = 0; i < nflat_ls; i+=4)
        {
            __m128 m1 = _mm_load_ps(&transf_buf[nflat_ls+nshort+i]);
            __m128 m2 = _mm_load_ps(&time_out[nlong+nflat_ls+nshort+i]);

            __m128 m3 = _mm_add_ps(m1, m2);

            _mm_store_ps(&time_out[nflat_ls+nshort+i], m3);
        }
        for (i = 0; i < nlong; i+=4)
        {
            __m128 m1 = _mm_load_ps(&transf_buf[nlong+i]);
            __m128 m2 = _mm_load_ps(&window_long[nlong-4-i]);
            __m128 m3, m4;

            m3 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(0, 1, 2, 3));

            m4 = _mm_mul_ps(m1, m3);

            _mm_store_ps(&time_out[nlong+i], m4);
        }
		break;
    }

#ifdef PROFILE
    count = faad_get_ts() - count;
    fb->cycles += count;
#endif
}
#endif

#ifdef LTP_DEC
/* only works for LTP -> no overlapping, no short blocks */
void filter_bank_ltp(fb_info *fb, uint8_t window_sequence, uint8_t window_shape,
                     uint8_t window_shape_prev, real_t *in_data, real_t *out_mdct,
                     uint8_t object_type, uint16_t frame_len)
{
    int16_t i;
    ALIGN real_t windowed_buf[2*1024] = {0};

    const real_t *window_long = NULL;
    const real_t *window_long_prev = NULL;
    const real_t *window_short = NULL;
    const real_t *window_short_prev = NULL;

    uint16_t nlong = frame_len;
    uint16_t nshort = frame_len/8;
    uint16_t nflat_ls = (nlong-nshort)/2;

    assert(window_sequence != EIGHT_SHORT_SEQUENCE);

#ifdef LD_DEC
    if (object_type == LD)
    {
        window_long       = fb->ld_window[window_shape];
        window_long_prev  = fb->ld_window[window_shape_prev];
    } else {
#endif
        window_long       = fb->long_window[window_shape];
        window_long_prev  = fb->long_window[window_shape_prev];
        window_short      = fb->short_window[window_shape];
        window_short_prev = fb->short_window[window_shape_prev];
#ifdef LD_DEC
    }
#endif

    switch(window_sequence)
    {
    case ONLY_LONG_SEQUENCE:
        for (i = nlong-1; i >= 0; i--)
        {
            windowed_buf[i] = MUL_F(in_data[i], window_long_prev[i]);
            windowed_buf[i+nlong] = MUL_F(in_data[i+nlong], window_long[nlong-1-i]);
        }
        mdct(fb, windowed_buf, out_mdct, 2*nlong);
        break;

    case LONG_START_SEQUENCE:
        for (i = 0; i < nlong; i++)
            windowed_buf[i] = MUL_F(in_data[i], window_long_prev[i]);
        for (i = 0; i < nflat_ls; i++)
            windowed_buf[i+nlong] = in_data[i+nlong];
        for (i = 0; i < nshort; i++)
            windowed_buf[i+nlong+nflat_ls] = MUL_F(in_data[i+nlong+nflat_ls], window_short[nshort-1-i]);
        for (i = 0; i < nflat_ls; i++)
            windowed_buf[i+nlong+nflat_ls+nshort] = 0;
        mdct(fb, windowed_buf, out_mdct, 2*nlong);
        break;

    case LONG_STOP_SEQUENCE:
        for (i = 0; i < nflat_ls; i++)
            windowed_buf[i] = 0;
        for (i = 0; i < nshort; i++)
            windowed_buf[i+nflat_ls] = MUL_F(in_data[i+nflat_ls], window_short_prev[i]);
        for (i = 0; i < nflat_ls; i++)
            windowed_buf[i+nflat_ls+nshort] = in_data[i+nflat_ls+nshort];
        for (i = 0; i < nlong; i++)
            windowed_buf[i+nlong] = MUL_F(in_data[i+nlong], window_long[nlong-1-i]);
        mdct(fb, windowed_buf, out_mdct, 2*nlong);
        break;
    }
}
#endif
