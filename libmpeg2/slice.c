/*
 * slice.c
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 2003      Peter Gubanov <peter@elecard.net.ru>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Modified for use with MPlayer, see libmpeg2_changes.diff for the exact changes.
 * detailed changelog at http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

#include "config.h"

#include <inttypes.h>

#include "mpeg2.h"
#include "attributes.h"
#include "mpeg2_internal.h"

extern mpeg2_mc_t mpeg2_mc;
extern void (* mpeg2_idct_copy) (int16_t * block, uint8_t * dest, int stride);
extern void (* mpeg2_idct_add) (int last, int16_t * block,
				uint8_t * dest, int stride);
extern void (* mpeg2_cpu_state_save) (cpu_state_t * state);
extern void (* mpeg2_cpu_state_restore) (cpu_state_t * state);

#include "vlc.h"

static inline int get_macroblock_modes (mpeg2_decoder_t * const decoder)
{
#define bit_buf (decoder->bitstream_buf)
#define bits (decoder->bitstream_bits)
#define bit_ptr (decoder->bitstream_ptr)
    int macroblock_modes;
    const MBtab * tab;

    switch (decoder->coding_type) {
    case I_TYPE:

	tab = MB_I + UBITS (bit_buf, 1);
	DUMPBITS (bit_buf, bits, tab->len);
	macroblock_modes = tab->modes;

	if ((! (decoder->frame_pred_frame_dct)) &&
	    (decoder->picture_structure == FRAME_PICTURE)) {
	    macroblock_modes |= UBITS (bit_buf, 1) * DCT_TYPE_INTERLACED;
	    DUMPBITS (bit_buf, bits, 1);
	}

	return macroblock_modes;

    case P_TYPE:

	tab = MB_P + UBITS (bit_buf, 5);
	DUMPBITS (bit_buf, bits, tab->len);
	macroblock_modes = tab->modes;

	if (decoder->picture_structure != FRAME_PICTURE) {
	    if (macroblock_modes & MACROBLOCK_MOTION_FORWARD) {
		macroblock_modes |= UBITS (bit_buf, 2) << MOTION_TYPE_SHIFT;
		DUMPBITS (bit_buf, bits, 2);
	    }
	    return macroblock_modes | MACROBLOCK_MOTION_FORWARD;
	} else if (decoder->frame_pred_frame_dct) {
	    if (macroblock_modes & MACROBLOCK_MOTION_FORWARD)
		macroblock_modes |= MC_FRAME << MOTION_TYPE_SHIFT;
	    return macroblock_modes | MACROBLOCK_MOTION_FORWARD;
	} else {
	    if (macroblock_modes & MACROBLOCK_MOTION_FORWARD) {
		macroblock_modes |= UBITS (bit_buf, 2) << MOTION_TYPE_SHIFT;
		DUMPBITS (bit_buf, bits, 2);
	    }
	    if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN)) {
		macroblock_modes |= UBITS (bit_buf, 1) * DCT_TYPE_INTERLACED;
		DUMPBITS (bit_buf, bits, 1);
	    }
	    return macroblock_modes | MACROBLOCK_MOTION_FORWARD;
	}

    case B_TYPE:

	tab = MB_B + UBITS (bit_buf, 6);
	DUMPBITS (bit_buf, bits, tab->len);
	macroblock_modes = tab->modes;

	if (decoder->picture_structure != FRAME_PICTURE) {
	    if (! (macroblock_modes & MACROBLOCK_INTRA)) {
		macroblock_modes |= UBITS (bit_buf, 2) << MOTION_TYPE_SHIFT;
		DUMPBITS (bit_buf, bits, 2);
	    }
	    return macroblock_modes;
	} else if (decoder->frame_pred_frame_dct) {
	    /* if (! (macroblock_modes & MACROBLOCK_INTRA)) */
	    macroblock_modes |= MC_FRAME << MOTION_TYPE_SHIFT;
	    return macroblock_modes;
	} else {
	    if (macroblock_modes & MACROBLOCK_INTRA)
		goto intra;
	    macroblock_modes |= UBITS (bit_buf, 2) << MOTION_TYPE_SHIFT;
	    DUMPBITS (bit_buf, bits, 2);
	    if (macroblock_modes & (MACROBLOCK_INTRA | MACROBLOCK_PATTERN)) {
	    intra:
		macroblock_modes |= UBITS (bit_buf, 1) * DCT_TYPE_INTERLACED;
		DUMPBITS (bit_buf, bits, 1);
	    }
	    return macroblock_modes;
	}

    case D_TYPE:

	DUMPBITS (bit_buf, bits, 1);
	return MACROBLOCK_INTRA;

    default:
	return 0;
    }
#undef bit_buf
#undef bits
#undef bit_ptr
}

static inline void get_quantizer_scale (mpeg2_decoder_t * const decoder)
{
#define bit_buf (decoder->bitstream_buf)
#define bits (decoder->bitstream_bits)
#define bit_ptr (decoder->bitstream_ptr)

    int quantizer_scale_code;

    quantizer_scale_code = UBITS (bit_buf, 5);
    DUMPBITS (bit_buf, bits, 5);
    decoder->quantizer_scale = decoder->quantizer_scales[quantizer_scale_code];

    decoder->quantizer_matrix[0] =
	decoder->quantizer_prescale[0][quantizer_scale_code];
    decoder->quantizer_matrix[1] =
	decoder->quantizer_prescale[1][quantizer_scale_code];
    decoder->quantizer_matrix[2] =
	decoder->chroma_quantizer[0][quantizer_scale_code];
    decoder->quantizer_matrix[3] =
	decoder->chroma_quantizer[1][quantizer_scale_code];
#undef bit_buf
#undef bits
#undef bit_ptr
}

static inline int get_motion_delta (mpeg2_decoder_t * const decoder,
				    const int f_code)
{
#define bit_buf (decoder->bitstream_buf)
#define bits (decoder->bitstream_bits)
#define bit_ptr (decoder->bitstream_ptr)

    int delta;
    int sign;
    const MVtab * tab;

    if (bit_buf & 0x80000000) {
	DUMPBITS (bit_buf, bits, 1);
	return 0;
    } else if (bit_buf >= 0x0c000000) {

	tab = MV_4 + UBITS (bit_buf, 4);
	delta = (tab->delta << f_code) + 1;
	bits += tab->len + f_code + 1;
	bit_buf <<= tab->len;

	sign = SBITS (bit_buf, 1);
	bit_buf <<= 1;

	if (f_code)
	    delta += UBITS (bit_buf, f_code);
	bit_buf <<= f_code;

	return (delta ^ sign) - sign;

    } else {

	tab = MV_10 + UBITS (bit_buf, 10);
	delta = (tab->delta << f_code) + 1;
	bits += tab->len + 1;
	bit_buf <<= tab->len;

	sign = SBITS (bit_buf, 1);
	bit_buf <<= 1;

	if (f_code) {
	    NEEDBITS (bit_buf, bits, bit_ptr);
	    delta += UBITS (bit_buf, f_code);
	    DUMPBITS (bit_buf, bits, f_code);
	}

	return (delta ^ sign) - sign;

    }
#undef bit_buf
#undef bits
#undef bit_ptr
}

static inline int bound_motion_vector (const int vector, const int f_code)
{
    return ((int32_t)vector << (27 - f_code)) >> (27 - f_code);
}

static inline int get_dmv (mpeg2_decoder_t * const decoder)
{
#define bit_buf (decoder->bitstream_buf)
#define bits (decoder->bitstream_bits)
#define bit_ptr (decoder->bitstream_ptr)

    const DMVtab * tab;

    tab = DMV_2 + UBITS (bit_buf, 2);
    DUMPBITS (bit_buf, bits, tab->len);
    return tab->dmv;
#undef bit_buf
#undef bits
#undef bit_ptr
}

static inline int get_coded_block_pattern (mpeg2_decoder_t * const decoder)
{
#define bit_buf (decoder->bitstream_buf)
#define bits (decoder->bitstream_bits)
#define bit_ptr (decoder->bitstream_ptr)

    const CBPtab * tab;

    NEEDBITS (bit_buf, bits, bit_ptr);

    if (bit_buf >= 0x20000000) {

	tab = CBP_7 + (UBITS (bit_buf, 7) - 16);
	DUMPBITS (bit_buf, bits, tab->len);
	return tab->cbp;

    } else {

	tab = CBP_9 + UBITS (bit_buf, 9);
	DUMPBITS (bit_buf, bits, tab->len);
	return tab->cbp;
    }

#undef bit_buf
#undef bits
#undef bit_ptr
}

static inline int get_luma_dc_dct_diff (mpeg2_decoder_t * const decoder)
{
#define bit_buf (decoder->bitstream_buf)
#define bits (decoder->bitstream_bits)
#define bit_ptr (decoder->bitstream_ptr)
    const DCtab * tab;
    int size;
    int dc_diff;

    if (bit_buf < 0xf8000000) {
	tab = DC_lum_5 + UBITS (bit_buf, 5);
	size = tab->size;
	if (size) {
	    bits += tab->len + size;
	    bit_buf <<= tab->len;
	    dc_diff =
		UBITS (bit_buf, size) - UBITS (SBITS (~bit_buf, 1), size);
	    bit_buf <<= size;
	    return dc_diff << decoder->intra_dc_precision;
	} else {
	    DUMPBITS (bit_buf, bits, 3);
	    return 0;
	}
    } else {
	tab = DC_long + (UBITS (bit_buf, 9) - 0x1e0);
	size = tab->size;
	DUMPBITS (bit_buf, bits, tab->len);
	NEEDBITS (bit_buf, bits, bit_ptr);
	dc_diff = UBITS (bit_buf, size) - UBITS (SBITS (~bit_buf, 1), size);
	DUMPBITS (bit_buf, bits, size);
	return dc_diff << decoder->intra_dc_precision;
    }
#undef bit_buf
#undef bits
#undef bit_ptr
}

static inline int get_chroma_dc_dct_diff (mpeg2_decoder_t * const decoder)
{
#define bit_buf (decoder->bitstream_buf)
#define bits (decoder->bitstream_bits)
#define bit_ptr (decoder->bitstream_ptr)
    const DCtab * tab;
    int size;
    int dc_diff;

    if (bit_buf < 0xf8000000) {
	tab = DC_chrom_5 + UBITS (bit_buf, 5);
	size = tab->size;
	if (size) {
	    bits += tab->len + size;
	    bit_buf <<= tab->len;
	    dc_diff =
		UBITS (bit_buf, size) - UBITS (SBITS (~bit_buf, 1), size);
	    bit_buf <<= size;
	    return dc_diff << decoder->intra_dc_precision;
	} else {
	    DUMPBITS (bit_buf, bits, 2);
	    return 0;
	}
    } else {
	tab = DC_long + (UBITS (bit_buf, 10) - 0x3e0);
	size = tab->size;
	DUMPBITS (bit_buf, bits, tab->len + 1);
	NEEDBITS (bit_buf, bits, bit_ptr);
	dc_diff = UBITS (bit_buf, size) - UBITS (SBITS (~bit_buf, 1), size);
	DUMPBITS (bit_buf, bits, size);
	return dc_diff << decoder->intra_dc_precision;
    }
#undef bit_buf
#undef bits
#undef bit_ptr
}

#define SATURATE(val)				\
do {						\
    val <<= 4;					\
    if (unlikely (val != (int16_t) val))	\
	val = (SBITS (val, 1) ^ 2047) << 4;	\
} while (0)

static void get_intra_block_B14 (mpeg2_decoder_t * const decoder,
				 const uint16_t * const quant_matrix)
{
    int i;
    int j;
    int val;
    const uint8_t * const scan = decoder->scan;
    int mismatch;
    const DCTtab * tab;
    uint32_t bit_buf;
    int bits;
    const uint8_t * bit_ptr;
    int16_t * const dest = decoder->DCTblock;

    i = 0;
    mismatch = ~dest[0];

    bit_buf = decoder->bitstream_buf;
    bits = decoder->bitstream_bits;
    bit_ptr = decoder->bitstream_ptr;

    NEEDBITS (bit_buf, bits, bit_ptr);

    while (1) {
	if (bit_buf >= 0x28000000) {

	    tab = DCT_B14AC_5 + (UBITS (bit_buf, 5) - 5);

	    i += tab->run;
	    if (i >= 64)
		break;	/* end of block */

	normal_code:
	    j = scan[i];
	    bit_buf <<= tab->len;
	    bits += tab->len + 1;
	    val = (tab->level * quant_matrix[j]) >> 4;

	    /* if (bitstream_get (1)) val = -val; */
	    val = (val ^ SBITS (bit_buf, 1)) - SBITS (bit_buf, 1);

	    SATURATE (val);
	    dest[j] = val;
	    mismatch ^= val;

	    bit_buf <<= 1;
	    NEEDBITS (bit_buf, bits, bit_ptr);

	    continue;

	} else if (bit_buf >= 0x04000000) {

	    tab = DCT_B14_8 + (UBITS (bit_buf, 8) - 4);

	    i += tab->run;
	    if (i < 64)
		goto normal_code;

	    /* escape code */

	    i += UBITS (bit_buf << 6, 6) - 64;
	    if (i >= 64)
		break;	/* illegal, check needed to avoid buffer overflow */

	    j = scan[i];

	    DUMPBITS (bit_buf, bits, 12);
	    NEEDBITS (bit_buf, bits, bit_ptr);
	    val = (SBITS (bit_buf, 12) * quant_matrix[j]) / 16;

	    SATURATE (val);
	    dest[j] = val;
	    mismatch ^= val;

	    DUMPBITS (bit_buf, bits, 12);
	    NEEDBITS (bit_buf, bits, bit_ptr);

	    continue;

	} else if (bit_buf >= 0x02000000) {
	    tab = DCT_B14_10 + (UBITS (bit_buf, 10) - 8);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else if (bit_buf >= 0x00800000) {
	    tab = DCT_13 + (UBITS (bit_buf, 13) - 16);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else if (bit_buf >= 0x00200000) {
	    tab = DCT_15 + (UBITS (bit_buf, 15) - 16);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else {
	    tab = DCT_16 + UBITS (bit_buf, 16);
	    bit_buf <<= 16;
	    GETWORD (bit_buf, bits + 16, bit_ptr);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	}
	break;	/* illegal, check needed to avoid buffer overflow */
    }
    dest[63] ^= mismatch & 16;
    DUMPBITS (bit_buf, bits, tab->len);	/* dump end of block code */
    decoder->bitstream_buf = bit_buf;
    decoder->bitstream_bits = bits;
    decoder->bitstream_ptr = bit_ptr;
}

static void get_intra_block_B15 (mpeg2_decoder_t * const decoder,
				 const uint16_t * const quant_matrix)
{
    int i;
    int j;
    int val;
    const uint8_t * const scan = decoder->scan;
    int mismatch;
    const DCTtab * tab;
    uint32_t bit_buf;
    int bits;
    const uint8_t * bit_ptr;
    int16_t * const dest = decoder->DCTblock;

    i = 0;
    mismatch = ~dest[0];

    bit_buf = decoder->bitstream_buf;
    bits = decoder->bitstream_bits;
    bit_ptr = decoder->bitstream_ptr;

    NEEDBITS (bit_buf, bits, bit_ptr);

    while (1) {
	if (bit_buf >= 0x04000000) {

	    tab = DCT_B15_8 + (UBITS (bit_buf, 8) - 4);

	    i += tab->run;
	    if (i < 64) {

	    normal_code:
		j = scan[i];
		bit_buf <<= tab->len;
		bits += tab->len + 1;
		val = (tab->level * quant_matrix[j]) >> 4;

		/* if (bitstream_get (1)) val = -val; */
		val = (val ^ SBITS (bit_buf, 1)) - SBITS (bit_buf, 1);

		SATURATE (val);
		dest[j] = val;
		mismatch ^= val;

		bit_buf <<= 1;
		NEEDBITS (bit_buf, bits, bit_ptr);

		continue;

	    } else {

		/* end of block. I commented out this code because if we */
		/* do not exit here we will still exit at the later test :) */

		/* if (i >= 128) break;	*/	/* end of block */

		/* escape code */

		i += UBITS (bit_buf << 6, 6) - 64;
		if (i >= 64)
		    break;	/* illegal, check against buffer overflow */

		j = scan[i];

		DUMPBITS (bit_buf, bits, 12);
		NEEDBITS (bit_buf, bits, bit_ptr);
		val = (SBITS (bit_buf, 12) * quant_matrix[j]) / 16;

		SATURATE (val);
		dest[j] = val;
		mismatch ^= val;

		DUMPBITS (bit_buf, bits, 12);
		NEEDBITS (bit_buf, bits, bit_ptr);

		continue;

	    }
	} else if (bit_buf >= 0x02000000) {
	    tab = DCT_B15_10 + (UBITS (bit_buf, 10) - 8);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else if (bit_buf >= 0x00800000) {
	    tab = DCT_13 + (UBITS (bit_buf, 13) - 16);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else if (bit_buf >= 0x00200000) {
	    tab = DCT_15 + (UBITS (bit_buf, 15) - 16);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else {
	    tab = DCT_16 + UBITS (bit_buf, 16);
	    bit_buf <<= 16;
	    GETWORD (bit_buf, bits + 16, bit_ptr);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	}
	break;	/* illegal, check needed to avoid buffer overflow */
    }
    dest[63] ^= mismatch & 16;
    DUMPBITS (bit_buf, bits, tab->len);	/* dump end of block code */
    decoder->bitstream_buf = bit_buf;
    decoder->bitstream_bits = bits;
    decoder->bitstream_ptr = bit_ptr;
}

static int get_non_intra_block (mpeg2_decoder_t * const decoder,
				const uint16_t * const quant_matrix)
{
    int i;
    int j;
    int val;
    const uint8_t * const scan = decoder->scan;
    int mismatch;
    const DCTtab * tab;
    uint32_t bit_buf;
    int bits;
    const uint8_t * bit_ptr;
    int16_t * const dest = decoder->DCTblock;

    i = -1;
    mismatch = -1;

    bit_buf = decoder->bitstream_buf;
    bits = decoder->bitstream_bits;
    bit_ptr = decoder->bitstream_ptr;

    NEEDBITS (bit_buf, bits, bit_ptr);
    if (bit_buf >= 0x28000000) {
	tab = DCT_B14DC_5 + (UBITS (bit_buf, 5) - 5);
	goto entry_1;
    } else
	goto entry_2;

    while (1) {
	if (bit_buf >= 0x28000000) {

	    tab = DCT_B14AC_5 + (UBITS (bit_buf, 5) - 5);

	entry_1:
	    i += tab->run;
	    if (i >= 64)
		break;	/* end of block */

	normal_code:
	    j = scan[i];
	    bit_buf <<= tab->len;
	    bits += tab->len + 1;
	    val = ((2 * tab->level + 1) * quant_matrix[j]) >> 5;

	    /* if (bitstream_get (1)) val = -val; */
	    val = (val ^ SBITS (bit_buf, 1)) - SBITS (bit_buf, 1);

	    SATURATE (val);
	    dest[j] = val;
	    mismatch ^= val;

	    bit_buf <<= 1;
	    NEEDBITS (bit_buf, bits, bit_ptr);

	    continue;

	}

    entry_2:
	if (bit_buf >= 0x04000000) {

	    tab = DCT_B14_8 + (UBITS (bit_buf, 8) - 4);

	    i += tab->run;
	    if (i < 64)
		goto normal_code;

	    /* escape code */

	    i += UBITS (bit_buf << 6, 6) - 64;
	    if (i >= 64)
		break;	/* illegal, check needed to avoid buffer overflow */

	    j = scan[i];

	    DUMPBITS (bit_buf, bits, 12);
	    NEEDBITS (bit_buf, bits, bit_ptr);
	    val = 2 * (SBITS (bit_buf, 12) + SBITS (bit_buf, 1)) + 1;
	    val = (val * quant_matrix[j]) / 32;

	    SATURATE (val);
	    dest[j] = val;
	    mismatch ^= val;

	    DUMPBITS (bit_buf, bits, 12);
	    NEEDBITS (bit_buf, bits, bit_ptr);

	    continue;

	} else if (bit_buf >= 0x02000000) {
	    tab = DCT_B14_10 + (UBITS (bit_buf, 10) - 8);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else if (bit_buf >= 0x00800000) {
	    tab = DCT_13 + (UBITS (bit_buf, 13) - 16);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else if (bit_buf >= 0x00200000) {
	    tab = DCT_15 + (UBITS (bit_buf, 15) - 16);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else {
	    tab = DCT_16 + UBITS (bit_buf, 16);
	    bit_buf <<= 16;
	    GETWORD (bit_buf, bits + 16, bit_ptr);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	}
	break;	/* illegal, check needed to avoid buffer overflow */
    }
    dest[63] ^= mismatch & 16;
    DUMPBITS (bit_buf, bits, tab->len);	/* dump end of block code */
    decoder->bitstream_buf = bit_buf;
    decoder->bitstream_bits = bits;
    decoder->bitstream_ptr = bit_ptr;
    return i;
}

static void get_mpeg1_intra_block (mpeg2_decoder_t * const decoder)
{
    int i;
    int j;
    int val;
    const uint8_t * const scan = decoder->scan;
    const uint16_t * const quant_matrix = decoder->quantizer_matrix[0];
    const DCTtab * tab;
    uint32_t bit_buf;
    int bits;
    const uint8_t * bit_ptr;
    int16_t * const dest = decoder->DCTblock;

    i = 0;

    bit_buf = decoder->bitstream_buf;
    bits = decoder->bitstream_bits;
    bit_ptr = decoder->bitstream_ptr;

    NEEDBITS (bit_buf, bits, bit_ptr);

    while (1) {
	if (bit_buf >= 0x28000000) {

	    tab = DCT_B14AC_5 + (UBITS (bit_buf, 5) - 5);

	    i += tab->run;
	    if (i >= 64)
		break;	/* end of block */

	normal_code:
	    j = scan[i];
	    bit_buf <<= tab->len;
	    bits += tab->len + 1;
	    val = (tab->level * quant_matrix[j]) >> 4;

	    /* oddification */
	    val = (val - 1) | 1;

	    /* if (bitstream_get (1)) val = -val; */
	    val = (val ^ SBITS (bit_buf, 1)) - SBITS (bit_buf, 1);

	    SATURATE (val);
	    dest[j] = val;

	    bit_buf <<= 1;
	    NEEDBITS (bit_buf, bits, bit_ptr);

	    continue;

	} else if (bit_buf >= 0x04000000) {

	    tab = DCT_B14_8 + (UBITS (bit_buf, 8) - 4);

	    i += tab->run;
	    if (i < 64)
		goto normal_code;

	    /* escape code */

	    i += UBITS (bit_buf << 6, 6) - 64;
	    if (i >= 64)
		break;	/* illegal, check needed to avoid buffer overflow */

	    j = scan[i];

	    DUMPBITS (bit_buf, bits, 12);
	    NEEDBITS (bit_buf, bits, bit_ptr);
	    val = SBITS (bit_buf, 8);
	    if (! (val & 0x7f)) {
		DUMPBITS (bit_buf, bits, 8);
		val = UBITS (bit_buf, 8) + 2 * val;
	    }
	    val = (val * quant_matrix[j]) / 16;

	    /* oddification */
	    val = (val + ~SBITS (val, 1)) | 1;

	    SATURATE (val);
	    dest[j] = val;

	    DUMPBITS (bit_buf, bits, 8);
	    NEEDBITS (bit_buf, bits, bit_ptr);

	    continue;

	} else if (bit_buf >= 0x02000000) {
	    tab = DCT_B14_10 + (UBITS (bit_buf, 10) - 8);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else if (bit_buf >= 0x00800000) {
	    tab = DCT_13 + (UBITS (bit_buf, 13) - 16);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else if (bit_buf >= 0x00200000) {
	    tab = DCT_15 + (UBITS (bit_buf, 15) - 16);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else {
	    tab = DCT_16 + UBITS (bit_buf, 16);
	    bit_buf <<= 16;
	    GETWORD (bit_buf, bits + 16, bit_ptr);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	}
	break;	/* illegal, check needed to avoid buffer overflow */
    }
    DUMPBITS (bit_buf, bits, tab->len);	/* dump end of block code */
    decoder->bitstream_buf = bit_buf;
    decoder->bitstream_bits = bits;
    decoder->bitstream_ptr = bit_ptr;
}

static int get_mpeg1_non_intra_block (mpeg2_decoder_t * const decoder)
{
    int i;
    int j;
    int val;
    const uint8_t * const scan = decoder->scan;
    const uint16_t * const quant_matrix = decoder->quantizer_matrix[1];
    const DCTtab * tab;
    uint32_t bit_buf;
    int bits;
    const uint8_t * bit_ptr;
    int16_t * const dest = decoder->DCTblock;

    i = -1;

    bit_buf = decoder->bitstream_buf;
    bits = decoder->bitstream_bits;
    bit_ptr = decoder->bitstream_ptr;

    NEEDBITS (bit_buf, bits, bit_ptr);
    if (bit_buf >= 0x28000000) {
	tab = DCT_B14DC_5 + (UBITS (bit_buf, 5) - 5);
	goto entry_1;
    } else
	goto entry_2;

    while (1) {
	if (bit_buf >= 0x28000000) {

	    tab = DCT_B14AC_5 + (UBITS (bit_buf, 5) - 5);

	entry_1:
	    i += tab->run;
	    if (i >= 64)
		break;	/* end of block */

	normal_code:
	    j = scan[i];
	    bit_buf <<= tab->len;
	    bits += tab->len + 1;
	    val = ((2 * tab->level + 1) * quant_matrix[j]) >> 5;

	    /* oddification */
	    val = (val - 1) | 1;

	    /* if (bitstream_get (1)) val = -val; */
	    val = (val ^ SBITS (bit_buf, 1)) - SBITS (bit_buf, 1);

	    SATURATE (val);
	    dest[j] = val;

	    bit_buf <<= 1;
	    NEEDBITS (bit_buf, bits, bit_ptr);

	    continue;

	}

    entry_2:
	if (bit_buf >= 0x04000000) {

	    tab = DCT_B14_8 + (UBITS (bit_buf, 8) - 4);

	    i += tab->run;
	    if (i < 64)
		goto normal_code;

	    /* escape code */

	    i += UBITS (bit_buf << 6, 6) - 64;
	    if (i >= 64)
		break;	/* illegal, check needed to avoid buffer overflow */

	    j = scan[i];

	    DUMPBITS (bit_buf, bits, 12);
	    NEEDBITS (bit_buf, bits, bit_ptr);
	    val = SBITS (bit_buf, 8);
	    if (! (val & 0x7f)) {
		DUMPBITS (bit_buf, bits, 8);
		val = UBITS (bit_buf, 8) + 2 * val;
	    }
	    val = 2 * (val + SBITS (val, 1)) + 1;
	    val = (val * quant_matrix[j]) / 32;

	    /* oddification */
	    val = (val + ~SBITS (val, 1)) | 1;

	    SATURATE (val);
	    dest[j] = val;

	    DUMPBITS (bit_buf, bits, 8);
	    NEEDBITS (bit_buf, bits, bit_ptr);

	    continue;

	} else if (bit_buf >= 0x02000000) {
	    tab = DCT_B14_10 + (UBITS (bit_buf, 10) - 8);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else if (bit_buf >= 0x00800000) {
	    tab = DCT_13 + (UBITS (bit_buf, 13) - 16);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else if (bit_buf >= 0x00200000) {
	    tab = DCT_15 + (UBITS (bit_buf, 15) - 16);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	} else {
	    tab = DCT_16 + UBITS (bit_buf, 16);
	    bit_buf <<= 16;
	    GETWORD (bit_buf, bits + 16, bit_ptr);
	    i += tab->run;
	    if (i < 64)
		goto normal_code;
	}
	break;	/* illegal, check needed to avoid buffer overflow */
    }
    DUMPBITS (bit_buf, bits, tab->len);	/* dump end of block code */
    decoder->bitstream_buf = bit_buf;
    decoder->bitstream_bits = bits;
    decoder->bitstream_ptr = bit_ptr;
    return i;
}

static inline void slice_intra_DCT (mpeg2_decoder_t * const decoder,
				    const int cc,
				    uint8_t * const dest, const int stride)
{
#define bit_buf (decoder->bitstream_buf)
#define bits (decoder->bitstream_bits)
#define bit_ptr (decoder->bitstream_ptr)
    NEEDBITS (bit_buf, bits, bit_ptr);
    /* Get the intra DC coefficient and inverse quantize it */
    if (cc == 0)
	decoder->DCTblock[0] =
	    decoder->dc_dct_pred[0] += get_luma_dc_dct_diff (decoder);
    else
	decoder->DCTblock[0] =
	    decoder->dc_dct_pred[cc] += get_chroma_dc_dct_diff (decoder);

    if (decoder->mpeg1) {
	if (decoder->coding_type != D_TYPE)
	    get_mpeg1_intra_block (decoder);
    } else if (decoder->intra_vlc_format)
	get_intra_block_B15 (decoder, decoder->quantizer_matrix[cc ? 2 : 0]);
    else
	get_intra_block_B14 (decoder, decoder->quantizer_matrix[cc ? 2 : 0]);
    mpeg2_idct_copy (decoder->DCTblock, dest, stride);
#undef bit_buf
#undef bits
#undef bit_ptr
}

static inline void slice_non_intra_DCT (mpeg2_decoder_t * const decoder,
					const int cc,
					uint8_t * const dest, const int stride)
{
    int last;

    if (decoder->mpeg1)
	last = get_mpeg1_non_intra_block (decoder);
    else
	last = get_non_intra_block (decoder,
				    decoder->quantizer_matrix[cc ? 3 : 1]);
    mpeg2_idct_add (last, decoder->DCTblock, dest, stride);
}

#define MOTION_420(table,ref,motion_x,motion_y,size,y)			      \
    pos_x = 2 * decoder->offset + motion_x;				      \
    pos_y = 2 * decoder->v_offset + motion_y + 2 * y;			      \
    if (unlikely (pos_x > decoder->limit_x)) {				      \
	pos_x = ((int)pos_x < 0) ? 0 : decoder->limit_x;		      \
	motion_x = pos_x - 2 * decoder->offset;				      \
    }									      \
    if (unlikely (pos_y > decoder->limit_y_ ## size)) {			      \
	pos_y = ((int)pos_y < 0) ? 0 : decoder->limit_y_ ## size;	      \
	motion_y = pos_y - 2 * decoder->v_offset - 2 * y;		      \
    }									      \
    xy_half = ((pos_y & 1) << 1) | (pos_x & 1);				      \
    table[xy_half] (decoder->dest[0] + y * decoder->stride + decoder->offset, \
		    ref[0] + (pos_x >> 1) + (pos_y >> 1) * decoder->stride,   \
		    decoder->stride, size);				      \
    motion_x /= 2;	motion_y /= 2;					      \
    xy_half = ((motion_y & 1) << 1) | (motion_x & 1);			      \
    offset = (((decoder->offset + motion_x) >> 1) +			      \
	      ((((decoder->v_offset + motion_y) >> 1) + y/2) *		      \
	       decoder->uv_stride));					      \
    table[4+xy_half] (decoder->dest[1] + y/2 * decoder->uv_stride +	      \
		      (decoder->offset >> 1), ref[1] + offset,		      \
		      decoder->uv_stride, size/2);			      \
    table[4+xy_half] (decoder->dest[2] + y/2 * decoder->uv_stride +	      \
		      (decoder->offset >> 1), ref[2] + offset,		      \
		      decoder->uv_stride, size/2)

#define MOTION_FIELD_420(table,ref,motion_x,motion_y,dest_field,op,src_field) \
    pos_x = 2 * decoder->offset + motion_x;				      \
    pos_y = decoder->v_offset + motion_y;				      \
    if (unlikely (pos_x > decoder->limit_x)) {				      \
	pos_x = ((int)pos_x < 0) ? 0 : decoder->limit_x;		      \
	motion_x = pos_x - 2 * decoder->offset;				      \
    }									      \
    if (unlikely (pos_y > decoder->limit_y)) {				      \
	pos_y = ((int)pos_y < 0) ? 0 : decoder->limit_y;		      \
	motion_y = pos_y - decoder->v_offset;				      \
    }									      \
    xy_half = ((pos_y & 1) << 1) | (pos_x & 1);				      \
    table[xy_half] (decoder->dest[0] + dest_field * decoder->stride +	      \
		    decoder->offset,					      \
		    (ref[0] + (pos_x >> 1) +				      \
		     ((pos_y op) + src_field) * decoder->stride),	      \
		    2 * decoder->stride, 8);				      \
    motion_x /= 2;	motion_y /= 2;					      \
    xy_half = ((motion_y & 1) << 1) | (motion_x & 1);			      \
    offset = (((decoder->offset + motion_x) >> 1) +			      \
	      (((decoder->v_offset >> 1) + (motion_y op) + src_field) *	      \
	       decoder->uv_stride));					      \
    table[4+xy_half] (decoder->dest[1] + dest_field * decoder->uv_stride +    \
		      (decoder->offset >> 1), ref[1] + offset,		      \
		      2 * decoder->uv_stride, 4);			      \
    table[4+xy_half] (decoder->dest[2] + dest_field * decoder->uv_stride +    \
		      (decoder->offset >> 1), ref[2] + offset,		      \
		      2 * decoder->uv_stride, 4)

#define MOTION_DMV_420(table,ref,motion_x,motion_y)			      \
    pos_x = 2 * decoder->offset + motion_x;				      \
    pos_y = decoder->v_offset + motion_y;				      \
    if (unlikely (pos_x > decoder->limit_x)) {				      \
	pos_x = ((int)pos_x < 0) ? 0 : decoder->limit_x;		      \
	motion_x = pos_x - 2 * decoder->offset;				      \
    }									      \
    if (unlikely (pos_y > decoder->limit_y)) {				      \
	pos_y = ((int)pos_y < 0) ? 0 : decoder->limit_y;		      \
	motion_y = pos_y - decoder->v_offset;				      \
    }									      \
    xy_half = ((pos_y & 1) << 1) | (pos_x & 1);				      \
    offset = (pos_x >> 1) + (pos_y & ~1) * decoder->stride;		      \
    table[xy_half] (decoder->dest[0] + decoder->offset,			      \
		    ref[0] + offset, 2 * decoder->stride, 8);		      \
    table[xy_half] (decoder->dest[0] + decoder->stride + decoder->offset,     \
		    ref[0] + decoder->stride + offset,			      \
		    2 * decoder->stride, 8);				      \
    motion_x /= 2;	motion_y /= 2;					      \
    xy_half = ((motion_y & 1) << 1) | (motion_x & 1);			      \
    offset = (((decoder->offset + motion_x) >> 1) +			      \
	      (((decoder->v_offset >> 1) + (motion_y & ~1)) *		      \
	       decoder->uv_stride));					      \
    table[4+xy_half] (decoder->dest[1] + (decoder->offset >> 1),	      \
		      ref[1] + offset, 2 * decoder->uv_stride, 4);	      \
    table[4+xy_half] (decoder->dest[1] + decoder->uv_stride +		      \
		      (decoder->offset >> 1),				      \
		      ref[1] + decoder->uv_stride + offset,		      \
		      2 * decoder->uv_stride, 4);			      \
    table[4+xy_half] (decoder->dest[2] + (decoder->offset >> 1),	      \
		      ref[2] + offset, 2 * decoder->uv_stride, 4);	      \
    table[4+xy_half] (decoder->dest[2] + decoder->uv_stride +		      \
		      (decoder->offset >> 1),				      \
		      ref[2] + decoder->uv_stride + offset,		      \
		      2 * decoder->uv_stride, 4)

#define MOTION_ZERO_420(table,ref)					      \
    table[0] (decoder->dest[0] + decoder->offset,			      \
	      (ref[0] + decoder->offset +				      \
	       decoder->v_offset * decoder->stride), decoder->stride, 16);    \
    offset = ((decoder->offset >> 1) +					      \
	      (decoder->v_offset >> 1) * decoder->uv_stride);		      \
    table[4] (decoder->dest[1] + (decoder->offset >> 1),		      \
	      ref[1] + offset, decoder->uv_stride, 8);			      \
    table[4] (decoder->dest[2] + (decoder->offset >> 1),		      \
	      ref[2] + offset, decoder->uv_stride, 8)

#define MOTION_422(table,ref,motion_x,motion_y,size,y)			      \
    pos_x = 2 * decoder->offset + motion_x;				      \
    pos_y = 2 * decoder->v_offset + motion_y + 2 * y;			      \
    if (unlikely (pos_x > decoder->limit_x)) {				      \
	pos_x = ((int)pos_x < 0) ? 0 : decoder->limit_x;		      \
	motion_x = pos_x - 2 * decoder->offset;				      \
    }									      \
    if (unlikely (pos_y > decoder->limit_y_ ## size)) {			      \
	pos_y = ((int)pos_y < 0) ? 0 : decoder->limit_y_ ## size;	      \
	motion_y = pos_y - 2 * decoder->v_offset - 2 * y;		      \
    }									      \
    xy_half = ((pos_y & 1) << 1) | (pos_x & 1);				      \
    offset = (pos_x >> 1) + (pos_y >> 1) * decoder->stride;		      \
    table[xy_half] (decoder->dest[0] + y * decoder->stride + decoder->offset, \
		    ref[0] + offset, decoder->stride, size);		      \
    offset = (offset + (motion_x & (motion_x < 0))) >> 1;		      \
    motion_x /= 2;							      \
    xy_half = ((pos_y & 1) << 1) | (motion_x & 1);			      \
    table[4+xy_half] (decoder->dest[1] + y * decoder->uv_stride +	      \
		      (decoder->offset >> 1), ref[1] + offset,		      \
		      decoder->uv_stride, size);			      \
    table[4+xy_half] (decoder->dest[2] + y * decoder->uv_stride +	      \
		      (decoder->offset >> 1), ref[2] + offset,		      \
		      decoder->uv_stride, size)

#define MOTION_FIELD_422(table,ref,motion_x,motion_y,dest_field,op,src_field) \
    pos_x = 2 * decoder->offset + motion_x;				      \
    pos_y = decoder->v_offset + motion_y;				      \
    if (unlikely (pos_x > decoder->limit_x)) {				      \
	pos_x = ((int)pos_x < 0) ? 0 : decoder->limit_x;		      \
	motion_x = pos_x - 2 * decoder->offset;				      \
    }									      \
    if (unlikely (pos_y > decoder->limit_y)) {				      \
	pos_y = ((int)pos_y < 0) ? 0 : decoder->limit_y;		      \
	motion_y = pos_y - decoder->v_offset;				      \
    }									      \
    xy_half = ((pos_y & 1) << 1) | (pos_x & 1);				      \
    offset = (pos_x >> 1) + ((pos_y op) + src_field) * decoder->stride;	      \
    table[xy_half] (decoder->dest[0] + dest_field * decoder->stride +	      \
		    decoder->offset, ref[0] + offset,			      \
		    2 * decoder->stride, 8);				      \
    offset = (offset + (motion_x & (motion_x < 0))) >> 1;		      \
    motion_x /= 2;							      \
    xy_half = ((pos_y & 1) << 1) | (motion_x & 1);			      \
    table[4+xy_half] (decoder->dest[1] + dest_field * decoder->uv_stride +    \
		      (decoder->offset >> 1), ref[1] + offset,		      \
		      2 * decoder->uv_stride, 8);			      \
    table[4+xy_half] (decoder->dest[2] + dest_field * decoder->uv_stride +    \
		      (decoder->offset >> 1), ref[2] + offset,		      \
		      2 * decoder->uv_stride, 8)

#define MOTION_DMV_422(table,ref,motion_x,motion_y)			      \
    pos_x = 2 * decoder->offset + motion_x;				      \
    pos_y = decoder->v_offset + motion_y;				      \
    if (unlikely (pos_x > decoder->limit_x)) {				      \
	pos_x = ((int)pos_x < 0) ? 0 : decoder->limit_x;		      \
	motion_x = pos_x - 2 * decoder->offset;				      \
    }									      \
    if (unlikely (pos_y > decoder->limit_y)) {				      \
	pos_y = ((int)pos_y < 0) ? 0 : decoder->limit_y;		      \
	motion_y = pos_y - decoder->v_offset;				      \
    }									      \
    xy_half = ((pos_y & 1) << 1) | (pos_x & 1);				      \
    offset = (pos_x >> 1) + (pos_y & ~1) * decoder->stride;		      \
    table[xy_half] (decoder->dest[0] + decoder->offset,			      \
		    ref[0] + offset, 2 * decoder->stride, 8);		      \
    table[xy_half] (decoder->dest[0] + decoder->stride + decoder->offset,     \
		    ref[0] + decoder->stride + offset,			      \
		    2 * decoder->stride, 8);				      \
    offset = (offset + (motion_x & (motion_x < 0))) >> 1;		      \
    motion_x /= 2;							      \
    xy_half = ((pos_y & 1) << 1) | (motion_x & 1);			      \
    table[4+xy_half] (decoder->dest[1] + (decoder->offset >> 1),	      \
		      ref[1] + offset, 2 * decoder->uv_stride, 8);	      \
    table[4+xy_half] (decoder->dest[1] + decoder->uv_stride +		      \
		      (decoder->offset >> 1),				      \
		      ref[1] + decoder->uv_stride + offset,		      \
		      2 * decoder->uv_stride, 8);			      \
    table[4+xy_half] (decoder->dest[2] + (decoder->offset >> 1),	      \
		      ref[2] + offset, 2 * decoder->uv_stride, 8);	      \
    table[4+xy_half] (decoder->dest[2] + decoder->uv_stride +		      \
		      (decoder->offset >> 1),				      \
		      ref[2] + decoder->uv_stride + offset,		      \
		      2 * decoder->uv_stride, 8)

#define MOTION_ZERO_422(table,ref)					      \
    offset = decoder->offset + decoder->v_offset * decoder->stride;	      \
    table[0] (decoder->dest[0] + decoder->offset,			      \
	      ref[0] + offset, decoder->stride, 16);			      \
    offset >>= 1;							      \
    table[4] (decoder->dest[1] + (decoder->offset >> 1),		      \
	      ref[1] + offset, decoder->uv_stride, 16);			      \
    table[4] (decoder->dest[2] + (decoder->offset >> 1),		      \
	      ref[2] + offset, decoder->uv_stride, 16)

#define MOTION_444(table,ref,motion_x,motion_y,size,y)			      \
    pos_x = 2 * decoder->offset + motion_x;				      \
    pos_y = 2 * decoder->v_offset + motion_y + 2 * y;			      \
    if (unlikely (pos_x > decoder->limit_x)) {				      \
	pos_x = ((int)pos_x < 0) ? 0 : decoder->limit_x;		      \
	motion_x = pos_x - 2 * decoder->offset;				      \
    }									      \
    if (unlikely (pos_y > decoder->limit_y_ ## size)) {			      \
	pos_y = ((int)pos_y < 0) ? 0 : decoder->limit_y_ ## size;	      \
	motion_y = pos_y - 2 * decoder->v_offset - 2 * y;		      \
    }									      \
    xy_half = ((pos_y & 1) << 1) | (pos_x & 1);				      \
    offset = (pos_x >> 1) + (pos_y >> 1) * decoder->stride;		      \
    table[xy_half] (decoder->dest[0] + y * decoder->stride + decoder->offset, \
		    ref[0] + offset, decoder->stride, size);		      \
    table[xy_half] (decoder->dest[1] + y * decoder->stride + decoder->offset, \
		    ref[1] + offset, decoder->stride, size);		      \
    table[xy_half] (decoder->dest[2] + y * decoder->stride + decoder->offset, \
		    ref[2] + offset, decoder->stride, size)

#define MOTION_FIELD_444(table,ref,motion_x,motion_y,dest_field,op,src_field) \
    pos_x = 2 * decoder->offset + motion_x;				      \
    pos_y = decoder->v_offset + motion_y;				      \
    if (unlikely (pos_x > decoder->limit_x)) {				      \
	pos_x = ((int)pos_x < 0) ? 0 : decoder->limit_x;		      \
	motion_x = pos_x - 2 * decoder->offset;				      \
    }									      \
    if (unlikely (pos_y > decoder->limit_y)) {				      \
	pos_y = ((int)pos_y < 0) ? 0 : decoder->limit_y;		      \
	motion_y = pos_y - decoder->v_offset;				      \
    }									      \
    xy_half = ((pos_y & 1) << 1) | (pos_x & 1);				      \
    offset = (pos_x >> 1) + ((pos_y op) + src_field) * decoder->stride;	      \
    table[xy_half] (decoder->dest[0] + dest_field * decoder->stride +	      \
		    decoder->offset, ref[0] + offset,			      \
		    2 * decoder->stride, 8);				      \
    table[xy_half] (decoder->dest[1] + dest_field * decoder->stride +	      \
		    decoder->offset, ref[1] + offset,			      \
		    2 * decoder->stride, 8);				      \
    table[xy_half] (decoder->dest[2] + dest_field * decoder->stride +	      \
		    decoder->offset, ref[2] + offset,			      \
		    2 * decoder->stride, 8)

#define MOTION_DMV_444(table,ref,motion_x,motion_y)			      \
    pos_x = 2 * decoder->offset + motion_x;				      \
    pos_y = decoder->v_offset + motion_y;				      \
    if (unlikely (pos_x > decoder->limit_x)) {				      \
	pos_x = ((int)pos_x < 0) ? 0 : decoder->limit_x;		      \
	motion_x = pos_x - 2 * decoder->offset;				      \
    }									      \
    if (unlikely (pos_y > decoder->limit_y)) {				      \
	pos_y = ((int)pos_y < 0) ? 0 : decoder->limit_y;		      \
	motion_y = pos_y - decoder->v_offset;				      \
    }									      \
    xy_half = ((pos_y & 1) << 1) | (pos_x & 1);				      \
    offset = (pos_x >> 1) + (pos_y & ~1) * decoder->stride;		      \
    table[xy_half] (decoder->dest[0] + decoder->offset,			      \
		    ref[0] + offset, 2 * decoder->stride, 8);		      \
    table[xy_half] (decoder->dest[0] + decoder->stride + decoder->offset,     \
		    ref[0] + decoder->stride + offset,			      \
		    2 * decoder->stride, 8);				      \
    table[xy_half] (decoder->dest[1] + decoder->offset,			      \
		    ref[1] + offset, 2 * decoder->stride, 8);		      \
    table[xy_half] (decoder->dest[1] + decoder->stride + decoder->offset,     \
		    ref[1] + decoder->stride + offset,			      \
		    2 * decoder->stride, 8);				      \
    table[xy_half] (decoder->dest[2] + decoder->offset,			      \
		    ref[2] + offset, 2 * decoder->stride, 8);		      \
    table[xy_half] (decoder->dest[2] + decoder->stride + decoder->offset,     \
		    ref[2] + decoder->stride + offset,			      \
		    2 * decoder->stride, 8)

#define MOTION_ZERO_444(table,ref)					      \
    offset = decoder->offset + decoder->v_offset * decoder->stride;	      \
    table[0] (decoder->dest[0] + decoder->offset,			      \
	      ref[0] + offset, decoder->stride, 16);			      \
    table[4] (decoder->dest[1] + decoder->offset,			      \
	      ref[1] + offset, decoder->stride, 16);			      \
    table[4] (decoder->dest[2] + decoder->offset,			      \
	      ref[2] + offset, decoder->stride, 16)

#define bit_buf (decoder->bitstream_buf)
#define bits (decoder->bitstream_bits)
#define bit_ptr (decoder->bitstream_ptr)

static void motion_mp1 (mpeg2_decoder_t * const decoder,
			motion_t * const motion,
			mpeg2_mc_fct * const * const table)
{
    int motion_x, motion_y;
    unsigned int pos_x, pos_y, xy_half, offset;

    NEEDBITS (bit_buf, bits, bit_ptr);
    motion_x = (motion->pmv[0][0] +
		(get_motion_delta (decoder,
				   motion->f_code[0]) << motion->f_code[1]));
    motion_x = bound_motion_vector (motion_x,
				    motion->f_code[0] + motion->f_code[1]);
    motion->pmv[0][0] = motion_x;

    NEEDBITS (bit_buf, bits, bit_ptr);
    motion_y = (motion->pmv[0][1] +
		(get_motion_delta (decoder,
				   motion->f_code[0]) << motion->f_code[1]));
    motion_y = bound_motion_vector (motion_y,
				    motion->f_code[0] + motion->f_code[1]);
    motion->pmv[0][1] = motion_y;

    MOTION_420 (table, motion->ref[0], motion_x, motion_y, 16, 0);
}

#define MOTION_FUNCTIONS(FORMAT,MOTION,MOTION_FIELD,MOTION_DMV,MOTION_ZERO)   \
									      \
static void motion_fr_frame_##FORMAT (mpeg2_decoder_t * const decoder,	      \
				      motion_t * const motion,		      \
				      mpeg2_mc_fct * const * const table)     \
{									      \
    int motion_x, motion_y;						      \
    unsigned int pos_x, pos_y, xy_half, offset;				      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    motion_x = motion->pmv[0][0] + get_motion_delta (decoder,		      \
						     motion->f_code[0]);      \
    motion_x = bound_motion_vector (motion_x, motion->f_code[0]);	      \
    motion->pmv[1][0] = motion->pmv[0][0] = motion_x;			      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    motion_y = motion->pmv[0][1] + get_motion_delta (decoder,		      \
						     motion->f_code[1]);      \
    motion_y = bound_motion_vector (motion_y, motion->f_code[1]);	      \
    motion->pmv[1][1] = motion->pmv[0][1] = motion_y;			      \
									      \
    MOTION (table, motion->ref[0], motion_x, motion_y, 16, 0);		      \
}									      \
									      \
static void motion_fr_field_##FORMAT (mpeg2_decoder_t * const decoder,	      \
				      motion_t * const motion,		      \
				      mpeg2_mc_fct * const * const table)     \
{									      \
    int motion_x, motion_y, field;					      \
    unsigned int pos_x, pos_y, xy_half, offset;				      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    field = UBITS (bit_buf, 1);						      \
    DUMPBITS (bit_buf, bits, 1);					      \
									      \
    motion_x = motion->pmv[0][0] + get_motion_delta (decoder,		      \
						     motion->f_code[0]);      \
    motion_x = bound_motion_vector (motion_x, motion->f_code[0]);	      \
    motion->pmv[0][0] = motion_x;					      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    motion_y = ((motion->pmv[0][1] >> 1) +				      \
		get_motion_delta (decoder, motion->f_code[1]));		      \
    /* motion_y = bound_motion_vector (motion_y, motion->f_code[1]); */	      \
    motion->pmv[0][1] = motion_y << 1;					      \
									      \
    MOTION_FIELD (table, motion->ref[0], motion_x, motion_y, 0, & ~1, field); \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    field = UBITS (bit_buf, 1);						      \
    DUMPBITS (bit_buf, bits, 1);					      \
									      \
    motion_x = motion->pmv[1][0] + get_motion_delta (decoder,		      \
						     motion->f_code[0]);      \
    motion_x = bound_motion_vector (motion_x, motion->f_code[0]);	      \
    motion->pmv[1][0] = motion_x;					      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    motion_y = ((motion->pmv[1][1] >> 1) +				      \
		get_motion_delta (decoder, motion->f_code[1]));		      \
    /* motion_y = bound_motion_vector (motion_y, motion->f_code[1]); */	      \
    motion->pmv[1][1] = motion_y << 1;					      \
									      \
    MOTION_FIELD (table, motion->ref[0], motion_x, motion_y, 1, & ~1, field); \
}									      \
									      \
static void motion_fr_dmv_##FORMAT (mpeg2_decoder_t * const decoder,	      \
				    motion_t * const motion,		      \
				    mpeg2_mc_fct * const * const table)	      \
{									      \
    int motion_x, motion_y, dmv_x, dmv_y, m, other_x, other_y;		      \
    unsigned int pos_x, pos_y, xy_half, offset;				      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    motion_x = motion->pmv[0][0] + get_motion_delta (decoder,		      \
						     motion->f_code[0]);      \
    motion_x = bound_motion_vector (motion_x, motion->f_code[0]);	      \
    motion->pmv[1][0] = motion->pmv[0][0] = motion_x;			      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    dmv_x = get_dmv (decoder);						      \
									      \
    motion_y = ((motion->pmv[0][1] >> 1) +				      \
		get_motion_delta (decoder, motion->f_code[1]));		      \
    /* motion_y = bound_motion_vector (motion_y, motion->f_code[1]); */	      \
    motion->pmv[1][1] = motion->pmv[0][1] = motion_y << 1;		      \
    dmv_y = get_dmv (decoder);						      \
									      \
    m = decoder->top_field_first ? 1 : 3;				      \
    other_x = ((motion_x * m + (motion_x > 0)) >> 1) + dmv_x;		      \
    other_y = ((motion_y * m + (motion_y > 0)) >> 1) + dmv_y - 1;	      \
    MOTION_FIELD (mpeg2_mc.put, motion->ref[0], other_x, other_y, 0, | 1, 0); \
									      \
    m = decoder->top_field_first ? 3 : 1;				      \
    other_x = ((motion_x * m + (motion_x > 0)) >> 1) + dmv_x;		      \
    other_y = ((motion_y * m + (motion_y > 0)) >> 1) + dmv_y + 1;	      \
    MOTION_FIELD (mpeg2_mc.put, motion->ref[0], other_x, other_y, 1, & ~1, 0);\
									      \
    MOTION_DMV (mpeg2_mc.avg, motion->ref[0], motion_x, motion_y);	      \
}									      \
									      \
static void motion_reuse_##FORMAT (mpeg2_decoder_t * const decoder,	      \
				   motion_t * const motion,		      \
				   mpeg2_mc_fct * const * const table)	      \
{									      \
    int motion_x, motion_y;						      \
    unsigned int pos_x, pos_y, xy_half, offset;				      \
									      \
    motion_x = motion->pmv[0][0];					      \
    motion_y = motion->pmv[0][1];					      \
									      \
    MOTION (table, motion->ref[0], motion_x, motion_y, 16, 0);		      \
}									      \
									      \
static void motion_zero_##FORMAT (mpeg2_decoder_t * const decoder,	      \
				  motion_t * const motion,		      \
				  mpeg2_mc_fct * const * const table)	      \
{									      \
    unsigned int offset;						      \
									      \
    motion->pmv[0][0] = motion->pmv[0][1] = 0;				      \
    motion->pmv[1][0] = motion->pmv[1][1] = 0;				      \
									      \
    MOTION_ZERO (table, motion->ref[0]);				      \
}									      \
									      \
static void motion_fi_field_##FORMAT (mpeg2_decoder_t * const decoder,	      \
				      motion_t * const motion,		      \
				      mpeg2_mc_fct * const * const table)     \
{									      \
    int motion_x, motion_y;						      \
    uint8_t ** ref_field;						      \
    unsigned int pos_x, pos_y, xy_half, offset;				      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    ref_field = motion->ref2[UBITS (bit_buf, 1)];			      \
    DUMPBITS (bit_buf, bits, 1);					      \
									      \
    motion_x = motion->pmv[0][0] + get_motion_delta (decoder,		      \
						     motion->f_code[0]);      \
    motion_x = bound_motion_vector (motion_x, motion->f_code[0]);	      \
    motion->pmv[1][0] = motion->pmv[0][0] = motion_x;			      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    motion_y = motion->pmv[0][1] + get_motion_delta (decoder,		      \
						     motion->f_code[1]);      \
    motion_y = bound_motion_vector (motion_y, motion->f_code[1]);	      \
    motion->pmv[1][1] = motion->pmv[0][1] = motion_y;			      \
									      \
    MOTION (table, ref_field, motion_x, motion_y, 16, 0);		      \
}									      \
									      \
static void motion_fi_16x8_##FORMAT (mpeg2_decoder_t * const decoder,	      \
				     motion_t * const motion,		      \
				     mpeg2_mc_fct * const * const table)      \
{									      \
    int motion_x, motion_y;						      \
    uint8_t ** ref_field;						      \
    unsigned int pos_x, pos_y, xy_half, offset;				      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    ref_field = motion->ref2[UBITS (bit_buf, 1)];			      \
    DUMPBITS (bit_buf, bits, 1);					      \
									      \
    motion_x = motion->pmv[0][0] + get_motion_delta (decoder,		      \
						     motion->f_code[0]);      \
    motion_x = bound_motion_vector (motion_x, motion->f_code[0]);	      \
    motion->pmv[0][0] = motion_x;					      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    motion_y = motion->pmv[0][1] + get_motion_delta (decoder,		      \
						     motion->f_code[1]);      \
    motion_y = bound_motion_vector (motion_y, motion->f_code[1]);	      \
    motion->pmv[0][1] = motion_y;					      \
									      \
    MOTION (table, ref_field, motion_x, motion_y, 8, 0);		      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    ref_field = motion->ref2[UBITS (bit_buf, 1)];			      \
    DUMPBITS (bit_buf, bits, 1);					      \
									      \
    motion_x = motion->pmv[1][0] + get_motion_delta (decoder,		      \
						     motion->f_code[0]);      \
    motion_x = bound_motion_vector (motion_x, motion->f_code[0]);	      \
    motion->pmv[1][0] = motion_x;					      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    motion_y = motion->pmv[1][1] + get_motion_delta (decoder,		      \
						     motion->f_code[1]);      \
    motion_y = bound_motion_vector (motion_y, motion->f_code[1]);	      \
    motion->pmv[1][1] = motion_y;					      \
									      \
    MOTION (table, ref_field, motion_x, motion_y, 8, 8);		      \
}									      \
									      \
static void motion_fi_dmv_##FORMAT (mpeg2_decoder_t * const decoder,	      \
				    motion_t * const motion,		      \
				    mpeg2_mc_fct * const * const table)	      \
{									      \
    int motion_x, motion_y, other_x, other_y;				      \
    unsigned int pos_x, pos_y, xy_half, offset;				      \
									      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    motion_x = motion->pmv[0][0] + get_motion_delta (decoder,		      \
						     motion->f_code[0]);      \
    motion_x = bound_motion_vector (motion_x, motion->f_code[0]);	      \
    motion->pmv[1][0] = motion->pmv[0][0] = motion_x;			      \
    NEEDBITS (bit_buf, bits, bit_ptr);					      \
    other_x = ((motion_x + (motion_x > 0)) >> 1) + get_dmv (decoder);	      \
									      \
    motion_y = motion->pmv[0][1] + get_motion_delta (decoder,		      \
						     motion->f_code[1]);      \
    motion_y = bound_motion_vector (motion_y, motion->f_code[1]);	      \
    motion->pmv[1][1] = motion->pmv[0][1] = motion_y;			      \
    other_y = (((motion_y + (motion_y > 0)) >> 1) + get_dmv (decoder) +	      \
	       decoder->dmv_offset);					      \
									      \
    MOTION (mpeg2_mc.put, motion->ref[0], motion_x, motion_y, 16, 0);	      \
    MOTION (mpeg2_mc.avg, motion->ref[1], other_x, other_y, 16, 0);	      \
}									      \

MOTION_FUNCTIONS (420, MOTION_420, MOTION_FIELD_420, MOTION_DMV_420,
		  MOTION_ZERO_420)
MOTION_FUNCTIONS (422, MOTION_422, MOTION_FIELD_422, MOTION_DMV_422,
		  MOTION_ZERO_422)
MOTION_FUNCTIONS (444, MOTION_444, MOTION_FIELD_444, MOTION_DMV_444,
		  MOTION_ZERO_444)

/* like motion_frame, but parsing without actual motion compensation */
static void motion_fr_conceal (mpeg2_decoder_t * const decoder)
{
    int tmp;

    NEEDBITS (bit_buf, bits, bit_ptr);
    tmp = (decoder->f_motion.pmv[0][0] +
	   get_motion_delta (decoder, decoder->f_motion.f_code[0]));
    tmp = bound_motion_vector (tmp, decoder->f_motion.f_code[0]);
    decoder->f_motion.pmv[1][0] = decoder->f_motion.pmv[0][0] = tmp;

    NEEDBITS (bit_buf, bits, bit_ptr);
    tmp = (decoder->f_motion.pmv[0][1] +
	   get_motion_delta (decoder, decoder->f_motion.f_code[1]));
    tmp = bound_motion_vector (tmp, decoder->f_motion.f_code[1]);
    decoder->f_motion.pmv[1][1] = decoder->f_motion.pmv[0][1] = tmp;

    DUMPBITS (bit_buf, bits, 1); /* remove marker_bit */
}

static void motion_fi_conceal (mpeg2_decoder_t * const decoder)
{
    int tmp;

    NEEDBITS (bit_buf, bits, bit_ptr);
    DUMPBITS (bit_buf, bits, 1); /* remove field_select */

    tmp = (decoder->f_motion.pmv[0][0] +
	   get_motion_delta (decoder, decoder->f_motion.f_code[0]));
    tmp = bound_motion_vector (tmp, decoder->f_motion.f_code[0]);
    decoder->f_motion.pmv[1][0] = decoder->f_motion.pmv[0][0] = tmp;

    NEEDBITS (bit_buf, bits, bit_ptr);
    tmp = (decoder->f_motion.pmv[0][1] +
	   get_motion_delta (decoder, decoder->f_motion.f_code[1]));
    tmp = bound_motion_vector (tmp, decoder->f_motion.f_code[1]);
    decoder->f_motion.pmv[1][1] = decoder->f_motion.pmv[0][1] = tmp;

    DUMPBITS (bit_buf, bits, 1); /* remove marker_bit */
}

#undef bit_buf
#undef bits
#undef bit_ptr

#define MOTION_CALL(routine,direction)				\
do {								\
    if ((direction) & MACROBLOCK_MOTION_FORWARD)		\
	routine (decoder, &(decoder->f_motion), mpeg2_mc.put);	\
    if ((direction) & MACROBLOCK_MOTION_BACKWARD)		\
	routine (decoder, &(decoder->b_motion),			\
		 ((direction) & MACROBLOCK_MOTION_FORWARD ?	\
		  mpeg2_mc.avg : mpeg2_mc.put));		\
} while (0)

#define NEXT_MACROBLOCK							\
do {									\
    if(decoder->quant_store) {						\
	if (decoder->picture_structure == TOP_FIELD)			\
	    decoder->quant_store[2 * decoder->quant_stride		\
				 * (decoder->v_offset >> 4)		\
				 + (decoder->offset >> 4)]		\
		= decoder->quantizer_scale;				\
	else if (decoder->picture_structure == BOTTOM_FIELD)		\
	    decoder->quant_store[2 * decoder->quant_stride		\
				 * (decoder->v_offset >> 4)		\
				 + decoder->quant_stride		\
				 + (decoder->offset >> 4)]		\
		= decoder->quantizer_scale;				\
	else								\
	    decoder->quant_store[decoder->quant_stride			\
				 * (decoder->v_offset >> 4)		\
				 + (decoder->offset >> 4)]		\
		= decoder->quantizer_scale;				\
    }									\
    decoder->offset += 16;						\
    if (decoder->offset == decoder->width) {				\
	do { /* just so we can use the break statement */		\
	    if (decoder->convert) {					\
		decoder->convert (decoder->convert_id, decoder->dest,	\
				  decoder->v_offset);			\
		if (decoder->coding_type == B_TYPE)			\
		    break;						\
	    }								\
	    decoder->dest[0] += decoder->slice_stride;			\
	    decoder->dest[1] += decoder->slice_uv_stride;		\
	    decoder->dest[2] += decoder->slice_uv_stride;		\
	} while (0);							\
	decoder->v_offset += 16;					\
	if (decoder->v_offset > decoder->limit_y) {			\
	    if (mpeg2_cpu_state_restore)				\
		mpeg2_cpu_state_restore (&cpu_state);			\
	    return;							\
	}								\
	decoder->offset = 0;						\
    }									\
} while (0)

/**
 * Dummy motion decoding function, to avoid calling NULL in
 * case of malformed streams.
 */
static void motion_dummy (mpeg2_decoder_t * const decoder,
                          motion_t * const motion,
                          mpeg2_mc_fct * const * const table)
{
}

void mpeg2_init_fbuf (mpeg2_decoder_t * decoder, uint8_t * current_fbuf[3],
		      uint8_t * forward_fbuf[3], uint8_t * backward_fbuf[3])
{
    int offset, stride, height, bottom_field;

    stride = decoder->stride_frame;
    bottom_field = (decoder->picture_structure == BOTTOM_FIELD);
    offset = bottom_field ? stride : 0;
    height = decoder->height;

    decoder->picture_dest[0] = current_fbuf[0] + offset;
    decoder->picture_dest[1] = current_fbuf[1] + (offset >> 1);
    decoder->picture_dest[2] = current_fbuf[2] + (offset >> 1);

    decoder->f_motion.ref[0][0] = forward_fbuf[0] + offset;
    decoder->f_motion.ref[0][1] = forward_fbuf[1] + (offset >> 1);
    decoder->f_motion.ref[0][2] = forward_fbuf[2] + (offset >> 1);

    decoder->b_motion.ref[0][0] = backward_fbuf[0] + offset;
    decoder->b_motion.ref[0][1] = backward_fbuf[1] + (offset >> 1);
    decoder->b_motion.ref[0][2] = backward_fbuf[2] + (offset >> 1);

    if (decoder->picture_structure != FRAME_PICTURE) {
	decoder->dmv_offset = bottom_field ? 1 : -1;
	decoder->f_motion.ref2[0] = decoder->f_motion.ref[bottom_field];
	decoder->f_motion.ref2[1] = decoder->f_motion.ref[!bottom_field];
	decoder->b_motion.ref2[0] = decoder->b_motion.ref[bottom_field];
	decoder->b_motion.ref2[1] = decoder->b_motion.ref[!bottom_field];
	offset = stride - offset;

	if (decoder->second_field && (decoder->coding_type != B_TYPE))
	    forward_fbuf = current_fbuf;

	decoder->f_motion.ref[1][0] = forward_fbuf[0] + offset;
	decoder->f_motion.ref[1][1] = forward_fbuf[1] + (offset >> 1);
	decoder->f_motion.ref[1][2] = forward_fbuf[2] + (offset >> 1);

	decoder->b_motion.ref[1][0] = backward_fbuf[0] + offset;
	decoder->b_motion.ref[1][1] = backward_fbuf[1] + (offset >> 1);
	decoder->b_motion.ref[1][2] = backward_fbuf[2] + (offset >> 1);

	stride <<= 1;
	height >>= 1;
    }

    decoder->stride = stride;
    decoder->uv_stride = stride >> 1;
    decoder->slice_stride = 16 * stride;
    decoder->slice_uv_stride =
	decoder->slice_stride >> (2 - decoder->chroma_format);
    decoder->limit_x = 2 * decoder->width - 32;
    decoder->limit_y_16 = 2 * height - 32;
    decoder->limit_y_8 = 2 * height - 16;
    decoder->limit_y = height - 16;

    if (decoder->mpeg1) {
	decoder->motion_parser[0] = motion_zero_420;
        decoder->motion_parser[MC_FIELD] = motion_dummy;
 	decoder->motion_parser[MC_FRAME] = motion_mp1;
        decoder->motion_parser[MC_DMV] = motion_dummy;
	decoder->motion_parser[4] = motion_reuse_420;
    } else if (decoder->picture_structure == FRAME_PICTURE) {
	if (decoder->chroma_format == 0) {
	    decoder->motion_parser[0] = motion_zero_420;
	    decoder->motion_parser[MC_FIELD] = motion_fr_field_420;
	    decoder->motion_parser[MC_FRAME] = motion_fr_frame_420;
	    decoder->motion_parser[MC_DMV] = motion_fr_dmv_420;
	    decoder->motion_parser[4] = motion_reuse_420;
	} else if (decoder->chroma_format == 1) {
	    decoder->motion_parser[0] = motion_zero_422;
	    decoder->motion_parser[MC_FIELD] = motion_fr_field_422;
	    decoder->motion_parser[MC_FRAME] = motion_fr_frame_422;
	    decoder->motion_parser[MC_DMV] = motion_fr_dmv_422;
	    decoder->motion_parser[4] = motion_reuse_422;
	} else {
	    decoder->motion_parser[0] = motion_zero_444;
	    decoder->motion_parser[MC_FIELD] = motion_fr_field_444;
	    decoder->motion_parser[MC_FRAME] = motion_fr_frame_444;
	    decoder->motion_parser[MC_DMV] = motion_fr_dmv_444;
	    decoder->motion_parser[4] = motion_reuse_444;
	}
    } else {
	if (decoder->chroma_format == 0) {
	    decoder->motion_parser[0] = motion_zero_420;
	    decoder->motion_parser[MC_FIELD] = motion_fi_field_420;
	    decoder->motion_parser[MC_16X8] = motion_fi_16x8_420;
	    decoder->motion_parser[MC_DMV] = motion_fi_dmv_420;
	    decoder->motion_parser[4] = motion_reuse_420;
	} else if (decoder->chroma_format == 1) {
	    decoder->motion_parser[0] = motion_zero_422;
	    decoder->motion_parser[MC_FIELD] = motion_fi_field_422;
	    decoder->motion_parser[MC_16X8] = motion_fi_16x8_422;
	    decoder->motion_parser[MC_DMV] = motion_fi_dmv_422;
	    decoder->motion_parser[4] = motion_reuse_422;
	} else {
	    decoder->motion_parser[0] = motion_zero_444;
	    decoder->motion_parser[MC_FIELD] = motion_fi_field_444;
	    decoder->motion_parser[MC_16X8] = motion_fi_16x8_444;
	    decoder->motion_parser[MC_DMV] = motion_fi_dmv_444;
	    decoder->motion_parser[4] = motion_reuse_444;
	}
    }
}

static inline int slice_init (mpeg2_decoder_t * const decoder, int code)
{
#define bit_buf (decoder->bitstream_buf)
#define bits (decoder->bitstream_bits)
#define bit_ptr (decoder->bitstream_ptr)
    int offset;
    const MBAtab * mba;

    decoder->dc_dct_pred[0] = decoder->dc_dct_pred[1] =
	decoder->dc_dct_pred[2] = 16384;

    decoder->f_motion.pmv[0][0] = decoder->f_motion.pmv[0][1] = 0;
    decoder->f_motion.pmv[1][0] = decoder->f_motion.pmv[1][1] = 0;
    decoder->b_motion.pmv[0][0] = decoder->b_motion.pmv[0][1] = 0;
    decoder->b_motion.pmv[1][0] = decoder->b_motion.pmv[1][1] = 0;

    if (decoder->vertical_position_extension) {
	code += UBITS (bit_buf, 3) << 7;
	DUMPBITS (bit_buf, bits, 3);
    }
    decoder->v_offset = (code - 1) * 16;
    offset = 0;
    if (!(decoder->convert) || decoder->coding_type != B_TYPE)
	offset = (code - 1) * decoder->slice_stride;

    decoder->dest[0] = decoder->picture_dest[0] + offset;
    offset >>= (2 - decoder->chroma_format);
    decoder->dest[1] = decoder->picture_dest[1] + offset;
    decoder->dest[2] = decoder->picture_dest[2] + offset;

    get_quantizer_scale (decoder);

    /* ignore intra_slice and all the extra data */
    while (bit_buf & 0x80000000) {
	DUMPBITS (bit_buf, bits, 9);
	NEEDBITS (bit_buf, bits, bit_ptr);
    }

    /* decode initial macroblock address increment */
    offset = 0;
    while (1) {
	if (bit_buf >= 0x08000000) {
	    mba = MBA_5 + (UBITS (bit_buf, 6) - 2);
	    break;
	} else if (bit_buf >= 0x01800000) {
	    mba = MBA_11 + (UBITS (bit_buf, 12) - 24);
	    break;
	} else switch (UBITS (bit_buf, 12)) {
	case 8:		/* macroblock_escape */
	    offset += 33;
	    DUMPBITS (bit_buf, bits, 11);
	    NEEDBITS (bit_buf, bits, bit_ptr);
	    continue;
	case 15:	/* macroblock_stuffing (MPEG1 only) */
	    bit_buf &= 0xfffff;
	    DUMPBITS (bit_buf, bits, 11);
	    NEEDBITS (bit_buf, bits, bit_ptr);
	    continue;
	default:	/* error */
	    return 1;
	}
    }
    DUMPBITS (bit_buf, bits, mba->len + 1);
    decoder->offset = (offset + mba->mba) << 4;

    while (decoder->offset - decoder->width >= 0) {
	decoder->offset -= decoder->width;
	if (!(decoder->convert) || decoder->coding_type != B_TYPE) {
	    decoder->dest[0] += decoder->slice_stride;
	    decoder->dest[1] += decoder->slice_uv_stride;
	    decoder->dest[2] += decoder->slice_uv_stride;
	}
	decoder->v_offset += 16;
    }
    if (decoder->v_offset > decoder->limit_y)
	return 1;

    return 0;
#undef bit_buf
#undef bits
#undef bit_ptr
}

void mpeg2_slice (mpeg2_decoder_t * const decoder, const int code,
		  const uint8_t * const buffer)
{
#define bit_buf (decoder->bitstream_buf)
#define bits (decoder->bitstream_bits)
#define bit_ptr (decoder->bitstream_ptr)
    cpu_state_t cpu_state;

    bitstream_init (decoder, buffer);

    if (slice_init (decoder, code))
	return;

    if (mpeg2_cpu_state_save)
	mpeg2_cpu_state_save (&cpu_state);

    while (1) {
	int macroblock_modes;
	int mba_inc;
	const MBAtab * mba;

	NEEDBITS (bit_buf, bits, bit_ptr);

	macroblock_modes = get_macroblock_modes (decoder);

	/* maybe integrate MACROBLOCK_QUANT test into get_macroblock_modes ? */
	if (macroblock_modes & MACROBLOCK_QUANT)
	    get_quantizer_scale (decoder);

	if (macroblock_modes & MACROBLOCK_INTRA) {

	    int DCT_offset, DCT_stride;
	    int offset;
	    uint8_t * dest_y;

	    if (decoder->concealment_motion_vectors) {
		if (decoder->picture_structure == FRAME_PICTURE)
		    motion_fr_conceal (decoder);
		else
		    motion_fi_conceal (decoder);
	    } else {
		decoder->f_motion.pmv[0][0] = decoder->f_motion.pmv[0][1] = 0;
		decoder->f_motion.pmv[1][0] = decoder->f_motion.pmv[1][1] = 0;
		decoder->b_motion.pmv[0][0] = decoder->b_motion.pmv[0][1] = 0;
		decoder->b_motion.pmv[1][0] = decoder->b_motion.pmv[1][1] = 0;
	    }

	    if (macroblock_modes & DCT_TYPE_INTERLACED) {
		DCT_offset = decoder->stride;
		DCT_stride = decoder->stride * 2;
	    } else {
		DCT_offset = decoder->stride * 8;
		DCT_stride = decoder->stride;
	    }

	    offset = decoder->offset;
	    dest_y = decoder->dest[0] + offset;
	    slice_intra_DCT (decoder, 0, dest_y, DCT_stride);
	    slice_intra_DCT (decoder, 0, dest_y + 8, DCT_stride);
	    slice_intra_DCT (decoder, 0, dest_y + DCT_offset, DCT_stride);
	    slice_intra_DCT (decoder, 0, dest_y + DCT_offset + 8, DCT_stride);
	    if (likely (decoder->chroma_format == 0)) {
		slice_intra_DCT (decoder, 1, decoder->dest[1] + (offset >> 1),
				 decoder->uv_stride);
		slice_intra_DCT (decoder, 2, decoder->dest[2] + (offset >> 1),
				 decoder->uv_stride);
		if (decoder->coding_type == D_TYPE) {
		    NEEDBITS (bit_buf, bits, bit_ptr);
		    DUMPBITS (bit_buf, bits, 1);
		}
	    } else if (likely (decoder->chroma_format == 1)) {
		uint8_t * dest_u = decoder->dest[1] + (offset >> 1);
		uint8_t * dest_v = decoder->dest[2] + (offset >> 1);
		DCT_stride >>= 1;
		DCT_offset >>= 1;
		slice_intra_DCT (decoder, 1, dest_u, DCT_stride);
		slice_intra_DCT (decoder, 2, dest_v, DCT_stride);
		slice_intra_DCT (decoder, 1, dest_u + DCT_offset, DCT_stride);
		slice_intra_DCT (decoder, 2, dest_v + DCT_offset, DCT_stride);
	    } else {
		uint8_t * dest_u = decoder->dest[1] + offset;
		uint8_t * dest_v = decoder->dest[2] + offset;
		slice_intra_DCT (decoder, 1, dest_u, DCT_stride);
		slice_intra_DCT (decoder, 2, dest_v, DCT_stride);
		slice_intra_DCT (decoder, 1, dest_u + DCT_offset, DCT_stride);
		slice_intra_DCT (decoder, 2, dest_v + DCT_offset, DCT_stride);
		slice_intra_DCT (decoder, 1, dest_u + 8, DCT_stride);
		slice_intra_DCT (decoder, 2, dest_v + 8, DCT_stride);
		slice_intra_DCT (decoder, 1, dest_u + DCT_offset + 8,
				 DCT_stride);
		slice_intra_DCT (decoder, 2, dest_v + DCT_offset + 8,
				 DCT_stride);
	    }
	} else {

	    motion_parser_t * parser;

	    if (   ((macroblock_modes >> MOTION_TYPE_SHIFT) < 0)
                || ((macroblock_modes >> MOTION_TYPE_SHIFT) >=
                    (int)(sizeof(decoder->motion_parser) 
                          / sizeof(decoder->motion_parser[0])))
	       ) {
		break; // Illegal !
	    }

	    parser =
		decoder->motion_parser[macroblock_modes >> MOTION_TYPE_SHIFT];
	    MOTION_CALL (parser, macroblock_modes);

	    if (macroblock_modes & MACROBLOCK_PATTERN) {
		int coded_block_pattern;
		int DCT_offset, DCT_stride;

		if (macroblock_modes & DCT_TYPE_INTERLACED) {
		    DCT_offset = decoder->stride;
		    DCT_stride = decoder->stride * 2;
		} else {
		    DCT_offset = decoder->stride * 8;
		    DCT_stride = decoder->stride;
		}

		coded_block_pattern = get_coded_block_pattern (decoder);

		if (likely (decoder->chroma_format == 0)) {
		    int offset = decoder->offset;
		    uint8_t * dest_y = decoder->dest[0] + offset;
		    if (coded_block_pattern & 1)
			slice_non_intra_DCT (decoder, 0, dest_y, DCT_stride);
		    if (coded_block_pattern & 2)
			slice_non_intra_DCT (decoder, 0, dest_y + 8,
					     DCT_stride);
		    if (coded_block_pattern & 4)
			slice_non_intra_DCT (decoder, 0, dest_y + DCT_offset,
					     DCT_stride);
		    if (coded_block_pattern & 8)
			slice_non_intra_DCT (decoder, 0,
					     dest_y + DCT_offset + 8,
					     DCT_stride);
		    if (coded_block_pattern & 16)
			slice_non_intra_DCT (decoder, 1,
					     decoder->dest[1] + (offset >> 1),
					     decoder->uv_stride);
		    if (coded_block_pattern & 32)
			slice_non_intra_DCT (decoder, 2,
					     decoder->dest[2] + (offset >> 1),
					     decoder->uv_stride);
		} else if (likely (decoder->chroma_format == 1)) {
		    int offset;
		    uint8_t * dest_y;

		    coded_block_pattern |= bit_buf & (3 << 30);
		    DUMPBITS (bit_buf, bits, 2);

		    offset = decoder->offset;
		    dest_y = decoder->dest[0] + offset;
		    if (coded_block_pattern & 1)
			slice_non_intra_DCT (decoder, 0, dest_y, DCT_stride);
		    if (coded_block_pattern & 2)
			slice_non_intra_DCT (decoder, 0, dest_y + 8,
					     DCT_stride);
		    if (coded_block_pattern & 4)
			slice_non_intra_DCT (decoder, 0, dest_y + DCT_offset,
					     DCT_stride);
		    if (coded_block_pattern & 8)
			slice_non_intra_DCT (decoder, 0,
					     dest_y + DCT_offset + 8,
					     DCT_stride);

		    DCT_stride >>= 1;
		    DCT_offset = (DCT_offset + offset) >> 1;
		    if (coded_block_pattern & 16)
			slice_non_intra_DCT (decoder, 1,
					     decoder->dest[1] + (offset >> 1),
					     DCT_stride);
		    if (coded_block_pattern & 32)
			slice_non_intra_DCT (decoder, 2,
					     decoder->dest[2] + (offset >> 1),
					     DCT_stride);
		    if (coded_block_pattern & (2 << 30))
			slice_non_intra_DCT (decoder, 1,
					     decoder->dest[1] + DCT_offset,
					     DCT_stride);
		    if (coded_block_pattern & (1 << 30))
			slice_non_intra_DCT (decoder, 2,
					     decoder->dest[2] + DCT_offset,
					     DCT_stride);
		} else {
		    int offset;
		    uint8_t * dest_y, * dest_u, * dest_v;

		    coded_block_pattern |= bit_buf & (63 << 26);
		    DUMPBITS (bit_buf, bits, 6);

		    offset = decoder->offset;
		    dest_y = decoder->dest[0] + offset;
		    dest_u = decoder->dest[1] + offset;
		    dest_v = decoder->dest[2] + offset;

		    if (coded_block_pattern & 1)
			slice_non_intra_DCT (decoder, 0, dest_y, DCT_stride);
		    if (coded_block_pattern & 2)
			slice_non_intra_DCT (decoder, 0, dest_y + 8,
					     DCT_stride);
		    if (coded_block_pattern & 4)
			slice_non_intra_DCT (decoder, 0, dest_y + DCT_offset,
					     DCT_stride);
		    if (coded_block_pattern & 8)
			slice_non_intra_DCT (decoder, 0,
					     dest_y + DCT_offset + 8,
					     DCT_stride);

		    if (coded_block_pattern & 16)
			slice_non_intra_DCT (decoder, 1, dest_u, DCT_stride);
		    if (coded_block_pattern & 32)
			slice_non_intra_DCT (decoder, 2, dest_v, DCT_stride);
		    if (coded_block_pattern & (32 << 26))
			slice_non_intra_DCT (decoder, 1, dest_u + DCT_offset,
					     DCT_stride);
		    if (coded_block_pattern & (16 << 26))
			slice_non_intra_DCT (decoder, 2, dest_v + DCT_offset,
					     DCT_stride);
		    if (coded_block_pattern & (8 << 26))
			slice_non_intra_DCT (decoder, 1, dest_u + 8,
					     DCT_stride);
		    if (coded_block_pattern & (4 << 26))
			slice_non_intra_DCT (decoder, 2, dest_v + 8,
					     DCT_stride);
		    if (coded_block_pattern & (2 << 26))
			slice_non_intra_DCT (decoder, 1,
					     dest_u + DCT_offset + 8,
					     DCT_stride);
		    if (coded_block_pattern & (1 << 26))
			slice_non_intra_DCT (decoder, 2,
					     dest_v + DCT_offset + 8,
					     DCT_stride);
		}
	    }

	    decoder->dc_dct_pred[0] = decoder->dc_dct_pred[1] =
		decoder->dc_dct_pred[2] = 16384;
	}

	NEXT_MACROBLOCK;

	NEEDBITS (bit_buf, bits, bit_ptr);
	mba_inc = 0;
	while (1) {
	    if (bit_buf >= 0x10000000) {
		mba = MBA_5 + (UBITS (bit_buf, 5) - 2);
		break;
	    } else if (bit_buf >= 0x03000000) {
		mba = MBA_11 + (UBITS (bit_buf, 11) - 24);
		break;
	    } else switch (UBITS (bit_buf, 11)) {
	    case 8:		/* macroblock_escape */
		mba_inc += 33;
		/* pass through */
	    case 15:	/* macroblock_stuffing (MPEG1 only) */
		DUMPBITS (bit_buf, bits, 11);
		NEEDBITS (bit_buf, bits, bit_ptr);
		continue;
	    default:	/* end of slice, or error */
		if (mpeg2_cpu_state_restore)
		    mpeg2_cpu_state_restore (&cpu_state);
		return;
	    }
	}
	DUMPBITS (bit_buf, bits, mba->len);
	mba_inc += mba->mba;

	if (mba_inc) {
	    decoder->dc_dct_pred[0] = decoder->dc_dct_pred[1] =
		decoder->dc_dct_pred[2] = 16384;

	    if (decoder->coding_type == P_TYPE) {
		do {
		    MOTION_CALL (decoder->motion_parser[0],
				 MACROBLOCK_MOTION_FORWARD);
		    NEXT_MACROBLOCK;
		} while (--mba_inc);
	    } else {
		do {
		    MOTION_CALL (decoder->motion_parser[4], macroblock_modes);
		    NEXT_MACROBLOCK;
		} while (--mba_inc);
	    }
	}
    }
#undef bit_buf
#undef bits
#undef bit_ptr
}
