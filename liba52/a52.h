/*
 * a52.h
 * Copyright (C) 2000-2001 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of a52dec, a free ATSC A-52 stream decoder.
 * See http://liba52.sourceforge.net/ for updates.
 *
 * a52dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * a52dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef A52_H
#define A52_H

#ifndef LIBA52_DOUBLE
typedef float sample_t;
#else
typedef double sample_t;
#endif

typedef struct a52_ba_s {
    uint16_t fsnroffst;		/* fine SNR offset */
    uint16_t fgaincod;		/* fast gain */
    uint16_t deltbae;		/* delta bit allocation exists */
    int8_t deltba[50];		/* per-band delta bit allocation */
} a52_ba_t;

typedef struct a52_state_s {
    uint8_t fscod;		/* sample rate */
    uint8_t halfrate;		/* halfrate factor */
    uint8_t acmod;		/* coded channels */
    sample_t clev;		/* centre channel mix level */
    sample_t slev;		/* surround channels mix level */
    uint8_t lfeon;		/* coded lfe channel */

    int output;			/* type of output */
    sample_t level;		/* output level */
    sample_t bias;		/* output bias */

    int dynrnge;		/* apply dynamic range */
    sample_t dynrng;		/* dynamic range */
    void * dynrngdata;		/* dynamic range callback funtion and data */
    sample_t (* dynrngcall) (sample_t range, void * dynrngdata);

    uint16_t cplinu;		/* coupling in use */
    uint16_t chincpl[5];	/* channel coupled */
    uint16_t phsflginu;		/* phase flags in use (stereo only) */
    uint16_t cplbndstrc[18];	/* coupling band structure */
    uint16_t cplstrtmant;	/* coupling channel start mantissa */
    uint16_t cplendmant;	/* coupling channel end mantissa */
    sample_t cplco[5][18];	/* coupling coordinates */

    /* derived information */
    uint16_t cplstrtbnd;	/* coupling start band (for bit allocation) */
    uint16_t ncplbnd;		/* number of coupling bands */

    uint16_t rematflg[4];	/* stereo rematrixing */

    uint16_t endmant[5];	/* channel end mantissa */

    uint8_t cpl_exp[256];	/* decoded coupling channel exponents */
    uint8_t fbw_exp[5][256];	/* decoded channel exponents */
    uint8_t lfe_exp[7];		/* decoded lfe channel exponents */

    uint16_t sdcycod;		/* slow decay */
    uint16_t fdcycod;		/* fast decay */
    uint16_t sgaincod;		/* slow gain */
    uint16_t dbpbcod;		/* dB per bit - encodes the dbknee value */
    uint16_t floorcod;		/* masking floor */

    uint16_t csnroffst;		/* coarse SNR offset */
    a52_ba_t cplba;		/* coupling bit allocation parameters */
    a52_ba_t ba[5];		/* channel bit allocation parameters */
    a52_ba_t lfeba;		/* lfe bit allocation parameters */

    uint16_t cplfleak;		/* coupling fast leak init */
    uint16_t cplsleak;		/* coupling slow leak init */

    /* derived bit allocation information */
    int8_t fbw_bap[5][256];
    int8_t cpl_bap[256];
    int8_t lfe_bap[7];
} a52_state_t;

#define A52_CHANNEL 0
#define A52_MONO 1
#define A52_STEREO 2
#define A52_3F 3
#define A52_2F1R 4
#define A52_3F1R 5
#define A52_2F2R 6
#define A52_3F2R 7
#define A52_CHANNEL1 8
#define A52_CHANNEL2 9
#define A52_DOLBY 10
#define A52_CHANNEL_MASK 15

#define A52_LFE 16
#define A52_ADJUST_LEVEL 32

sample_t * a52_init (uint32_t mm_accel);
int a52_syncinfo (uint8_t * buf, int * flags,
		  int * sample_rate, int * bit_rate);
int a52_frame (a52_state_t * state, uint8_t * buf, int * flags,
	       sample_t * level, sample_t bias);
void a52_dynrng (a52_state_t * state,
		 sample_t (* call) (sample_t, void *), void * data);
int a52_block (a52_state_t * state, sample_t * samples);

void a52_resample_init(int _flags,int _chans);
extern int (* a52_resample) (float * _f, int16_t * s16);

uint16_t crc16_block(uint8_t *data,uint32_t num_bytes);

#endif /* A52_H */
