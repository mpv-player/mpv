/* Straightforward (to be) optimized JPEG encoder for the YUV422 format 
 * based on mjpeg code from ffmpeg. 
 *
 * Copyright (c) 2002, Rik Snel
 * Parts from ffmpeg Copyright (c) 2000, 2001 Gerard Lantau
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * For an excellent introduction to the JPEG format, see:
 * http://www.ece.purdue.edu/~bourman/grad-labs/lab8/pdf/lab.pdf
 */


/* stuff from libavcodec/common.h */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#ifdef USE_FASTMEMCPY
#include "fastmemcpy.h"
#endif
#include "../mp_msg.h"
#include "../libavcodec/common.h"
#include "../libavcodec/dsputil.h"


static int height, width, fields, cheap_upsample, qscale, bw = 0, first = 1;

/* from dsputils.c */

static DCTELEM **blck;

extern void (*av_fdct)(DCTELEM *b);

static UINT8 zr_zigzag_direct[64] = {
    0, 1, 8, 16, 9, 2, 3, 10,
    17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

/* bit output */

static PutBitContext pb;

/* from mpegvideo.c */

#define QMAT_SHIFT 25
#define QMAT_SHIFT_MMX 19

static const unsigned short aanscales[64] = {
    /* precomputed values scaled up by 14 bits */
    16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
    22725, 31521, 29692, 26722, 22725, 17855, 12299,  6270,
    21407, 29692, 27969, 25172, 21407, 16819, 11585,  5906,
    19266, 26722, 25172, 22654, 19266, 15137, 10426,  5315,
    16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
    12873, 17855, 16819, 15137, 12873, 10114,  6967,  3552,
    8867, 12299, 11585, 10426,  8867,  6967,  4799,  2446,
    4520,  6270,  5906,  5315,  4520,  3552,  2446,  1247
};


static unsigned int simple_mmx_permutation[64]={
	0x00, 0x08, 0x01, 0x09, 0x04, 0x0C, 0x05, 0x0D,
	0x10, 0x18, 0x11, 0x19, 0x14, 0x1C, 0x15, 0x1D,
	0x02, 0x0A, 0x03, 0x0B, 0x06, 0x0E, 0x07, 0x0F,
	0x12, 0x1A, 0x13, 0x1B, 0x16, 0x1E, 0x17, 0x1F,
	0x20, 0x28, 0x21, 0x29, 0x24, 0x2C, 0x25, 0x2D,
	0x30, 0x38, 0x31, 0x39, 0x34, 0x3C, 0x35, 0x3D,
	0x22, 0x2A, 0x23, 0x2B, 0x26, 0x2E, 0x27, 0x2F,
	0x32, 0x3A, 0x33, 0x3B, 0x36, 0x3E, 0x37, 0x3F,
};

#if 0
void block_permute(short int *block)
{
    int tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
    int i;

    for(i=0;i<8;i++) {
        tmp1 = block[1];
        tmp2 = block[2];
        tmp3 = block[3];
        tmp4 = block[4];
        tmp5 = block[5];
        tmp6 = block[6];
        block[1] = tmp2;
        block[2] = tmp4;
        block[3] = tmp6;
        block[4] = tmp1;
        block[5] = tmp3;
        block[6] = tmp5;
        block += 8;
    }
}
#endif

static int q_intra_matrix[64];

static int dct_quantize(DCTELEM *block, int n,
                        int qscale)
{
    int i, j, level, last_non_zero, q;
    const int *qmat;

    av_fdct (block);

    /* we need this permutation so that we correct the IDCT
       permutation. will be moved into DCT code */
    //block_permute(block);

    /*if (n < 4)
        q = s->y_dc_scale;
    else
        q = s->c_dc_scale;
    q = q << 3;*/
    q = 64;   
    /* note: block[0] is assumed to be positive */
    block[0] = (block[0] + (q >> 1)) / q;
    i = 1;
    last_non_zero = 0;

    qmat = q_intra_matrix;
    for(;i<64;i++) {
        j = zr_zigzag_direct[i];
        level = block[j];
        level = level * qmat[j];
        /* XXX: slight error for the low range. Test should be equivalent to
           (level <= -(1 << (QMAT_SHIFT - 3)) || level >= (1 <<
           (QMAT_SHIFT - 3)))
        */
        if (((level << (31 - (QMAT_SHIFT - 3))) >> (31 - (QMAT_SHIFT - 3))) != 
            level) {
            level = level / (1 << (QMAT_SHIFT - 3));
            /* XXX: currently, this code is not optimal. the range should be:
               mpeg1: -255..255
               mpeg2: -2048..2047
               h263:  -128..127
               mpeg4: -2048..2047
            */
            if (level > 255)
                level = 255;
            else if (level < -255)
                level = -255;
            block[j] = level;
            last_non_zero = i;
        } else {
            block[j] = 0;
        }
	
    }
    return last_non_zero;
}

static int dct_quantize_mmx(DCTELEM *block, int n, int qscale)
{
    int i, j, level, last_non_zero, q;
    const int *qmat;
    DCTELEM *b = block;

    /*for (i = 0; i < 8; i++) {
	    printf("%i %i %i %i %i %i %i %i\n", b[8*i], b[8*i+1], b[8*i+2],
			    b[8*i+3], b[8*i+4], b[8*i+5], b[8*i+6], b[8*i+7]);
    }*/
    av_fdct (block);
    /*for (i = 0; i < 8; i++) {
	    printf("%i %i %i %i %i %i %i %i\n", b[8*i], b[8*i+1], b[8*i+2],
			    b[8*i+3], b[8*i+4], b[8*i+5], b[8*i+6], b[8*i+7]);
    }*/


    /* we need this permutation so that we correct the IDCT
       permutation. will be moved into DCT code */
    //block_permute(block);

    //if (n < 2)
        q = 8;
    /*else
        q = 8;*/
    
    /* note: block[0] is assumed to be positive */
    block[0] = (block[0] + (q >> 1)) / q;
    i = 1;
    last_non_zero = 0;
    qmat = q_intra_matrix;

    for(;i<64;i++) {
        j = zr_zigzag_direct[i];
        level = block[j];
        level = level * qmat[j];
        /* XXX: slight error for the low range. Test should be equivalent to
           (level <= -(1 << (QMAT_SHIFT_MMX - 3)) || level >= (1 <<
           (QMAT_SHIFT_MMX - 3)))
        */
        if (((level << (31 - (QMAT_SHIFT_MMX - 3))) >> (31 - (QMAT_SHIFT_MMX - 3))) != 
            level) {
            level = level / (1 << (QMAT_SHIFT_MMX - 3));
            /* XXX: currently, this code is not optimal. the range should be:
               mpeg1: -255..255
               mpeg2: -2048..2047
               h263:  -128..127
               mpeg4: -2048..2047
	    *  jpeg: -1024..1023   11 bit */
            if (level > 1023)
                level = 1023;
            else if (level < -1024)
                level = -1024;
            block[j] = level;
            last_non_zero = i;
        } else {
            block[j] = 0;
        }
    }
    /*for (i = 0; i < 8; i++) {
	    printf("%i %i %i %i %i %i %i %i\n", b[8*i], b[8*i+1], b[8*i+2],
			    b[8*i+3], b[8*i+4], b[8*i+5], b[8*i+6], b[8*i+7]);
    }*/

    return last_non_zero;
}

static void convert_matrix(int *qmat, const unsigned short *quant_matrix, 
		int qscale)
{
    int i;

    if (av_fdct == jpeg_fdct_ifast) {
        for(i=0;i<64;i++) {
            /* 16 <= qscale * quant_matrix[i] <= 7905 */
            /* 19952 <= aanscales[i] * qscale * quant_matrix[i] <= 249205026 */
            
            qmat[i] = (int)(((unsigned long long)1 << (QMAT_SHIFT + 11)) / 
                            (aanscales[i] * qscale * quant_matrix[i]));
        }
    } else {
        for(i=0;i<64;i++) {
            /* We can safely suppose that 16 <= quant_matrix[i] <= 255
               So 16 <= qscale * quant_matrix[i] <= 7905
               so (1 << QMAT_SHIFT) / 16 >= qmat[i] >= (1 << QMAT_SHIFT) / 7905
            */
            qmat[i] = (1 << QMAT_SHIFT_MMX) / (qscale * quant_matrix[i]);
        }
    }
}

#define SOF0	0xC0
#define SOI	0xD8
#define	EOI	0xD9
#define DQT	0xDB
#define DHT	0xC4
#define SOS	0xDA

/* this is almost the quantisation table, used for luminance and chrominance */
/*short int zr_default_intra_matrix[64] = {
    16,  11,  10,  16,  24,  40,  51,  61,
    12,  12,  14,  19,  26,  58,  60,  55,
    14,  13,  16,  24,  40,  57,  69,  56,
    14,  17,  22,  29,  51,  87,  80,  62,
    18,  22,  37,  56,  68, 109, 103,  77,
    24,  35,  55,  64,  81, 104, 113,  92,
    49,  64,  78,  87, 103, 121, 120, 101,
    72,  92,  95,  98, 112, 100, 103,  99
};*/
/*
short int default_intra_matrix[64] = {
	8, 16, 19, 22, 26, 27, 29, 34,
	16, 16, 22, 24, 27, 29, 34, 37,
	19, 22, 26, 27, 29, 34, 34, 38,
	22, 22, 26, 27, 29, 34, 37, 40,
	22, 26, 27, 29, 32, 35, 40, 48,
	26, 27, 29, 32, 35, 40, 48, 58,
	26, 27, 29, 34, 38, 46, 56, 69,
	27, 29, 35, 38, 46, 56, 69, 83
};
*/
extern short int default_intra_matrix[64];

static short int intra_matrix[64];

/* Set up the standard Huffman tables (cf. JPEG standard section K.3) */
/* IMPORTANT: these are only valid for 8-bit data precision! */
static const unsigned char bits_dc_luminance[17] =
{ /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static const unsigned char val_dc_luminance[] =
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

#if 0
static const unsigned char bits_dc_chrominance[17] =
{ /* 0-base */ 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
static const unsigned char val_dc_chrominance[] =
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
#endif

static const unsigned char bits_ac_luminance[17] =
{ /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
static const unsigned char val_ac_luminance[] =
{ 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
  0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
  0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
  0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
  0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
  0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
  0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
  0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
  0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
  0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
  0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
  0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
  0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
  0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
  0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
  0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
  0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa 
};

#if 0
static const unsigned char bits_ac_chrominance[17] =
{ /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };

static const unsigned char val_ac_chrominance[] =
{ 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
  0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
  0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
  0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
  0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
  0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
  0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
  0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
  0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
  0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
  0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
  0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
  0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa 
};
#endif

static unsigned char huff_size_dc_luminance[12];
static unsigned short huff_code_dc_luminance[12];
#if 0
unsigned char huff_size_dc_chrominance[12];
unsigned short huff_code_dc_chrominance[12];
#endif

static unsigned char huff_size_ac_luminance[256];
static unsigned short huff_code_ac_luminance[256];
#if 0
unsigned char huff_size_ac_chrominance[256];
unsigned short huff_code_ac_chrominance[256];
#endif 

static int last_dc[3];
static int block_last_index[4];

/* isn't this function nicer than the one in the libjpeg ? */
static void build_huffman_codes(unsigned char *huff_size, 
		unsigned short *huff_code, const unsigned char *bits_table, 
		const unsigned char *val_table)
{
    int i, j, k,nb, code, sym;

    code = 0;
    k = 0;
    for(i=1;i<=16;i++) {
        nb = bits_table[i];
        for(j=0;j<nb;j++) {
            sym = val_table[k++];
            huff_size[sym] = i;
            huff_code[sym] = code;
            code++;
        }
        code <<= 1;
    }
}

static int zr_mjpeg_init()
{
    /* build all the huffman tables */
    build_huffman_codes(huff_size_dc_luminance, huff_code_dc_luminance,
                        bits_dc_luminance, val_dc_luminance);
    //build_huffman_codes(huff_size_dc_chrominance, huff_code_dc_chrominance,
    //                    bits_dc_chrominance, val_dc_chrominance);
    build_huffman_codes(huff_size_ac_luminance, huff_code_ac_luminance,
                        bits_ac_luminance, val_ac_luminance);
    //build_huffman_codes(huff_size_ac_chrominance, huff_code_ac_chrominance,
    //                    bits_ac_chrominance, val_ac_chrominance);
    
    return 0;
}

static void zr_mjpeg_close()
{
}

static inline void put_marker(PutBitContext *p, int code)
{
    put_bits(p, 8, 0xff);
    put_bits(p, 8, code);
}

/* table_class: 0 = DC coef, 1 = AC coefs */
static int put_huffman_table(int table_class, int table_id,
                             const unsigned char *bits_table, 
			     const unsigned char *value_table)
{
    PutBitContext *p = &pb;
    int n, i;

    put_bits(p, 4, table_class);
    put_bits(p, 4, table_id);

    n = 0;
    for(i=1;i<=16;i++) {
        n += bits_table[i];
        put_bits(p, 8, bits_table[i]);
    }

    for(i=0;i<n;i++)
        put_bits(p, 8, value_table[i]);

    return n + 17;
}

static void jpeg_qtable_header()
{
    PutBitContext *p = &pb;
    int i, j, size;

    /* quant matrixes */
    put_marker(p, DQT);
    put_bits(p, 16, 2 + 1 * (1 + 64));
    put_bits(p, 4, 0); /* 8 bit precision */
    put_bits(p, 4, 0); /* table 0 */
    for(i=0;i<64;i++) {
        j = zr_zigzag_direct[i];
        put_bits(p, 8, intra_matrix[j]);
    }
}

static void jpeg_htable_header() {
    PutBitContext *p = &pb;
    int i, j, size;
    unsigned char *ptr;
    /* huffman table */
    put_marker(p, DHT);
    flush_put_bits(p);
    ptr = p->buf_ptr;
    put_bits(p, 16, 0); /* patched later */
    size = 2;
    size += put_huffman_table(0, 0, bits_dc_luminance, val_dc_luminance);
  //  size += put_huffman_table(0, 1, bits_dc_chrominance, val_dc_chrominance);
    
    ptr[0] = size >> 8;
    ptr[1] = size;
    put_marker(p, DHT);
    flush_put_bits(p);
    ptr = p->buf_ptr;
    put_bits(p, 16, 0); /* patched later */
    size = 2;
    size += put_huffman_table(1, 0, bits_ac_luminance, val_ac_luminance);
   // size += put_huffman_table(1, 1, bits_ac_chrominance, val_ac_chrominance);
    ptr[0] = size >> 8;
    ptr[1] = size;
}

static void zr_mjpeg_picture_header()
{
    put_marker(&pb, SOI);

    if (first) {
    	jpeg_qtable_header();
    	jpeg_htable_header();
	first = 0;
    }
    put_marker(&pb, SOF0);

    put_bits(&pb, 16, 17);
    put_bits(&pb, 8, 8); /* 8 bits/component */
    put_bits(&pb, 16, height);
    put_bits(&pb, 16, width);
    put_bits(&pb, 8, 3); /* 3 components */
    
    /* Y component */
    put_bits(&pb, 8, 0); /* component number */
    put_bits(&pb, 4, 2); /* H factor */
    put_bits(&pb, 4, 1); /* V factor */
    put_bits(&pb, 8, 0); /* select matrix */
    
    /* Cb component */
    put_bits(&pb, 8, 1); /* component number */
    put_bits(&pb, 4, 1); /* H factor */
    put_bits(&pb, 4, 1); /* V factor */
    put_bits(&pb, 8, 0); /* select matrix */

    /* Cr component */
    put_bits(&pb, 8, 2); /* component number */
    put_bits(&pb, 4, 1); /* H factor */
    put_bits(&pb, 4, 1); /* V factor */
    put_bits(&pb, 8, 0); /* select matrix */


    /* scan header */
    put_marker(&pb, SOS);
    put_bits(&pb, 16, 12); /* length */
    put_bits(&pb, 8, 3); /* 3 components */
    
    /* Y component */
    put_bits(&pb, 8, 0); /* index */
    put_bits(&pb, 4, 0); /* DC huffman table index */
    put_bits(&pb, 4, 0); /* AC huffman table index */
    
    /* Cb component */
    put_bits(&pb, 8, 1); /* index */
    put_bits(&pb, 4, 0); /* DC huffman table index */
    put_bits(&pb, 4, 0); /* AC huffman table index */
    
    /* Cr component */
    put_bits(&pb, 8, 2); /* index */
    put_bits(&pb, 4, 0); /* DC huffman table index */
    put_bits(&pb, 4, 0); /* AC huffman table index */

    put_bits(&pb, 8, 0); /* Ss (not used) */
    put_bits(&pb, 8, 63); /* Se (not used) */
    put_bits(&pb, 8, 0); /* (not used) */
}

static void zr_flush_buffer(PutBitContext *s)
{
    int size;
    if (s->write_data) {
        size = s->buf_ptr - s->buf;
        if (size > 0)
            s->write_data(s->opaque, s->buf, size);
        s->buf_ptr = s->buf;
        s->data_out_size += size;
    }
}

/* pad the end of the output stream with ones */
static void zr_jflush_put_bits(PutBitContext *s)
{
    unsigned int b;
    s->bit_buf |= ~1U >> s->bit_cnt; /* set all the unused bits to one */

    while (s->bit_cnt > 0) {
        b = s->bit_buf >> 24;
        *s->buf_ptr++ = b;
        if (b == 0xff)
            *s->buf_ptr++ = 0;
        s->bit_buf<<=8;
        s->bit_cnt-=8;
    }
    zr_flush_buffer(s);
    s->bit_cnt=0;
    s->bit_buf=0;
}

static void zr_mjpeg_picture_trailer()
{
    zr_jflush_put_bits(&pb);
    put_marker(&pb, EOI);
}

static inline void encode_dc(int val, unsigned char *huff_size, 
		unsigned short *huff_code)
{
    int mant, nbits;

    if (val == 0) {
	 //   printf("dc val=0 ");
        jput_bits(&pb, huff_size[0], huff_code[0]);
	//printf("dc encoding %d %d\n", huff_size[0], huff_code[0]);
    } else {
        mant = val;
        if (val < 0) {
            val = -val;
            mant--;
        }
        
        /* compute the log (XXX: optimize) */
        nbits = 0;
        while (val != 0) {
            val = val >> 1;
            nbits++;
        }
	/*nbits = av_log2(val);*/
            
	//printf("dc ");
        jput_bits(&pb, huff_size[nbits], huff_code[nbits]);
	//printf("dc encoding %d %d\n", huff_size[nbits], huff_code[nbits]);
        
	//printf("dc ");
        jput_bits(&pb, nbits, mant & ((1 << nbits) - 1));
	//printf("dc encoding %d %d\n", huff_size[nbits], huff_code[nbits]);
    }
}

static void encode_block(DCTELEM *b, int n)
{
    int mant, nbits, code, i, j;
    int component, dc, run, last_index, val;
    unsigned char *huff_size_ac;
    unsigned short *huff_code_ac;
    
    /* DC coef */
    component = (n <= 1 ? 0 : n - 2 + 1);
    dc = b[0]; /* overflow is impossible */
    /*for (i = 0; i < 8; i++) {
	    printf("%i %i %i %i %i %i %i %i\n", b[8*i], b[8*i+1], b[8*i+2],
			    b[8*i+3], b[8*i+4], b[8+i*5], b[8+i*6], b[8+i*7]);
    }*/
    val = dc - last_dc[component];
    //if (n < 2) {
        encode_dc(val, huff_size_dc_luminance, huff_code_dc_luminance);
        huff_size_ac = huff_size_ac_luminance;
        huff_code_ac = huff_code_ac_luminance;
    //} else {
    //    encode_dc(val, huff_size_dc_chrominance, huff_code_dc_chrominance);
    //    huff_size_ac = huff_size_ac_chrominance;
    //    huff_code_ac = huff_code_ac_chrominance;
    //}
    last_dc[component] = dc;
    
    /* AC coefs */
    
    run = 0;
    last_index = block_last_index[n];
    for(i=1;i<=last_index;i++) {
        j = zr_zigzag_direct[i];
        val = b[j];
        if (val == 0) {
            run++;
        } else {
            while (run >= 16) {
		//printf("ac 16 white ");
                jput_bits(&pb, huff_size_ac[0xf0], huff_code_ac[0xf0]);
                run -= 16;
            }
            mant = val;
            if (val < 0) {
                val = -val;
                mant--;
            }
            
            /* compute the log (XXX: optimize) */
            nbits = 0;
            while (val != 0) {
                val = val >> 1;
                nbits++;
            }
            code = (run << 4) | nbits;

	    //printf("ac ");
            jput_bits(&pb, huff_size_ac[code], huff_code_ac[code]);
        
	    //printf("ac ");
            jput_bits(&pb, nbits, mant & ((1 << nbits) - 1));
            run = 0;
        }
    }

    /* output EOB only if not already 64 values */
    if (last_index < 63 || run != 0) {
	//printf("ac EOB ");
        jput_bits(&pb, huff_size_ac[0], huff_code_ac[0]);
    }
}

static void zr_mjpeg_encode_mb(DCTELEM **bla)
{
    encode_block(*(bla), 0);
    encode_block(*(bla+1), 1);
    if (bw) {
    	jput_bits(&pb, 12, 512+128+8+2); /* 2 times code for 'no color'
				      * 001010001010 */
    } else {
	    encode_block(*(bla+2), 2);
	    encode_block(*(bla+3), 3);
    }
}

static int mb_width, mb_height, mb_x, mb_y;
static unsigned char *y_data, *u_data, *v_data;
static int y_ps, u_ps, v_ps, y_rs, u_rs, v_rs;
static char code[256*1024]; // 256kb!
/* this function can take all kinds of YUV colorspaces
 * YV12, YVYU, UYVY. The necesary parameters must be set up by te caller
 * y_ps means "y pixel size", y_rs means "y row size".
 * For YUYV, for example, is u = y + 1, v = y + 3, y_ps = 2, u_ps = 4
 * v_ps = 4, y_rs = u_rs = v_rs.
 *  
 * The data is straightened out at the moment it is put in DCT
 * blocks, there are therefore no spurious memcopies involved */
/* Notice that w must be a multiple of 16 and h must be a multiple of
 * fields*8 */
/* We produce YUV422 jpegs, the colors must be subsampled horizontally,
 * if the colors are also subsampled vertically, then this function
 * performs cheap upsampling (better solution will be: a DCT that is
 * optimized in the case that every two rows are the same) */
/* cu = 0 means 'No cheap upsampling'
 * cu = 1 means 'perform cheap upsampling' */
void mjpeg_encoder_init(int w, int h, 
		unsigned char* y, int y_psize, int y_rsize, 
		unsigned char* u, int u_psize, int u_rsize,
		unsigned char* v, int v_psize, int v_rsize,
		int f, int cu, int q, int b) {
	int i;
	mp_msg(MSGT_VO, MSGL_V, "JPEnc init: %dx%d %p %d %d %p %d %d %p %d %d\n",
			w, h, y, y_psize, y_rsize, 
			u, u_psize, u_rsize,
			v, v_psize, v_rsize);
	y_data = y; u_data = u; v_data = v;
	y_ps = y_psize; u_ps = u_psize; v_ps = v_psize;
	y_rs = y_rsize*f; 
	u_rs = u_rsize*f; 
	v_rs = v_rsize*f;
	width = w;
	height = h/f;
	fields = f;
	qscale = q;
	cheap_upsample = cu;
	mb_width = width/16;
	mb_height = height/8;
	bw = b;
	zr_mjpeg_init();
	i = 0;
	intra_matrix[0] = default_intra_matrix[0];
	for (i = 1; i < 64; i++) {
		intra_matrix[i] = (default_intra_matrix[i]*qscale) >> 3;
	}
	if (
#ifdef HAVE_MMX
			av_fdct != fdct_mmx && 
#endif
			av_fdct != jpeg_fdct_ifast) {
		/* libavcodec is probably not yet initialized */
		av_fdct = jpeg_fdct_ifast;
#ifdef HAVE_MMX
		dsputil_init_mmx();
#endif
	}
	convert_matrix(q_intra_matrix, intra_matrix, 8);
	blck = malloc(4*sizeof(DCTELEM*));
	blck[0] = malloc(64*sizeof(DCTELEM));
	blck[1] = malloc(64*sizeof(DCTELEM));
	blck[2] = malloc(64*sizeof(DCTELEM));
	blck[3] = malloc(64*sizeof(DCTELEM));
}	

int mjpeg_encode_frame(char *bufr, int field) {
	int i, j, k, l;
	short int *dest;
	unsigned char *source;
	/* initialize the buffer */
	if (field == 1) {
		y_data += y_rs/2;
		u_data += u_rs/2;
		v_data += v_rs/2;
	}
	init_put_bits(&pb, bufr, 1024*256, NULL, NULL);

	zr_mjpeg_picture_header();

	last_dc[0] = 128; last_dc[1] = 128; last_dc[2] = 128;
	mb_x = 0;
	mb_y = 0;
	for (mb_y = 0; mb_y < mb_height; mb_y++) {
		for (mb_x = 0; mb_x < mb_width; mb_x++) {
			//printf("Processing macroblock mb_x=%d, mb_y=%d, mb_width=%d, mb_height=%d, size=%d\n", mb_x, mb_y, mb_width, mb_height, pb.buf_ptr - pb.buf);
			/* fill 2 Y macroblocks and one U and one V */
			source = mb_y * 8 * y_rs + 16 * y_ps * mb_x + y_data;
			dest = blck[0];
			for (i = 0; i < 8; i++) {
				for (j = 0; j < 8; j++) {
					dest[j] = source[j*y_ps];
				}
				dest += 8;
				source += y_rs;
			}
			source = mb_y * 8 * y_rs + (16*mb_x + 8)*y_ps + y_data;
			dest = blck[1];
			for (i = 0; i < 8; i++) {
				for (j = 0; j < 8; j++) {
					dest[j] = source[j*y_ps];
				}
				dest += 8;
				source += y_rs;
			}
			if (!bw) {
			if (cheap_upsample) {
				source = mb_y*4*u_rs + 8*mb_x*u_ps + u_data;
				dest = blck[2];
				for (i = 0; i < 4; i++) {
					for (j = 0; j < 8; j++) {
						dest[j] = source[j*u_ps];
						dest[j+8] = source[j*u_ps];
					}
					dest += 16;
					source += u_rs;
				}
				source = mb_y*4*v_rs + 8*mb_x*v_ps + v_data;
				dest = blck[3];
				for (i = 0; i < 4; i++) {
					for (j = 0; j < 8; j++) {
						dest[j] = source[j*v_ps];
						dest[j+8] = source[j*v_ps];
					}
					dest += 16;
					source += u_rs;
				}
			} else {
				source = mb_y*8*u_rs + 8*mb_x*u_ps + u_data;
				dest = blck[2];
				for (i = 0; i < 8; i++) {
					for (j = 0; j < 8; j++) {
						dest[j] = source[j*u_ps];
					}
					dest += 8;
					source += u_rs;
				}
				source = mb_y*8*v_rs + 8*mb_x*v_ps + v_data;
				dest = blck[3];
				for (i = 0; i < 8; i++) {
					for (j = 0; j < 8; j++) {
						dest[j] = source[j*v_ps];
					}
					dest += 8;
					source += u_rs;
				}
			}
			}
			/* so, **blck is filled now... */

			for(i = 0; i < 2; i++) {
				if (av_fdct == jpeg_fdct_ifast)
					block_last_index[i] = 
						dct_quantize(blck[i], 
								i, qscale);
				else
					block_last_index[i] = 
						dct_quantize_mmx(blck[i],
								i, qscale);
			}
			if (!bw) {
			for(i = 2; i < 4; i++) {
				if (av_fdct == jpeg_fdct_ifast)
					block_last_index[i] = 
						dct_quantize(blck[i], 
								i, qscale);
				else
					block_last_index[i] = 
						dct_quantize_mmx(blck[i],
								i, qscale);
			}
			}
				zr_mjpeg_encode_mb(blck);
		}
	}
	emms_c();
	zr_mjpeg_picture_trailer();
	flush_put_bits(&pb);	
	zr_mjpeg_close();
	if (field == 1) {
		y_data -= y_rs/2;
		u_data -= u_rs/2;
		v_data -= v_rs/2;
	}
	return pb.buf_ptr - pb.buf;
}

