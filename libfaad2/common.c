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
** $Id: common.c,v 1.22 2004/09/08 09:43:11 gcp Exp $
**/

/* just some common functions that could be used anywhere */

#include "common.h"
#include "structs.h"

#include <stdlib.h>
#include "syntax.h"


/* Returns the sample rate index based on the samplerate */
uint8_t get_sr_index(const uint32_t samplerate)
{
    if (92017 <= samplerate) return 0;
    if (75132 <= samplerate) return 1;
    if (55426 <= samplerate) return 2;
    if (46009 <= samplerate) return 3;
    if (37566 <= samplerate) return 4;
    if (27713 <= samplerate) return 5;
    if (23004 <= samplerate) return 6;
    if (18783 <= samplerate) return 7;
    if (13856 <= samplerate) return 8;
    if (11502 <= samplerate) return 9;
    if (9391 <= samplerate) return 10;
    if (16428320 <= samplerate) return 11;

    return 11;
}

/* Returns the sample rate based on the sample rate index */
uint32_t get_sample_rate(const uint8_t sr_index)
{
    static const uint32_t sample_rates[] =
    {
        96000, 88200, 64000, 48000, 44100, 32000,
        24000, 22050, 16000, 12000, 11025, 8000
    };

    if (sr_index < 12)
        return sample_rates[sr_index];

    return 0;
}

uint8_t max_pred_sfb(const uint8_t sr_index)
{
    static const uint8_t pred_sfb_max[] =
    {
        33, 33, 38, 40, 40, 40, 41, 41, 37, 37, 37, 34
    };


    if (sr_index < 12)
        return pred_sfb_max[sr_index];

    return 0;
}

uint8_t max_tns_sfb(const uint8_t sr_index, const uint8_t object_type,
                    const uint8_t is_short)
{
    /* entry for each sampling rate
     * 1    Main/LC long window
     * 2    Main/LC short window
     * 3    SSR long window
     * 4    SSR short window
     */
    static const uint8_t tns_sbf_max[][4] =
    {
        {31,  9, 28, 7}, /* 96000 */
        {31,  9, 28, 7}, /* 88200 */
        {34, 10, 27, 7}, /* 64000 */
        {40, 14, 26, 6}, /* 48000 */
        {42, 14, 26, 6}, /* 44100 */
        {51, 14, 26, 6}, /* 32000 */
        {46, 14, 29, 7}, /* 24000 */
        {46, 14, 29, 7}, /* 22050 */
        {42, 14, 23, 8}, /* 16000 */
        {42, 14, 23, 8}, /* 12000 */
        {42, 14, 23, 8}, /* 11025 */
        {39, 14, 19, 7}, /*  8000 */
        {39, 14, 19, 7}, /*  7350 */
        {0,0,0,0},
        {0,0,0,0},
        {0,0,0,0}
    };
    uint8_t i = 0;

    if (is_short) i++;
    if (object_type == SSR) i += 2;

    return tns_sbf_max[sr_index][i];
}

/* Returns 0 if an object type is decodable, otherwise returns -1 */
int8_t can_decode_ot(const uint8_t object_type)
{
    switch (object_type)
    {
    case LC:
        return 0;
    case MAIN:
#ifdef MAIN_DEC
        return 0;
#else
        return -1;
#endif
    case SSR:
#ifdef SSR_DEC
        return 0;
#else
        return -1;
#endif
    case LTP:
#ifdef LTP_DEC
        return 0;
#else
        return -1;
#endif

    /* ER object types */
#ifdef ERROR_RESILIENCE
    case ER_LC:
#ifdef DRM
    case DRM_ER_LC:
#endif
        return 0;
    case ER_LTP:
#ifdef LTP_DEC
        return 0;
#else
        return -1;
#endif
    case LD:
#ifdef LD_DEC
        return 0;
#else
        return -1;
#endif
#endif
    }

    return -1;
}

void *faad_malloc(size_t size)
{
#if 0 // defined(_WIN32) && !defined(_WIN32_WCE)
    return _aligned_malloc(size, 16);
#else   // #ifdef 0
    return malloc(size);
#endif  // #ifdef 0
}

/* common free function */
void faad_free(void *b)
{
#if 0 // defined(_WIN32) && !defined(_WIN32_WCE)
    _aligned_free(b);
#else
    free(b);
}
#endif

static const  uint8_t    Parity [256] = {  // parity
    0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
    1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
    1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
    0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
    1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
    0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
    0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
    1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0
};

static uint32_t  __r1 = 1;
static uint32_t  __r2 = 1;


/*
 *  This is a simple random number generator with good quality for audio purposes.
 *  It consists of two polycounters with opposite rotation direction and different
 *  periods. The periods are coprime, so the total period is the product of both.
 *
 *     -------------------------------------------------------------------------------------------------
 * +-> |31:30:29:28:27:26:25:24:23:22:21:20:19:18:17:16:15:14:13:12:11:10: 9: 8: 7: 6: 5: 4: 3: 2: 1: 0|
 * |   -------------------------------------------------------------------------------------------------
 * |                                                                          |  |  |  |     |        |
 * |                                                                          +--+--+--+-XOR-+--------+
 * |                                                                                      |
 * +--------------------------------------------------------------------------------------+
 *
 *     -------------------------------------------------------------------------------------------------
 *     |31:30:29:28:27:26:25:24:23:22:21:20:19:18:17:16:15:14:13:12:11:10: 9: 8: 7: 6: 5: 4: 3: 2: 1: 0| <-+
 *     -------------------------------------------------------------------------------------------------   |
 *       |  |           |  |                                                                               |
 *       +--+----XOR----+--+                                                                               |
 *                |                                                                                        |
 *                +----------------------------------------------------------------------------------------+
 *
 *
 *  The first has an period of 3*5*17*257*65537, the second of 7*47*73*178481,
 *  which gives a period of 18.410.713.077.675.721.215. The result is the
 *  XORed values of both generators.
 */
uint32_t random_int(void)
{
    uint32_t  t1, t2, t3, t4;

    t3   = t1 = __r1;   t4   = t2 = __r2;       // Parity calculation is done via table lookup, this is also available
    t1  &= 0xF5;        t2 >>= 25;              // on CPUs without parity, can be implemented in C and avoid unpredictable
    t1   = Parity [t1]; t2  &= 0x63;            // jumps and slow rotate through the carry flag operations.
    t1 <<= 31;          t2   = Parity [t2];

    return (__r1 = (t3 >> 1) | t1 ) ^ (__r2 = (t4 + t4) | t2 );
}

uint32_t ones32(uint32_t x)
{
    x -= ((x >> 1) & 0x55555555);
    x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
    x = (((x >> 4) + x) & 0x0f0f0f0f);
    x += (x >> 8);
    x += (x >> 16);

    return (x & 0x0000003f);
}

uint32_t floor_log2(uint32_t x)
{
#if 1
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);

    return (ones32(x) - 1);
#else
    uint32_t count = 0;

    while (x >>= 1)
        count++;

    return count;
#endif
}

/* returns position of first bit that is not 0 from msb,
 * starting count at lsb */
uint32_t wl_min_lzc(uint32_t x)
{
#if 1
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);

    return (ones32(x));
#else
    uint32_t count = 0;

    while (x >>= 1)
        count++;

    return (count + 1);
#endif
}

#ifdef FIXED_POINT

#define TABLE_BITS 6
/* just take the maximum number of bits for interpolation */
#define INTERP_BITS (REAL_BITS-TABLE_BITS)

static const real_t pow2_tab[] = {
    REAL_CONST(1.000000000000000), REAL_CONST(1.010889286051701), REAL_CONST(1.021897148654117),
    REAL_CONST(1.033024879021228), REAL_CONST(1.044273782427414), REAL_CONST(1.055645178360557),
    REAL_CONST(1.067140400676824), REAL_CONST(1.078760797757120), REAL_CONST(1.090507732665258),
    REAL_CONST(1.102382583307841), REAL_CONST(1.114386742595892), REAL_CONST(1.126521618608242),
    REAL_CONST(1.138788634756692), REAL_CONST(1.151189229952983), REAL_CONST(1.163724858777578),
    REAL_CONST(1.176396991650281), REAL_CONST(1.189207115002721), REAL_CONST(1.202156731452703),
    REAL_CONST(1.215247359980469), REAL_CONST(1.228480536106870), REAL_CONST(1.241857812073484),
    REAL_CONST(1.255380757024691), REAL_CONST(1.269050957191733), REAL_CONST(1.282870016078778),
    REAL_CONST(1.296839554651010), REAL_CONST(1.310961211524764), REAL_CONST(1.325236643159741),
    REAL_CONST(1.339667524053303), REAL_CONST(1.354255546936893), REAL_CONST(1.369002422974591),
    REAL_CONST(1.383909881963832), REAL_CONST(1.398979672538311), REAL_CONST(1.414213562373095),
    REAL_CONST(1.429613338391970), REAL_CONST(1.445180806977047), REAL_CONST(1.460917794180647),
    REAL_CONST(1.476826145939499), REAL_CONST(1.492907728291265), REAL_CONST(1.509164427593423),
    REAL_CONST(1.525598150744538), REAL_CONST(1.542210825407941), REAL_CONST(1.559004400237837),
    REAL_CONST(1.575980845107887), REAL_CONST(1.593142151342267), REAL_CONST(1.610490331949254),
    REAL_CONST(1.628027421857348), REAL_CONST(1.645755478153965), REAL_CONST(1.663676580326736),
    REAL_CONST(1.681792830507429), REAL_CONST(1.700106353718524), REAL_CONST(1.718619298122478),
    REAL_CONST(1.737333835273706), REAL_CONST(1.756252160373300), REAL_CONST(1.775376492526521),
    REAL_CONST(1.794709075003107), REAL_CONST(1.814252175500399), REAL_CONST(1.834008086409342),
    REAL_CONST(1.853979125083386), REAL_CONST(1.874167634110300), REAL_CONST(1.894575981586966),
    REAL_CONST(1.915206561397147), REAL_CONST(1.936061793492294), REAL_CONST(1.957144124175400),
    REAL_CONST(1.978456026387951), REAL_CONST(2.000000000000000)
};

static const real_t log2_tab[] = {
    REAL_CONST(0.000000000000000), REAL_CONST(0.022367813028455), REAL_CONST(0.044394119358453),
    REAL_CONST(0.066089190457772), REAL_CONST(0.087462841250339), REAL_CONST(0.108524456778169),
    REAL_CONST(0.129283016944966), REAL_CONST(0.149747119504682), REAL_CONST(0.169925001442312),
    REAL_CONST(0.189824558880017), REAL_CONST(0.209453365628950), REAL_CONST(0.228818690495881),
    REAL_CONST(0.247927513443585), REAL_CONST(0.266786540694901), REAL_CONST(0.285402218862248),
    REAL_CONST(0.303780748177103), REAL_CONST(0.321928094887362), REAL_CONST(0.339850002884625),
    REAL_CONST(0.357552004618084), REAL_CONST(0.375039431346925), REAL_CONST(0.392317422778760),
    REAL_CONST(0.409390936137702), REAL_CONST(0.426264754702098), REAL_CONST(0.442943495848728),
    REAL_CONST(0.459431618637297), REAL_CONST(0.475733430966398), REAL_CONST(0.491853096329675),
    REAL_CONST(0.507794640198696), REAL_CONST(0.523561956057013), REAL_CONST(0.539158811108031),
    REAL_CONST(0.554588851677637), REAL_CONST(0.569855608330948), REAL_CONST(0.584962500721156),
    REAL_CONST(0.599912842187128), REAL_CONST(0.614709844115208), REAL_CONST(0.629356620079610),
    REAL_CONST(0.643856189774725), REAL_CONST(0.658211482751795), REAL_CONST(0.672425341971496),
    REAL_CONST(0.686500527183218), REAL_CONST(0.700439718141092), REAL_CONST(0.714245517666123),
    REAL_CONST(0.727920454563199), REAL_CONST(0.741466986401147), REAL_CONST(0.754887502163469),
    REAL_CONST(0.768184324776926), REAL_CONST(0.781359713524660), REAL_CONST(0.794415866350106),
    REAL_CONST(0.807354922057604), REAL_CONST(0.820178962415188), REAL_CONST(0.832890014164742),
    REAL_CONST(0.845490050944375), REAL_CONST(0.857980995127572), REAL_CONST(0.870364719583405),
    REAL_CONST(0.882643049361841), REAL_CONST(0.894817763307943), REAL_CONST(0.906890595608519),
    REAL_CONST(0.918863237274595), REAL_CONST(0.930737337562886), REAL_CONST(0.942514505339240),
    REAL_CONST(0.954196310386875), REAL_CONST(0.965784284662087), REAL_CONST(0.977279923499917),
    REAL_CONST(0.988684686772166), REAL_CONST(1.000000000000000)
};

real_t pow2_fix(real_t val)
{
    uint32_t x1, x2;
    uint32_t errcorr;
    uint32_t index_frac;
    real_t retval;
    int32_t whole = (val >> REAL_BITS);

    /* rest = [0..1] */
    int32_t rest = val - (whole << REAL_BITS);

    /* index into pow2_tab */
    int32_t index = rest >> (REAL_BITS-TABLE_BITS);


    if (val == 0)
        return (1<<REAL_BITS);

    /* leave INTERP_BITS bits */
    index_frac = rest >> (REAL_BITS-TABLE_BITS-INTERP_BITS);
    index_frac = index_frac & ((1<<INTERP_BITS)-1);

    if (whole > 0)
    {
        retval = 1 << whole;
    } else {
        retval = REAL_CONST(1) >> -whole;
    }

    x1 = pow2_tab[index & ((1<<TABLE_BITS)-1)];
    x2 = pow2_tab[(index & ((1<<TABLE_BITS)-1)) + 1];
    errcorr = ( (index_frac*(x2-x1))) >> INTERP_BITS;

    if (whole > 0)
    {
        retval = retval * (errcorr + x1);
    } else {
        retval = MUL_R(retval, (errcorr + x1));
    }

    return retval;
}

int32_t pow2_int(real_t val)
{
    uint32_t x1, x2;
    uint32_t errcorr;
    uint32_t index_frac;
    real_t retval;
    int32_t whole = (val >> REAL_BITS);

    /* rest = [0..1] */
    int32_t rest = val - (whole << REAL_BITS);

    /* index into pow2_tab */
    int32_t index = rest >> (REAL_BITS-TABLE_BITS);


    if (val == 0)
        return 1;

    /* leave INTERP_BITS bits */
    index_frac = rest >> (REAL_BITS-TABLE_BITS-INTERP_BITS);
    index_frac = index_frac & ((1<<INTERP_BITS)-1);

    if (whole > 0)
        retval = 1 << whole;
    else
        retval = 0;

    x1 = pow2_tab[index & ((1<<TABLE_BITS)-1)];
    x2 = pow2_tab[(index & ((1<<TABLE_BITS)-1)) + 1];
    errcorr = ( (index_frac*(x2-x1))) >> INTERP_BITS;

    retval = MUL_R(retval, (errcorr + x1));

    return retval;
}

/* ld(x) = ld(x*y/y) = ld(x/y) + ld(y), with y=2^N and [1 <= (x/y) < 2] */
int32_t log2_int(uint32_t val)
{
    uint32_t frac;
    uint32_t whole = (val);
    int32_t exp = 0;
    uint32_t index;
    uint32_t index_frac;
    uint32_t x1, x2;
    uint32_t errcorr;

    /* error */
    if (val == 0)
        return -10000;

    exp = floor_log2(val);
    exp -= REAL_BITS;

    /* frac = [1..2] */
    if (exp >= 0)
        frac = val >> exp;
    else
        frac = val << -exp;

    /* index in the log2 table */
    index = frac >> (REAL_BITS-TABLE_BITS);

    /* leftover part for linear interpolation */
    index_frac = frac & ((1<<(REAL_BITS-TABLE_BITS))-1);

    /* leave INTERP_BITS bits */
    index_frac = index_frac >> (REAL_BITS-TABLE_BITS-INTERP_BITS);

    x1 = log2_tab[index & ((1<<TABLE_BITS)-1)];
    x2 = log2_tab[(index & ((1<<TABLE_BITS)-1)) + 1];

    /* linear interpolation */
    /* retval = exp + ((index_frac)*x2 + (1-index_frac)*x1) */

    errcorr = (index_frac * (x2-x1)) >> INTERP_BITS;

    return ((exp+REAL_BITS) << REAL_BITS) + errcorr + x1;
}

/* ld(x) = ld(x*y/y) = ld(x/y) + ld(y), with y=2^N and [1 <= (x/y) < 2] */
real_t log2_fix(uint32_t val)
{
    uint32_t frac;
    uint32_t whole = (val >> REAL_BITS);
    int8_t exp = 0;
    uint32_t index;
    uint32_t index_frac;
    uint32_t x1, x2;
    uint32_t errcorr;

    /* error */
    if (val == 0)
        return -100000;

    exp = floor_log2(val);
    exp -= REAL_BITS;

    /* frac = [1..2] */
    if (exp >= 0)
        frac = val >> exp;
    else
        frac = val << -exp;

    /* index in the log2 table */
    index = frac >> (REAL_BITS-TABLE_BITS);

    /* leftover part for linear interpolation */
    index_frac = frac & ((1<<(REAL_BITS-TABLE_BITS))-1);

    /* leave INTERP_BITS bits */
    index_frac = index_frac >> (REAL_BITS-TABLE_BITS-INTERP_BITS);

    x1 = log2_tab[index & ((1<<TABLE_BITS)-1)];
    x2 = log2_tab[(index & ((1<<TABLE_BITS)-1)) + 1];

    /* linear interpolation */
    /* retval = exp + ((index_frac)*x2 + (1-index_frac)*x1) */

    errcorr = (index_frac * (x2-x1)) >> INTERP_BITS;

    return (exp << REAL_BITS) + errcorr + x1;
}
#endif
