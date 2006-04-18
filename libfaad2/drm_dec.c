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
** $Id$
**/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "common.h"

#ifdef DRM

#include "sbr_dec.h"
#include "drm_dec.h"
#include "bits.h"

/* constants */
#define DECAY_CUTOFF         3
#define DECAY_SLOPE          0.05f

/* type definitaions */
typedef const int8_t (*drm_ps_huff_tab)[2];


/* binary search huffman tables */
static const int8_t f_huffman_sa[][2] = 
{
    { /*0*/ -15, 1 },             /* index 0: 1 bits:  x */
    { 2, 3 },                     /* index 1: 2 bits:  1x */
    { /*7*/ -8, 4 },              /* index 2: 3 bits:  10x */
    { 5, 6 },                     /* index 3: 3 bits:  11x */
    { /*1*/ -14, /*-1*/ -16 },    /* index 4: 4 bits:  101x */
    { /*-2*/ -17, 7 },            /* index 5: 4 bits:  110x */
    { 8, 9 },                     /* index 6: 4 bits:  111x */
    { /*2*/ -13, /*-3*/ -18 },    /* index 7: 5 bits:  1101x */
    { /*3*/ -12, 10 },            /* index 8: 5 bits:  1110x */
    { 11, 12 },                   /* index 9: 5 bits:  1111x */
    { /*4*/ -11, /*5*/ -10 },     /* index 10: 6 bits: 11101x */
    { /*-4*/ -19, /*-5*/ -20 },   /* index 11: 6 bits: 11110x */
    { /*6*/ -9, 13 },             /* index 12: 6 bits: 11111x */
    { /*-7*/ -22, /*-6*/ -21 }    /* index 13: 7 bits: 111111x */
};

static const int8_t t_huffman_sa[][2] = 
{
    { /*0*/ -15, 1 },             /* index 0: 1 bits: x */
    { 2, 3 },                     /* index 1: 2 bits: 1x */
    { /*-1*/ -16, /*1*/ -14 },    /* index 2: 3 bits: 10x */
    { 4, 5 },                     /* index 3: 3 bits: 11x */
    { /*-2*/ -17, /*2*/ -13 },    /* index 4: 4 bits: 110x */
    { 6, 7 },                     /* index 5: 4 bits: 111x */
    { /*-3*/ -18, /*3*/ -12 },    /* index 6: 5 bits: 1110x */
    { 8, 9 },                     /* index 7: 5 bits: 1111x */
    { /*-4*/ -19, /*4*/ -11 },    /* index 8: 6 bits: 11110x */
    { 10, 11 },                   /* index 9: 6 bits: 11111x */
    { /*-5*/ -20, /*5*/ -10 },    /* index 10: 7 bits: 111110x */
    { /*-6*/ -21, 12 },           /* index 11: 7 bits: 111111x */
    { /*-7*/ -22, 13 },           /* index 12: 8 bits: 1111111x */
    { /*6*/ -9, /*7*/ -8 }        /* index 13: 9 bits: 11111111x */
};

static const int8_t f_huffman_pan[][2] = 
{
    { /*0*/ -15, 1 },             /* index 0: 1 bits: x */
    { /*-1*/ -16, 2 },            /* index 1: 2 bits: 1x */
    { /*1*/ -14, 3 },             /* index 2: 3 bits: 11x */
    { 4, 5 },                     /* index 3: 4 bits: 111x */
    { /*-2*/ -17, /*2*/ -13 },    /* index 4: 5 bits: 1110x */
    { 6, 7 },                     /* index 5: 5 bits: 1111x */
    { /*-3*/ -18, /*3*/ -12 },    /* index 6: 6 bits: 11110x */
    { 8, 9 },                     /* index 7: 6 bits: 11111x */
    { /*-4*/ -19, /*4*/ -11 },    /* index 8: 7 bits: 111110x */
    { 10, 11 },                   /* index 9: 7 bits: 111111x */
    { /*-5*/ -20, /*5*/ -10 },    /* index 10: 8 bits: 1111110x */
    { 12, 13 },                   /* index 11: 8 bits: 1111111x */
    { /*-6*/ -21, /*6*/ -9 },     /* index 12: 9 bits: 11111110x */
    { /*-7*/ -22, 14 },           /* index 13: 9 bits: 11111111x */
    { /*7*/ -8, 15 },             /* index 14: 10 bits: 111111111x */
    { 16, 17 },                   /* index 15: 11 bits: 1111111111x */
    { /*-8*/ -23, /*8*/ -7 },     /* index 16: 12 bits: 11111111110x */
    { 18, 19 },                   /* index 17: 12 bits: 11111111111x */
    { /*-10*/ -25, 20 },          /* index 18: 13 bits: 111111111110x */
    { 21, 22 },                   /* index 19: 13 bits: 111111111111x */
    { /*-9*/ -24, /*9*/ -6 },     /* index 20: 14 bits: 1111111111101x */
    { /*10*/ -5, 23 },            /* index 21: 14 bits: 1111111111110x */
    { 24, 25 },                   /* index 22: 14 bits: 1111111111111x */
    { /*-13*/ -28, /*-11*/ -26 }, /* index 23: 15 bits: 11111111111101x */
    { /*11*/ -4, /*13*/ -2 },     /* index 24: 15 bits: 11111111111110x */
    { 26, 27 },                   /* index 25: 15 bits: 11111111111111x */
    { /*-14*/ -29, /*-12*/ -27 }, /* index 26: 16 bits: 111111111111110x */
    { /*12*/ -3, /*14*/ -1 }      /* index 27: 16 bits: 111111111111111x */
};

static const int8_t t_huffman_pan[][2] = 
{
    { /*0*/ -15, 1 },             /* index 0: 1 bits: x */
    { /*-1*/ -16, 2 },            /* index 1: 2 bits: 1x */
    { /*1*/ -14, 3 },             /* index 2: 3 bits: 11x */
    { /*-2*/ -17, 4 },            /* index 3: 4 bits: 111x */
    { /*2*/ -13, 5 },             /* index 4: 5 bits: 1111x */
    { /*-3*/ -18, 6 },            /* index 5: 6 bits: 11111x */
    { /*3*/ -12, 7 },             /* index 6: 7 bits: 111111x */
    { /*-4*/ -19, 8 },            /* index 7: 8 bits: 1111111x */
    { /*4*/ -11, 9 },             /* index 8: 9 bits: 11111111x */
    { 10, 11 },                   /* index 9: 10 bits: 111111111x */
    { /*-5*/ -20, /*5*/ -10 },    /* index 10: 11 bits: 1111111110x */
    { 12, 13 },                   /* index 11: 11 bits: 1111111111x */
    { /*-6*/ -21, /*6*/ -9 },     /* index 12: 12 bits: 11111111110x */
    { 14, 15 },                   /* index 13: 12 bits: 11111111111x */
    { /*-7*/ -22, /*7*/ -8 },     /* index 14: 13 bits: 111111111110x */
    { 16, 17 },                   /* index 15: 13 bits: 111111111111x */
    { /*-8*/ -23, /*8*/ -7 },     /* index 16: 14 bits: 1111111111110x */
    { 18, 19 },                   /* index 17: 14 bits: 1111111111111x */
    { /*-10*/ -25, /*10*/ -5 },   /* index 18: 15 bits: 11111111111110x */
    { 20, 21 },                   /* index 19: 15 bits: 11111111111111x */
    { /*-9*/ -24, /*9*/ -6 },     /* index 20: 16 bits: 111111111111110x */
    { 22, 23 },                   /* index 21: 16 bits: 111111111111111x */
    { 24, 25 },                   /* index 22: 17 bits: 1111111111111110x */
    { 26, 27 },                   /* index 23: 17 bits: 1111111111111111x */
    { /*-14*/ -29, /*-13*/ -28 }, /* index 24: 18 bits: 11111111111111100x */
    { /*-12*/ -27, /*-11*/ -26 }, /* index 25: 18 bits: 11111111111111101x */
    { /*11*/ -4, /*12*/ -3 },     /* index 26: 18 bits: 11111111111111110x */
    { /*13*/ -2, /*14*/ -1 }      /* index 27: 18 bits: 11111111111111111x */
};

/* There are 3 classes in the standard but the last 2 are identical */
static const real_t sa_quant[8][2] = 
{
    { FRAC_CONST(0.0000), FRAC_CONST(0.0000) },
    { FRAC_CONST(0.0501), FRAC_CONST(0.1778) },
    { FRAC_CONST(0.0706), FRAC_CONST(0.2818) },
    { FRAC_CONST(0.0995), FRAC_CONST(0.4467) },
    { FRAC_CONST(0.1399), FRAC_CONST(0.5623) },
    { FRAC_CONST(0.1957), FRAC_CONST(0.7079) },
    { FRAC_CONST(0.2713), FRAC_CONST(0.8913) },
    { FRAC_CONST(0.3699), FRAC_CONST(1.0000) },
};

/* We don't need the actual quantizer values */
#if 0
static const real_t pan_quant[8][5] = 
{
    { COEF_CONST(0.0000), COEF_CONST(0.0000), COEF_CONST(0.0000), COEF_CONST(0.0000), COEF_CONST(0.0000) },
    { COEF_CONST(0.1661), COEF_CONST(0.1661), COEF_CONST(0.3322), COEF_CONST(0.3322), COEF_CONST(0.3322) },
    { COEF_CONST(0.3322), COEF_CONST(0.3322), COEF_CONST(0.6644), COEF_CONST(0.8305), COEF_CONST(0.8305) },
    { COEF_CONST(0.4983), COEF_CONST(0.6644), COEF_CONST(0.9966), COEF_CONST(1.4949), COEF_CONST(1.6610) },
    { COEF_CONST(0.6644), COEF_CONST(0.9966), COEF_CONST(1.4949), COEF_CONST(2.1593), COEF_CONST(2.4914) },
    { COEF_CONST(0.8305), COEF_CONST(1.3288), COEF_CONST(2.1593), COEF_CONST(2.9897), COEF_CONST(3.4880) },
    { COEF_CONST(0.9966), COEF_CONST(1.8271), COEF_CONST(2.8236), COEF_CONST(3.8202), COEF_CONST(4.6507) },
    { COEF_CONST(1.3288), COEF_CONST(2.3253), COEF_CONST(3.4880), COEF_CONST(4.6507), COEF_CONST(5.8134) },
};
#endif

/* 2^(pan_quant[x][y] */
static const real_t pan_pow_2_pos[8][5] = {
    { REAL_CONST(1.0000000), REAL_CONST(1.0000000), REAL_CONST(1.0000000), REAL_CONST(1.0000000), REAL_CONST(1.0000000)  },
    { REAL_CONST(1.1220021), REAL_CONST(1.1220021), REAL_CONST(1.2589312), REAL_CONST(1.2589312), REAL_CONST(1.2589312)  },
    { REAL_CONST(1.2589312), REAL_CONST(1.2589312), REAL_CONST(1.5849090), REAL_CONST(1.7783016), REAL_CONST(1.7783016)  },
    { REAL_CONST(1.4125481), REAL_CONST(1.5849090), REAL_CONST(1.9952921), REAL_CONST(2.8184461), REAL_CONST(3.1623565)  },
    { REAL_CONST(1.5849090), REAL_CONST(1.9952922), REAL_CONST(2.8184461), REAL_CONST(4.4669806), REAL_CONST(5.6232337)  },
    { REAL_CONST(1.7783016), REAL_CONST(2.5119365), REAL_CONST(4.4669806), REAL_CONST(7.9430881), REAL_CONST(11.219994)  },
    { REAL_CONST(1.9952921), REAL_CONST(3.5482312), REAL_CONST(7.0792671), REAL_CONST(14.125206), REAL_CONST(25.118876)  },
    { REAL_CONST(2.5119365), REAL_CONST(5.0116998), REAL_CONST(11.219994), REAL_CONST(25.118876), REAL_CONST(56.235140)  }
};

/* 2^(-pan_quant[x][y] */
static const real_t pan_pow_2_neg[8][5] = {
    { REAL_CONST(1),         REAL_CONST(1),         REAL_CONST(1),         REAL_CONST(1),         REAL_CONST(1)          },
    { REAL_CONST(0.8912487), REAL_CONST(0.8912487), REAL_CONST(0.7943242), REAL_CONST(0.7943242), REAL_CONST(0.7943242)  },
    { REAL_CONST(0.7943242), REAL_CONST(0.7943242), REAL_CONST(0.6309511), REAL_CONST(0.5623344), REAL_CONST(0.5623344)  },
    { REAL_CONST(0.7079405), REAL_CONST(0.6309511), REAL_CONST(0.5011797), REAL_CONST(0.3548054), REAL_CONST(0.3162199)  },
    { REAL_CONST(0.6309511), REAL_CONST(0.5011797), REAL_CONST(0.3548054), REAL_CONST(0.2238649), REAL_CONST(0.1778336)  },
    { REAL_CONST(0.5623343), REAL_CONST(0.3980992), REAL_CONST(0.2238649), REAL_CONST(0.1258956), REAL_CONST(0.0891266)  },
    { REAL_CONST(0.5011797), REAL_CONST(0.2818306), REAL_CONST(0.1412576), REAL_CONST(0.0707954), REAL_CONST(0.0398107)  },
    { REAL_CONST(0.3980992), REAL_CONST(0.1995331), REAL_CONST(0.0891267), REAL_CONST(0.0398107), REAL_CONST(0.0177825)  }
};

/* 2^(pan_quant[x][y]/30) */
static const real_t pan_pow_2_30_pos[8][5] = {
    { COEF_CONST(1),           COEF_CONST(1),           COEF_CONST(1),           COEF_CONST(1),           COEF_CONST(1)           }, 
    { COEF_CONST(1.003845098), COEF_CONST(1.003845098), COEF_CONST(1.007704982), COEF_CONST(1.007704982), COEF_CONST(1.007704982) }, 
    { COEF_CONST(1.007704982), COEF_CONST(1.007704982), COEF_CONST(1.01546933),  COEF_CONST(1.019373909), COEF_CONST(1.019373909) }, 
    { COEF_CONST(1.011579706), COEF_CONST(1.01546933),  COEF_CONST(1.023293502), COEF_CONST(1.035142941), COEF_CONST(1.039123167) }, 
    { COEF_CONST(1.01546933),  COEF_CONST(1.023293502), COEF_CONST(1.035142941), COEF_CONST(1.051155908), COEF_CONST(1.059252598) },
    { COEF_CONST(1.019373909), COEF_CONST(1.03117796),  COEF_CONST(1.051155908), COEF_CONST(1.071518432), COEF_CONST(1.0839263)   }, 
    { COEF_CONST(1.023293502), COEF_CONST(1.043118698), COEF_CONST(1.067414119), COEF_CONST(1.092277933), COEF_CONST(1.113439626) }, 
    { COEF_CONST(1.03117796),  COEF_CONST(1.055195268), COEF_CONST(1.0839263),   COEF_CONST(1.113439626), COEF_CONST(1.143756546) }
};

/* 2^(-pan_quant[x][y]/30) */
static const real_t pan_pow_2_30_neg[8][5] = {
    { COEF_CONST(1),           COEF_CONST(1),           COEF_CONST(1),           COEF_CONST(1),           COEF_CONST(1)           },
    { COEF_CONST(0.99616963),  COEF_CONST(0.99616963),  COEF_CONST(0.992353931), COEF_CONST(0.992353931), COEF_CONST(0.99235393)  }, 
    { COEF_CONST(0.992353931), COEF_CONST(0.992353931), COEF_CONST(0.984766325), COEF_CONST(0.980994305), COEF_CONST(0.980994305) }, 
    { COEF_CONST(0.988552848), COEF_CONST(0.984766325), COEF_CONST(0.977236734), COEF_CONST(0.966050157), COEF_CONST(0.962349827) }, 
    { COEF_CONST(0.984766325), COEF_CONST(0.977236734), COEF_CONST(0.966050157), COEF_CONST(0.951333663), COEF_CONST(0.944061881) }, 
    { COEF_CONST(0.980994305), COEF_CONST(0.969764715), COEF_CONST(0.951333663), COEF_CONST(0.933255062), COEF_CONST(0.922571949) }, 
    { COEF_CONST(0.977236734), COEF_CONST(0.958663671), COEF_CONST(0.936843519), COEF_CONST(0.915517901), COEF_CONST(0.898117847) }, 
    { COEF_CONST(0.969764715), COEF_CONST(0.947691892), COEF_CONST(0.922571949), COEF_CONST(0.898117847), COEF_CONST(0.874311936) }
};

static const real_t g_decayslope[MAX_SA_BAND] = {
    FRAC_CONST(1),   FRAC_CONST(1),   FRAC_CONST(1),   FRAC_CONST(0.95),FRAC_CONST(0.9), FRAC_CONST(0.85), FRAC_CONST(0.8), 
    FRAC_CONST(0.75),FRAC_CONST(0.7), FRAC_CONST(0.65),FRAC_CONST(0.6), FRAC_CONST(0.55),FRAC_CONST(0.5),  FRAC_CONST(0.45), 
    FRAC_CONST(0.4), FRAC_CONST(0.35),FRAC_CONST(0.3), FRAC_CONST(0.25),FRAC_CONST(0.2), FRAC_CONST(0.15), FRAC_CONST(0.1),
    FRAC_CONST(0.05),FRAC_CONST(0),   FRAC_CONST(0),   FRAC_CONST(0),   FRAC_CONST(0),   FRAC_CONST(0),    FRAC_CONST(0), 
    FRAC_CONST(0),   FRAC_CONST(0),   FRAC_CONST(0),   FRAC_CONST(0),   FRAC_CONST(0),   FRAC_CONST(0),    FRAC_CONST(0),  
    FRAC_CONST(0),   FRAC_CONST(0),   FRAC_CONST(0),   FRAC_CONST(0),   FRAC_CONST(0),   FRAC_CONST(0),    FRAC_CONST(0),   
    FRAC_CONST(0),   FRAC_CONST(0),   FRAC_CONST(0)
};

static const real_t sa_sqrt_1_minus[8][2] = {
    { FRAC_CONST(1),            FRAC_CONST(1)           },
    { FRAC_CONST(0.998744206),  FRAC_CONST(0.984066644) },
    { FRAC_CONST(0.997504707),  FRAC_CONST(0.959473168) },
    { FRAC_CONST(0.995037562),  FRAC_CONST(0.894683804) },
    { FRAC_CONST(0.990165638),  FRAC_CONST(0.826933317) },
    { FRAC_CONST(0.980663811),  FRAC_CONST(0.706312672) },
    { FRAC_CONST(0.962494836),  FRAC_CONST(0.45341406)  },
    { FRAC_CONST(0.929071574),  FRAC_CONST(0)           }
};

static const uint8_t sa_freq_scale[9][2] = 
{
    { 0, 0},  
    { 1, 1},  
    { 2, 2},  
    { 3, 3},  
    { 5, 5},  
    { 7, 7},  
    {10,10},  
    {13,13},  
    {46,23}
};

static const uint8_t pan_freq_scale[21] = 
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 18, 22, 26, 32, 64
};

static const uint8_t pan_quant_class[20] = 
{
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 3, 3, 3, 4, 4, 4
};

/* Inverse mapping lookup */
static const uint8_t pan_inv_freq[64] = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 
    15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19
};

static const uint8_t sa_inv_freq[MAX_SA_BAND] = {
    0, 1, 2, 3, 3, 4, 4, 5, 5, 5, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
    7, 7, 7, 7, 7, 7, 7
};

static const real_t filter_coeff[] = 
{
    FRAC_CONST(0.65143905754106),
    FRAC_CONST(0.56471812200776),
    FRAC_CONST(0.48954165955695)
};

static const uint8_t delay_length[][2] = 
{
    { 1, 3 }, { 2, 4 }, { 3, 5 }
};

static const real_t delay_fraction[] = 
{
    FRAC_CONST(0.43), FRAC_CONST(0.75), FRAC_CONST(0.347)
};

static const real_t peak_decay[2] = 
{
    FRAC_CONST(0.58664621951003), FRAC_CONST(0.76592833836465)
};

static const real_t smooth_coeff[2] = 
{
    FRAC_CONST(0.6), FRAC_CONST(0.25)
};

/* Please note that these are the same tables as in plain PS */
static const complex_t Q_Fract_allpass_Qmf[][3] = {
    { { FRAC_CONST(0.7804303765), FRAC_CONST(0.6252426505) }, { FRAC_CONST(0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(0.8550928831), FRAC_CONST(0.5184748173) } },
    { { FRAC_CONST(-0.4399392009), FRAC_CONST(0.8980275393) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(-0.0643581524), FRAC_CONST(0.9979268909) } },
    { { FRAC_CONST(-0.9723699093), FRAC_CONST(-0.2334454209) }, { FRAC_CONST(0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(-0.9146071672), FRAC_CONST(0.4043435752) } },
    { { FRAC_CONST(0.0157073960), FRAC_CONST(-0.9998766184) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(-0.7814115286), FRAC_CONST(-0.6240159869) } },
    { { FRAC_CONST(0.9792228341), FRAC_CONST(-0.2027871907) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(0.1920081824), FRAC_CONST(-0.9813933372) } },
    { { FRAC_CONST(0.4115142524), FRAC_CONST(0.9114032984) }, { FRAC_CONST(0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(0.9589683414), FRAC_CONST(-0.2835132182) } },
    { { FRAC_CONST(-0.7996847630), FRAC_CONST(0.6004201174) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(0.6947838664), FRAC_CONST(0.7192186117) } },
    { { FRAC_CONST(-0.7604058385), FRAC_CONST(-0.6494481564) }, { FRAC_CONST(0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(-0.3164770305), FRAC_CONST(0.9486001730) } },
    { { FRAC_CONST(0.4679299891), FRAC_CONST(-0.8837655187) }, { FRAC_CONST(0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(-0.9874414206), FRAC_CONST(0.1579856575) } },
    { { FRAC_CONST(0.9645573497), FRAC_CONST(0.2638732493) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(-0.5966450572), FRAC_CONST(-0.8025052547) } },
    { { FRAC_CONST(-0.0471066870), FRAC_CONST(0.9988898635) }, { FRAC_CONST(0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(0.4357025325), FRAC_CONST(-0.9000906944) } },
    { { FRAC_CONST(-0.9851093888), FRAC_CONST(0.1719288528) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(0.9995546937), FRAC_CONST(-0.0298405960) } },
    { { FRAC_CONST(-0.3826831877), FRAC_CONST(-0.9238796234) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(0.4886211455), FRAC_CONST(0.8724960685) } },
    { { FRAC_CONST(0.8181498647), FRAC_CONST(-0.5750049949) }, { FRAC_CONST(0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(-0.5477093458), FRAC_CONST(0.8366686702) } },
    { { FRAC_CONST(0.7396308780), FRAC_CONST(0.6730127335) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(-0.9951074123), FRAC_CONST(-0.0987988561) } },
    { { FRAC_CONST(-0.4954589605), FRAC_CONST(0.8686313629) }, { FRAC_CONST(0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(-0.3725017905), FRAC_CONST(-0.9280315042) } },
    { { FRAC_CONST(-0.9557929039), FRAC_CONST(-0.2940406799) }, { FRAC_CONST(0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(0.6506417990), FRAC_CONST(-0.7593847513) } },
    { { FRAC_CONST(0.0784594864), FRAC_CONST(-0.9969173074) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(0.9741733670), FRAC_CONST(0.2258014232) } },
    { { FRAC_CONST(0.9900237322), FRAC_CONST(-0.1409008205) }, { FRAC_CONST(0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(0.2502108514), FRAC_CONST(0.9681913853) } },
    { { FRAC_CONST(0.3534744382), FRAC_CONST(0.9354441762) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(-0.7427945137), FRAC_CONST(0.6695194840) } },
    { { FRAC_CONST(-0.8358076215), FRAC_CONST(0.5490224361) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(-0.9370992780), FRAC_CONST(-0.3490629196) } },
    { { FRAC_CONST(-0.7181259394), FRAC_CONST(-0.6959131360) }, { FRAC_CONST(0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(-0.1237744763), FRAC_CONST(-0.9923103452) } },
    { { FRAC_CONST(0.5224990249), FRAC_CONST(-0.8526399136) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(0.8226406574), FRAC_CONST(-0.5685616732) } },
    { { FRAC_CONST(0.9460852146), FRAC_CONST(0.3239179254) }, { FRAC_CONST(0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(0.8844994903), FRAC_CONST(0.4665412009) } },
    { { FRAC_CONST(-0.1097348556), FRAC_CONST(0.9939609170) }, { FRAC_CONST(0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(-0.0047125919), FRAC_CONST(0.9999889135) } },
    { { FRAC_CONST(-0.9939610362), FRAC_CONST(0.1097337380) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(-0.8888573647), FRAC_CONST(0.4581840038) } },
    { { FRAC_CONST(-0.3239168525), FRAC_CONST(-0.9460855722) }, { FRAC_CONST(0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(-0.8172453642), FRAC_CONST(-0.5762898922) } },
    { { FRAC_CONST(0.8526405096), FRAC_CONST(-0.5224980116) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(0.1331215799), FRAC_CONST(-0.9910997152) } },
    { { FRAC_CONST(0.6959123611), FRAC_CONST(0.7181267142) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(0.9403476119), FRAC_CONST(-0.3402152061) } },
    { { FRAC_CONST(-0.5490233898), FRAC_CONST(0.8358070254) }, { FRAC_CONST(0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(0.7364512086), FRAC_CONST(0.6764906645) } },
    { { FRAC_CONST(-0.9354437590), FRAC_CONST(-0.3534754813) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(-0.2593250275), FRAC_CONST(0.9657900929) } },
    { { FRAC_CONST(0.1409019381), FRAC_CONST(-0.9900235534) }, { FRAC_CONST(0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(-0.9762582779), FRAC_CONST(0.2166097313) } },
    { { FRAC_CONST(0.9969173670), FRAC_CONST(-0.0784583688) }, { FRAC_CONST(0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(-0.6434556246), FRAC_CONST(-0.7654833794) } },
    { { FRAC_CONST(0.2940396070), FRAC_CONST(0.9557932615) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(0.3812320232), FRAC_CONST(-0.9244794250) } },
    { { FRAC_CONST(-0.8686318994), FRAC_CONST(0.4954580069) }, { FRAC_CONST(0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(0.9959943891), FRAC_CONST(-0.0894154981) } },
    { { FRAC_CONST(-0.6730118990), FRAC_CONST(-0.7396316528) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(0.5397993922), FRAC_CONST(0.8417937160) } },
    { { FRAC_CONST(0.5750059485), FRAC_CONST(-0.8181492686) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(-0.4968227744), FRAC_CONST(0.8678520322) } },
    { { FRAC_CONST(0.9238792062), FRAC_CONST(0.3826842010) }, { FRAC_CONST(0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(-0.9992290139), FRAC_CONST(-0.0392601527) } },
    { { FRAC_CONST(-0.1719299555), FRAC_CONST(0.9851091504) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(-0.4271997511), FRAC_CONST(-0.9041572809) } },
    { { FRAC_CONST(-0.9988899231), FRAC_CONST(0.0471055657) }, { FRAC_CONST(0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(0.6041822433), FRAC_CONST(-0.7968461514) } },
    { { FRAC_CONST(-0.2638721764), FRAC_CONST(-0.9645576477) }, { FRAC_CONST(0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(0.9859085083), FRAC_CONST(0.1672853529) } },
    { { FRAC_CONST(0.8837660551), FRAC_CONST(-0.4679289758) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(0.3075223565), FRAC_CONST(0.9515408874) } },
    { { FRAC_CONST(0.6494473219), FRAC_CONST(0.7604066133) }, { FRAC_CONST(0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(-0.7015317082), FRAC_CONST(0.7126382589) } },
    { { FRAC_CONST(-0.6004210114), FRAC_CONST(0.7996840477) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(-0.9562535882), FRAC_CONST(-0.2925389707) } },
    { { FRAC_CONST(-0.9114028811), FRAC_CONST(-0.4115152657) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(-0.1827499419), FRAC_CONST(-0.9831594229) } },
    { { FRAC_CONST(0.2027882934), FRAC_CONST(-0.9792225957) }, { FRAC_CONST(0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(0.7872582674), FRAC_CONST(-0.6166234016) } },
    { { FRAC_CONST(0.9998766780), FRAC_CONST(-0.0157062728) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(0.9107555747), FRAC_CONST(0.4129458666) } },
    { { FRAC_CONST(0.2334443331), FRAC_CONST(0.9723701477) }, { FRAC_CONST(0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(0.0549497530), FRAC_CONST(0.9984891415) } },
    { { FRAC_CONST(-0.8980280757), FRAC_CONST(0.4399381876) }, { FRAC_CONST(0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(-0.8599416018), FRAC_CONST(0.5103924870) } },
    { { FRAC_CONST(-0.6252418160), FRAC_CONST(-0.7804310918) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(-0.8501682281), FRAC_CONST(-0.5265110731) } },
    { { FRAC_CONST(0.6252435446), FRAC_CONST(-0.7804297209) }, { FRAC_CONST(0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(0.0737608299), FRAC_CONST(-0.9972759485) } },
    { { FRAC_CONST(0.8980270624), FRAC_CONST(0.4399402142) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(0.9183775187), FRAC_CONST(-0.3957053721) } },
    { { FRAC_CONST(-0.2334465086), FRAC_CONST(0.9723696709) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(0.7754954696), FRAC_CONST(0.6313531399) } },
    { { FRAC_CONST(-0.9998766184), FRAC_CONST(-0.0157085191) }, { FRAC_CONST(0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(-0.2012493610), FRAC_CONST(0.9795400500) } },
    { { FRAC_CONST(-0.2027861029), FRAC_CONST(-0.9792230725) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(-0.9615978599), FRAC_CONST(0.2744622827) } },
    { { FRAC_CONST(0.9114037752), FRAC_CONST(-0.4115132093) }, { FRAC_CONST(0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(-0.6879743338), FRAC_CONST(-0.7257350087) } },
    { { FRAC_CONST(0.6004192233), FRAC_CONST(0.7996854186) }, { FRAC_CONST(0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(0.3254036009), FRAC_CONST(-0.9455752373) } },
    { { FRAC_CONST(-0.6494490504), FRAC_CONST(0.7604051232) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(0.9888865948), FRAC_CONST(-0.1486719251) } },
    { { FRAC_CONST(-0.8837650418), FRAC_CONST(-0.4679309726) }, { FRAC_CONST(0.9238795042), FRAC_CONST(-0.3826834261) }, { FRAC_CONST(0.5890548825), FRAC_CONST(0.8080930114) } },
    { { FRAC_CONST(0.2638743520), FRAC_CONST(-0.9645570517) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(0.9238795042) }, { FRAC_CONST(-0.4441666007), FRAC_CONST(0.8959442377) } },
    { { FRAC_CONST(0.9988898039), FRAC_CONST(0.0471078083) }, { FRAC_CONST(-0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(-0.9997915030), FRAC_CONST(0.0204183888) } },
    { { FRAC_CONST(0.1719277352), FRAC_CONST(0.9851095676) }, { FRAC_CONST(0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(-0.4803760946), FRAC_CONST(-0.8770626187) } },
    { { FRAC_CONST(-0.9238800406), FRAC_CONST(0.3826821446) }, { FRAC_CONST(-0.9238795042), FRAC_CONST(0.3826834261) }, { FRAC_CONST(0.5555707216), FRAC_CONST(-0.8314692974) } },
    { { FRAC_CONST(-0.5750041008), FRAC_CONST(-0.8181505203) }, { FRAC_CONST(0.3826834261), FRAC_CONST(-0.9238795042) }, { FRAC_CONST(0.9941320419), FRAC_CONST(0.1081734300) } }
};

static const complex_t Phi_Fract_Qmf[] = {
    { FRAC_CONST(0.8181497455), FRAC_CONST(0.5750052333) },
    { FRAC_CONST(-0.2638730407), FRAC_CONST(0.9645574093) },
    { FRAC_CONST(-0.9969173074), FRAC_CONST(0.0784590989) },
    { FRAC_CONST(-0.4115143716), FRAC_CONST(-0.9114032984) },
    { FRAC_CONST(0.7181262970), FRAC_CONST(-0.6959127784) },
    { FRAC_CONST(0.8980275989), FRAC_CONST(0.4399391711) },
    { FRAC_CONST(-0.1097343117), FRAC_CONST(0.9939609766) },
    { FRAC_CONST(-0.9723699093), FRAC_CONST(0.2334453613) },
    { FRAC_CONST(-0.5490227938), FRAC_CONST(-0.8358073831) },
    { FRAC_CONST(0.6004202366), FRAC_CONST(-0.7996846437) },
    { FRAC_CONST(0.9557930231), FRAC_CONST(0.2940403223) },
    { FRAC_CONST(0.0471064523), FRAC_CONST(0.9988898635) },
    { FRAC_CONST(-0.9238795042), FRAC_CONST(0.3826834261) },
    { FRAC_CONST(-0.6730124950), FRAC_CONST(-0.7396311164) },
    { FRAC_CONST(0.4679298103), FRAC_CONST(-0.8837656379) },
    { FRAC_CONST(0.9900236726), FRAC_CONST(0.1409012377) },
    { FRAC_CONST(0.2027872950), FRAC_CONST(0.9792228341) },
    { FRAC_CONST(-0.8526401520), FRAC_CONST(0.5224985480) },
    { FRAC_CONST(-0.7804304361), FRAC_CONST(-0.6252426505) },
    { FRAC_CONST(0.3239174187), FRAC_CONST(-0.9460853338) },
    { FRAC_CONST(0.9998766184), FRAC_CONST(-0.0157073177) },
    { FRAC_CONST(0.3534748554), FRAC_CONST(0.9354440570) },
    { FRAC_CONST(-0.7604059577), FRAC_CONST(0.6494480371) },
    { FRAC_CONST(-0.8686315417), FRAC_CONST(-0.4954586625) },
    { FRAC_CONST(0.1719291061), FRAC_CONST(-0.9851093292) },
    { FRAC_CONST(0.9851093292), FRAC_CONST(-0.1719291061) },
    { FRAC_CONST(0.4954586625), FRAC_CONST(0.8686315417) },
    { FRAC_CONST(-0.6494480371), FRAC_CONST(0.7604059577) },
    { FRAC_CONST(-0.9354440570), FRAC_CONST(-0.3534748554) },
    { FRAC_CONST(0.0157073177), FRAC_CONST(-0.9998766184) },
    { FRAC_CONST(0.9460853338), FRAC_CONST(-0.3239174187) },
    { FRAC_CONST(0.6252426505), FRAC_CONST(0.7804304361) },
    { FRAC_CONST(-0.5224985480), FRAC_CONST(0.8526401520) },
    { FRAC_CONST(-0.9792228341), FRAC_CONST(-0.2027872950) },
    { FRAC_CONST(-0.1409012377), FRAC_CONST(-0.9900236726) },
    { FRAC_CONST(0.8837656379), FRAC_CONST(-0.4679298103) },
    { FRAC_CONST(0.7396311164), FRAC_CONST(0.6730124950) },
    { FRAC_CONST(-0.3826834261), FRAC_CONST(0.9238795042) },
    { FRAC_CONST(-0.9988898635), FRAC_CONST(-0.0471064523) },
    { FRAC_CONST(-0.2940403223), FRAC_CONST(-0.9557930231) },
    { FRAC_CONST(0.7996846437), FRAC_CONST(-0.6004202366) },
    { FRAC_CONST(0.8358073831), FRAC_CONST(0.5490227938) },
    { FRAC_CONST(-0.2334453613), FRAC_CONST(0.9723699093) },
    { FRAC_CONST(-0.9939609766), FRAC_CONST(0.1097343117) },
    { FRAC_CONST(-0.4399391711), FRAC_CONST(-0.8980275989) },
    { FRAC_CONST(0.6959127784), FRAC_CONST(-0.7181262970) },
    { FRAC_CONST(0.9114032984), FRAC_CONST(0.4115143716) },
    { FRAC_CONST(-0.0784590989), FRAC_CONST(0.9969173074) },
    { FRAC_CONST(-0.9645574093), FRAC_CONST(0.2638730407) },
    { FRAC_CONST(-0.5750052333), FRAC_CONST(-0.8181497455) },
    { FRAC_CONST(0.5750052333), FRAC_CONST(-0.8181497455) },
    { FRAC_CONST(0.9645574093), FRAC_CONST(0.2638730407) },
    { FRAC_CONST(0.0784590989), FRAC_CONST(0.9969173074) },
    { FRAC_CONST(-0.9114032984), FRAC_CONST(0.4115143716) },
    { FRAC_CONST(-0.6959127784), FRAC_CONST(-0.7181262970) },
    { FRAC_CONST(0.4399391711), FRAC_CONST(-0.8980275989) },
    { FRAC_CONST(0.9939609766), FRAC_CONST(0.1097343117) },
    { FRAC_CONST(0.2334453613), FRAC_CONST(0.9723699093) },
    { FRAC_CONST(-0.8358073831), FRAC_CONST(0.5490227938) },
    { FRAC_CONST(-0.7996846437), FRAC_CONST(-0.6004202366) },
    { FRAC_CONST(0.2940403223), FRAC_CONST(-0.9557930231) },
    { FRAC_CONST(0.9988898635), FRAC_CONST(-0.0471064523) },
    { FRAC_CONST(0.3826834261), FRAC_CONST(0.9238795042) },
    { FRAC_CONST(-0.7396311164), FRAC_CONST(0.6730124950) }
};


/* static function declarations */
static void drm_ps_sa_element(drm_ps_info *ps, bitfile *ld);
static void drm_ps_pan_element(drm_ps_info *ps, bitfile *ld);
static int8_t huff_dec(bitfile *ld, drm_ps_huff_tab huff);


uint16_t drm_ps_data(drm_ps_info *ps, bitfile *ld)
{
    uint16_t bits = (uint16_t)faad_get_processed_bits(ld);

    ps->drm_ps_data_available = 1;

    ps->bs_enable_sa = faad_get1bit(ld);
    ps->bs_enable_pan = faad_get1bit(ld);

    if (ps->bs_enable_sa)
    {
        drm_ps_sa_element(ps, ld);
    }

    if (ps->bs_enable_pan)
    {
        drm_ps_pan_element(ps, ld);
    }

    bits = (uint16_t)faad_get_processed_bits(ld) - bits;

    return bits;
}

static void drm_ps_sa_element(drm_ps_info *ps, bitfile *ld)
{
    drm_ps_huff_tab huff;
    uint8_t band;

    ps->bs_sa_dt_flag = faad_get1bit(ld);
    if (ps->bs_sa_dt_flag)
    {
        huff = t_huffman_sa;
    } else {
        huff = f_huffman_sa;
    }

    for (band = 0; band < DRM_NUM_SA_BANDS; band++)
    {
        ps->bs_sa_data[band] = huff_dec(ld, huff);
    }
}

static void drm_ps_pan_element(drm_ps_info *ps, bitfile *ld)
{
    drm_ps_huff_tab huff;
    uint8_t band;

    ps->bs_pan_dt_flag = faad_get1bit(ld);
    if (ps->bs_pan_dt_flag)
    {
        huff = t_huffman_pan;
    } else {
        huff = f_huffman_pan;
    }

    for (band = 0; band < DRM_NUM_PAN_BANDS; band++)
    {
        ps->bs_pan_data[band] = huff_dec(ld, huff);
    }
}

/* binary search huffman decoding */
static int8_t huff_dec(bitfile *ld, drm_ps_huff_tab huff)
{
    uint8_t bit;
    int16_t index = 0;

    while (index >= 0)
    {
        bit = (uint8_t)faad_get1bit(ld);
        index = huff[index][bit];
    }

    return index + 15;
}


static int8_t sa_delta_clip(drm_ps_info *ps, int8_t i)
{
    if (i < 0) {
      /*  printf(" SAminclip %d", i); */
        ps->sa_decode_error = 1;
        return 0;
    } else if (i > 7) {
     /*   printf(" SAmaxclip %d", i); */
        ps->sa_decode_error = 1;
        return 7;
    } else
        return i;
}

static int8_t pan_delta_clip(drm_ps_info *ps, int8_t i)
{   
    if (i < -7) {
        /* printf(" PANminclip %d", i); */
        ps->pan_decode_error = 1;
        return -7;
    } else if (i > 7) {
       /* printf(" PANmaxclip %d", i);  */
        ps->pan_decode_error = 1;
        return 7;
    } else
        return i;
}

static void drm_ps_delta_decode(drm_ps_info *ps) 
{
    uint8_t band;    

    if (ps->bs_enable_sa) 
    {    
        if (ps->bs_sa_dt_flag && !ps->g_last_had_sa) 
        {        
            for (band = 0; band < DRM_NUM_SA_BANDS; band++)
            {   
                ps->g_prev_sa_index[band] = 0;
            }           
        }       
        if (ps->bs_sa_dt_flag)
        {
            ps->g_sa_index[0] = sa_delta_clip(ps, ps->g_prev_sa_index[0]+ps->bs_sa_data[0]);            

        } else {
            ps->g_sa_index[0] = sa_delta_clip(ps,ps->bs_sa_data[0]);          
        }
        
        for (band = 1; band < DRM_NUM_SA_BANDS; band++)
        {   
            if (ps->bs_sa_dt_flag)
            {
                ps->g_sa_index[band] = sa_delta_clip(ps, ps->g_prev_sa_index[band] + ps->bs_sa_data[band]);
            } else {
                ps->g_sa_index[band] = sa_delta_clip(ps, ps->g_sa_index[band-1] + ps->bs_sa_data[band]);                
            }
        }
    }

    /* An error during SA decoding implies PAN data will be undecodable, too */
    /* Also, we don't like on/off switching in PS, so we force to last settings */
    if (ps->sa_decode_error) {
        ps->pan_decode_error = 1;
        ps->bs_enable_pan = ps->g_last_had_pan;
        ps->bs_enable_sa = ps->g_last_had_sa;
    }
    
       
    if (ps->bs_enable_sa) 
    {    
        if (ps->sa_decode_error) {
            for (band = 0; band < DRM_NUM_SA_BANDS; band++)
            {   
                ps->g_sa_index[band] = ps->g_last_good_sa_index[band];
            }
        } else {
            for (band = 0; band < DRM_NUM_SA_BANDS; band++)
            {   
                ps->g_last_good_sa_index[band] = ps->g_sa_index[band];
            }
        }
    }
    
    if (ps->bs_enable_pan) 
    {
        if (ps->bs_pan_dt_flag && !ps->g_last_had_pan) 
        {
/* The DRM PS spec doesn't say anything about this case. (deltacoded in time without a previous frame)
   AAC PS spec you must tread previous frame as 0, so that's what we try. 
*/
            for (band = 0; band < DRM_NUM_PAN_BANDS; band++)
            {   
                ps->g_prev_pan_index[band] = 0;
            }
        } 

        if (ps->bs_pan_dt_flag)
        {   
             ps->g_pan_index[0] = pan_delta_clip(ps,  ps->g_prev_pan_index[0]+ps->bs_pan_data[0]);
        } else {
             ps->g_pan_index[0] = pan_delta_clip(ps, ps->bs_pan_data[0]);
        }
    
        for (band = 1; band < DRM_NUM_PAN_BANDS; band++)
        {   
            if (ps->bs_pan_dt_flag)
            {
                ps->g_pan_index[band] = pan_delta_clip(ps, ps->g_prev_pan_index[band] + ps->bs_pan_data[band]);
            } else {
                ps->g_pan_index[band] = pan_delta_clip(ps, ps->g_pan_index[band-1] + ps->bs_pan_data[band]);
            }
        }
 
        if (ps->pan_decode_error) {
            for (band = 0; band < DRM_NUM_PAN_BANDS; band++)
            {   
                ps->g_pan_index[band] = ps->g_last_good_pan_index[band];
            }
        } else {
            for (band = 0; band < DRM_NUM_PAN_BANDS; band++)
            {   
                ps->g_last_good_pan_index[band] = ps->g_pan_index[band];
            }
        }
    }
}

static void drm_calc_sa_side_signal(drm_ps_info *ps, qmf_t X[38][64], uint8_t rateselect) 
{      
    uint8_t s, b, k;
    complex_t qfrac, tmp0, tmp, in, R0;
    real_t peakdiff;
    real_t nrg;
    real_t power;
    real_t transratio;
    real_t new_delay_slopes[NUM_OF_LINKS];
    uint8_t temp_delay_ser[NUM_OF_LINKS];
    complex_t Phi_Fract;
#ifdef FIXED_POINT
    uint32_t in_re, in_im;
#endif

    for (b = 0; b < sa_freq_scale[DRM_NUM_SA_BANDS][rateselect]; b++)
    {
        /* set delay indices */    
        for (k = 0; k < NUM_OF_LINKS; k++)
            temp_delay_ser[k] = ps->delay_buf_index_ser[k];

        RE(Phi_Fract) = RE(Phi_Fract_Qmf[b]);
        IM(Phi_Fract) = IM(Phi_Fract_Qmf[b]);

        for (s = 0; s < NUM_OF_SUBSAMPLES; s++)
        {            
            const real_t gamma = REAL_CONST(1.5);
            const real_t sigma = REAL_CONST(1.5625);

            RE(in) = QMF_RE(X[s][b]);
            IM(in) = QMF_IM(X[s][b]);

#ifdef FIXED_POINT
            /* NOTE: all input is scaled by 2^(-5) because of fixed point QMF
            * meaning that P will be scaled by 2^(-10) compared to floating point version
            */
            in_re = ((abs(RE(in))+(1<<(REAL_BITS-1)))>>REAL_BITS);
            in_im = ((abs(IM(in))+(1<<(REAL_BITS-1)))>>REAL_BITS);
            power = in_re*in_re + in_im*in_im;
#else
            power = MUL_R(RE(in),RE(in)) + MUL_R(IM(in),IM(in));
#endif

            ps->peakdecay_fast[b] = MUL_F(ps->peakdecay_fast[b], peak_decay[rateselect]);
            if (ps->peakdecay_fast[b] < power)
                ps->peakdecay_fast[b] = power;

            peakdiff = ps->prev_peakdiff[b];
            peakdiff += MUL_F((ps->peakdecay_fast[b] - power - ps->prev_peakdiff[b]), smooth_coeff[rateselect]);
            ps->prev_peakdiff[b] = peakdiff;

            nrg = ps->prev_nrg[b];
            nrg += MUL_F((power - ps->prev_nrg[b]), smooth_coeff[rateselect]);
            ps->prev_nrg[b] = nrg;

            if (MUL_R(peakdiff, gamma) <= nrg) {
                transratio = sigma;
            } else {
                transratio = MUL_R(DIV_R(nrg, MUL_R(peakdiff, gamma)), sigma);
            }
            
            for (k = 0; k < NUM_OF_LINKS; k++) 
            {
                new_delay_slopes[k] = MUL_F(g_decayslope[b], filter_coeff[k]);
            }

            RE(tmp0) = RE(ps->d_buff[0][b]);
            IM(tmp0) = IM(ps->d_buff[0][b]);

            RE(ps->d_buff[0][b]) = RE(ps->d_buff[1][b]);
            IM(ps->d_buff[0][b]) = IM(ps->d_buff[1][b]);

            RE(ps->d_buff[1][b]) = RE(in);
            IM(ps->d_buff[1][b]) = IM(in);               

            ComplexMult(&RE(tmp), &IM(tmp), RE(tmp0), IM(tmp0), RE(Phi_Fract), IM(Phi_Fract));

            RE(R0) = RE(tmp);
            IM(R0) = IM(tmp);

            for (k = 0; k < NUM_OF_LINKS; k++) 
            {
                RE(qfrac) = RE(Q_Fract_allpass_Qmf[b][k]);
                IM(qfrac) = IM(Q_Fract_allpass_Qmf[b][k]);

                RE(tmp0) = RE(ps->d2_buff[k][temp_delay_ser[k]][b]);
                IM(tmp0) = IM(ps->d2_buff[k][temp_delay_ser[k]][b]);

                ComplexMult(&RE(tmp), &IM(tmp), RE(tmp0), IM(tmp0), RE(qfrac), IM(qfrac));

                RE(tmp) += -MUL_F(new_delay_slopes[k], RE(R0));
                IM(tmp) += -MUL_F(new_delay_slopes[k], IM(R0));

                RE(ps->d2_buff[k][temp_delay_ser[k]][b]) = RE(R0) + MUL_F(new_delay_slopes[k], RE(tmp));
                IM(ps->d2_buff[k][temp_delay_ser[k]][b]) = IM(R0) + MUL_F(new_delay_slopes[k], IM(tmp));

                RE(R0) = RE(tmp);
                IM(R0) = IM(tmp);
            }

            QMF_RE(ps->SA[s][b]) = MUL_R(RE(R0), transratio);
            QMF_IM(ps->SA[s][b]) = MUL_R(IM(R0), transratio);

            for (k = 0; k < NUM_OF_LINKS; k++)
            {
                if (++temp_delay_ser[k] >= delay_length[k][rateselect])
                    temp_delay_ser[k] = 0;
            }
        }       
    }

    for (k = 0; k < NUM_OF_LINKS; k++)
        ps->delay_buf_index_ser[k] = temp_delay_ser[k];
}

static void drm_add_ambiance(drm_ps_info *ps, uint8_t rateselect, qmf_t X_left[38][64], qmf_t X_right[38][64]) 
{
    uint8_t s, b, ifreq, qclass;    
    real_t sa_map[MAX_SA_BAND], sa_dir_map[MAX_SA_BAND], k_sa_map[MAX_SA_BAND], k_sa_dir_map[MAX_SA_BAND];
    real_t new_dir_map, new_sa_map;
    
    if (ps->bs_enable_sa)
    {
        /* Instead of dequantization and mapping, we use an inverse mapping
           to look up all the values we need */
        for (b = 0; b < sa_freq_scale[DRM_NUM_SA_BANDS][rateselect]; b++)
        {
            const real_t inv_f_num_of_subsamples = FRAC_CONST(0.03333333333);

            ifreq = sa_inv_freq[b];
            qclass = (b != 0);

            sa_map[b]  = sa_quant[ps->g_prev_sa_index[ifreq]][qclass];
            new_sa_map = sa_quant[ps->g_sa_index[ifreq]][qclass];

            k_sa_map[b] = MUL_F(inv_f_num_of_subsamples, (new_sa_map - sa_map[b]));    
            
            sa_dir_map[b] = sa_sqrt_1_minus[ps->g_prev_sa_index[ifreq]][qclass];                        
            new_dir_map   = sa_sqrt_1_minus[ps->g_sa_index[ifreq]][qclass];
                                                   
            k_sa_dir_map[b] = MUL_F(inv_f_num_of_subsamples, (new_dir_map - sa_dir_map[b]));

        }

        for (s = 0; s < NUM_OF_SUBSAMPLES; s++)
        {
            for (b = 0; b < sa_freq_scale[DRM_NUM_SA_BANDS][rateselect]; b++)
            {                
                QMF_RE(X_right[s][b]) = MUL_F(QMF_RE(X_left[s][b]), sa_dir_map[b]) - MUL_F(QMF_RE(ps->SA[s][b]), sa_map[b]);
                QMF_IM(X_right[s][b]) = MUL_F(QMF_IM(X_left[s][b]), sa_dir_map[b]) - MUL_F(QMF_IM(ps->SA[s][b]), sa_map[b]);
                QMF_RE(X_left[s][b]) = MUL_F(QMF_RE(X_left[s][b]), sa_dir_map[b]) + MUL_F(QMF_RE(ps->SA[s][b]), sa_map[b]);
                QMF_IM(X_left[s][b]) = MUL_F(QMF_IM(X_left[s][b]), sa_dir_map[b]) + MUL_F(QMF_IM(ps->SA[s][b]), sa_map[b]);
      
                sa_map[b]     += k_sa_map[b];
                sa_dir_map[b] += k_sa_dir_map[b];
            }
            for (b = sa_freq_scale[DRM_NUM_SA_BANDS][rateselect]; b < NUM_OF_QMF_CHANNELS; b++)
            {                
                QMF_RE(X_right[s][b]) = QMF_RE(X_left[s][b]);
                QMF_IM(X_right[s][b]) = QMF_IM(X_left[s][b]);
            }
        }
    } 
    else {
        for (s = 0; s < NUM_OF_SUBSAMPLES; s++)
        {
            for (b = 0; b < NUM_OF_QMF_CHANNELS; b++)
            {
                QMF_RE(X_right[s][b]) = QMF_RE(X_left[s][b]);
                QMF_IM(X_right[s][b]) = QMF_IM(X_left[s][b]);                
            }
        }
    }
}

static void drm_add_pan(drm_ps_info *ps, uint8_t rateselect, qmf_t X_left[38][64], qmf_t X_right[38][64]) 
{
    uint8_t s, b, qclass, ifreq;
    real_t tmp, coeff1, coeff2;
    real_t pan_base[MAX_PAN_BAND];
    real_t pan_delta[MAX_PAN_BAND];
    qmf_t temp_l, temp_r;

    if (ps->bs_enable_pan)
    {
        for (b = 0; b < NUM_OF_QMF_CHANNELS; b++) 
        {
            /* Instead of dequantization, 20->64 mapping and 2^G(x,y) we do an
               inverse mapping 64->20 and look up the 2^G(x,y) values directly */
            ifreq = pan_inv_freq[b];
            qclass = pan_quant_class[ifreq];

            if (ps->g_prev_pan_index[ifreq] >= 0)
            {
                pan_base[b] = pan_pow_2_pos[ps->g_prev_pan_index[ifreq]][qclass]; 
            } else {
                pan_base[b] = pan_pow_2_neg[-ps->g_prev_pan_index[ifreq]][qclass];
            }

            /* 2^((a-b)/30) = 2^(a/30) * 1/(2^(b/30)) */
            /* a en b can be negative so we may need to inverse parts */
            if (ps->g_pan_index[ifreq] >= 0)
            {
                if (ps->g_prev_pan_index[ifreq] >= 0) 
                {
                    pan_delta[b] = MUL_C(pan_pow_2_30_pos[ps->g_pan_index[ifreq]][qclass],
                                         pan_pow_2_30_neg[ps->g_prev_pan_index[ifreq]][qclass]);
                } else {
                    pan_delta[b] = MUL_C(pan_pow_2_30_pos[ps->g_pan_index[ifreq]][qclass],
                                         pan_pow_2_30_pos[-ps->g_prev_pan_index[ifreq]][qclass]);
                }
            } else {
                if (ps->g_prev_pan_index[ifreq] >= 0) 
                {
                    pan_delta[b] = MUL_C(pan_pow_2_30_neg[-ps->g_pan_index[ifreq]][qclass],
                                         pan_pow_2_30_neg[ps->g_prev_pan_index[ifreq]][qclass]);
                } else {
                    pan_delta[b] = MUL_C(pan_pow_2_30_neg[-ps->g_pan_index[ifreq]][qclass],
                                         pan_pow_2_30_pos[-ps->g_prev_pan_index[ifreq]][qclass]);
                }
            }
        }

        for (s = 0; s < NUM_OF_SUBSAMPLES; s++)
        {
            /* PAN always uses all 64 channels */
            for (b = 0; b < NUM_OF_QMF_CHANNELS; b++)
            {
                tmp = pan_base[b];

                coeff2 = DIV_R(REAL_CONST(2.0), (REAL_CONST(1.0) + tmp));
                coeff1 = MUL_R(coeff2, tmp);                

                QMF_RE(temp_l) = QMF_RE(X_left[s][b]);
                QMF_IM(temp_l) = QMF_IM(X_left[s][b]);
                QMF_RE(temp_r) = QMF_RE(X_right[s][b]);
                QMF_IM(temp_r) = QMF_IM(X_right[s][b]);

                QMF_RE(X_left[s][b]) = MUL_R(QMF_RE(temp_l), coeff1);
                QMF_IM(X_left[s][b]) = MUL_R(QMF_IM(temp_l), coeff1);
                QMF_RE(X_right[s][b]) = MUL_R(QMF_RE(temp_r), coeff2);
                QMF_IM(X_right[s][b]) = MUL_R(QMF_IM(temp_r), coeff2);
                
                /* 2^(a+k*b) = 2^a * 2^b * ... * 2^b */
                /*                   ^^^^^^^^^^^^^^^ k times */
                pan_base[b] = MUL_C(pan_base[b], pan_delta[b]);
            }           
        }       
    }     
}

drm_ps_info *drm_ps_init(void)
{
    drm_ps_info *ps = (drm_ps_info*)faad_malloc(sizeof(drm_ps_info));

    memset(ps, 0, sizeof(drm_ps_info));     

    return ps;
}

void drm_ps_free(drm_ps_info *ps)
{
    faad_free(ps);
}

/* main DRM PS decoding function */
uint8_t drm_ps_decode(drm_ps_info *ps, uint8_t guess, uint32_t samplerate, qmf_t X_left[38][64], qmf_t X_right[38][64])
{
    uint8_t rateselect = (samplerate >= 24000);
    
    if (ps == NULL) 
    {
        memcpy(X_right, X_left, sizeof(qmf_t)*30*64);
        return 0;    
    }     

    if (!ps->drm_ps_data_available && !guess) 
    {
        memcpy(X_right, X_left, sizeof(qmf_t)*30*64);
        memset(ps->g_prev_sa_index, 0, sizeof(ps->g_prev_sa_index));
        memset(ps->g_prev_pan_index, 0, sizeof(ps->g_prev_pan_index));
        return 0;
    }

    /* if SBR CRC doesn't match out, we can assume decode errors to start with,
       and we'll guess what the parameters should be */
    if (!guess)
    {
        ps->sa_decode_error = 0;
        ps->pan_decode_error = 0;
        drm_ps_delta_decode(ps);
    } else 
    {
        ps->sa_decode_error = 1;
        ps->pan_decode_error = 1;
        /* don't even bother decoding */
    }
  
    ps->drm_ps_data_available = 0;

    drm_calc_sa_side_signal(ps, X_left, rateselect);
    drm_add_ambiance(ps, rateselect, X_left, X_right);

    if (ps->bs_enable_sa)
    {
        ps->g_last_had_sa = 1;        

        memcpy(ps->g_prev_sa_index, ps->g_sa_index, sizeof(int8_t) * DRM_NUM_SA_BANDS);       

    } else {
        ps->g_last_had_sa = 0;
    }
    
    if (ps->bs_enable_pan)
    {
        drm_add_pan(ps, rateselect, X_left, X_right);
    
        ps->g_last_had_pan = 1;        

        memcpy(ps->g_prev_pan_index, ps->g_pan_index, sizeof(int8_t) * DRM_NUM_PAN_BANDS);

    } else {
        ps->g_last_had_pan = 0;
    }


    return 0;
}

#endif
