/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2002 A. Kurpiers
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
** $Id: hcr.c,v 1.5 2003/07/29 08:20:12 menno Exp $
**/


#include "common.h"
#include "structs.h"

#include <stdlib.h>
#include <string.h>

#include "syntax.h"
#include "specrec.h"
#include "bits.h"
#include "pulse.h"
#include "analysis.h"
#include "bits.h"
#include "codebook/hcb.h"

/* Implements the HCR11 tool as described in ISO/IEC 14496-3/Amd.1, 8.5.3.3 */

#ifdef ERROR_RESILIENCE

typedef struct
{
    /* bit input */
    uint32_t bufa;
    uint32_t bufb;
    int8_t len; 
} bits_t;


static INLINE uint32_t showbits(bits_t *ld, uint8_t bits)
{
    if (bits == 0) return 0;
    if (ld->len <= 32){
        /* huffman_spectral_data_2 needs to read more than may be available, bits maybe
           > ld->len, deliver 0 than */
        if (ld->len >= bits)
            return ((ld->bufa >> (ld->len - bits)) & (0xFFFFFFFF >> (32 - bits)));
        else
            return ((ld->bufa << (bits - ld->len)) & (0xFFFFFFFF >> (32 - bits)));        
    } else {
        if ((ld->len - bits) < 32)
        {
            return ( (ld->bufb & (0xFFFFFFFF >> (64 - ld->len))) << (bits - ld->len + 32)) |
                (ld->bufa >> (ld->len - bits));
        } else {
            return ((ld->bufb >> (ld->len - bits - 32)) & (0xFFFFFFFF >> (32 - bits)));
        }
    }
}

/* return 1 if position is outside of buffer, 0 otherwise */
static INLINE int8_t flushbits( bits_t *ld, uint8_t bits)
{
    ld->len -= bits;

    if (ld->len <0)
    {
        ld->len = 0;
        return 1;
    } else {
        return 0;
    }
}


static INLINE int8_t getbits(bits_t *ld, uint8_t n, uint32_t *result)
{
    *result = showbits(ld, n);
    return flushbits(ld, n);
}

static INLINE int8_t get1bit(bits_t *ld, uint8_t *result)
{
    uint32_t res;
    int8_t ret;

    ret = getbits(ld, 1, &res);
    *result = (int8_t)(res & 1);
    return ret;
}

/* Special version of huffman_spectral_data adapted from huffman.h
Will not read from a bitfile but a bits_t structure.
Will keep track of the bits decoded and return the number of bits remaining.
Do not read more than ld->len, return -1 if codeword would be longer */

static int8_t huffman_spectral_data_2(uint8_t cb, bits_t *ld, int16_t *sp )
{
    uint32_t cw;
    uint16_t offset = 0;
    uint8_t extra_bits;
    uint8_t i;
    uint8_t save_cb = cb;


    switch (cb)
    {
    case 1: /* 2-step method for data quadruples */
    case 2:
    case 4:

        cw = showbits(ld, hcbN[cb]);
        offset = hcb_table[cb][cw].offset;
        extra_bits = hcb_table[cb][cw].extra_bits;

        if (extra_bits)
        {
            /* we know for sure it's more than hcbN[cb] bits long */
            if ( flushbits(ld, hcbN[cb]) ) return -1;
            offset += (uint16_t)showbits(ld, extra_bits);
            if ( flushbits(ld, hcb_2_quad_table[cb][offset].bits - hcbN[cb]) ) return -1;
        } else {
            if ( flushbits(ld, hcb_2_quad_table[cb][offset].bits) ) return -1;
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

        /* TODO: If ER is used, some extra error checking should be done */
        if (cb >= 16)
            cb = 11;
            
        cw = showbits(ld, hcbN[cb]);
        offset = hcb_table[cb][cw].offset;
        extra_bits = hcb_table[cb][cw].extra_bits;

        if (extra_bits)
        {
            /* we know for sure it's more than hcbN[cb] bits long */
            if ( flushbits(ld, hcbN[cb]) ) return -1;
            offset += (uint16_t)showbits(ld, extra_bits);
            if ( flushbits(ld, hcb_2_pair_table[cb][offset].bits - hcbN[cb]) ) return -1;
        } else {
            if ( flushbits(ld, hcb_2_pair_table[cb][offset].bits) ) return -1;
        }
        sp[0] = hcb_2_pair_table[cb][offset].x;
        sp[1] = hcb_2_pair_table[cb][offset].y;
        break;

    case 3: /* binary search for data quadruples */

        while (!hcb3[offset].is_leaf)
        {
            uint8_t b;
            
            if ( get1bit(ld, &b) ) return -1;
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
            
            if (get1bit(ld, &b) ) return -1;
            offset += hcb_bin_table[cb][offset].data[b];
        }

        sp[0] = hcb_bin_table[cb][offset].data[0];
        sp[1] = hcb_bin_table[cb][offset].data[1];

        break;
    }

	/* decode sign bits */
    if (unsigned_cb[cb]) {

        for(i = 0; i < ((cb < FIRST_PAIR_HCB) ? QUAD_LEN : PAIR_LEN); i++)
        {
            if(sp[i])
            {
            	uint8_t b;
                if ( get1bit(ld, &b) ) return -1;
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
                    if (get1bit(ld, &b))
                        return -1;
                    if (b == 0)
                        break;
                }
// TODO: here we would need to test "off" if VCB11 is used!
                if (getbits(ld, i, &off))
                    return -1;
                j = off + (1<<i);
                sp[k] = (int16_t)((neg) ? -j : j);
            }
        }
    }    
    return ld->len;
}

/* rewind len (max. 32) bits so that the MSB becomes LSB */

static uint32_t rewind_word( uint32_t W, uint8_t len)
{
    uint8_t i;
    uint32_t tmp_W=0;

    for ( i=0; i<len; i++ )
    {
        tmp_W<<=1;
        if (W & (1<<i)) tmp_W |= 1;
    }
    return tmp_W;
}

static void rewind_lword( uint32_t *highW, uint32_t *lowW, uint8_t len)
{
    uint32_t tmp_lW=0;

    if (len > 32)
    {
        tmp_lW = rewind_word( (*highW << (64-len)) | (*lowW >> (len-32)), 32);
        *highW = rewind_word( *lowW << (64-len) , 32);
        *lowW = tmp_lW;
    } else {
        *highW = 0;
        *lowW = rewind_word( *lowW, len);
    }
}    

/* Takes a codeword as stored in r, rewinds the remaining bits and stores it back */
static void rewind_bits(bits_t * r)
{
    uint32_t hw, lw;

    if (r->len == 0) return;

    if (r->len >32)
    {
        lw = r->bufa;
        hw = r->bufb & (0xFFFFFFFF >> (64 - r->len));
        rewind_lword( &hw, &lw, r->len );
        r->bufa = lw;
        r->bufb = hw;

    } else {
        lw = showbits(r, r->len );
        r->bufa = rewind_word( lw, r->len);
        r->bufb = 0;
    }
}

/* takes codewords from a and b, concatenate them and store them in b */
static void concat_bits( bits_t * a, bits_t * b)
{
    uint32_t	hwa, lwa, hwb, lwb;

    if (a->len == 0) return;

    if (a->len >32)
    {
        lwa = a->bufa;
        hwa = a->bufb & (0xFFFFFFFF >> (64 - a->len));
    } else {
        lwa = showbits(a, a->len );
        hwa = 0;
    }
    if (b->len >=32) {
        lwb = b->bufa;
        hwb = (b->bufb & (0xFFFFFFFF >> (64 - b->len)) ) | ( lwa << (b->len - 32));
    } else {
        lwb = showbits(b, b->len ) | (lwa << (b->len));
        hwb = (lwa >> (32 - b->len)) | (hwa << (b->len));
    }

    b->bufa = lwb;
    b->bufb = hwb;
    b->len += a->len;
}

/* 8.5.3.3.1 */

static const uint8_t PresortedCodebook_VCB11[] = { 11, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 9, 7, 5, 3, 1};
static const uint8_t PresortedCodebook[] = { 11, 9, 7, 5, 3, 1};

static const uint8_t maxCwLen[32] = {0, 11, 9, 20, 16, 13, 11, 14, 12, 17, 14, 49,
    0, 0, 0, 0, 14, 17, 21, 21, 25, 25, 29, 29, 29, 29, 33, 33, 33, 37, 37, 41};

typedef struct
{
    bits_t		bits;
    uint8_t		decoded;
    uint16_t	sp_offset;
    uint8_t		cb;
} codeword_state;


#define segmentWidth( codebook )	min( maxCwLen[codebook], ics->length_of_longest_codeword )
     
uint8_t reordered_spectral_data(faacDecHandle hDecoder, ic_stream *ics, bitfile *ld,
                                int16_t *spectral_data)
{
    uint16_t sp_offset[8];
    uint16_t g,i, presort;
    uint16_t NrCodeWords=0, numberOfSegments=0, BitsRead=0;
    uint8_t numberOfSets, set;
    codeword_state Codewords[ 1024 ];	// FIXME max length? PCWs are not stored, so index is Codewordnr - numberOfSegments!, maybe malloc()?
    bits_t	Segment[ 512 ];

    uint8_t PCW_decoded=0;
    uint16_t segment_index=0, codeword_index=0;
    uint16_t nshort = hDecoder->frameLength/8;


    memset (spectral_data, 0, hDecoder->frameLength*sizeof(uint16_t));

    if (ics->length_of_reordered_spectral_data == 0)
        return 0; /* nothing to do */

    /* if we have a corrupted bitstream this can happen... */
    if ((ics->length_of_longest_codeword == 0) ||
        (ics->length_of_reordered_spectral_data <
        ics->length_of_longest_codeword) ||
        (ics->max_sfb == 0))
    {
        return 10; /* this is not good... */
    }

    /* store the offset into the spectral data for all the window groups because we can't do it later */

    sp_offset[0] = 0;
    for (g=1; g < ics->num_window_groups; g++)
    {
        sp_offset[g] = sp_offset[g-1] + nshort*ics->window_group_length[g-1];
    }

    /* All data is sorted according to the codebook used */        
    for (presort = 0; presort < (hDecoder->aacSectionDataResilienceFlag ? 22 : 6); presort++)
    {
        uint8_t sfb;

        /* next codebook that has to be processed according to presorting */
        uint8_t nextCB = hDecoder->aacSectionDataResilienceFlag ? PresortedCodebook_VCB11[ presort ] : PresortedCodebook[ presort ];

        /* Data belonging to the same spectral unit and having the same codebook comes in consecutive codewords.
           This is done by scanning all sfbs for possible codewords. For sfbs with more than 4 elements this has to be
           repeated */

        for (sfb=0; sfb<ics->max_sfb; sfb ++)
        {
            uint8_t sect_cb, w;

            for (w=0; w< (ics->swb_offset[sfb+1] - ics->swb_offset[sfb]); w+=4)
            {
                for(g = 0; g < ics->num_window_groups; g++)
                {
                    for (i = 0; i < ics->num_sec[g]; i++)
                    {
                        sect_cb = ics->sect_cb[g][i];

                        if (
                            /* process only sections that are due now */
                            (( sect_cb == nextCB ) || (( nextCB < ESC_HCB ) && ( sect_cb == nextCB+1)) ) &&

                            /* process only sfb's that are due now */
                            ((ics->sect_start[g][i] <= sfb) && (ics->sect_end[g][i] > sfb))
                            )
                        {
                            if ((sect_cb != ZERO_HCB) &&
                                (sect_cb != NOISE_HCB) &&
                                (sect_cb != INTENSITY_HCB) &&
                                (sect_cb != INTENSITY_HCB2))
                            {
                                uint8_t inc = (sect_cb < FIRST_PAIR_HCB) ? QUAD_LEN : PAIR_LEN;
                                uint16_t k;

                                uint32_t	hw, lw;

                                for  (k=0; (k < (4/inc)*ics->window_group_length[g]) &&
                                    ( (k+w*ics->window_group_length[g]/inc) < (ics->sect_sfb_offset[g][sfb+1] - ics->sect_sfb_offset[g][sfb])); k++)
                                {
                                    uint16_t sp = sp_offset[g] + ics->sect_sfb_offset[g][sfb] + inc*(k+w*ics->window_group_length[g]/inc);

                                    if (!PCW_decoded)
                                    {
                                        /* if we haven't yet read until the end of the buffer, we can directly decode the so-called PCWs */
                                        if ((BitsRead + segmentWidth( sect_cb ))<= ics->length_of_reordered_spectral_data)
                                        {
                                            Segment[ numberOfSegments ].len = segmentWidth( sect_cb );

                                            if (segmentWidth( sect_cb ) > 32)
                                            {
                                                Segment[ numberOfSegments ].bufb = faad_showbits(ld, segmentWidth( sect_cb ) - 32);
                                                faad_flushbits(ld, segmentWidth( sect_cb) - 32);
                                                Segment[ numberOfSegments ].bufa = faad_showbits(ld, 32),
                                                    faad_flushbits(ld, 32 );

                                            } else {
                                                Segment[ numberOfSegments ].bufa = faad_showbits(ld,  segmentWidth( sect_cb ));
                                                Segment[ numberOfSegments ].bufb = 0;
                                                faad_flushbits(ld, segmentWidth( sect_cb) );
                                            }

                                            huffman_spectral_data_2(sect_cb, &Segment[ numberOfSegments ], &spectral_data[sp]);

                                            BitsRead += segmentWidth( sect_cb );

                                            /* skip to next segment, but store left bits in new buffer */
                                            rewind_bits( &Segment[ numberOfSegments ]);

                                            numberOfSegments++;
                                        } else {

                                            /* the last segment is extended until length_of_reordered_spectral_data */

                                            if (BitsRead < ics->length_of_reordered_spectral_data)
                                            {

                                                uint8_t additional_bits = (ics->length_of_reordered_spectral_data - BitsRead);

                                                if ( additional_bits > 32)
                                                {
                                                    hw = faad_showbits(ld, additional_bits - 32);
                                                    faad_flushbits(ld, additional_bits - 32);
                                                    lw = faad_showbits(ld, 32);
                                                    faad_flushbits(ld, 32 );
                                                } else {
                                                    lw = faad_showbits(ld, additional_bits);
                                                    hw = 0;
                                                    faad_flushbits(ld, additional_bits );
                                                }
                                                rewind_lword( &hw, &lw, additional_bits + Segment[ numberOfSegments-1 ].len );
                                                if (Segment[ numberOfSegments-1 ].len > 32)
                                                {
                                                    Segment[ numberOfSegments-1 ].bufb = hw + 
                                                        showbits(&Segment[ numberOfSegments-1 ], Segment[ numberOfSegments-1 ].len - 32);
                                                    Segment[ numberOfSegments-1 ].bufa = lw + 
                                                        showbits(&Segment[ numberOfSegments-1 ], 32);
                                                } else {
                                                    Segment[ numberOfSegments-1 ].bufa = lw + 
                                                        showbits(&Segment[ numberOfSegments-1 ], Segment[ numberOfSegments-1 ].len);
                                                    Segment[ numberOfSegments-1 ].bufb = hw;
                                                }
                                                Segment[ numberOfSegments-1 ].len += additional_bits;
                                            }
                                            BitsRead = ics->length_of_reordered_spectral_data;
                                            PCW_decoded = 1;

                                            Codewords[ 0 ].sp_offset = sp;
                                            Codewords[ 0 ].cb = sect_cb;
                                            Codewords[ 0 ].decoded = 0;
                                            Codewords[ 0 ].bits.len = 0;
                                        }
                                    } else {
                                        Codewords[ NrCodeWords - numberOfSegments ].sp_offset = sp;
                                        Codewords[ NrCodeWords - numberOfSegments ].cb = sect_cb;
                                        Codewords[ NrCodeWords - numberOfSegments ].decoded = 0;
                                        Codewords[ NrCodeWords - numberOfSegments ].bits.len = 0;

                                    } /* PCW decoded */
                                    NrCodeWords++;
                                } /* of k */
                            }
                        }
                    } /* of i */
                 } /* of g */
             } /* of w */
         } /* of sfb */
    } /* of presort */

    /* Avoid divide by zero */
    if (numberOfSegments == 0)
        return 10; /* this is not good... */

    numberOfSets = NrCodeWords / numberOfSegments;     

    /* second step: decode nonPCWs */

    for (set = 1; set <= numberOfSets; set++)
    {
        uint16_t trial;

        for (trial = 0; trial < numberOfSegments; trial++)
        {
            uint16_t codewordBase;
            uint16_t set_decoded=numberOfSegments;

            if (set == numberOfSets)
                set_decoded = NrCodeWords - set*numberOfSegments;	/* last set is shorter than the rest */

            for (codewordBase = 0; codewordBase < numberOfSegments; codewordBase++)
            {
                uint16_t segment_index = (trial + codewordBase) % numberOfSegments;
                uint16_t codeword_index = codewordBase + set*numberOfSegments - numberOfSegments;

                if ((codeword_index + numberOfSegments) >= NrCodeWords)
                    break;
                if (!Codewords[ codeword_index ].decoded)
                {
                    if ( Segment[ segment_index ].len > 0)
                    {
                        uint8_t tmplen;

                        if (Codewords[ codeword_index ].bits.len != 0)
                        {
                            /* on the first trial the data is only stored in Segment[], not in Codewords[]. 
                               On next trials first collect the data stored for this codeword and
                               concatenate the new data from Segment[] */

                            concat_bits( &Codewords[ codeword_index ].bits, &Segment[ segment_index ]);                            
                            /* Now everthing is stored in Segment[] */
                        }
                        tmplen = Segment[ segment_index ].len;
                        if ( huffman_spectral_data_2(Codewords[ codeword_index ].cb, &Segment[ segment_index ],
                            &spectral_data[ Codewords[ codeword_index ].sp_offset ]) >=0)
                        {
                            /* CW did fit into segment */

                            Codewords[ codeword_index ].decoded = 1;
                            set_decoded--;
                        } else {

                            /* CW did not fit, so store for later use */

                            Codewords[ codeword_index ].bits.len = tmplen;
                            Codewords[ codeword_index ].bits.bufa = Segment[ segment_index ].bufa;
                            Codewords[ codeword_index ].bits.bufb = Segment[ segment_index ].bufb;
                        }
                    }                        
                }
            } /* of codewordBase */

            if (set_decoded == 0) break;	/* no undecoded codewords left in this set */

        } /* of trial */

        /* rewind all bits in remaining segments with len>0 */
        for (i=0; i < numberOfSegments; i++)
            rewind_bits( &Segment[ i ] );
    }

#if 0
    {
        int i, r=0, c=0;
        for (i=0; i< numberOfSegments; i++)
            r += Segment[ i ].len;
        if (r != 0)
        {
            printf("reordered_spectral_data: %d bits remaining!\n", r);
        }
        for (i=0; i< NrCodeWords - numberOfSegments; i++)
        {
            if (Codewords[ i ].decoded == 0)
            {
                c++;
            }
        }
        if (c != 0)
        {
            printf("reordered_spectral_data: %d Undecoded Codewords remaining!\n",c );
        }
        if ((r !=0) || (c!=0))	return 10;
    }
#endif

    return 0;
}
#endif
