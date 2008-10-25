/*
 * common functions for reordering audio channels
 *
 * Copyright (C) 2007 Ulion <ulion A gmail P com>
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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "libvo/fastmemcpy.h"

#include "reorder_ch.h"

#ifdef TEST
#define mp_msg(mod,lev, fmt, args... )  printf( fmt, ## args )
#else
#include "mp_msg.h"
#endif


#define REORDER_COPY_5(DEST,SRC,SAMPLES,S0,S1,S2,S3,S4) \
for (i = 0; i < SAMPLES; i += 5) {\
    DEST[i]   = SRC[i+S0];\
    DEST[i+1] = SRC[i+S1];\
    DEST[i+2] = SRC[i+S2];\
    DEST[i+3] = SRC[i+S3];\
    DEST[i+4] = SRC[i+S4];\
}

static int reorder_copy_5ch(void *dest, const void *src,
                            unsigned int samples, unsigned int samplesize,
                            int s0, int s1, int s2, int s3, int s4)
{
    int i;
    switch (samplesize) {
    case 1:
    {
        int8_t *dest_8 = dest;
        const int8_t *src_8 = src;
        REORDER_COPY_5(dest_8,src_8,samples,s0,s1,s2,s3,s4);
        break;
    }
    case 2:
    {
        int16_t *dest_16 = dest;
        const int16_t *src_16 = src;
        REORDER_COPY_5(dest_16,src_16,samples,s0,s1,s2,s3,s4);
        break;
    }
    case 3:
    {
        int8_t *dest_8 = dest;
        const int8_t *src_8 = src;
        for (i = 0; i < samples; i += 15) {
            dest_8[i]    = src_8[i+s0*3];
            dest_8[i+1]  = src_8[i+s0*3+1];
            dest_8[i+2]  = src_8[i+s0*3+2];
            dest_8[i+3]  = src_8[i+s1*3];
            dest_8[i+4]  = src_8[i+s1*3+1];
            dest_8[i+5]  = src_8[i+s1*3+2];
            dest_8[i+6]  = src_8[i+s2*3];
            dest_8[i+7]  = src_8[i+s2*3+1];
            dest_8[i+8]  = src_8[i+s2*3+2];
            dest_8[i+9]  = src_8[i+s3*3];
            dest_8[i+10] = src_8[i+s3*3+1];
            dest_8[i+11] = src_8[i+s3*3+2];
            dest_8[i+12] = src_8[i+s4*3];
            dest_8[i+13] = src_8[i+s4*3+1];
            dest_8[i+14] = src_8[i+s4*3+2];
        }
    }
    case 4:
    {
        int32_t *dest_32 = dest;
        const int32_t *src_32 = src;
        REORDER_COPY_5(dest_32,src_32,samples,s0,s1,s2,s3,s4);
        break;
    }
    case 8:
    {
        int64_t *dest_64 = dest;
        const int64_t *src_64 = src;
        REORDER_COPY_5(dest_64,src_64,samples,s0,s1,s2,s3,s4);
        break;
    }
    default:
        mp_msg(MSGT_GLOBAL, MSGL_WARN,
               "[reorder_ch] Unsupported sample size: %d, please "
               "report this error on the MPlayer mailing list.\n",samplesize);
        return 0;
    }
    return 1;
}

#define REORDER_COPY_6(DEST,SRC,SAMPLES,S0,S1,S2,S3,S4,S5) \
for (i = 0; i < SAMPLES; i += 6) {\
    DEST[i]   = SRC[i+S0];\
    DEST[i+1] = SRC[i+S1];\
    DEST[i+2] = SRC[i+S2];\
    DEST[i+3] = SRC[i+S3];\
    DEST[i+4] = SRC[i+S4];\
    DEST[i+5] = SRC[i+S5];\
}

static int reorder_copy_6ch(void *dest, const void *src,
                            unsigned int samples, uint8_t samplesize,
                            int s0, int s1, int s2, int s3, int s4, int s5)
{
    int i;
    switch (samplesize) {
    case 1:
    {
        int8_t *dest_8 = dest;
        const int8_t *src_8 = src;
        REORDER_COPY_6(dest_8,src_8,samples,s0,s1,s2,s3,s4,s5);
        break;
    }
    case 2:
    {
        int16_t *dest_16 = dest;
        const int16_t *src_16 = src;
        REORDER_COPY_6(dest_16,src_16,samples,s0,s1,s2,s3,s4,s5);
        break;
    }
    case 3:
    {
        int8_t *dest_8 = dest;
        const int8_t *src_8 = src;
        for (i = 0; i < samples; i += 18) {
            dest_8[i]    = src_8[i+s0*3];
            dest_8[i+1]  = src_8[i+s0*3+1];
            dest_8[i+2]  = src_8[i+s0*3+2];
            dest_8[i+3]  = src_8[i+s1*3];
            dest_8[i+4]  = src_8[i+s1*3+1];
            dest_8[i+5]  = src_8[i+s1*3+2];
            dest_8[i+6]  = src_8[i+s2*3];
            dest_8[i+7]  = src_8[i+s2*3+1];
            dest_8[i+8]  = src_8[i+s2*3+2];
            dest_8[i+9]  = src_8[i+s3*3];
            dest_8[i+10] = src_8[i+s3*3+1];
            dest_8[i+11] = src_8[i+s3*3+2];
            dest_8[i+12] = src_8[i+s4*3];
            dest_8[i+13] = src_8[i+s4*3+1];
            dest_8[i+14] = src_8[i+s4*3+2];
            dest_8[i+15] = src_8[i+s5*3];
            dest_8[i+16] = src_8[i+s5*3+1];
            dest_8[i+17] = src_8[i+s5*3+2];
        }
    }
    case 4:
    {
        int32_t *dest_32 = dest;
        const int32_t *src_32 = src;
        REORDER_COPY_6(dest_32,src_32,samples,s0,s1,s2,s3,s4,s5);
        break;
    }
    case 8:
    {
        int64_t *dest_64 = dest;
        const int64_t *src_64 = src;
        REORDER_COPY_6(dest_64,src_64,samples,s0,s1,s2,s3,s4,s5);
        break;
    }
    default:
        mp_msg(MSGT_GLOBAL, MSGL_WARN,
               "[reorder_ch] Unsupported sample size: %d, please "
               "report this error on the MPlayer mailing list.\n",samplesize);
        return 0;
    }
    return 1;
}

void reorder_channel_copy(void *src,
                          int src_layout,
                          void *dest,
                          int dest_layout,
                          int samples,
                          int samplesize)
{
    if (dest_layout==src_layout) {
        fast_memcpy(dest, src, samples*samplesize);
        return;
    }
    if (!AF_IS_SAME_CH_NUM(dest_layout,src_layout)) {
        mp_msg(MSGT_GLOBAL, MSGL_WARN, "[reorder_ch] different channel count "
               "between src and dest: %x, %x\n",
               AF_GET_CH_NUM_WITH_LFE(src_layout),
               AF_GET_CH_NUM_WITH_LFE(dest_layout));
        return;
    }
    switch ((src_layout<<16)|dest_layout) {
    // AF_CHANNEL_LAYOUT_5_0_A   L R C Ls Rs
    // AF_CHANNEL_LAYOUT_5_0_B   L R Ls Rs C
    // AF_CHANNEL_LAYOUT_5_0_C   L C R Ls Rs
    // AF_CHANNEL_LAYOUT_5_0_D   C L R Ls Rs
    case AF_CHANNEL_LAYOUT_5_0_A << 16 | AF_CHANNEL_LAYOUT_5_0_B:
        reorder_copy_5ch(dest, src, samples, samplesize, 0, 1, 3, 4, 2);
        break;
    case AF_CHANNEL_LAYOUT_5_0_A << 16 | AF_CHANNEL_LAYOUT_5_0_C:
        reorder_copy_5ch(dest, src, samples, samplesize, 0, 2, 1, 3, 4);
        break;
    case AF_CHANNEL_LAYOUT_5_0_A << 16 | AF_CHANNEL_LAYOUT_5_0_D:
        reorder_copy_5ch(dest, src, samples, samplesize, 2, 0, 1, 3, 4);
        break;
    case AF_CHANNEL_LAYOUT_5_0_B << 16 | AF_CHANNEL_LAYOUT_5_0_A:
        reorder_copy_5ch(dest, src, samples, samplesize, 0, 1, 4, 2, 3);
        break;
    case AF_CHANNEL_LAYOUT_5_0_B << 16 | AF_CHANNEL_LAYOUT_5_0_C:
        reorder_copy_5ch(dest, src, samples, samplesize, 0, 4, 1, 2, 3);
        break;
    case AF_CHANNEL_LAYOUT_5_0_B << 16 | AF_CHANNEL_LAYOUT_5_0_D:
        reorder_copy_5ch(dest, src, samples, samplesize, 4, 0, 1, 2, 3);
        break;
    case AF_CHANNEL_LAYOUT_5_0_C << 16 | AF_CHANNEL_LAYOUT_5_0_A:
        reorder_copy_5ch(dest, src, samples, samplesize, 0, 2, 1, 3, 4);
        break;
    case AF_CHANNEL_LAYOUT_5_0_C << 16 | AF_CHANNEL_LAYOUT_5_0_B:
        reorder_copy_5ch(dest, src, samples, samplesize, 0, 2, 3, 4, 1);
        break;
    case AF_CHANNEL_LAYOUT_5_0_C << 16 | AF_CHANNEL_LAYOUT_5_0_D:
        reorder_copy_5ch(dest, src, samples, samplesize, 1, 0, 2, 3, 4);
        break;
    case AF_CHANNEL_LAYOUT_5_0_D << 16 | AF_CHANNEL_LAYOUT_5_0_A:
        reorder_copy_5ch(dest, src, samples, samplesize, 1, 2, 0, 3, 4);
        break;
    case AF_CHANNEL_LAYOUT_5_0_D << 16 | AF_CHANNEL_LAYOUT_5_0_B:
        reorder_copy_5ch(dest, src, samples, samplesize, 1, 2, 3, 4, 0);
        break;
    case AF_CHANNEL_LAYOUT_5_0_D << 16 | AF_CHANNEL_LAYOUT_5_0_C:
        reorder_copy_5ch(dest, src, samples, samplesize, 1, 0, 2, 3, 4);
        break;
    // AF_CHANNEL_LAYOUT_5_1_A   L R C LFE Ls Rs
    // AF_CHANNEL_LAYOUT_5_1_B   L R Ls Rs C LFE
    // AF_CHANNEL_LAYOUT_5_1_C   L C R Ls Rs LFE
    // AF_CHANNEL_LAYOUT_5_1_D   C L R Ls Rs LFE
    // AF_CHANNEL_LAYOUT_5_1_E   LFE L C R Ls Rs
    case AF_CHANNEL_LAYOUT_5_1_A << 16 | AF_CHANNEL_LAYOUT_5_1_B:
        reorder_copy_6ch(dest, src, samples, samplesize, 0, 1, 4, 5, 2, 3);
        break;
    case AF_CHANNEL_LAYOUT_5_1_A << 16 | AF_CHANNEL_LAYOUT_5_1_C:
        reorder_copy_6ch(dest, src, samples, samplesize, 0, 2, 1, 4, 5, 3);
        break;
    case AF_CHANNEL_LAYOUT_5_1_A << 16 | AF_CHANNEL_LAYOUT_5_1_D:
        reorder_copy_6ch(dest, src, samples, samplesize, 2, 0, 1, 4, 5, 3);
        break;
    case AF_CHANNEL_LAYOUT_5_1_B << 16 | AF_CHANNEL_LAYOUT_5_1_A:
        reorder_copy_6ch(dest, src, samples, samplesize, 0, 1, 4, 5, 2, 3);
        break;
    case AF_CHANNEL_LAYOUT_5_1_B << 16 | AF_CHANNEL_LAYOUT_5_1_C:
        reorder_copy_6ch(dest, src, samples, samplesize, 0, 4, 1, 2, 3, 5);
        break;
    case AF_CHANNEL_LAYOUT_5_1_B << 16 | AF_CHANNEL_LAYOUT_5_1_D:
        reorder_copy_6ch(dest, src, samples, samplesize, 4, 0, 1, 2, 3, 5);
        break;
    case AF_CHANNEL_LAYOUT_5_1_B << 16 | AF_CHANNEL_LAYOUT_5_1_E:
        reorder_copy_6ch(dest, src, samples, samplesize, 5, 0, 4, 1, 2, 3);
        break;
    case AF_CHANNEL_LAYOUT_5_1_C << 16 | AF_CHANNEL_LAYOUT_5_1_A:
        reorder_copy_6ch(dest, src, samples, samplesize, 0, 2, 1, 5, 3, 4);
        break;
    case AF_CHANNEL_LAYOUT_5_1_C << 16 | AF_CHANNEL_LAYOUT_5_1_B:
        reorder_copy_6ch(dest, src, samples, samplesize, 0, 2, 3, 4, 1, 5);
        break;
    case AF_CHANNEL_LAYOUT_5_1_C << 16 | AF_CHANNEL_LAYOUT_5_1_D:
        reorder_copy_6ch(dest, src, samples, samplesize, 1, 0, 2, 3, 4, 5);
        break;
    case AF_CHANNEL_LAYOUT_5_1_D << 16 | AF_CHANNEL_LAYOUT_5_1_A:
        reorder_copy_6ch(dest, src, samples, samplesize, 1, 2, 0, 5, 3, 4);
        break;
    case AF_CHANNEL_LAYOUT_5_1_D << 16 | AF_CHANNEL_LAYOUT_5_1_B:
        reorder_copy_6ch(dest, src, samples, samplesize, 1, 2, 3, 4, 0, 5);
        break;
    case AF_CHANNEL_LAYOUT_5_1_D << 16 | AF_CHANNEL_LAYOUT_5_1_C:
        reorder_copy_6ch(dest, src, samples, samplesize, 1, 0, 2, 3, 4, 5);
        break;
    case AF_CHANNEL_LAYOUT_5_1_E << 16 | AF_CHANNEL_LAYOUT_5_1_B:
        reorder_copy_6ch(dest, src, samples, samplesize, 1, 3, 4, 5, 2, 0);
        break;
    default:
        mp_msg(MSGT_GLOBAL, MSGL_WARN, "[reorder_channel_copy] unsupport "
               "from %x to %x, %d * %d\n", src_layout, dest_layout,
               samples, samplesize);
        fast_memcpy(dest, src, samples*samplesize);
    }
}


#define REORDER_SELF_SWAP_2(SRC,TMP,SAMPLES,CHNUM,S0,S1) \
for (i = 0; i < SAMPLES; i += CHNUM) {\
    TMP = SRC[i+S0];\
    SRC[i+S0] = SRC[i+S1];\
    SRC[i+S1] = TMP;\
}

static int reorder_self_2(void *src, unsigned int samples,
                          unsigned int samplesize, unsigned int chnum,
                          int s0, int s1)
{
    int i;
    switch (samplesize) {
    case 1:
    {
        int8_t *src_8 = src;
        int8_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_2(src_8,tmp,samples,6,s0,s1);
        }
        else {
            REORDER_SELF_SWAP_2(src_8,tmp,samples,5,s0,s1);
        }
        break;
    }
    case 2:
    {
        int16_t *src_16 = src;
        int16_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_2(src_16,tmp,samples,6,s0,s1);
        }
        else if (chnum==3) {
            REORDER_SELF_SWAP_2(src_16,tmp,samples,3,s0,s1);
        }
        else {
            REORDER_SELF_SWAP_2(src_16,tmp,samples,5,s0,s1);
        }
        break;
    }
    case 3:
    {
        int8_t *src_8 = src;
        int8_t tmp0, tmp1, tmp2;
        for (i = 0; i < samples; i += chnum*3) {
            tmp0 = src_8[i+s0*3];
            tmp1 = src_8[i+s0*3+1];
            tmp2 = src_8[i+s0*3+2];
            src_8[i+s0*3]   = src_8[i+s1*3];
            src_8[i+s0*3+1] = src_8[i+s1*3+1];
            src_8[i+s0*3+2] = src_8[i+s1*3+2];
            src_8[i+s1*3]   = tmp0;
            src_8[i+s1*3+1] = tmp1;
            src_8[i+s1*3+2] = tmp2;
        }
    }
    case 4:
    {
        int32_t *src_32 = src;
        int32_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_2(src_32,tmp,samples,6,s0,s1);
        }
        else if (chnum==3) {
            REORDER_SELF_SWAP_2(src_32,tmp,samples,3,s0,s1);
        }
        else {
            REORDER_SELF_SWAP_2(src_32,tmp,samples,5,s0,s1);
        }
        break;
    }
    case 8:
    {
        int64_t *src_64 = src;
        int64_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_2(src_64,tmp,samples,6,s0,s1);
        }
        else if (chnum==3) {
            REORDER_SELF_SWAP_2(src_64,tmp,samples,3,s0,s1);
        }
        else {
            REORDER_SELF_SWAP_2(src_64,tmp,samples,5,s0,s1);
        }
        break;
    }
    default:
        mp_msg(MSGT_GLOBAL, MSGL_WARN,
               "[reorder_ch] Unsupported sample size: %d, please "
               "report this error on the MPlayer mailing list.\n",samplesize);
        return 0;
    }
    return 1;
}

#define REORDER_SELF_SWAP_3(SRC,TMP,SAMPLES,CHNUM,S0,S1,S2) \
for (i = 0; i < SAMPLES; i += CHNUM) {\
    TMP = SRC[i+S0];\
    SRC[i+S0] = SRC[i+S1];\
    SRC[i+S1] = SRC[i+S2];\
    SRC[i+S2] = TMP;\
}

static int reorder_self_3(void *src, unsigned int samples,
                          unsigned int samplesize, unsigned int chnum,
                          int s0, int s1, int s2)
{
    int i;
    switch (samplesize) {
    case 1:
    {
        int8_t *src_8 = src;
        int8_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_3(src_8,tmp,samples,6,s0,s1,s2);
        }
        else {
            REORDER_SELF_SWAP_3(src_8,tmp,samples,5,s0,s1,s2);
        }
        break;
    }
    case 2:
    {
        int16_t *src_16 = src;
        int16_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_3(src_16,tmp,samples,6,s0,s1,s2);
        }
        else {
            REORDER_SELF_SWAP_3(src_16,tmp,samples,5,s0,s1,s2);
        }
        break;
    }
    case 3:
    {
        int8_t *src_8 = src;
        int8_t tmp0, tmp1, tmp2;
        for (i = 0; i < samples; i += chnum*3) {
            tmp0 = src_8[i+s0*3];
            tmp1 = src_8[i+s0*3+1];
            tmp2 = src_8[i+s0*3+2];
            src_8[i+s0*3]   = src_8[i+s1*3];
            src_8[i+s0*3+1] = src_8[i+s1*3+1];
            src_8[i+s0*3+2] = src_8[i+s1*3+2];
            src_8[i+s1*3]   = src_8[i+s2*3];
            src_8[i+s1*3+1] = src_8[i+s2*3+1];
            src_8[i+s1*3+2] = src_8[i+s2*3+2];
            src_8[i+s2*3]   = tmp0;
            src_8[i+s2*3+1] = tmp1;
            src_8[i+s2*3+2] = tmp2;
        }
    }
    case 4:
    {
        int32_t *src_32 = src;
        int32_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_3(src_32,tmp,samples,6,s0,s1,s2);
        }
        else {
            REORDER_SELF_SWAP_3(src_32,tmp,samples,5,s0,s1,s2);
        }
        break;
    }
    case 8:
    {
        int64_t *src_64 = src;
        int64_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_3(src_64,tmp,samples,6,s0,s1,s2);
        }
        else {
            REORDER_SELF_SWAP_3(src_64,tmp,samples,5,s0,s1,s2);
        }
        break;
    }
    default:
        mp_msg(MSGT_GLOBAL, MSGL_WARN,
               "[reorder_ch] Unsupported sample size: %d, please "
               "report this error on the MPlayer mailing list.\n",samplesize);
        return 0;
    }
    return 1;
}

#define REORDER_SELF_SWAP_4_STEP_1(SRC,TMP,SAMPLES,CHNUM,S0,S1,S2,S3) \
for (i = 0; i < SAMPLES; i += CHNUM) {\
    TMP = SRC[i+S0];\
    SRC[i+S0] = SRC[i+S1];\
    SRC[i+S1] = SRC[i+S2];\
    SRC[i+S2] = SRC[i+S3];\
    SRC[i+S3] = TMP;\
}

static int reorder_self_4_step_1(void *src, unsigned int samples,
                                 unsigned int samplesize, unsigned int chnum,
                                 int s0, int s1, int s2, int s3)
{
    int i;
    switch (samplesize) {
    case 1:
    {
        int8_t *src_8 = src;
        int8_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_4_STEP_1(src_8,tmp,samples,6,s0,s1,s2,s3);
        }
        else {
            REORDER_SELF_SWAP_4_STEP_1(src_8,tmp,samples,5,s0,s1,s2,s3);
        }
        break;
    }
    case 2:
    {
        int16_t *src_16 = src;
        int16_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_4_STEP_1(src_16,tmp,samples,6,s0,s1,s2,s3);
        }
        else {
            REORDER_SELF_SWAP_4_STEP_1(src_16,tmp,samples,5,s0,s1,s2,s3);
        }
        break;
    }
    case 3:
    {
        int8_t *src_8 = src;
        int8_t tmp0, tmp1, tmp2;
        for (i = 0; i < samples; i += chnum*3) {
            tmp0 = src_8[i+s0*3];
            tmp1 = src_8[i+s0*3+1];
            tmp2 = src_8[i+s0*3+2];
            src_8[i+s0*3]   = src_8[i+s1*3];
            src_8[i+s0*3+1] = src_8[i+s1*3+1];
            src_8[i+s0*3+2] = src_8[i+s1*3+2];
            src_8[i+s1*3]   = src_8[i+s2*3];
            src_8[i+s1*3+1] = src_8[i+s2*3+1];
            src_8[i+s1*3+2] = src_8[i+s2*3+2];
            src_8[i+s2*3]   = src_8[i+s3*3];
            src_8[i+s2*3+1] = src_8[i+s3*3+1];
            src_8[i+s2*3+2] = src_8[i+s3*3+2];
            src_8[i+s3*3]   = tmp0;
            src_8[i+s3*3+1] = tmp1;
            src_8[i+s3*3+2] = tmp2;
        }
    }
    case 4:
    {
        int32_t *src_32 = src;
        int32_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_4_STEP_1(src_32,tmp,samples,6,s0,s1,s2,s3);
        }
        else {
            REORDER_SELF_SWAP_4_STEP_1(src_32,tmp,samples,5,s0,s1,s2,s3);
        }
        break;
    }
    case 8:
    {
        int64_t *src_64 = src;
        int64_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_4_STEP_1(src_64,tmp,samples,6,s0,s1,s2,s3);
        }
        else {
            REORDER_SELF_SWAP_4_STEP_1(src_64,tmp,samples,5,s0,s1,s2,s3);
        }
        break;
    }
    default:
        mp_msg(MSGT_GLOBAL, MSGL_WARN,
               "[reorder_ch] Unsupported sample size: %d, please "
               "report this error on the MPlayer mailing list.\n",samplesize);
        return 0;
    }
    return 1;
}

#define REORDER_SELF_SWAP_4_STEP_2(SRC,TMP,SAMPLES,CHNUM,S0,S1,S2,S3) \
for (i = 0; i < SAMPLES; i += CHNUM) {\
    TMP = SRC[i+S0];\
    SRC[i+S0] = SRC[i+S2];\
    SRC[i+S2] = TMP;\
    TMP = SRC[i+S1];\
    SRC[i+S1] = SRC[i+S3];\
    SRC[i+S3] = TMP;\
}

static int reorder_self_4_step_2(void *src, unsigned int samples,
                                 unsigned int samplesize, unsigned int chnum,
                                 int s0, int s1, int s2, int s3)
{
    int i;
    switch (samplesize) {
    case 3:
    {
        int8_t *src_8 = src;
        int8_t tmp0, tmp1, tmp2;
        for (i = 0; i < samples; i += chnum*3) {
            tmp0 = src_8[i+s0*3];
            tmp1 = src_8[i+s0*3+1];
            tmp2 = src_8[i+s0*3+2];
            src_8[i+s0*3]   = src_8[i+s2*3];
            src_8[i+s0*3+1] = src_8[i+s2*3+1];
            src_8[i+s0*3+2] = src_8[i+s2*3+2];
            src_8[i+s2*3]   = tmp0;
            src_8[i+s2*3+1] = tmp1;
            src_8[i+s2*3+2] = tmp2;
            tmp0 = src_8[i+s1*3];
            tmp1 = src_8[i+s1*3+1];
            tmp2 = src_8[i+s1*3+2];
            src_8[i+s1*3]   = src_8[i+s3*3];
            src_8[i+s1*3+1] = src_8[i+s3*3+1];
            src_8[i+s1*3+2] = src_8[i+s3*3+2];
            src_8[i+s3*3]   = tmp0;
            src_8[i+s3*3+1] = tmp1;
            src_8[i+s3*3+2] = tmp2;
        }
    }
    default:
        mp_msg(MSGT_GLOBAL, MSGL_WARN,
               "[reorder_ch] Unsupported sample size: %d, please "
               "report this error on the MPlayer mailing list.\n",samplesize);
        return 0;
    }
    return 1;
}

#define REORDER_SELF_SWAP_5_STEP_1(SRC,TMP,SAMPLES,CHNUM,S0,S1,S2,S3,S4) \
for (i = 0; i < SAMPLES; i += CHNUM) {\
    TMP = SRC[i+S0];\
    SRC[i+S0] = SRC[i+S1];\
    SRC[i+S1] = SRC[i+S2];\
    SRC[i+S2] = SRC[i+S3];\
    SRC[i+S3] = SRC[i+S4];\
    SRC[i+S4] = TMP;\
}

static int reorder_self_5_step_1(void *src, unsigned int samples,
                                 unsigned int samplesize, unsigned int chnum,
                                 int s0, int s1, int s2, int s3, int s4)
{
    int i;
    switch (samplesize) {
    case 1:
    {
        int8_t *src_8 = src;
        int8_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_5_STEP_1(src_8,tmp,samples,6,s0,s1,s2,s3,s4);
        }
        else {
            REORDER_SELF_SWAP_5_STEP_1(src_8,tmp,samples,5,s0,s1,s2,s3,s4);
        }
        break;
    }
    case 2:
    {
        int16_t *src_16 = src;
        int16_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_5_STEP_1(src_16,tmp,samples,6,s0,s1,s2,s3,s4);
        }
        else {
            REORDER_SELF_SWAP_5_STEP_1(src_16,tmp,samples,5,s0,s1,s2,s3,s4);
        }
        break;
    }
    case 3:
    {
        int8_t *src_8 = src;
        int8_t tmp0, tmp1, tmp2;
        for (i = 0; i < samples; i += chnum*3) {
            tmp0 = src_8[i+s0*3];
            tmp1 = src_8[i+s0*3+1];
            tmp2 = src_8[i+s0*3+2];
            src_8[i+s0*3]   = src_8[i+s1*3];
            src_8[i+s0*3+1] = src_8[i+s1*3+1];
            src_8[i+s0*3+2] = src_8[i+s1*3+2];
            src_8[i+s1*3]   = src_8[i+s2*3];
            src_8[i+s1*3+1] = src_8[i+s2*3+1];
            src_8[i+s1*3+2] = src_8[i+s2*3+2];
            src_8[i+s2*3]   = src_8[i+s3*3];
            src_8[i+s2*3+1] = src_8[i+s3*3+1];
            src_8[i+s2*3+2] = src_8[i+s3*3+2];
            src_8[i+s3*3]   = src_8[i+s4*3];
            src_8[i+s3*3+1] = src_8[i+s4*3+1];
            src_8[i+s3*3+2] = src_8[i+s4*3+2];
            src_8[i+s4*3]   = tmp0;
            src_8[i+s4*3+1] = tmp1;
            src_8[i+s4*3+2] = tmp2;
        }
    }
    case 4:
    {
        int32_t *src_32 = src;
        int32_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_5_STEP_1(src_32,tmp,samples,6,s0,s1,s2,s3,s4);
        }
        else {
            REORDER_SELF_SWAP_5_STEP_1(src_32,tmp,samples,5,s0,s1,s2,s3,s4);
        }
        break;
    }
    case 8:
    {
        int64_t *src_64 = src;
        int64_t tmp;
        if (chnum==6) {
            REORDER_SELF_SWAP_5_STEP_1(src_64,tmp,samples,6,s0,s1,s2,s3,s4);
        }
        else {
            REORDER_SELF_SWAP_5_STEP_1(src_64,tmp,samples,5,s0,s1,s2,s3,s4);
        }
        break;
    }
    default:
        mp_msg(MSGT_GLOBAL, MSGL_WARN,
               "[reorder_ch] Unsupported sample size: %d, please "
               "report this error on the MPlayer mailing list.\n",samplesize);
        return 0;
    }
    return 1;
}

#define REORDER_SELF_SWAP_2_3(SRC,TMP,SAMPLES,CHNUM,S0,S1,S2,S3,S4) \
for (i = 0; i < SAMPLES; i += CHNUM) {\
    TMP = SRC[i+S0];\
    SRC[i+S0] = SRC[i+S1];\
    SRC[i+S1] = TMP;\
    TMP = SRC[i+S2];\
    SRC[i+S2] = SRC[i+S3];\
    SRC[i+S3] = SRC[i+S4];\
    SRC[i+S4] = TMP;\
}

static int reorder_self_2_3(void *src, unsigned int samples,
                            unsigned int samplesize,
                            int s0, int s1, int s2, int s3, int s4)
{
    int i;
    switch (samplesize) {
    case 1:
    {
        int8_t *src_8 = src;
        int8_t tmp;
        REORDER_SELF_SWAP_2_3(src_8,tmp,samples,6,s0,s1,s2,s3,s4);
        break;
    }
    case 2:
    {
        int16_t *src_16 = src;
        int16_t tmp;
        REORDER_SELF_SWAP_2_3(src_16,tmp,samples,6,s0,s1,s2,s3,s4);
        break;
    }
    case 3:
    {
        int8_t *src_8 = src;
        int8_t tmp0, tmp1, tmp2;
        for (i = 0; i < samples; i += 18) {
            tmp0 = src_8[i+s0*3];
            tmp1 = src_8[i+s0*3+1];
            tmp2 = src_8[i+s0*3+2];
            src_8[i+s0*3]   = src_8[i+s1*3];
            src_8[i+s0*3+1] = src_8[i+s1*3+1];
            src_8[i+s0*3+2] = src_8[i+s1*3+2];
            src_8[i+s1*3]   = tmp0;
            src_8[i+s1*3+1] = tmp1;
            src_8[i+s1*3+2] = tmp2;
            tmp0 = src_8[i+s2*3];
            tmp1 = src_8[i+s2*3+1];
            tmp2 = src_8[i+s2*3+2];
            src_8[i+s2*3]   = src_8[i+s3*3];
            src_8[i+s2*3+1] = src_8[i+s3*3+1];
            src_8[i+s2*3+2] = src_8[i+s3*3+2];
            src_8[i+s3*3]   = src_8[i+s4*3];
            src_8[i+s3*3+1] = src_8[i+s4*3+1];
            src_8[i+s3*3+2] = src_8[i+s4*3+2];
            src_8[i+s4*3]   = tmp0;
            src_8[i+s4*3+1] = tmp1;
            src_8[i+s4*3+2] = tmp2;
        }
    }
    case 4:
    {
        int32_t *src_32 = src;
        int32_t tmp;
        REORDER_SELF_SWAP_2_3(src_32,tmp,samples,6,s0,s1,s2,s3,s4);
        break;
    }
    case 8:
    {
        int64_t *src_64 = src;
        int64_t tmp;
        REORDER_SELF_SWAP_2_3(src_64,tmp,samples,6,s0,s1,s2,s3,s4);
        break;
    }
    default:
        mp_msg(MSGT_GLOBAL, MSGL_WARN,
               "[reorder_ch] Unsupported sample size: %d, please "
               "report this error on the MPlayer mailing list.\n",samplesize);
        return 0;
    }
    return 1;
}

#define REORDER_SELF_SWAP_3_3(SRC,TMP,SAMPLES,CHNUM,S0,S1,S2,S3,S4,S5) \
for (i = 0; i < SAMPLES; i += CHNUM) {\
    TMP = SRC[i+S0];\
    SRC[i+S0] = SRC[i+S1];\
    SRC[i+S1] = SRC[i+S2];\
    SRC[i+S2] = TMP;\
    TMP = SRC[i+S3];\
    SRC[i+S3] = SRC[i+S4];\
    SRC[i+S4] = SRC[i+S5];\
    SRC[i+S5] = TMP;\
}

static int reorder_self_3_3(void *src, unsigned int samples,
                            unsigned int samplesize,
                            int s0, int s1, int s2, int s3, int s4, int s5)
{
    int i;
    switch (samplesize) {
    case 1:
    {
        int8_t *src_8 = src;
        int8_t tmp;
        REORDER_SELF_SWAP_3_3(src_8,tmp,samples,6,s0,s1,s2,s3,s4,s5);
        break;
    }
    case 2:
    {
        int16_t *src_16 = src;
        int16_t tmp;
        REORDER_SELF_SWAP_3_3(src_16,tmp,samples,6,s0,s1,s2,s3,s4,s5);
        break;
    }
    case 3:
    {
        int8_t *src_8 = src;
        int8_t tmp0, tmp1, tmp2;
        for (i = 0; i < samples; i += 18) {
            tmp0 = src_8[i+s0*3];
            tmp1 = src_8[i+s0*3+1];
            tmp2 = src_8[i+s0*3+2];
            src_8[i+s0*3]   = src_8[i+s1*3];
            src_8[i+s0*3+1] = src_8[i+s1*3+1];
            src_8[i+s0*3+2] = src_8[i+s1*3+2];
            src_8[i+s1*3]   = src_8[i+s2*3];
            src_8[i+s1*3+1] = src_8[i+s2*3+1];
            src_8[i+s1*3+2] = src_8[i+s2*3+2];
            src_8[i+s2*3]   = tmp0;
            src_8[i+s2*3+1] = tmp1;
            src_8[i+s2*3+2] = tmp2;
            tmp0 = src_8[i+s3*3];
            tmp1 = src_8[i+s3*3+1];
            tmp2 = src_8[i+s3*3+2];
            src_8[i+s3*3]   = src_8[i+s4*3];
            src_8[i+s3*3+1] = src_8[i+s4*3+1];
            src_8[i+s3*3+2] = src_8[i+s4*3+2];
            src_8[i+s4*3]   = src_8[i+s5*3];
            src_8[i+s4*3+1] = src_8[i+s5*3+1];
            src_8[i+s4*3+2] = src_8[i+s5*3+2];
            src_8[i+s5*3]   = tmp0;
            src_8[i+s5*3+1] = tmp1;
            src_8[i+s5*3+2] = tmp2;
        }
    }
    case 4:
    {
        int32_t *src_32 = src;
        int32_t tmp;
        REORDER_SELF_SWAP_3_3(src_32,tmp,samples,6,s0,s1,s2,s3,s4,s5);
        break;
    }
    case 8:
    {
        int64_t *src_64 = src;
        int64_t tmp;
        REORDER_SELF_SWAP_3_3(src_64,tmp,samples,6,s0,s1,s2,s3,s4,s5);
        break;
    }
    default:
        mp_msg(MSGT_GLOBAL, MSGL_WARN,
               "[reorder_ch] Unsupported sample size: %d, please "
               "report this error on the MPlayer mailing list.\n",samplesize);
        return 0;
    }
    return 1;
}

#define REORDER_SELF_SWAP_2_4(SRC,TMP,SAMPLES,CHNUM,S0,S1,S2,S3,S4,S5) \
for (i = 0; i < SAMPLES; i += CHNUM) {\
    TMP = SRC[i+S0];\
    SRC[i+S0] = SRC[i+S1];\
    SRC[i+S1] = TMP;\
    TMP = SRC[i+S2];\
    SRC[i+S2] = SRC[i+S3];\
    SRC[i+S3] = SRC[i+S4];\
    SRC[i+S4] = SRC[i+S5];\
    SRC[i+S5] = TMP;\
}

static int reorder_self_2_4(void *src, unsigned int samples,
                            unsigned int samplesize,
                            int s0, int s1, int s2, int s3, int s4, int s5)
{
    int i;
    switch (samplesize) {
    case 1:
    {
        int8_t *src_8 = src;
        int8_t tmp;
        REORDER_SELF_SWAP_2_4(src_8,tmp,samples,6,s0,s1,s2,s3,s4,s5);
        break;
    }
    case 2:
    {
        int16_t *src_16 = src;
        int16_t tmp;
        REORDER_SELF_SWAP_2_4(src_16,tmp,samples,6,s0,s1,s2,s3,s4,s5);
        break;
    }
    case 3:
    {
        int8_t *src_8 = src;
        int8_t tmp0, tmp1, tmp2;
        for (i = 0; i < samples; i += 18) {
            tmp0 = src_8[i+s0*3];
            tmp1 = src_8[i+s0*3+1];
            tmp2 = src_8[i+s0*3+2];
            src_8[i+s0*3]   = src_8[i+s1*3];
            src_8[i+s0*3+1] = src_8[i+s1*3+1];
            src_8[i+s0*3+2] = src_8[i+s1*3+2];
            src_8[i+s1*3]   = tmp0;
            src_8[i+s1*3+1] = tmp1;
            src_8[i+s1*3+2] = tmp2;
            tmp0 = src_8[i+s2*3];
            tmp1 = src_8[i+s2*3+1];
            tmp2 = src_8[i+s2*3+2];
            src_8[i+s2*3]   = src_8[i+s3*3];
            src_8[i+s2*3+1] = src_8[i+s3*3+1];
            src_8[i+s2*3+2] = src_8[i+s3*3+2];
            src_8[i+s3*3]   = src_8[i+s4*3];
            src_8[i+s3*3+1] = src_8[i+s4*3+1];
            src_8[i+s3*3+2] = src_8[i+s4*3+2];
            src_8[i+s4*3]   = src_8[i+s5*3];
            src_8[i+s4*3+1] = src_8[i+s5*3+1];
            src_8[i+s4*3+2] = src_8[i+s5*3+2];
            src_8[i+s5*3]   = tmp0;
            src_8[i+s5*3+1] = tmp1;
            src_8[i+s5*3+2] = tmp2;
        }
    }
    case 4:
    {
        int32_t *src_32 = src;
        int32_t tmp;
        REORDER_SELF_SWAP_2_4(src_32,tmp,samples,6,s0,s1,s2,s3,s4,s5);
        break;
    }
    case 8:
    {
        int64_t *src_64 = src;
        int64_t tmp;
        REORDER_SELF_SWAP_2_4(src_64,tmp,samples,6,s0,s1,s2,s3,s4,s5);
        break;
    }
    default:
        mp_msg(MSGT_GLOBAL, MSGL_WARN,
               "[reorder_ch] Unsupported sample size: %d, please "
               "report this error on the MPlayer mailing list.\n",samplesize);
        return 0;
    }
    return 1;
}

void reorder_channel(void *src,
                     int src_layout,
                     int dest_layout,
                     int samples,
                     int samplesize)
{
    if (dest_layout==src_layout)
        return;
    if (!AF_IS_SAME_CH_NUM(dest_layout,src_layout)) {
        mp_msg(MSGT_GLOBAL, MSGL_WARN,
               "[reorder_channel] different channel count "
               "between current and target: %x, %x\n",
               AF_GET_CH_NUM_WITH_LFE(src_layout),
               AF_GET_CH_NUM_WITH_LFE(dest_layout));
        return;
    }
    switch ((src_layout<<16)|dest_layout) {
    // AF_CHANNEL_LAYOUT_5_0_A   L R C Ls Rs
    // AF_CHANNEL_LAYOUT_5_0_B   L R Ls Rs C
    // AF_CHANNEL_LAYOUT_5_0_C   L C R Ls Rs
    // AF_CHANNEL_LAYOUT_5_0_D   C L R Ls Rs
    case AF_CHANNEL_LAYOUT_5_0_A << 16 | AF_CHANNEL_LAYOUT_5_0_B:
        reorder_self_3(src, samples, samplesize, 5, 2, 3, 4);
        break;
    case AF_CHANNEL_LAYOUT_5_0_A << 16 | AF_CHANNEL_LAYOUT_5_0_C:
        reorder_self_2(src, samples, samplesize, 5, 1, 2);
        break;
    case AF_CHANNEL_LAYOUT_5_0_A << 16 | AF_CHANNEL_LAYOUT_5_0_D:
        reorder_self_3(src, samples, samplesize, 5, 2, 1, 0);
        break;
    case AF_CHANNEL_LAYOUT_5_0_B << 16 | AF_CHANNEL_LAYOUT_5_0_A:
        reorder_self_3(src, samples, samplesize, 5, 4, 3, 2);
        break;
    case AF_CHANNEL_LAYOUT_5_0_B << 16 | AF_CHANNEL_LAYOUT_5_0_C:
        reorder_self_4_step_1(src, samples, samplesize, 5, 4, 3, 2, 1);
        break;
    case AF_CHANNEL_LAYOUT_5_0_B << 16 | AF_CHANNEL_LAYOUT_5_0_D:
        reorder_self_5_step_1(src, samples, samplesize, 5, 4, 3, 2, 1, 0);
        break;
    case AF_CHANNEL_LAYOUT_5_0_C << 16 | AF_CHANNEL_LAYOUT_5_0_A:
        reorder_self_2(src, samples, samplesize, 5, 1, 2);
        break;
    case AF_CHANNEL_LAYOUT_5_0_C << 16 | AF_CHANNEL_LAYOUT_5_0_B:
        reorder_self_4_step_1(src, samples, samplesize, 5, 1, 2, 3, 4);
        break;
    case AF_CHANNEL_LAYOUT_5_0_C << 16 | AF_CHANNEL_LAYOUT_5_0_D:
        reorder_self_2(src, samples, samplesize, 5, 0, 1);
        break;
    case AF_CHANNEL_LAYOUT_5_0_D << 16 | AF_CHANNEL_LAYOUT_5_0_A:
        reorder_self_3(src, samples, samplesize, 5, 0, 1, 2);
        break;
    case AF_CHANNEL_LAYOUT_5_0_D << 16 | AF_CHANNEL_LAYOUT_5_0_B:
        reorder_self_5_step_1(src, samples, samplesize, 5, 0, 1, 2, 3, 4);
        break;
    case AF_CHANNEL_LAYOUT_5_0_D << 16 | AF_CHANNEL_LAYOUT_5_0_C:
        reorder_self_2(src, samples, samplesize, 5, 0, 1);
        break;
    // AF_CHANNEL_LAYOUT_5_1_A   L R C LFE Ls Rs
    // AF_CHANNEL_LAYOUT_5_1_B   L R Ls Rs C LFE
    // AF_CHANNEL_LAYOUT_5_1_C   L C R Ls Rs LFE
    // AF_CHANNEL_LAYOUT_5_1_D   C L R Ls Rs LFE
    // AF_CHANNEL_LAYOUT_5_1_E   LFE L C R Ls Rs
    case AF_CHANNEL_LAYOUT_5_1_A << 16 | AF_CHANNEL_LAYOUT_5_1_B:
        if (samplesize != 3)
            reorder_self_2(src, samples/2, samplesize*2, 3, 1, 2);
        else
            reorder_self_4_step_2(src, samples, samplesize, 6, 2, 3, 4, 5);
        break;
    case AF_CHANNEL_LAYOUT_5_1_A << 16 | AF_CHANNEL_LAYOUT_5_1_C:
        reorder_self_2_3(src, samples, samplesize, 1, 2, 3, 4, 5);
        break;
    case AF_CHANNEL_LAYOUT_5_1_A << 16 | AF_CHANNEL_LAYOUT_5_1_D:
        reorder_self_3_3(src, samples, samplesize, 2, 1, 0, 3, 4, 5);
        break;
    case AF_CHANNEL_LAYOUT_5_1_B << 16 | AF_CHANNEL_LAYOUT_5_1_A:
        if (samplesize != 3)
            reorder_self_2(src, samples/2, samplesize*2, 3, 1, 2);
        else
            reorder_self_4_step_2(src, samples, samplesize, 6, 2, 3, 4, 5);
        break;
    case AF_CHANNEL_LAYOUT_5_1_B << 16 | AF_CHANNEL_LAYOUT_5_1_C:
        reorder_self_4_step_1(src, samples, samplesize, 6, 4, 3, 2, 1);
        break;
    case AF_CHANNEL_LAYOUT_5_1_B << 16 | AF_CHANNEL_LAYOUT_5_1_D:
        reorder_self_5_step_1(src, samples, samplesize, 6, 4, 3, 2, 1, 0);
        break;
    case AF_CHANNEL_LAYOUT_5_1_B << 16 | AF_CHANNEL_LAYOUT_5_1_E:
        reorder_self_2_4(src, samples, samplesize, 2, 4, 5, 3, 1, 0);
        break;
    case AF_CHANNEL_LAYOUT_5_1_C << 16 | AF_CHANNEL_LAYOUT_5_1_A:
        reorder_self_2_3(src, samples, samplesize, 1, 2, 5, 4, 3);
        break;
    case AF_CHANNEL_LAYOUT_5_1_C << 16 | AF_CHANNEL_LAYOUT_5_1_B:
        reorder_self_4_step_1(src, samples, samplesize, 6, 1, 2, 3, 4);
        break;
    case AF_CHANNEL_LAYOUT_5_1_C << 16 | AF_CHANNEL_LAYOUT_5_1_D:
        reorder_self_2(src, samples, samplesize, 6, 0, 1);
        break;
    case AF_CHANNEL_LAYOUT_5_1_D << 16 | AF_CHANNEL_LAYOUT_5_1_A:
        reorder_self_3_3(src, samples, samplesize, 0, 1, 2, 5, 4, 3);
        break;
    case AF_CHANNEL_LAYOUT_5_1_D << 16 | AF_CHANNEL_LAYOUT_5_1_B:
        reorder_self_5_step_1(src, samples, samplesize, 6, 0, 1, 2, 3, 4);
        break;
    case AF_CHANNEL_LAYOUT_5_1_D << 16 | AF_CHANNEL_LAYOUT_5_1_C:
        reorder_self_2(src, samples, samplesize, 6, 0, 1);
        break;
    case AF_CHANNEL_LAYOUT_5_1_E << 16 | AF_CHANNEL_LAYOUT_5_1_B:
        reorder_self_2_4(src, samples, samplesize, 2, 4, 0, 1, 3, 5);
        break;
    default:
        mp_msg(MSGT_GLOBAL, MSGL_WARN,
               "[reorder_channel] unsupported from %x to %x, %d * %d\n",
               src_layout, dest_layout, samples, samplesize);
    }
}


static int channel_layout_mapping_5ch[AF_CHANNEL_LAYOUT_SOURCE_NUM] = {
    AF_CHANNEL_LAYOUT_ALSA_5CH_DEFAULT,
    AF_CHANNEL_LAYOUT_AAC_5CH_DEFAULT,
    AF_CHANNEL_LAYOUT_WAVEEX_5CH_DEFAULT,
    AF_CHANNEL_LAYOUT_LAVC_AC3_5CH_DEFAULT,
    AF_CHANNEL_LAYOUT_LAVC_LIBA52_5CH_DEFAULT,
    AF_CHANNEL_LAYOUT_LAVC_DCA_5CH_DEFAULT,
    AF_CHANNEL_LAYOUT_VORBIS_5CH_DEFAULT,
    AF_CHANNEL_LAYOUT_FLAC_5CH_DEFAULT,
};

static int channel_layout_mapping_6ch[AF_CHANNEL_LAYOUT_SOURCE_NUM] = {
    AF_CHANNEL_LAYOUT_ALSA_6CH_DEFAULT,
    AF_CHANNEL_LAYOUT_AAC_6CH_DEFAULT,
    AF_CHANNEL_LAYOUT_WAVEEX_6CH_DEFAULT,
    AF_CHANNEL_LAYOUT_LAVC_AC3_6CH_DEFAULT,
    AF_CHANNEL_LAYOUT_LAVC_LIBA52_6CH_DEFAULT,
    AF_CHANNEL_LAYOUT_LAVC_DCA_6CH_DEFAULT,
    AF_CHANNEL_LAYOUT_VORBIS_6CH_DEFAULT,
    AF_CHANNEL_LAYOUT_FLAC_6CH_DEFAULT,
};

void reorder_channel_copy_nch(void *src,
                              int src_layout,
                              void *dest,
                              int dest_layout,
                              int chnum,
                              int samples,
                              int samplesize)
{
    if (chnum < 5 || chnum > 6 || src_layout < 0 || dest_layout < 0 ||
            src_layout >= AF_CHANNEL_LAYOUT_SOURCE_NUM ||
            dest_layout >= AF_CHANNEL_LAYOUT_SOURCE_NUM)
        fast_memcpy(dest, src, samples*samplesize);
    else if (chnum == 6)
        reorder_channel_copy(src, channel_layout_mapping_6ch[src_layout],
                             dest, channel_layout_mapping_6ch[dest_layout],
                             samples, samplesize);
    else
        reorder_channel_copy(src, channel_layout_mapping_5ch[src_layout],
                             dest, channel_layout_mapping_5ch[dest_layout],
                             samples, samplesize);
}

void reorder_channel_nch(void *buf,
                         int src_layout,
                         int dest_layout,
                         int chnum,
                         int samples,
                         int samplesize)
{
    if (src_layout == dest_layout || chnum < 5 || chnum > 6 ||
            src_layout < 0 || dest_layout < 0 ||
            src_layout >= AF_CHANNEL_LAYOUT_SOURCE_NUM ||
            dest_layout >= AF_CHANNEL_LAYOUT_SOURCE_NUM ||
            src_layout == dest_layout)
        return;
    if (chnum == 6)
        reorder_channel(buf, channel_layout_mapping_6ch[src_layout],
                        channel_layout_mapping_6ch[dest_layout],
                        samples, samplesize);
    else
        reorder_channel(buf, channel_layout_mapping_5ch[src_layout],
                        channel_layout_mapping_5ch[dest_layout],
                        samples, samplesize);
}


#ifdef TEST

static void test_copy(int channels) {
    int samples = 12*1024*1024;
    int samplesize = 2;
    int i;
    unsigned char *bufin = malloc((samples+100)*samplesize);
    unsigned char *bufout = malloc((samples+100)*samplesize);
    memset(bufin, 0xFF, samples*samplesize);
    for (i = 0;i < 100; ++i)
        reorder_channel_copy(bufin, AF_CHANNEL_LAYOUT_5_1_A,
                             bufout, AF_CHANNEL_LAYOUT_5_1_B,
                             samples, samplesize);
//    reorder_channel(bufin, AF_CHANNEL_LAYOUT_5_1_B,
//                         AF_CHANNEL_LAYOUT_5_1_D,
//                         samples, samplesize);
    free(bufin);
    free(bufout);
}

int main(int argc, char *argv[]) {
    int channels = 6;
    if (argc > 1)
        channels = atoi(argv[1]);
    test_copy(channels);
    return 0;
}

#endif

