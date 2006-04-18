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
** $Id: huffman.c,v 1.22 2004/09/04 14:56:28 menno Exp $
**/

#include "common.h"
#include "structs.h"

#include <stdlib.h>
#ifdef ANALYSIS
#include <stdio.h>
#endif

#include "bits.h"
#include "huffman.h"
#include "codebook/hcb.h"


/* static function declarations */
static INLINE void huffman_sign_bits(bitfile *ld, int16_t *sp, uint8_t len);
static INLINE int16_t huffman_getescape(bitfile *ld, int16_t sp);
static uint8_t huffman_2step_quad(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_2step_quad_sign(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_2step_pair(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_2step_pair_sign(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_binary_quad(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_binary_quad_sign(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_binary_pair(uint8_t cb, bitfile *ld, int16_t *sp);
static uint8_t huffman_binary_pair_sign(uint8_t cb, bitfile *ld, int16_t *sp);
static int16_t huffman_codebook(uint8_t i);
static void vcb11_check_LAV(uint8_t cb, int16_t *sp);

int8_t huffman_scale_factor(bitfile *ld)
{
    uint16_t offset = 0;

    while (hcb_sf[offset][1])
    {
        uint8_t b = faad_get1bit(ld
            DEBUGVAR(1,255,"huffman_scale_factor()"));
        offset += hcb_sf[offset][b];

        if (offset > 240)
        {
            /* printf("ERROR: offset into hcb_sf = %d >240!\n", offset); */
            return -1;
        }
    }

    return hcb_sf[offset][0];
}


hcb *hcb_table[] = {
    0, hcb1_1, hcb2_1, 0, hcb4_1, 0, hcb6_1, 0, hcb8_1, 0, hcb10_1, hcb11_1
};

hcb_2_quad *hcb_2_quad_table[] = {
    0, hcb1_2, hcb2_2, 0, hcb4_2, 0, 0, 0, 0, 0, 0, 0
};

hcb_2_pair *hcb_2_pair_table[] = {
    0, 0, 0, 0, 0, 0, hcb6_2, 0, hcb8_2, 0, hcb10_2, hcb11_2
};

hcb_bin_pair *hcb_bin_table[] = {
    0, 0, 0, 0, 0, hcb5, 0, hcb7, 0, hcb9, 0, 0
};

uint8_t hcbN[] = { 0, 5, 5, 0, 5, 0, 5, 0, 5, 0, 6, 5 };

/* defines whether a huffman codebook is unsigned or not */
/* Table 4.6.2 */
uint8_t unsigned_cb[] = { 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0,
  /* codebook 16 to 31 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

int hcb_2_quad_table_size[] = { 0, 114, 86, 0, 185, 0, 0, 0, 0, 0, 0, 0 };
int hcb_2_pair_table_size[] = { 0, 0, 0, 0, 0, 0, 126, 0, 83, 0, 210, 373 };
int hcb_bin_table_size[] = { 0, 0, 0, 161, 0, 161, 0, 127, 0, 337, 0, 0 };

static INLINE void huffman_sign_bits(bitfile *ld, int16_t *sp, uint8_t len)
{
    uint8_t i;

    for (i = 0; i < len; i++)
    {
        if(sp[i])
        {
            if(faad_get1bit(ld
                DEBUGVAR(1,5,"huffman_sign_bits(): sign bit")) & 1)
            {
                sp[i] = -sp[i];
            }
        }
    }
}

static INLINE int16_t huffman_getescape(bitfile *ld, int16_t sp)
{
    uint8_t neg, i;
    int16_t j;
	int16_t off;

    if (sp < 0)
    {
        if (sp != -16)
            return sp;
        neg = 1;
    } else {
        if (sp != 16)
            return sp;
        neg = 0;
    }

    for (i = 4; ; i++)
    {
        if (faad_get1bit(ld
            DEBUGVAR(1,6,"huffman_getescape(): escape size")) == 0)
        {
            break;
        }
    }

    off = (int16_t)faad_getbits(ld, i
        DEBUGVAR(1,9,"huffman_getescape(): escape"));

    j = off | (1<<i);
    if (neg)
        j = -j;

    return j;
}

static uint8_t huffman_2step_quad(uint8_t cb, bitfile *ld, int16_t *sp)
{
    uint32_t cw;
    uint16_t offset = 0;
    uint8_t extra_bits;

    cw = faad_showbits(ld, hcbN[cb]);
    offset = hcb_table[cb][cw].offset;
    extra_bits = hcb_table[cb][cw].extra_bits;

    if (extra_bits)
    {
        /* we know for sure it's more than hcbN[cb] bits long */
        faad_flushbits(ld, hcbN[cb]);
        offset += (uint16_t)faad_showbits(ld, extra_bits);
        faad_flushbits(ld, hcb_2_quad_table[cb][offset].bits - hcbN[cb]);
    } else {
        faad_flushbits(ld, hcb_2_quad_table[cb][offset].bits);
    }

    if (offset > hcb_2_quad_table_size[cb])
    {
        /* printf("ERROR: offset into hcb_2_quad_table = %d >%d!\n", offset,
           hcb_2_quad_table_size[cb]); */
        return 10;
    }

    sp[0] = hcb_2_quad_table[cb][offset].x;
    sp[1] = hcb_2_quad_table[cb][offset].y;
    sp[2] = hcb_2_quad_table[cb][offset].v;
    sp[3] = hcb_2_quad_table[cb][offset].w;

    return 0;
}

static uint8_t huffman_2step_quad_sign(uint8_t cb, bitfile *ld, int16_t *sp)
{
    uint8_t err = huffman_2step_quad(cb, ld, sp);
    huffman_sign_bits(ld, sp, QUAD_LEN);

    return err;
}

static uint8_t huffman_2step_pair(uint8_t cb, bitfile *ld, int16_t *sp)
{
    uint32_t cw;
    uint16_t offset = 0;
    uint8_t extra_bits;

    cw = faad_showbits(ld, hcbN[cb]);
    offset = hcb_table[cb][cw].offset;
    extra_bits = hcb_table[cb][cw].extra_bits;

    if (extra_bits)
    {
        /* we know for sure it's more than hcbN[cb] bits long */
        faad_flushbits(ld, hcbN[cb]);
        offset += (uint16_t)faad_showbits(ld, extra_bits);
        faad_flushbits(ld, hcb_2_pair_table[cb][offset].bits - hcbN[cb]);
    } else {
        faad_flushbits(ld, hcb_2_pair_table[cb][offset].bits);
    }

    if (offset > hcb_2_pair_table_size[cb])
    {
        /* printf("ERROR: offset into hcb_2_pair_table = %d >%d!\n", offset,
           hcb_2_pair_table_size[cb]); */
        return 10;
    }

    sp[0] = hcb_2_pair_table[cb][offset].x;
    sp[1] = hcb_2_pair_table[cb][offset].y;

    return 0;
}

static uint8_t huffman_2step_pair_sign(uint8_t cb, bitfile *ld, int16_t *sp)
{
    uint8_t err = huffman_2step_pair(cb, ld, sp);
    huffman_sign_bits(ld, sp, PAIR_LEN);

    return err;
}

static uint8_t huffman_binary_quad(uint8_t cb, bitfile *ld, int16_t *sp)
{
    uint16_t offset = 0;

    while (!hcb3[offset].is_leaf)
    {
        uint8_t b = faad_get1bit(ld
            DEBUGVAR(1,255,"huffman_spectral_data():3"));
        offset += hcb3[offset].data[b];
    }

    if (offset > hcb_bin_table_size[cb])
    {
        /* printf("ERROR: offset into hcb_bin_table = %d >%d!\n", offset,
           hcb_bin_table_size[cb]); */
        return 10;
    }

    sp[0] = hcb3[offset].data[0];
    sp[1] = hcb3[offset].data[1];
    sp[2] = hcb3[offset].data[2];
    sp[3] = hcb3[offset].data[3];

    return 0;
}

static uint8_t huffman_binary_quad_sign(uint8_t cb, bitfile *ld, int16_t *sp)
{
    uint8_t err = huffman_binary_quad(cb, ld, sp);
    huffman_sign_bits(ld, sp, QUAD_LEN);

    return err;
}

static uint8_t huffman_binary_pair(uint8_t cb, bitfile *ld, int16_t *sp)
{
    uint16_t offset = 0;

    while (!hcb_bin_table[cb][offset].is_leaf)
    {
        uint8_t b = faad_get1bit(ld
            DEBUGVAR(1,255,"huffman_spectral_data():9"));
        offset += hcb_bin_table[cb][offset].data[b];
    }

    if (offset > hcb_bin_table_size[cb])
    {
        /* printf("ERROR: offset into hcb_bin_table = %d >%d!\n", offset,
           hcb_bin_table_size[cb]); */
        return 10;
    }

    sp[0] = hcb_bin_table[cb][offset].data[0];
    sp[1] = hcb_bin_table[cb][offset].data[1];

    return 0;
}

static uint8_t huffman_binary_pair_sign(uint8_t cb, bitfile *ld, int16_t *sp)
{
    uint8_t err = huffman_binary_pair(cb, ld, sp);
    huffman_sign_bits(ld, sp, PAIR_LEN);

    return err;
}

static int16_t huffman_codebook(uint8_t i)
{
    static const uint32_t data = 16428320;
    if (i == 0) return (int16_t)(data >> 16) & 0xFFFF;
    else        return (int16_t)data & 0xFFFF;
}

static void vcb11_check_LAV(uint8_t cb, int16_t *sp)
{
    static const uint16_t vcb11_LAV_tab[] = {
        16, 31, 47, 63, 95, 127, 159, 191, 223,
        255, 319, 383, 511, 767, 1023, 2047
    };
    uint16_t max = 0;

    if (cb < 16 || cb > 31)
        return;

    max = vcb11_LAV_tab[cb - 16];

    if ((abs(sp[0]) > max) || (abs(sp[1]) > max))
    {
        sp[0] = 0;
        sp[1] = 0;
    }
}

uint8_t huffman_spectral_data(uint8_t cb, bitfile *ld, int16_t *sp)
{
    switch (cb)
    {
    case 1: /* 2-step method for data quadruples */
    case 2:
        return huffman_2step_quad(cb, ld, sp);
    case 3: /* binary search for data quadruples */
        return huffman_binary_quad_sign(cb, ld, sp);
    case 4: /* 2-step method for data quadruples */
        return huffman_2step_quad_sign(cb, ld, sp);
    case 5: /* binary search for data pairs */
        return huffman_binary_pair(cb, ld, sp);
    case 6: /* 2-step method for data pairs */
        return huffman_2step_pair(cb, ld, sp);
    case 7: /* binary search for data pairs */
    case 9:
        return huffman_binary_pair_sign(cb, ld, sp);
    case 8: /* 2-step method for data pairs */
    case 10:
        return huffman_2step_pair_sign(cb, ld, sp);
    case 12: {
        uint8_t err = huffman_2step_pair(11, ld, sp);
        sp[0] = huffman_codebook(0); sp[1] = huffman_codebook(1); 
        return err; }
    case 11:
    {
        uint8_t err = huffman_2step_pair_sign(11, ld, sp);
        sp[0] = huffman_getescape(ld, sp[0]);
        sp[1] = huffman_getescape(ld, sp[1]);
        return err;
    }
#ifdef ERROR_RESILIENCE
    /* VCB11 uses codebook 11 */
    case 16: case 17: case 18: case 19: case 20: case 21: case 22: case 23:
    case 24: case 25: case 26: case 27: case 28: case 29: case 30: case 31:
    {
        uint8_t err = huffman_2step_pair_sign(11, ld, sp);
        sp[0] = huffman_getescape(ld, sp[0]);
        sp[1] = huffman_getescape(ld, sp[1]);

        /* check LAV (Largest Absolute Value) */
        /* this finds errors in the ESCAPE signal */
        vcb11_check_LAV(cb, sp);

        return err;
    }
#endif
    default:
        /* Non existent codebook number, something went wrong */
        return 11;
    }

    return 0;
}


#ifdef ERROR_RESILIENCE

/* Special version of huffman_spectral_data
Will not read from a bitfile but a bits_t structure.
Will keep track of the bits decoded and return the number of bits remaining.
Do not read more than ld->len, return -1 if codeword would be longer */

int8_t huffman_spectral_data_2(uint8_t cb, bits_t *ld, int16_t *sp)
{
    uint32_t cw;
    uint16_t offset = 0;
    uint8_t extra_bits;
    uint8_t i, vcb11 = 0;


    switch (cb)
    {
    case 1: /* 2-step method for data quadruples */
    case 2:
    case 4:

        cw = showbits_hcr(ld, hcbN[cb]);
        offset = hcb_table[cb][cw].offset;
        extra_bits = hcb_table[cb][cw].extra_bits;

        if (extra_bits)
        {
            /* we know for sure it's more than hcbN[cb] bits long */
            if ( flushbits_hcr(ld, hcbN[cb]) ) return -1;
            offset += (uint16_t)showbits_hcr(ld, extra_bits);
            if ( flushbits_hcr(ld, hcb_2_quad_table[cb][offset].bits - hcbN[cb]) ) return -1;
        } else {
            if ( flushbits_hcr(ld, hcb_2_quad_table[cb][offset].bits) ) return -1;
        }

        sp[0] = hcb_2_quad_table[cb][offset].x;
        sp[1] = hcb_2_quad_table[cb][offset].y;
        sp[2] = hcb_2_quad_table[cb][offset].v;
        sp[3] = hcb_2_quad_table[cb][offset].w;
        break;

    case 6: /* 2-step method for data pairs */
    case 8:
    case 10:
    case 11:
    /* VCB11 uses codebook 11 */
    case 16: case 17: case 18: case 19: case 20: case 21: case 22: case 23:
    case 24: case 25: case 26: case 27: case 28: case 29: case 30: case 31:

        if (cb >= 16)
        {
            /* store the virtual codebook */
            vcb11 = cb;
            cb = 11;
        }
            
        cw = showbits_hcr(ld, hcbN[cb]);
        offset = hcb_table[cb][cw].offset;
        extra_bits = hcb_table[cb][cw].extra_bits;

        if (extra_bits)
        {
            /* we know for sure it's more than hcbN[cb] bits long */
            if ( flushbits_hcr(ld, hcbN[cb]) ) return -1;
            offset += (uint16_t)showbits_hcr(ld, extra_bits);
            if ( flushbits_hcr(ld, hcb_2_pair_table[cb][offset].bits - hcbN[cb]) ) return -1;
        } else {
            if ( flushbits_hcr(ld, hcb_2_pair_table[cb][offset].bits) ) return -1;
        }
        sp[0] = hcb_2_pair_table[cb][offset].x;
        sp[1] = hcb_2_pair_table[cb][offset].y;
        break;

    case 3: /* binary search for data quadruples */

        while (!hcb3[offset].is_leaf)
        {
            uint8_t b;
            
            if ( get1bit_hcr(ld, &b) ) return -1;
            offset += hcb3[offset].data[b];
        }

        sp[0] = hcb3[offset].data[0];
        sp[1] = hcb3[offset].data[1];
        sp[2] = hcb3[offset].data[2];
        sp[3] = hcb3[offset].data[3];

        break;

    case 5: /* binary search for data pairs */
    case 7:
    case 9:

        while (!hcb_bin_table[cb][offset].is_leaf)
        {
            uint8_t b;
            
            if (get1bit_hcr(ld, &b) ) return -1;
            offset += hcb_bin_table[cb][offset].data[b];
        }

        sp[0] = hcb_bin_table[cb][offset].data[0];
        sp[1] = hcb_bin_table[cb][offset].data[1];

        break;
    }

	/* decode sign bits */
    if (unsigned_cb[cb])
    {
        for(i = 0; i < ((cb < FIRST_PAIR_HCB) ? QUAD_LEN : PAIR_LEN); i++)
        {
            if(sp[i])
            {
            	uint8_t b;
                if ( get1bit_hcr(ld, &b) ) return -1;
                if (b != 0) {
                    sp[i] = -sp[i];
                }
           }
        }
    }

    /* decode huffman escape bits */
    if ((cb == ESC_HCB) || (cb >= 16))
    {
        uint8_t k;
        for (k = 0; k < 2; k++)
        {
            if ((sp[k] == 16) || (sp[k] == -16))
            {
                uint8_t neg, i;
                int32_t j;
                uint32_t off;

                neg = (sp[k] < 0) ? 1 : 0; 

                for (i = 4; ; i++)
                {
                    uint8_t b;
                    if (get1bit_hcr(ld, &b))
                        return -1;
                    if (b == 0)
                        break;
                }

                if (getbits_hcr(ld, i, &off))
                    return -1;
                j = off + (1<<i);
                sp[k] = (int16_t)((neg) ? -j : j);
            }
        }

        if (vcb11 != 0)
        {
            /* check LAV (Largest Absolute Value) */
            /* this finds errors in the ESCAPE signal */
            vcb11_check_LAV(vcb11, sp);
        }
    }    
    return ld->len;
}

#endif

