/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2002-2004 A. Kurpiers
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
** $Id: hcr.c,v 1.15 2004/03/02 20:09:58 menno Exp $
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
#include "huffman.h"

/* Implements the HCR11 tool as described in ISO/IEC 14496-3/Amd.1, 8.5.3.3 */

#ifdef ERROR_RESILIENCE

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
        lw = showbits_hcr(r, r->len );
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
        lwa = showbits_hcr(a, a->len );
        hwa = 0;
    }
    if (b->len >=32) {
        lwb = b->bufa;
        hwb = (b->bufb & (0xFFFFFFFF >> (64 - b->len)) ) | ( lwa << (b->len - 32));
    } else {
        lwb = showbits_hcr(b, b->len ) | (lwa << (b->len));
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
     
uint8_t reordered_spectral_data(NeAACDecHandle hDecoder, ic_stream *ics, bitfile *ld,
                                int16_t *spectral_data)
{
    uint16_t sp_offset[8];
    uint16_t g,i, presort;
    uint16_t NrCodeWords=0, numberOfSegments=0, BitsRead=0;
    uint8_t numberOfSets, set;
    codeword_state Codewords[ 1024 ];	// FIXME max length? PCWs are not stored, so index is Codewordnr - numberOfSegments!, maybe malloc()?
    bits_t	Segment[ 512 ];

    uint8_t PCW_decoded=0;
    uint16_t nshort = hDecoder->frameLength/8;


    /*memset (spectral_data, 0, hDecoder->frameLength*sizeof(uint16_t));*/

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
                                                        showbits_hcr(&Segment[ numberOfSegments-1 ], Segment[ numberOfSegments-1 ].len - 32);
                                                    Segment[ numberOfSegments-1 ].bufa = lw + 
                                                        showbits_hcr(&Segment[ numberOfSegments-1 ], 32);
                                                } else {
                                                    Segment[ numberOfSegments-1 ].bufa = lw + 
                                                        showbits_hcr(&Segment[ numberOfSegments-1 ], Segment[ numberOfSegments-1 ].len);
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
