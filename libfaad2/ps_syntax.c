/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR and PS decoding
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
** $Id: ps_syntax.c,v 1.6 2004/09/04 14:56:28 menno Exp $
**/

#include "common.h"

#ifdef PS_DEC

#include "bits.h"
#include "ps_dec.h"

/* type definitaions */
typedef const int8_t (*ps_huff_tab)[2];

/* static data tables */
static const uint8_t nr_iid_par_tab[] = {
    10, 20, 34, 10, 20, 34, 0, 0
};
static const uint8_t nr_ipdopd_par_tab[] = {
    5, 11, 17, 5, 11, 17, 0, 0
};
static const uint8_t nr_icc_par_tab[] = {
    10, 20, 34, 10, 20, 34, 0, 0
};
static const uint8_t num_env_tab[][4] = {
    { 0, 1, 2, 4 },
    { 1, 2, 3, 4 }
};

/* binary lookup huffman tables */
static const int8_t f_huff_iid_def[][2] = {
    { /*0*/ -31, 1 },             /* index 0: 1 bits: x */
    { 2, 3 },                     /* index 1: 2 bits: 1x */
    { /*1*/ -30, /*-1*/ -32 },    /* index 2: 3 bits: 10x */
    { 4, 5 },                     /* index 3: 3 bits: 11x */
    { /*2*/ -29, /*-2*/ -33 },    /* index 4: 4 bits: 110x */
    { 6, 7 },                     /* index 5: 4 bits: 111x */
    { /*3*/ -28, /*-3*/ -34 },    /* index 6: 5 bits: 1110x */
    { 8, 9 },                     /* index 7: 5 bits: 1111x */
    { /*-4*/ -35, /*4*/ -27 },    /* index 8: 6 bits: 11110x */
    { /*5*/ -26, 10 },            /* index 9: 6 bits: 11111x */
    { /*-5*/ -36, 11 },           /* index 10: 7 bits: 111111x */
    { /*6*/ -25, 12 },            /* index 11: 8 bits: 1111111x */
    { /*-6*/ -37, 13 },           /* index 12: 9 bits: 11111111x */
    { /*-7*/ -38, 14 },           /* index 13: 10 bits: 111111111x */
    { /*7*/ -24, 15 },            /* index 14: 11 bits: 1111111111x */
    { 16, 17 },                   /* index 15: 12 bits: 11111111111x */
    { /*8*/ -23, /*-8*/ -39 },    /* index 16: 13 bits: 111111111110x */
    { 18, 19 },                   /* index 17: 13 bits: 111111111111x */
    { /*9*/ -22, /*10*/ -21 },    /* index 18: 14 bits: 1111111111110x */
    { 20, 21 },                   /* index 19: 14 bits: 1111111111111x */
    { /*-9*/ -40, /*11*/ -20 },   /* index 20: 15 bits: 11111111111110x */
    { 22, 23 },                   /* index 21: 15 bits: 11111111111111x */
    { /*-10*/ -41, 24 },          /* index 22: 16 bits: 111111111111110x */
    { 25, 26 },                   /* index 23: 16 bits: 111111111111111x */
    { /*-11*/ -42, /*-14*/ -45 }, /* index 24: 17 bits: 1111111111111101x */
    { /*-13*/ -44, /*-12*/ -43 }, /* index 25: 17 bits: 1111111111111110x */
    { /*12*/ -19, 27 },           /* index 26: 17 bits: 1111111111111111x */
    { /*13*/ -18, /*14*/ -17 }    /* index 27: 18 bits: 11111111111111111x */
};

static const int8_t t_huff_iid_def[][2] = {
    { /*0*/ -31, 1 },             /* index 0: 1 bits: x */
    { /*-1*/ -32, 2 },            /* index 1: 2 bits: 1x */
    { /*1*/ -30, 3 },             /* index 2: 3 bits: 11x */
    { /*-2*/ -33, 4 },            /* index 3: 4 bits: 111x */
    { /*2*/ -29, 5 },             /* index 4: 5 bits: 1111x */
    { /*-3*/ -34, 6 },            /* index 5: 6 bits: 11111x */
    { /*3*/ -28, 7 },             /* index 6: 7 bits: 111111x */
    { /*-4*/ -35, 8 },            /* index 7: 8 bits: 1111111x */
    { /*4*/ -27, 9 },             /* index 8: 9 bits: 11111111x */
    { /*-5*/ -36, 10 },           /* index 9: 10 bits: 111111111x */
    { /*5*/ -26, 11 },            /* index 10: 11 bits: 1111111111x */
    { /*-6*/ -37, 12 },           /* index 11: 12 bits: 11111111111x */
    { /*6*/ -25, 13 },            /* index 12: 13 bits: 111111111111x */
    { /*7*/ -24, 14 },            /* index 13: 14 bits: 1111111111111x */
    { /*-7*/ -38, 15 },           /* index 14: 15 bits: 11111111111111x */
    { 16, 17 },                   /* index 15: 16 bits: 111111111111111x */
    { /*8*/ -23, /*-8*/ -39 },    /* index 16: 17 bits: 1111111111111110x */
    { 18, 19 },                   /* index 17: 17 bits: 1111111111111111x */
    { 20, 21 },                   /* index 18: 18 bits: 11111111111111110x */
    { 22, 23 },                   /* index 19: 18 bits: 11111111111111111x */
    { /*9*/ -22, /*-14*/ -45 },   /* index 20: 19 bits: 111111111111111100x */
    { /*-13*/ -44, /*-12*/ -43 }, /* index 21: 19 bits: 111111111111111101x */
    { 24, 25 },                   /* index 22: 19 bits: 111111111111111110x */
    { 26, 27 },                   /* index 23: 19 bits: 111111111111111111x */
    { /*-11*/ -42, /*-10*/ -41 }, /* index 24: 20 bits: 1111111111111111100x */
    { /*-9*/ -40, /*10*/ -21 },   /* index 25: 20 bits: 1111111111111111101x */
    { /*11*/ -20, /*12*/ -19 },   /* index 26: 20 bits: 1111111111111111110x */
    { /*13*/ -18, /*14*/ -17 }    /* index 27: 20 bits: 1111111111111111111x */
};

static const int8_t f_huff_iid_fine[][2] = {
    { 1, /*0*/ -31 },             /* index 0: 1 bits: x */
    { 2, 3 },                     /* index 1: 2 bits: 0x */
    { 4, /*-1*/ -32 },            /* index 2: 3 bits: 00x */
    { /*1*/ -30, 5 },             /* index 3: 3 bits: 01x */
    { /*-2*/ -33, /*2*/ -29 },    /* index 4: 4 bits: 000x */
    { 6, 7 },                     /* index 5: 4 bits: 011x */
    { /*-3*/ -34, /*3*/ -28 },    /* index 6: 5 bits: 0110x */
    { 8, 9 },                     /* index 7: 5 bits: 0111x */
    { /*-4*/ -35, /*4*/ -27 },    /* index 8: 6 bits: 01110x */
    { 10, 11 },                   /* index 9: 6 bits: 01111x */
    { /*-5*/ -36, /*5*/ -26 },    /* index 10: 7 bits: 011110x */
    { 12, 13 },                   /* index 11: 7 bits: 011111x */
    { /*-6*/ -37, /*6*/ -25 },    /* index 12: 8 bits: 0111110x */
    { 14, 15 },                   /* index 13: 8 bits: 0111111x */
    { /*7*/ -24, 16 },            /* index 14: 9 bits: 01111110x */
    { 17, 18 },                   /* index 15: 9 bits: 01111111x */
    { 19, /*-8*/ -39 },           /* index 16: 10 bits: 011111101x */
    { /*8*/ -23, 20 },            /* index 17: 10 bits: 011111110x */
    { 21, /*-7*/ -38 },           /* index 18: 10 bits: 011111111x */
    { /*10*/ -21, 22 },           /* index 19: 11 bits: 0111111010x */
    { 23, /*-9*/ -40 },           /* index 20: 11 bits: 0111111101x */
    { /*9*/ -22, 24 },            /* index 21: 11 bits: 0111111110x */
    { /*-11*/ -42, /*11*/ -20 },  /* index 22: 12 bits: 01111110101x */
    { 25, 26 },                   /* index 23: 12 bits: 01111111010x */
    { 27, /*-10*/ -41 },          /* index 24: 12 bits: 01111111101x */
    { 28, /*-12*/ -43 },          /* index 25: 13 bits: 011111110100x */
    { /*12*/ -19, 29 },           /* index 26: 13 bits: 011111110101x */
    { 30, 31 },                   /* index 27: 13 bits: 011111111010x */
    { 32, /*-14*/ -45 },          /* index 28: 14 bits: 0111111101000x */
    { /*14*/ -17, 33 },           /* index 29: 14 bits: 0111111101011x */
    { 34, /*-13*/ -44 },          /* index 30: 14 bits: 0111111110100x */
    { /*13*/ -18, 35 },           /* index 31: 14 bits: 0111111110101x */
    { 36, 37 },                   /* index 32: 15 bits: 01111111010000x */
    { 38, /*-15*/ -46 },          /* index 33: 15 bits: 01111111010111x */
    { /*15*/ -16, 39 },           /* index 34: 15 bits: 01111111101000x */
    { 40, 41 },                   /* index 35: 15 bits: 01111111101011x */
    { 42, 43 },                   /* index 36: 16 bits: 011111110100000x */
    { /*-17*/ -48, /*17*/ -14 },  /* index 37: 16 bits: 011111110100001x */
    { 44, 45 },                   /* index 38: 16 bits: 011111110101110x */
    { 46, 47 },                   /* index 39: 16 bits: 011111111010001x */
    { 48, 49 },                   /* index 40: 16 bits: 011111111010110x */
    { /*-16*/ -47, /*16*/ -15 },  /* index 41: 16 bits: 011111111010111x */
    { /*-21*/ -52, /*21*/ -10 },  /* index 42: 17 bits: 0111111101000000x */
    { /*-19*/ -50, /*19*/ -12 },  /* index 43: 17 bits: 0111111101000001x */
    { /*-18*/ -49, /*18*/ -13 },  /* index 44: 17 bits: 0111111101011100x */
    { 50, 51 },                   /* index 45: 17 bits: 0111111101011101x */
    { 52, 53 },                   /* index 46: 17 bits: 0111111110100010x */
    { 54, 55 },                   /* index 47: 17 bits: 0111111110100011x */
    { 56, 57 },                   /* index 48: 17 bits: 0111111110101100x */
    { 58, 59 },                   /* index 49: 17 bits: 0111111110101101x */
    { /*-26*/ -57, /*-25*/ -56 }, /* index 50: 18 bits: 01111111010111010x */
    { /*-28*/ -59, /*-27*/ -58 }, /* index 51: 18 bits: 01111111010111011x */
    { /*-22*/ -53, /*22*/ -9 },   /* index 52: 18 bits: 01111111101000100x */
    { /*-24*/ -55, /*-23*/ -54 }, /* index 53: 18 bits: 01111111101000101x */
    { /*25*/ -6, /*26*/ -5 },     /* index 54: 18 bits: 01111111101000110x */
    { /*23*/ -8, /*24*/ -7 },     /* index 55: 18 bits: 01111111101000111x */
    { /*29*/ -2, /*30*/ -1 },     /* index 56: 18 bits: 01111111101011000x */
    { /*27*/ -4, /*28*/ -3 },     /* index 57: 18 bits: 01111111101011001x */
    { /*-30*/ -61, /*-29*/ -60 }, /* index 58: 18 bits: 01111111101011010x */
    { /*-20*/ -51, /*20*/ -11 }   /* index 59: 18 bits: 01111111101011011x */
};

static const int8_t t_huff_iid_fine[][2] = {
    { 1, /*0*/ -31 },             /* index 0: 1 bits: x */
    { /*1*/ -30, 2 },             /* index 1: 2 bits: 0x */
    { 3, /*-1*/ -32 },            /* index 2: 3 bits: 01x */
    { 4, 5 },                     /* index 3: 4 bits: 010x */
    { 6, 7 },                     /* index 4: 5 bits: 0100x */
    { /*-2*/ -33, /*2*/ -29 },    /* index 5: 5 bits: 0101x */
    { 8, /*-3*/ -34 },            /* index 6: 6 bits: 01000x */
    { /*3*/ -28, 9 },             /* index 7: 6 bits: 01001x */
    { /*-4*/ -35, /*4*/ -27 },    /* index 8: 7 bits: 010000x */
    { 10, 11 },                   /* index 9: 7 bits: 010011x */
    { /*5*/ -26, 12 },            /* index 10: 8 bits: 0100110x */
    { 13, 14 },                   /* index 11: 8 bits: 0100111x */
    { /*-6*/ -37, /*6*/ -25 },    /* index 12: 9 bits: 01001101x */
    { 15, 16 },                   /* index 13: 9 bits: 01001110x */
    { 17, /*-5*/ -36 },           /* index 14: 9 bits: 01001111x */
    { 18, /*-7*/ -38 },           /* index 15: 10 bits: 010011100x */
    { /*7*/ -24, 19 },            /* index 16: 10 bits: 010011101x */
    { 20, 21 },                   /* index 17: 10 bits: 010011110x */
    { /*9*/ -22, 22 },            /* index 18: 11 bits: 0100111000x */
    { 23, 24 },                   /* index 19: 11 bits: 0100111011x */
    { /*-8*/ -39, /*8*/ -23 },    /* index 20: 11 bits: 0100111100x */
    { 25, 26 },                   /* index 21: 11 bits: 0100111101x */
    { /*11*/ -20, 27 },           /* index 22: 12 bits: 01001110001x */
    { 28, 29 },                   /* index 23: 12 bits: 01001110110x */
    { /*-10*/ -41, /*10*/ -21 },  /* index 24: 12 bits: 01001110111x */
    { 30, 31 },                   /* index 25: 12 bits: 01001111010x */
    { 32, /*-9*/ -40 },           /* index 26: 12 bits: 01001111011x */
    { 33, /*-13*/ -44 },          /* index 27: 13 bits: 010011100011x */
    { /*13*/ -18, 34 },           /* index 28: 13 bits: 010011101100x */
    { 35, 36 },                   /* index 29: 13 bits: 010011101101x */
    { 37, /*-12*/ -43 },          /* index 30: 13 bits: 010011110100x */
    { /*12*/ -19, 38 },           /* index 31: 13 bits: 010011110101x */
    { 39, /*-11*/ -42 },          /* index 32: 13 bits: 010011110110x */
    { 40, 41 },                   /* index 33: 14 bits: 0100111000110x */
    { 42, 43 },                   /* index 34: 14 bits: 0100111011001x */
    { 44, 45 },                   /* index 35: 14 bits: 0100111011010x */
    { 46, /*-15*/ -46 },          /* index 36: 14 bits: 0100111011011x */
    { /*15*/ -16, 47 },           /* index 37: 14 bits: 0100111101000x */
    { /*-14*/ -45, /*14*/ -17 },  /* index 38: 14 bits: 0100111101011x */
    { 48, 49 },                   /* index 39: 14 bits: 0100111101100x */
    { /*-21*/ -52, /*-20*/ -51 }, /* index 40: 15 bits: 01001110001100x */
    { /*18*/ -13, /*19*/ -12 },   /* index 41: 15 bits: 01001110001101x */
    { /*-19*/ -50, /*-18*/ -49 }, /* index 42: 15 bits: 01001110110010x */
    { 50, 51 },                   /* index 43: 15 bits: 01001110110011x */
    { 52, 53 },                   /* index 44: 15 bits: 01001110110100x */
    { 54, 55 },                   /* index 45: 15 bits: 01001110110101x */
    { 56, /*-17*/ -48 },          /* index 46: 15 bits: 01001110110110x */
    { /*17*/ -14, 57 },           /* index 47: 15 bits: 01001111010001x */
    { 58, /*-16*/ -47 },          /* index 48: 15 bits: 01001111011000x */
    { /*16*/ -15, 59 },           /* index 49: 15 bits: 01001111011001x */
    { /*-26*/ -57, /*26*/ -5 },   /* index 50: 16 bits: 010011101100110x */
    { /*-28*/ -59, /*-27*/ -58 }, /* index 51: 16 bits: 010011101100111x */
    { /*29*/ -2, /*30*/ -1 },     /* index 52: 16 bits: 010011101101000x */
    { /*27*/ -4, /*28*/ -3 },     /* index 53: 16 bits: 010011101101001x */
    { /*-30*/ -61, /*-29*/ -60 }, /* index 54: 16 bits: 010011101101010x */
    { /*-25*/ -56, /*25*/ -6 },   /* index 55: 16 bits: 010011101101011x */
    { /*-24*/ -55, /*24*/ -7 },   /* index 56: 16 bits: 010011101101100x */
    { /*-23*/ -54, /*23*/ -8 },   /* index 57: 16 bits: 010011110100011x */
    { /*-22*/ -53, /*22*/ -9 },   /* index 58: 16 bits: 010011110110000x */
    { /*20*/ -11, /*21*/ -10 }    /* index 59: 16 bits: 010011110110011x */
};

static const int8_t f_huff_icc[][2] = {
    { /*0*/ -31, 1 },            /* index 0: 1 bits: x */
    { /*1*/ -30, 2 },            /* index 1: 2 bits: 1x */
    { /*-1*/ -32, 3 },           /* index 2: 3 bits: 11x */
    { /*2*/ -29, 4 },            /* index 3: 4 bits: 111x */
    { /*-2*/ -33, 5 },           /* index 4: 5 bits: 1111x */
    { /*3*/ -28, 6 },            /* index 5: 6 bits: 11111x */
    { /*-3*/ -34, 7 },           /* index 6: 7 bits: 111111x */
    { /*4*/ -27, 8 },            /* index 7: 8 bits: 1111111x */
    { /*5*/ -26, 9 },            /* index 8: 9 bits: 11111111x */
    { /*-4*/ -35, 10 },          /* index 9: 10 bits: 111111111x */
    { /*6*/ -25, 11 },           /* index 10: 11 bits: 1111111111x */
    { /*-5*/ -36, 12 },          /* index 11: 12 bits: 11111111111x */
    { /*7*/ -24, 13 },           /* index 12: 13 bits: 111111111111x */
    { /*-6*/ -37, /*-7*/ -38 }   /* index 13: 14 bits: 1111111111111x */
};

static const int8_t t_huff_icc[][2] = {
    { /*0*/ -31, 1 },            /* index 0: 1 bits: x */
    { /*1*/ -30, 2 },            /* index 1: 2 bits: 1x */
    { /*-1*/ -32, 3 },           /* index 2: 3 bits: 11x */
    { /*2*/ -29, 4 },            /* index 3: 4 bits: 111x */
    { /*-2*/ -33, 5 },           /* index 4: 5 bits: 1111x */
    { /*3*/ -28, 6 },            /* index 5: 6 bits: 11111x */
    { /*-3*/ -34, 7 },           /* index 6: 7 bits: 111111x */
    { /*4*/ -27, 8 },            /* index 7: 8 bits: 1111111x */
    { /*-4*/ -35, 9 },           /* index 8: 9 bits: 11111111x */
    { /*5*/ -26, 10 },           /* index 9: 10 bits: 111111111x */
    { /*-5*/ -36, 11 },          /* index 10: 11 bits: 1111111111x */
    { /*6*/ -25, 12 },           /* index 11: 12 bits: 11111111111x */
    { /*-6*/ -37, 13 },          /* index 12: 13 bits: 111111111111x */
    { /*-7*/ -38, /*7*/ -24 }    /* index 13: 14 bits: 1111111111111x */
};

static const int8_t f_huff_ipd[][2] = {
    { 1, /*0*/ -31 },         /* index 0: 1 bits: x */
    { 2, 3 },                 /* index 1: 2 bits: 0x */
    { /*1*/ -30, 4 },         /* index 2: 3 bits: 00x */
    { 5, 6 },                 /* index 3: 3 bits: 01x */
    { /*4*/ -27, /*5*/ -26 }, /* index 4: 4 bits: 001x */
    { /*3*/ -28, /*6*/ -25 }, /* index 5: 4 bits: 010x */
    { /*2*/ -29, /*7*/ -24 }  /* index 6: 4 bits: 011x */
};

static const int8_t t_huff_ipd[][2] = {
    { 1, /*0*/ -31 },         /* index 0: 1 bits: x */
    { 2, 3 },                 /* index 1: 2 bits: 0x */
    { 4, 5 },                 /* index 2: 3 bits: 00x */
    { /*1*/ -30, /*7*/ -24 }, /* index 3: 3 bits: 01x */
    { /*5*/ -26, 6 },         /* index 4: 4 bits: 000x */
    { /*2*/ -29, /*6*/ -25 }, /* index 5: 4 bits: 001x */
    { /*4*/ -27, /*3*/ -28 }  /* index 6: 5 bits: 0001x */
};

static const int8_t f_huff_opd[][2] = {
    { 1, /*0*/ -31 },         /* index 0: 1 bits: x */
    { 2, 3 },                 /* index 1: 2 bits: 0x */
    { /*7*/ -24, /*1*/ -30 }, /* index 2: 3 bits: 00x */
    { 4, 5 },                 /* index 3: 3 bits: 01x */
    { /*3*/ -28, /*6*/ -25 }, /* index 4: 4 bits: 010x */
    { /*2*/ -29, 6 },         /* index 5: 4 bits: 011x */
    { /*5*/ -26, /*4*/ -27 }  /* index 6: 5 bits: 0111x */
};

static const int8_t t_huff_opd[][2] = {
    { 1, /*0*/ -31 },         /* index 0: 1 bits: x */
    { 2, 3 },                 /* index 1: 2 bits: 0x */
    { 4, 5 },                 /* index 2: 3 bits: 00x */
    { /*1*/ -30, /*7*/ -24 }, /* index 3: 3 bits: 01x */
    { /*5*/ -26, 6 },         /* index 4: 4 bits: 000x */
    { /*2*/ -29, /*6*/ -25 }, /* index 5: 4 bits: 001x */
    { /*4*/ -27, /*3*/ -28 }  /* index 6: 5 bits: 0001x */
};

/* static function declarations */
static uint16_t ps_extension(ps_info *ps, bitfile *ld,
                             const uint8_t ps_extension_id,
                             const uint16_t num_bits_left);
static void huff_data(bitfile *ld, const uint8_t dt, const uint8_t nr_par,
                      ps_huff_tab t_huff, ps_huff_tab f_huff, int8_t *par);
static INLINE int8_t ps_huff_dec(bitfile *ld, ps_huff_tab t_huff);


uint16_t ps_data(ps_info *ps, bitfile *ld, uint8_t *header)
{
    uint8_t tmp, n;
    uint16_t bits = (uint16_t)faad_get_processed_bits(ld);

    *header = 0;

    /* check for new PS header */
    if (faad_get1bit(ld
        DEBUGVAR(1,1000,"ps_data(): enable_ps_header")))
    {
        *header = 1;

        ps->header_read = 1;

        ps->use34hybrid_bands = 0;

        /* Inter-channel Intensity Difference (IID) parameters enabled */
        ps->enable_iid = (uint8_t)faad_get1bit(ld
            DEBUGVAR(1,1001,"ps_data(): enable_iid"));

        if (ps->enable_iid)
        {
            ps->iid_mode = (uint8_t)faad_getbits(ld, 3
                DEBUGVAR(1,1002,"ps_data(): iid_mode"));

            ps->nr_iid_par = nr_iid_par_tab[ps->iid_mode];
            ps->nr_ipdopd_par = nr_ipdopd_par_tab[ps->iid_mode];

            if (ps->iid_mode == 2 || ps->iid_mode == 5)
                ps->use34hybrid_bands = 1;

            /* IPD freq res equal to IID freq res */
            ps->ipd_mode = ps->iid_mode;
        }

        /* Inter-channel Coherence (ICC) parameters enabled */
        ps->enable_icc = (uint8_t)faad_get1bit(ld
            DEBUGVAR(1,1003,"ps_data(): enable_icc"));

        if (ps->enable_icc)
        {
            ps->icc_mode = (uint8_t)faad_getbits(ld, 3
                DEBUGVAR(1,1004,"ps_data(): icc_mode"));

            ps->nr_icc_par = nr_icc_par_tab[ps->icc_mode];

            if (ps->icc_mode == 2 || ps->icc_mode == 5)
                ps->use34hybrid_bands = 1;
        }

        /* PS extension layer enabled */
        ps->enable_ext = (uint8_t)faad_get1bit(ld
            DEBUGVAR(1,1005,"ps_data(): enable_ext"));
    }

    /* we are here, but no header has been read yet */
    if (ps->header_read == 0)
        return 1;

    ps->frame_class = (uint8_t)faad_get1bit(ld
        DEBUGVAR(1,1006,"ps_data(): frame_class"));
    tmp = (uint8_t)faad_getbits(ld, 2
        DEBUGVAR(1,1007,"ps_data(): num_env_idx"));

    ps->num_env = num_env_tab[ps->frame_class][tmp];

    if (ps->frame_class)
    {
        for (n = 1; n < ps->num_env+1; n++)
        {
            ps->border_position[n] = (uint8_t)faad_getbits(ld, 5
                DEBUGVAR(1,1008,"ps_data(): border_position"));
        }
    }

    if (ps->enable_iid)
    {
        for (n = 0; n < ps->num_env; n++)
        {
            ps->iid_dt[n] = (uint8_t)faad_get1bit(ld
                DEBUGVAR(1,1009,"ps_data(): iid_dt"));

            /* iid_data */
            if (ps->iid_mode < 3)
            {
                huff_data(ld, ps->iid_dt[n], ps->nr_iid_par, t_huff_iid_def,
                    f_huff_iid_def, ps->iid_index[n]);
            } else {
                huff_data(ld, ps->iid_dt[n], ps->nr_iid_par, t_huff_iid_fine,
                    f_huff_iid_fine, ps->iid_index[n]);
            }
        }
    }

    if (ps->enable_icc)
    {
        for (n = 0; n < ps->num_env; n++)
        {
            ps->icc_dt[n] = (uint8_t)faad_get1bit(ld
                DEBUGVAR(1,1010,"ps_data(): icc_dt"));

            /* icc_data */
            huff_data(ld, ps->icc_dt[n], ps->nr_icc_par, t_huff_icc,
                f_huff_icc, ps->icc_index[n]);
        }
    }

    if (ps->enable_ext)
    {
        uint16_t num_bits_left;
        uint16_t cnt = (uint16_t)faad_getbits(ld, 4
            DEBUGVAR(1,1011,"ps_data(): ps_extension_size"));
        if (cnt == 15)
        {
            cnt += (uint16_t)faad_getbits(ld, 8
                DEBUGVAR(1,1012,"ps_data(): esc_count"));
        }

        num_bits_left = 8 * cnt;
        while (num_bits_left > 7)
        {
            uint8_t ps_extension_id = (uint8_t)faad_getbits(ld, 2
                DEBUGVAR(1,1013,"ps_data(): ps_extension_size"));

            num_bits_left -= 2;
            num_bits_left -= ps_extension(ps, ld, ps_extension_id, num_bits_left);
        }

        faad_getbits(ld, num_bits_left
            DEBUGVAR(1,1014,"ps_data(): fill_bits"));
    }

    bits = (uint16_t)faad_get_processed_bits(ld) - bits;

    ps->ps_data_available = 1;

    return bits;
}

static uint16_t ps_extension(ps_info *ps, bitfile *ld,
                             const uint8_t ps_extension_id,
                             const uint16_t num_bits_left)
{
    uint8_t n;
    uint16_t bits = (uint16_t)faad_get_processed_bits(ld);

    if (ps_extension_id == 0)
    {
        ps->enable_ipdopd = (uint8_t)faad_get1bit(ld
            DEBUGVAR(1,1015,"ps_extension(): enable_ipdopd"));

        if (ps->enable_ipdopd)
        {
            for (n = 0; n < ps->num_env; n++)
            {
                ps->ipd_dt[n] = (uint8_t)faad_get1bit(ld
                    DEBUGVAR(1,1016,"ps_extension(): ipd_dt"));

                /* ipd_data */
                huff_data(ld, ps->ipd_dt[n], ps->nr_ipdopd_par, t_huff_ipd,
                    f_huff_ipd, ps->ipd_index[n]);

                ps->opd_dt[n] = (uint8_t)faad_get1bit(ld
                    DEBUGVAR(1,1017,"ps_extension(): opd_dt"));

                /* opd_data */
                huff_data(ld, ps->opd_dt[n], ps->nr_ipdopd_par, t_huff_opd,
                    f_huff_opd, ps->opd_index[n]);
            }
        }
        faad_get1bit(ld
            DEBUGVAR(1,1018,"ps_extension(): reserved_ps"));
    }

    /* return number of bits read */
    bits = (uint16_t)faad_get_processed_bits(ld) - bits;

    return bits;
}

/* read huffman data coded in either the frequency or the time direction */
static void huff_data(bitfile *ld, const uint8_t dt, const uint8_t nr_par,
                      ps_huff_tab t_huff, ps_huff_tab f_huff, int8_t *par)
{
    uint8_t n;

    if (dt)
    {
        /* coded in time direction */
        for (n = 0; n < nr_par; n++)
        {
            par[n] = ps_huff_dec(ld, t_huff);
        }
    } else {
        /* coded in frequency direction */
        par[0] = ps_huff_dec(ld, f_huff);

        for (n = 1; n < nr_par; n++)
        {
            par[n] = ps_huff_dec(ld, f_huff);
        }
    }
}

/* binary search huffman decoding */
static INLINE int8_t ps_huff_dec(bitfile *ld, ps_huff_tab t_huff)
{
    uint8_t bit;
    int16_t index = 0;

    while (index >= 0)
    {
        bit = (uint8_t)faad_get1bit(ld);
        index = t_huff[index][bit];
    }

    return index + 31;
}

#endif
