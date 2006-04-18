/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2004 G.C. Pascutto, Ahead Software AG, http://www.nero.com
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
** $Id: hcr.c,v 1.18 2004/09/04 14:56:28 menno Exp $
**/

#include "common.h"
#include "structs.h"

#include <stdlib.h>
#include <string.h>

#include "specrec.h"
#include "huffman.h"

/* ISO/IEC 14496-3/Amd.1 
 * 8.5.3.3: Huffman Codeword Reordering for AAC spectral data (HCR) 
 *
 * HCR devides the spectral data in known fixed size segments, and 
 * sorts it by the importance of the data. The importance is firstly 
 * the (lower) position in the spectrum, and secondly the largest 
 * value in the used codebook. 
 * The most important data is written at the start of each segment
 * (at known positions), the remaining data is interleaved inbetween, 
 * with the writing direction alternating.
 * Data length is not increased.
*/

#ifdef ERROR_RESILIENCE

/* 8.5.3.3.1 Pre-sorting */

#define NUM_CB      6
#define NUM_CB_ER   22
#define MAX_CB      32
#define VCB11_FIRST 16
#define VCB11_LAST  31

static const uint8_t PreSortCB_STD[NUM_CB] = 
    { 11, 9, 7, 5, 3, 1};

static const uint8_t PreSortCB_ER[NUM_CB_ER] = 
    { 11, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 9, 7, 5, 3, 1};

/* 8.5.3.3.2 Derivation of segment width */

static const uint8_t maxCwLen[MAX_CB] = {0, 11, 9, 20, 16, 13, 11, 14, 12, 17, 14, 49,
    0, 0, 0, 0, 14, 17, 21, 21, 25, 25, 29, 29, 29, 29, 33, 33, 33, 37, 37, 41};

#define segmentWidth(cb)    min(maxCwLen[cb], ics->length_of_longest_codeword)

/* bit-twiddling helpers */
static const uint8_t  S[] = {1, 2, 4, 8, 16};    
static const uint32_t B[] = {0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF};

typedef struct
{
    uint8_t     cb;
    uint8_t     decoded;
    uint16_t	sp_offset;
    bits_t      bits;
} codeword_t;

/* rewind and reverse */
/* 32 bit version */
static uint32_t rewrev_word(uint32_t v, const uint8_t len)
{  
    /* 32 bit reverse */
    v = ((v >> S[0]) & B[0]) | ((v << S[0]) & ~B[0]); 
    v = ((v >> S[1]) & B[1]) | ((v << S[1]) & ~B[1]); 
    v = ((v >> S[2]) & B[2]) | ((v << S[2]) & ~B[2]); 
    v = ((v >> S[3]) & B[3]) | ((v << S[3]) & ~B[3]);
    v = ((v >> S[4]) & B[4]) | ((v << S[4]) & ~B[4]);

    /* shift off low bits */
    v >>= (32 - len);

    return v;
}

/* 64 bit version */
static void rewrev_lword(uint32_t *hi, uint32_t *lo, const uint8_t len)
{   
    if (len <= 32) {
        *hi = 0;
        *lo = rewrev_word(*lo, len);
    } else
    {
        uint32_t t = *hi, v = *lo;

        /* double 32 bit reverse */
        v = ((v >> S[0]) & B[0]) | ((v << S[0]) & ~B[0]); 
        t = ((t >> S[0]) & B[0]) | ((t << S[0]) & ~B[0]); 
        v = ((v >> S[1]) & B[1]) | ((v << S[1]) & ~B[1]); 
        t = ((t >> S[1]) & B[1]) | ((t << S[1]) & ~B[1]); 
        v = ((v >> S[2]) & B[2]) | ((v << S[2]) & ~B[2]); 
        t = ((t >> S[2]) & B[2]) | ((t << S[2]) & ~B[2]); 
        v = ((v >> S[3]) & B[3]) | ((v << S[3]) & ~B[3]);
        t = ((t >> S[3]) & B[3]) | ((t << S[3]) & ~B[3]);
        v = ((v >> S[4]) & B[4]) | ((v << S[4]) & ~B[4]);                
        t = ((t >> S[4]) & B[4]) | ((t << S[4]) & ~B[4]);

        /* last 32<>32 bit swap is implicit below */
        
        /* shift off low bits (this is really only one 64 bit shift) */
        *lo = (t >> (64 - len)) | (v << (len - 32));
        *hi = v >> (64 - len);          
    }
}


/* bits_t version */
static void rewrev_bits(bits_t *bits)
{
    if (bits->len == 0) return;
    rewrev_lword(&bits->bufb, &bits->bufa,  bits->len);
}


/* merge bits of a to b */
static void concat_bits(bits_t *b, bits_t *a)
{
    uint32_t bl, bh, al, ah;

    if (a->len == 0) return;

    al = a->bufa;
    ah = a->bufb;
    
    if (b->len > 32)
    {
        /* maskoff superfluous high b bits */
        bl = b->bufa;
        bh = b->bufb & ((1 << (b->len-32)) - 1);
        /* left shift a b->len bits */
        ah = al << (b->len - 32);
        al = 0;
    } else {
        bl = b->bufa & ((1 << (b->len)) - 1);
        bh = 0;   
        ah = (ah << (b->len)) | (al >> (32 - b->len));
        al = al << b->len;
    }

    /* merge */
    b->bufa = bl | al;
    b->bufb = bh | ah;

    b->len += a->len;
}
     
uint8_t is_good_cb(uint8_t this_CB, uint8_t this_sec_CB)
{
    /* only want spectral data CB's */
    if ((this_sec_CB > ZERO_HCB && this_sec_CB <= ESC_HCB) || (this_sec_CB >= VCB11_FIRST && this_sec_CB <= VCB11_LAST))
    {
        if (this_CB < ESC_HCB)
        {
            /* normal codebook pairs */
            return ((this_sec_CB == this_CB) || (this_sec_CB == this_CB + 1));
        } else
        {
            /* escape codebook */
            return (this_sec_CB == this_CB);
        }
    }
    return 0;
}
                    
void read_segment(bits_t *segment, uint8_t segwidth, bitfile *ld)
{
    segment->len = segwidth;

     if (segwidth > 32)
     {
        segment->bufb = faad_getbits(ld, segwidth - 32);        
        segment->bufa = faad_getbits(ld, 32);        

    } else {
        segment->bufa = faad_getbits(ld, segwidth);
        segment->bufb = 0;        
    }    
}

void fill_in_codeword(codeword_t *codeword, uint16_t index, uint16_t sp, uint8_t cb)
{
    codeword[index].sp_offset = sp;
    codeword[index].cb = cb;
    codeword[index].decoded = 0;
    codeword[index].bits.len = 0;
}

uint8_t reordered_spectral_data(NeAACDecHandle hDecoder, ic_stream *ics, 
                                bitfile *ld, int16_t *spectral_data)
{   
    uint16_t PCWs_done;
    uint16_t numberOfSegments, numberOfSets, numberOfCodewords;  

    codeword_t codeword[512];
    bits_t segment[512];

    uint16_t sp_offset[8];
    uint16_t g, i, sortloop, set, bitsread;
    uint8_t w_idx, sfb, this_CB, last_CB, this_sec_CB; 
    
    const uint16_t nshort = hDecoder->frameLength/8;
    const uint16_t sp_data_len = ics->length_of_reordered_spectral_data;
    
    const uint8_t *PreSortCb;

    /* no data (e.g. silence) */
    if (sp_data_len == 0)
        return 0;

    /* since there is spectral data, at least one codeword has nonzero length */
    if (ics->length_of_longest_codeword == 0)
        return 10;
    
    if (sp_data_len < ics->length_of_longest_codeword)
        return 10; 

    sp_offset[0] = 0;
    for (g = 1; g < ics->num_window_groups; g++)
    {
        sp_offset[g] = sp_offset[g-1] + nshort*ics->window_group_length[g-1];
    }

    PCWs_done = 0;
    numberOfSegments = 0;
    numberOfCodewords = 0;
    bitsread = 0;

    /* VCB11 code books in use */
    if (hDecoder->aacSectionDataResilienceFlag)
    {
        PreSortCb = PreSortCB_ER;
        last_CB = NUM_CB_ER;
    } else
    {
        PreSortCb = PreSortCB_STD;
        last_CB = NUM_CB;
    }
 
    /* step 1: decode PCW's (set 0), and stuff data in easier-to-use format */
    for (sortloop = 0; sortloop < last_CB; sortloop++)
    {
        /* select codebook to process this pass */
        this_CB = PreSortCb[sortloop];
        
        /* loop over sfbs */
        for (sfb = 0; sfb < ics->max_sfb; sfb++)
        {
            /* loop over all in this sfb, 4 lines per loop */
            for (w_idx = 0; 4*w_idx < (ics->swb_offset[sfb+1] - ics->swb_offset[sfb]); w_idx++)
            {
                for(g = 0; g < ics->num_window_groups; g++)
                {
                    for (i = 0; i < ics->num_sec[g]; i++)
                    {
                        /* check whether sfb used here is the one we want to process */
                        if ((ics->sect_start[g][i] <= sfb) && (ics->sect_end[g][i] > sfb))
                        {                            
                            /* check whether codebook used here is the one we want to process */
                            this_sec_CB = ics->sect_cb[g][i];
                 
                            if (is_good_cb(this_CB, this_sec_CB))                              
                            {
                                /* precalculate some stuff */
                                uint16_t sect_sfb_size = ics->sect_sfb_offset[g][sfb+1] - ics->sect_sfb_offset[g][sfb];
                                uint8_t inc = (this_sec_CB < FIRST_PAIR_HCB) ? QUAD_LEN : PAIR_LEN;
                                uint16_t group_cws_count = (4*ics->window_group_length[g])/inc;
                                uint8_t segwidth = segmentWidth(this_sec_CB);
                                uint16_t cws;                                

                                /* read codewords until end of sfb or end of window group (shouldn't only 1 trigger?) */                                 
                                for (cws = 0; (cws < group_cws_count) && ((cws + w_idx*group_cws_count) < sect_sfb_size); cws++)
                                {
                                    uint16_t sp = sp_offset[g] + ics->sect_sfb_offset[g][sfb] + inc * (cws + w_idx*group_cws_count);                                   

                                    /* read and decode PCW */
                                    if (!PCWs_done)
                                    {         
                                        /* read in normal segments */
                                        if (bitsread + segwidth <= sp_data_len)
                                        {                                            
                                            read_segment(&segment[numberOfSegments], segwidth, ld);                          
                                            bitsread += segwidth;
                                            
                                            huffman_spectral_data_2(this_sec_CB, &segment[numberOfSegments], &spectral_data[sp]);                                            

                                            /* keep leftover bits */
                                            rewrev_bits(&segment[numberOfSegments]);

                                            numberOfSegments++;
                                        } else {
                                            /* remaining stuff after last segment, we unfortunately couldn't read
                                               this in earlier because it might not fit in 64 bits. since we already
                                               decoded (and removed) the PCW it is now guaranteed to fit */
                                            if (bitsread < sp_data_len)
                                            {                                                
                                                const uint8_t additional_bits = sp_data_len - bitsread;                                               

                                                read_segment(&segment[numberOfSegments], additional_bits, ld);                                                
                                                segment[numberOfSegments].len += segment[numberOfSegments-1].len;
                                                rewrev_bits(&segment[numberOfSegments]);                                               

                                                if (segment[numberOfSegments-1].len > 32)
                                                {
                                                    segment[numberOfSegments-1].bufb = segment[numberOfSegments].bufb + 
                                                        showbits_hcr(&segment[numberOfSegments-1], segment[numberOfSegments-1].len - 32);
                                                    segment[numberOfSegments-1].bufa = segment[numberOfSegments].bufa + 
                                                        showbits_hcr(&segment[numberOfSegments-1], 32);
                                                } else {
                                                    segment[numberOfSegments-1].bufa = segment[numberOfSegments].bufa + 
                                                        showbits_hcr(&segment[numberOfSegments-1], segment[numberOfSegments-1].len);
                                                    segment[numberOfSegments-1].bufb = segment[numberOfSegments].bufb;
                                                }                                                
                                                segment[numberOfSegments-1].len += additional_bits;
                                            }
                                            bitsread = sp_data_len;
                                            PCWs_done = 1;

                                            fill_in_codeword(codeword, 0, sp, this_sec_CB);                                            
                                        }
                                    } else {    
                                        fill_in_codeword(codeword, numberOfCodewords - numberOfSegments, sp, this_sec_CB);                                         
                                    }
                                    numberOfCodewords++;
                                }                             
                            }
                        }
                    } 
                 } 
             }
         }
    }

    if (numberOfSegments == 0)
        return 10; 

    numberOfSets = numberOfCodewords / numberOfSegments;     

    /* step 2: decode nonPCWs */
    for (set = 1; set <= numberOfSets; set++)
    {
        uint16_t trial;

        for (trial = 0; trial < numberOfSegments; trial++)
        {
            uint16_t codewordBase;

            for (codewordBase = 0; codewordBase < numberOfSegments; codewordBase++)
            {
                const uint16_t segment_idx = (trial + codewordBase) % numberOfSegments;
                const uint16_t codeword_idx = codewordBase + set*numberOfSegments - numberOfSegments;

                /* data up */
                if (codeword_idx >= numberOfCodewords - numberOfSegments) break;

                if (!codeword[codeword_idx].decoded && segment[segment_idx].len > 0)
                {
                    uint8_t tmplen;

                    if (codeword[codeword_idx].bits.len != 0)                   
                        concat_bits(&segment[segment_idx], &codeword[codeword_idx].bits);                            
                    
                    tmplen = segment[segment_idx].len;

                    if (huffman_spectral_data_2(codeword[codeword_idx].cb, &segment[segment_idx],
                                               &spectral_data[codeword[codeword_idx].sp_offset]) >= 0)
                    {
                        codeword[codeword_idx].decoded = 1;
                    } else 
                    {   
                        codeword[codeword_idx].bits = segment[segment_idx];
                        codeword[codeword_idx].bits.len = tmplen;                        
                    }
                                            
                }
            }
        }
        for (i = 0; i < numberOfSegments; i++)
            rewrev_bits(&segment[i]);
    }

    return 0;
}
#endif
