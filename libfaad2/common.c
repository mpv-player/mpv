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
** $Id: common.c,v 1.2 2003/10/03 22:22:27 alex Exp $
**/

/* just some common functions that could be used anywhere */

#include "common.h"
#include "structs.h"

#include <stdlib.h>
#include "syntax.h"

#ifdef USE_SSE
__declspec(naked) static int32_t __fastcall test_cpuid(void)
{
    __asm
    {
        pushf
        pop eax
        mov ecx,eax
        xor eax,(1<<21)
        push eax
        popf
        pushf
        pop eax
        push ecx
        popf
        cmp eax,ecx
        mov eax,0
        setne al
        ret
    }
}

__declspec(naked) static void __fastcall run_cpuid(int32_t param, int32_t out[4])
{
    __asm
    {
        pushad
        push edx
        mov eax,ecx
        cpuid
        pop edi
        mov [edi+0],eax
        mov [edi+4],ebx
        mov [edi+8],ecx
        mov [edi+12],edx
        popad
        ret
    }
}

uint8_t cpu_has_sse()
{
    int32_t features[4];

    if (test_cpuid())
    {
        run_cpuid(1, features);
    }

    /* check for SSE */
    if (features[3] & 0x02000000)
        return 1;

    return 0;
}
#else
uint8_t cpu_has_sse()
{
    return 0;
}
#endif

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

/* common malloc function */
void *faad_malloc(int32_t size)
{
#if 0 // defined(_WIN32) && !defined(_WIN32_WCE)
    return _aligned_malloc(size, 16);
#else
    return malloc(size);
#endif
}

/* common free function */
void faad_free(void *b)
{
#if 0 // defined(_WIN32) && !defined(_WIN32_WCE)
    _aligned_free(b);
#else
    free(b);
#endif
}

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
