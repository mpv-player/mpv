/*
 * header.c
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 2003      Regis Duchesne <hpreg@zoy.org>
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
#include <stdlib.h>	/* defines NULL */
#include <string.h>	/* memcmp */

#include "mpeg2.h"
#include "attributes.h"
#include "mpeg2_internal.h"

#define SEQ_EXT 2
#define SEQ_DISPLAY_EXT 4
#define QUANT_MATRIX_EXT 8
#define COPYRIGHT_EXT 0x10
#define PIC_DISPLAY_EXT 0x80
#define PIC_CODING_EXT 0x100

/* default intra quant matrix, in zig-zag order */
static const uint8_t default_intra_quantizer_matrix[64] ATTR_ALIGN(16) = {
    8,
    16, 16,
    19, 16, 19,
    22, 22, 22, 22,
    22, 22, 26, 24, 26,
    27, 27, 27, 26, 26, 26,
    26, 27, 27, 27, 29, 29, 29,
    34, 34, 34, 29, 29, 29, 27, 27,
    29, 29, 32, 32, 34, 34, 37,
    38, 37, 35, 35, 34, 35,
    38, 38, 40, 40, 40,
    48, 48, 46, 46,
    56, 56, 58,
    69, 69,
    83
};

uint8_t mpeg2_scan_norm[64] ATTR_ALIGN(16) = {
    /* Zig-Zag scan pattern */
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

uint8_t mpeg2_scan_alt[64] ATTR_ALIGN(16) = {
    /* Alternate scan pattern */
     0, 8,  16, 24,  1,  9,  2, 10, 17, 25, 32, 40, 48, 56, 57, 49,
    41, 33, 26, 18,  3, 11,  4, 12, 19, 27, 34, 42, 50, 58, 35, 43,
    51, 59, 20, 28,  5, 13,  6, 14, 21, 29, 36, 44, 52, 60, 37, 45,
    53, 61, 22, 30,  7, 15, 23, 31, 38, 46, 54, 62, 39, 47, 55, 63
};

void mpeg2_header_state_init (mpeg2dec_t * mpeg2dec)
{
    if (mpeg2dec->sequence.width != (unsigned)-1) {
	int i;

	mpeg2dec->sequence.width = (unsigned)-1;
	if (!mpeg2dec->custom_fbuf)
	    for (i = mpeg2dec->alloc_index_user;
		 i < mpeg2dec->alloc_index; i++) {
		mpeg2_free (mpeg2dec->fbuf_alloc[i].fbuf.buf[0]);
		mpeg2_free (mpeg2dec->fbuf_alloc[i].fbuf.buf[1]);
		mpeg2_free (mpeg2dec->fbuf_alloc[i].fbuf.buf[2]);
	    }
	if (mpeg2dec->convert_start)
	    for (i = 0; i < 3; i++) {
		mpeg2_free (mpeg2dec->yuv_buf[i][0]);
		mpeg2_free (mpeg2dec->yuv_buf[i][1]);
		mpeg2_free (mpeg2dec->yuv_buf[i][2]);
	    }
	if (mpeg2dec->decoder.convert_id)
	    mpeg2_free (mpeg2dec->decoder.convert_id);
    }
    mpeg2dec->decoder.coding_type = I_TYPE;
    mpeg2dec->decoder.convert = NULL;
    mpeg2dec->decoder.convert_id = NULL;
    mpeg2dec->picture = mpeg2dec->pictures;
    memset(&mpeg2dec->fbuf_alloc[0].fbuf, 0, sizeof(mpeg2_fbuf_t));
    memset(&mpeg2dec->fbuf_alloc[1].fbuf, 0, sizeof(mpeg2_fbuf_t));
    memset(&mpeg2dec->fbuf_alloc[2].fbuf, 0, sizeof(mpeg2_fbuf_t));
    mpeg2dec->fbuf[0] = &mpeg2dec->fbuf_alloc[0].fbuf;
    mpeg2dec->fbuf[1] = &mpeg2dec->fbuf_alloc[1].fbuf;
    mpeg2dec->fbuf[2] = &mpeg2dec->fbuf_alloc[2].fbuf;
    mpeg2dec->first = 1;
    mpeg2dec->alloc_index = 0;
    mpeg2dec->alloc_index_user = 0;
    mpeg2dec->first_decode_slice = 1;
    mpeg2dec->nb_decode_slices = 0xb0 - 1;
    mpeg2dec->convert = NULL;
    mpeg2dec->convert_start = NULL;
    mpeg2dec->custom_fbuf = 0;
    mpeg2dec->yuv_index = 0;
}

void mpeg2_reset_info (mpeg2_info_t * info)
{
    info->current_picture = info->current_picture_2nd = NULL;
    info->display_picture = info->display_picture_2nd = NULL;
    info->current_fbuf = info->display_fbuf = info->discard_fbuf = NULL;
}

static void info_user_data (mpeg2dec_t * mpeg2dec)
{
    if (mpeg2dec->user_data_len) {
	mpeg2dec->info.user_data = mpeg2dec->chunk_buffer;
	mpeg2dec->info.user_data_len = mpeg2dec->user_data_len - 3;
    }
}

int mpeg2_header_sequence (mpeg2dec_t * mpeg2dec)
{
    uint8_t * buffer = mpeg2dec->chunk_start;
    mpeg2_sequence_t * sequence = &(mpeg2dec->new_sequence);
    static unsigned int frame_period[16] = {
	0, 1126125, 1125000, 1080000, 900900, 900000, 540000, 450450, 450000,
	/* unofficial: xing 15 fps */
	1800000,
	/* unofficial: libmpeg3 "Unofficial economy rates" 5/10/12/15 fps */
	5400000, 2700000, 2250000, 1800000, 0, 0
    };
    int i;

    if ((buffer[6] & 0x20) != 0x20)	/* missing marker_bit */
	return 1;

    i = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
    if (! (sequence->display_width = sequence->picture_width = i >> 12))
	return 1;
    if (! (sequence->display_height = sequence->picture_height = i & 0xfff))
	return 1;
    sequence->width = (sequence->picture_width + 15) & ~15;
    sequence->height = (sequence->picture_height + 15) & ~15;
    sequence->chroma_width = sequence->width >> 1;
    sequence->chroma_height = sequence->height >> 1;

    sequence->flags = (SEQ_FLAG_PROGRESSIVE_SEQUENCE |
		       SEQ_VIDEO_FORMAT_UNSPECIFIED);

    sequence->pixel_width = buffer[3] >> 4;	/* aspect ratio */
    sequence->frame_period = frame_period[buffer[3] & 15];

    sequence->byte_rate = (buffer[4]<<10) | (buffer[5]<<2) | (buffer[6]>>6);

    sequence->vbv_buffer_size = ((buffer[6]<<16)|(buffer[7]<<8))&0x1ff800;

    if (buffer[7] & 4)
	sequence->flags |= SEQ_FLAG_CONSTRAINED_PARAMETERS;

    mpeg2dec->copy_matrix = 3;
    if (buffer[7] & 2) {
	for (i = 0; i < 64; i++)
	    mpeg2dec->new_quantizer_matrix[0][mpeg2_scan_norm[i]] =
		(buffer[i+7] << 7) | (buffer[i+8] >> 1);
	buffer += 64;
    } else
	for (i = 0; i < 64; i++)
	    mpeg2dec->new_quantizer_matrix[0][mpeg2_scan_norm[i]] =
		default_intra_quantizer_matrix[i];

    if (buffer[7] & 1)
	for (i = 0; i < 64; i++)
	    mpeg2dec->new_quantizer_matrix[1][mpeg2_scan_norm[i]] =
		buffer[i+8];
    else
	memset (mpeg2dec->new_quantizer_matrix[1], 16, 64);

    sequence->profile_level_id = 0x80;
    sequence->colour_primaries = 0;
    sequence->transfer_characteristics = 0;
    sequence->matrix_coefficients = 0;

    mpeg2dec->ext_state = SEQ_EXT;
    mpeg2dec->state = STATE_SEQUENCE;
    mpeg2dec->display_offset_x = mpeg2dec->display_offset_y = 0;

    return 0;
}

static int sequence_ext (mpeg2dec_t * mpeg2dec)
{
    uint8_t * buffer = mpeg2dec->chunk_start;
    mpeg2_sequence_t * sequence = &(mpeg2dec->new_sequence);
    uint32_t flags;

    if (!(buffer[3] & 1))
	return 1;

    sequence->profile_level_id = (buffer[0] << 4) | (buffer[1] >> 4);

    sequence->display_width = sequence->picture_width +=
	((buffer[1] << 13) | (buffer[2] << 5)) & 0x3000;
    sequence->display_height = sequence->picture_height +=
	(buffer[2] << 7) & 0x3000;
    sequence->width = (sequence->picture_width + 15) & ~15;
    sequence->height = (sequence->picture_height + 15) & ~15;
    flags = sequence->flags | SEQ_FLAG_MPEG2;
    if (!(buffer[1] & 8)) {
	flags &= ~SEQ_FLAG_PROGRESSIVE_SEQUENCE;
	sequence->height = (sequence->height + 31) & ~31;
    }
    if (buffer[5] & 0x80)
	flags |= SEQ_FLAG_LOW_DELAY;
    sequence->flags = flags;
    sequence->chroma_width = sequence->width;
    sequence->chroma_height = sequence->height;
    switch (buffer[1] & 6) {
    case 0:	/* invalid */
	return 1;
    case 2:	/* 4:2:0 */
	sequence->chroma_height >>= 1;
    case 4:	/* 4:2:2 */
	sequence->chroma_width >>= 1;
    }

    sequence->byte_rate += ((buffer[2]<<25) | (buffer[3]<<17)) & 0x3ffc0000;

    sequence->vbv_buffer_size |= buffer[4] << 21;

    sequence->frame_period =
	sequence->frame_period * ((buffer[5]&31)+1) / (((buffer[5]>>5)&3)+1);

    mpeg2dec->ext_state = SEQ_DISPLAY_EXT;

    return 0;
}

static int sequence_display_ext (mpeg2dec_t * mpeg2dec)
{
    uint8_t * buffer = mpeg2dec->chunk_start;
    mpeg2_sequence_t * sequence = &(mpeg2dec->new_sequence);

    sequence->flags = ((sequence->flags & ~SEQ_MASK_VIDEO_FORMAT) |
		       ((buffer[0]<<4) & SEQ_MASK_VIDEO_FORMAT));
    if (buffer[0] & 1) {
	sequence->flags |= SEQ_FLAG_COLOUR_DESCRIPTION;
	sequence->colour_primaries = buffer[1];
	sequence->transfer_characteristics = buffer[2];
	sequence->matrix_coefficients = buffer[3];
	buffer += 3;
    }

    if (!(buffer[2] & 2))	/* missing marker_bit */
	return 1;

    if( (buffer[1] << 6) | (buffer[2] >> 2) )
	sequence->display_width = (buffer[1] << 6) | (buffer[2] >> 2);
    if( ((buffer[2]& 1 ) << 13) | (buffer[3] << 5) | (buffer[4] >> 3) )
	sequence->display_height =
	    ((buffer[2]& 1 ) << 13) | (buffer[3] << 5) | (buffer[4] >> 3);

    return 0;
}

static inline void simplify (unsigned int * u, unsigned int * v)
{
    unsigned int a, b, tmp;

    a = *u;	b = *v;
    while (a) {	/* find greatest common divisor */
	tmp = a;	a = b % tmp;	b = tmp;
    }
    *u /= b;	*v /= b;
}

static inline void finalize_sequence (mpeg2_sequence_t * sequence)
{
    int width;
    int height;

    sequence->byte_rate *= 50;

    if (sequence->flags & SEQ_FLAG_MPEG2) {
	switch (sequence->pixel_width) {
	case 1:		/* square pixels */
	    sequence->pixel_width = sequence->pixel_height = 1;	return;
	case 2:		/* 4:3 aspect ratio */
	    width = 4; height = 3;	break;
	case 3:		/* 16:9 aspect ratio */
	    width = 16; height = 9;	break;
	case 4:		/* 2.21:1 aspect ratio */
	    width = 221; height = 100;	break;
	default:	/* illegal */
	    sequence->pixel_width = sequence->pixel_height = 0;	return;
	}
	width *= sequence->display_height;
	height *= sequence->display_width;

    } else {
	if (sequence->byte_rate == 50 * 0x3ffff) 
	    sequence->byte_rate = 0;        /* mpeg-1 VBR */ 

	switch (sequence->pixel_width) {
	case 0:	case 15:	/* illegal */
	    sequence->pixel_width = sequence->pixel_height = 0;		return;
	case 1:	/* square pixels */
	    sequence->pixel_width = sequence->pixel_height = 1;		return;
	case 3:	/* 720x576 16:9 */
	    sequence->pixel_width = 64;	sequence->pixel_height = 45;	return;
	case 6:	/* 720x480 16:9 */
	    sequence->pixel_width = 32;	sequence->pixel_height = 27;	return;
	case 8: /* BT.601 625 lines 4:3 */
	    sequence->pixel_width = 59;	sequence->pixel_height = 54;	return;
	case 12: /* BT.601 525 lines 4:3 */
	    sequence->pixel_width = 10;	sequence->pixel_height = 11;	return;
	default:
	    height = 88 * sequence->pixel_width + 1171;
	    width = 2000;
	}
    }

    sequence->pixel_width = width;
    sequence->pixel_height = height;
    simplify (&sequence->pixel_width, &sequence->pixel_height);
}

int mpeg2_guess_aspect (const mpeg2_sequence_t * sequence,
			unsigned int * pixel_width,
			unsigned int * pixel_height)
{
    static struct {
	unsigned int width, height;
    } video_modes[] = {
	{720, 576}, /* 625 lines, 13.5 MHz (D1, DV, DVB, DVD) */
	{704, 576}, /* 625 lines, 13.5 MHz (1/1 D1, DVB, DVD, 4CIF) */
	{544, 576}, /* 625 lines, 10.125 MHz (DVB, laserdisc) */
	{528, 576}, /* 625 lines, 10.125 MHz (3/4 D1, DVB, laserdisc) */
	{480, 576}, /* 625 lines, 9 MHz (2/3 D1, DVB, SVCD) */
	{352, 576}, /* 625 lines, 6.75 MHz (D2, 1/2 D1, CVD, DVB, DVD) */
	{352, 288}, /* 625 lines, 6.75 MHz, 1 field (D4, VCD, DVB, DVD, CIF) */
	{176, 144}, /* 625 lines, 3.375 MHz, half field (QCIF) */
	{720, 486}, /* 525 lines, 13.5 MHz (D1) */
	{704, 486}, /* 525 lines, 13.5 MHz */
	{720, 480}, /* 525 lines, 13.5 MHz (DV, DSS, DVD) */
	{704, 480}, /* 525 lines, 13.5 MHz (1/1 D1, ATSC, DVD) */
	{544, 480}, /* 525 lines. 10.125 MHz (DSS, laserdisc) */
	{528, 480}, /* 525 lines. 10.125 MHz (3/4 D1, laserdisc) */
	{480, 480}, /* 525 lines, 9 MHz (2/3 D1, SVCD) */
	{352, 480}, /* 525 lines, 6.75 MHz (D2, 1/2 D1, CVD, DVD) */
	{352, 240}  /* 525  lines. 6.75 MHz, 1 field (D4, VCD, DSS, DVD) */
    };
    unsigned int width, height, pix_width, pix_height, i, DAR_16_9;

    *pixel_width = sequence->pixel_width;
    *pixel_height = sequence->pixel_height;
    width = sequence->picture_width;
    height = sequence->picture_height;
    for (i = 0; i < sizeof (video_modes) / sizeof (video_modes[0]); i++)
	if (width == video_modes[i].width && height == video_modes[i].height)
	    break;
    if (i == sizeof (video_modes) / sizeof (video_modes[0]) ||
	(sequence->pixel_width == 1 && sequence->pixel_height == 1) ||
	width != sequence->display_width || height != sequence->display_height)
	return 0;

    for (pix_height = 1; height * pix_height < 480; pix_height <<= 1);
    height *= pix_height;
    for (pix_width = 1; width * pix_width <= 352; pix_width <<= 1);
    width *= pix_width;

    if (! (sequence->flags & SEQ_FLAG_MPEG2)) {
	static unsigned int mpeg1_check[2][2] = {{11, 54}, {27, 45}};
	DAR_16_9 = (sequence->pixel_height == 27 ||
		    sequence->pixel_height == 45);
	if (width < 704 ||
	    sequence->pixel_height != mpeg1_check[DAR_16_9][height == 576])
	    return 0;
    } else {
	DAR_16_9 = (3 * sequence->picture_width * sequence->pixel_width >
		    4 * sequence->picture_height * sequence->pixel_height);
	switch (width) {
	case 528: case 544:	pix_width *= 4; pix_height *= 3; break;
	case 480:		pix_width *= 3; pix_height *= 2; break;
	}
    }
    if (DAR_16_9) {
	pix_width *= 4; pix_height *= 3;
    }
    if (height == 576) {
	pix_width *= 59; pix_height *= 54;
    } else {
	pix_width *= 10; pix_height *= 11;
    }
    *pixel_width = pix_width;
    *pixel_height = pix_height;
    simplify (pixel_width, pixel_height);
    return (height == 576) ? 1 : 2;
}

static void copy_matrix (mpeg2dec_t * mpeg2dec, int idx)
{
    if (memcmp (mpeg2dec->quantizer_matrix[idx],
		mpeg2dec->new_quantizer_matrix[idx], 64)) {
	memcpy (mpeg2dec->quantizer_matrix[idx],
		mpeg2dec->new_quantizer_matrix[idx], 64);
	mpeg2dec->scaled[idx] = -1;
    }
}

static void finalize_matrix (mpeg2dec_t * mpeg2dec)
{
    mpeg2_decoder_t * decoder = &(mpeg2dec->decoder);
    int i;

    for (i = 0; i < 2; i++) {
	if (mpeg2dec->copy_matrix & (1 << i))
	    copy_matrix (mpeg2dec, i);
	if ((mpeg2dec->copy_matrix & (4 << i)) &&
	    memcmp (mpeg2dec->quantizer_matrix[i],
		    mpeg2dec->new_quantizer_matrix[i+2], 64)) {
	    copy_matrix (mpeg2dec, i + 2);
	    decoder->chroma_quantizer[i] = decoder->quantizer_prescale[i+2];
	} else if (mpeg2dec->copy_matrix & (5 << i))
	    decoder->chroma_quantizer[i] = decoder->quantizer_prescale[i];
    }
}

static mpeg2_state_t invalid_end_action (mpeg2dec_t * mpeg2dec)
{
    mpeg2_reset_info (&(mpeg2dec->info));
    mpeg2dec->info.gop = NULL;
    info_user_data (mpeg2dec);
    mpeg2_header_state_init (mpeg2dec);
    mpeg2dec->sequence = mpeg2dec->new_sequence;
    mpeg2dec->action = mpeg2_seek_header;
    mpeg2dec->state = STATE_SEQUENCE;
    return STATE_SEQUENCE;
}

void mpeg2_header_sequence_finalize (mpeg2dec_t * mpeg2dec)
{
    mpeg2_sequence_t * sequence = &(mpeg2dec->new_sequence);
    mpeg2_decoder_t * decoder = &(mpeg2dec->decoder);

    finalize_sequence (sequence);
    finalize_matrix (mpeg2dec);

    decoder->mpeg1 = !(sequence->flags & SEQ_FLAG_MPEG2);
    decoder->width = sequence->width;
    decoder->height = sequence->height;
    decoder->vertical_position_extension = (sequence->picture_height > 2800);
    decoder->chroma_format = ((sequence->chroma_width == sequence->width) +
			      (sequence->chroma_height == sequence->height));

    if (mpeg2dec->sequence.width != (unsigned)-1) {
	/*
	 * According to 6.1.1.6, repeat sequence headers should be
	 * identical to the original. However some encoders do not
	 * respect that and change various fields (including bitrate
	 * and aspect ratio) in the repeat sequence headers. So we
	 * choose to be as conservative as possible and only restart
	 * the decoder if the width, height, chroma_width,
	 * chroma_height or low_delay flag are modified.
	 */
	if (sequence->width != mpeg2dec->sequence.width ||
	    sequence->height != mpeg2dec->sequence.height ||
	    sequence->chroma_width != mpeg2dec->sequence.chroma_width ||
	    sequence->chroma_height != mpeg2dec->sequence.chroma_height ||
	    ((sequence->flags ^ mpeg2dec->sequence.flags) &
	     SEQ_FLAG_LOW_DELAY)) {
	    decoder->stride_frame = sequence->width;
	    mpeg2_header_end (mpeg2dec);
	    mpeg2dec->action = invalid_end_action;
	    mpeg2dec->state = STATE_INVALID_END;
	    return;
	}
	mpeg2dec->state = (memcmp (&(mpeg2dec->sequence), sequence,
				   sizeof (mpeg2_sequence_t)) ?
			   STATE_SEQUENCE_MODIFIED : STATE_SEQUENCE_REPEATED);
    } else
	decoder->stride_frame = sequence->width;
    mpeg2dec->sequence = *sequence;
    mpeg2_reset_info (&(mpeg2dec->info));
    mpeg2dec->info.sequence = &(mpeg2dec->sequence);
    mpeg2dec->info.gop = NULL;
    info_user_data (mpeg2dec);
}

int mpeg2_header_gop (mpeg2dec_t * mpeg2dec)
{
    uint8_t * buffer = mpeg2dec->chunk_start;
    mpeg2_gop_t * gop = &(mpeg2dec->new_gop);

    if (! (buffer[1] & 8))
	return 1;
    gop->hours = (buffer[0] >> 2) & 31;
    gop->minutes = ((buffer[0] << 4) | (buffer[1] >> 4)) & 63;
    gop->seconds = ((buffer[1] << 3) | (buffer[2] >> 5)) & 63;
    gop->pictures = ((buffer[2] << 1) | (buffer[3] >> 7)) & 63;
    gop->flags = (buffer[0] >> 7) | ((buffer[3] >> 4) & 6);
    mpeg2dec->state = STATE_GOP;
    return 0;
}

void mpeg2_header_gop_finalize (mpeg2dec_t * mpeg2dec)
{
    mpeg2dec->gop = mpeg2dec->new_gop;
    mpeg2_reset_info (&(mpeg2dec->info));
    mpeg2dec->info.gop = &(mpeg2dec->gop);
    info_user_data (mpeg2dec);
}

void mpeg2_set_fbuf (mpeg2dec_t * mpeg2dec, int b_type)
{
    int i;

    for (i = 0; i < 3; i++)
	if (mpeg2dec->fbuf[1] != &mpeg2dec->fbuf_alloc[i].fbuf &&
	    mpeg2dec->fbuf[2] != &mpeg2dec->fbuf_alloc[i].fbuf) {
	    mpeg2dec->fbuf[0] = &mpeg2dec->fbuf_alloc[i].fbuf;
	    mpeg2dec->info.current_fbuf = mpeg2dec->fbuf[0];
	    if (b_type || (mpeg2dec->sequence.flags & SEQ_FLAG_LOW_DELAY)) {
		if (b_type || mpeg2dec->convert)
		    mpeg2dec->info.discard_fbuf = mpeg2dec->fbuf[0];
		mpeg2dec->info.display_fbuf = mpeg2dec->fbuf[0];
	    }
	    break;
	}
}

int mpeg2_header_picture (mpeg2dec_t * mpeg2dec)
{
    uint8_t * buffer = mpeg2dec->chunk_start;
    mpeg2_picture_t * picture = &(mpeg2dec->new_picture);
    mpeg2_decoder_t * decoder = &(mpeg2dec->decoder);
    int type;

    mpeg2dec->state = ((mpeg2dec->state != STATE_SLICE_1ST) ?
		       STATE_PICTURE : STATE_PICTURE_2ND);
    mpeg2dec->ext_state = PIC_CODING_EXT;

    picture->temporal_reference = (buffer[0] << 2) | (buffer[1] >> 6);

    type = (buffer [1] >> 3) & 7;
    if (type == PIC_FLAG_CODING_TYPE_P || type == PIC_FLAG_CODING_TYPE_B) {
	/* forward_f_code and backward_f_code - used in mpeg1 only */
	decoder->f_motion.f_code[1] = (buffer[3] >> 2) & 1;
	decoder->f_motion.f_code[0] =
	    (((buffer[3] << 1) | (buffer[4] >> 7)) & 7) - 1;
	decoder->b_motion.f_code[1] = (buffer[4] >> 6) & 1;
	decoder->b_motion.f_code[0] = ((buffer[4] >> 3) & 7) - 1;
    }

    picture->flags = PIC_FLAG_PROGRESSIVE_FRAME | type;
    picture->tag = picture->tag2 = 0;
    if (mpeg2dec->num_tags) {
	if (mpeg2dec->bytes_since_tag >= mpeg2dec->chunk_ptr - buffer + 4) {
	    mpeg2dec->num_tags = 0;
	    picture->tag = mpeg2dec->tag_current;
	    picture->tag2 = mpeg2dec->tag2_current;
	    picture->flags |= PIC_FLAG_TAGS;
	} else if (mpeg2dec->num_tags > 1) {
	    mpeg2dec->num_tags = 1;
	    picture->tag = mpeg2dec->tag_previous;
	    picture->tag2 = mpeg2dec->tag2_previous;
	    picture->flags |= PIC_FLAG_TAGS;
	}
    }
    picture->nb_fields = 2;
    picture->display_offset[0].x = picture->display_offset[1].x =
	picture->display_offset[2].x = mpeg2dec->display_offset_x;
    picture->display_offset[0].y = picture->display_offset[1].y =
	picture->display_offset[2].y = mpeg2dec->display_offset_y;

    /* XXXXXX decode extra_information_picture as well */

    decoder->q_scale_type = 0;
    decoder->intra_dc_precision = 7;
    decoder->frame_pred_frame_dct = 1;
    decoder->concealment_motion_vectors = 0;
    decoder->scan = mpeg2_scan_norm;
    decoder->picture_structure = FRAME_PICTURE;
    mpeg2dec->copy_matrix = 0;

    return 0;
}

static int picture_coding_ext (mpeg2dec_t * mpeg2dec)
{
    uint8_t * buffer = mpeg2dec->chunk_start;
    mpeg2_picture_t * picture = &(mpeg2dec->new_picture);
    mpeg2_decoder_t * decoder = &(mpeg2dec->decoder);
    uint32_t flags;

    /* pre subtract 1 for use later in compute_motion_vector */
    decoder->f_motion.f_code[0] = (buffer[0] & 15) - 1;
    decoder->f_motion.f_code[1] = (buffer[1] >> 4) - 1;
    decoder->b_motion.f_code[0] = (buffer[1] & 15) - 1;
    decoder->b_motion.f_code[1] = (buffer[2] >> 4) - 1;

    flags = picture->flags;
    decoder->intra_dc_precision = 7 - ((buffer[2] >> 2) & 3);
    decoder->picture_structure = buffer[2] & 3;
    switch (decoder->picture_structure) {
    case TOP_FIELD:
	flags |= PIC_FLAG_TOP_FIELD_FIRST;
    case BOTTOM_FIELD:
	picture->nb_fields = 1;
	break;
    case FRAME_PICTURE:
	if (!(mpeg2dec->sequence.flags & SEQ_FLAG_PROGRESSIVE_SEQUENCE)) {
	    picture->nb_fields = (buffer[3] & 2) ? 3 : 2;
	    flags |= (buffer[3] & 128) ? PIC_FLAG_TOP_FIELD_FIRST : 0;
	    flags |= (buffer[3] &   2) ? PIC_FLAG_REPEAT_FIRST_FIELD : 0;
	} else
	    picture->nb_fields = (buffer[3]&2) ? ((buffer[3]&128) ? 6 : 4) : 2;
	break;
    default:
	return 1;
    }
    decoder->top_field_first = buffer[3] >> 7;
    decoder->frame_pred_frame_dct = (buffer[3] >> 6) & 1;
    decoder->concealment_motion_vectors = (buffer[3] >> 5) & 1;
    decoder->q_scale_type = buffer[3] & 16;
    decoder->intra_vlc_format = (buffer[3] >> 3) & 1;
    decoder->scan = (buffer[3] & 4) ? mpeg2_scan_alt : mpeg2_scan_norm;
    if (!(buffer[4] & 0x80))
	flags &= ~PIC_FLAG_PROGRESSIVE_FRAME;
    if (buffer[4] & 0x40)
	flags |= (((buffer[4]<<26) | (buffer[5]<<18) | (buffer[6]<<10)) &
		  PIC_MASK_COMPOSITE_DISPLAY) | PIC_FLAG_COMPOSITE_DISPLAY;
    picture->flags = flags;

    mpeg2dec->ext_state = PIC_DISPLAY_EXT | COPYRIGHT_EXT | QUANT_MATRIX_EXT;

    return 0;
}

static int picture_display_ext (mpeg2dec_t * mpeg2dec)
{
    uint8_t * buffer = mpeg2dec->chunk_start;
    mpeg2_picture_t * picture = &(mpeg2dec->new_picture);
    int i, nb_pos;

    nb_pos = picture->nb_fields;
    if (mpeg2dec->sequence.flags & SEQ_FLAG_PROGRESSIVE_SEQUENCE)
	nb_pos >>= 1;

    for (i = 0; i < nb_pos; i++) {
	int x, y;

	x = ((buffer[4*i] << 24) | (buffer[4*i+1] << 16) |
	     (buffer[4*i+2] << 8) | buffer[4*i+3]) >> (11-2*i);
	y = ((buffer[4*i+2] << 24) | (buffer[4*i+3] << 16) |
	     (buffer[4*i+4] << 8) | buffer[4*i+5]) >> (10-2*i);
	if (! (x & y & 1))
	    return 1;
	picture->display_offset[i].x = mpeg2dec->display_offset_x = x >> 1;
	picture->display_offset[i].y = mpeg2dec->display_offset_y = y >> 1;
    }
    for (; i < 3; i++) {
	picture->display_offset[i].x = mpeg2dec->display_offset_x;
	picture->display_offset[i].y = mpeg2dec->display_offset_y;
    }
    return 0;
}

void mpeg2_header_picture_finalize (mpeg2dec_t * mpeg2dec, uint32_t accels)
{
    mpeg2_decoder_t * decoder = &(mpeg2dec->decoder);
    int old_type_b = (decoder->coding_type == B_TYPE);
    int low_delay = mpeg2dec->sequence.flags & SEQ_FLAG_LOW_DELAY;

    finalize_matrix (mpeg2dec);
    decoder->coding_type = mpeg2dec->new_picture.flags & PIC_MASK_CODING_TYPE;

    if (mpeg2dec->state == STATE_PICTURE) {
	mpeg2_picture_t * picture;
	mpeg2_picture_t * other;

	decoder->second_field = 0;

	picture = other = mpeg2dec->pictures;
	if (old_type_b ^ (mpeg2dec->picture < mpeg2dec->pictures + 2))
	    picture += 2;
	else
	    other += 2;
	mpeg2dec->picture = picture;
	*picture = mpeg2dec->new_picture;

	if (!old_type_b) {
	    mpeg2dec->fbuf[2] = mpeg2dec->fbuf[1];
	    mpeg2dec->fbuf[1] = mpeg2dec->fbuf[0];
	}
	mpeg2dec->fbuf[0] = NULL;
	mpeg2_reset_info (&(mpeg2dec->info));
	mpeg2dec->info.current_picture = picture;
	mpeg2dec->info.display_picture = picture;
	if (decoder->coding_type != B_TYPE) {
	    if (!low_delay) {
		if (mpeg2dec->first) {
		    mpeg2dec->info.display_picture = NULL;
		    mpeg2dec->first = 0;
		} else {
		    mpeg2dec->info.display_picture = other;
		    if (other->nb_fields == 1)
			mpeg2dec->info.display_picture_2nd = other + 1;
		    mpeg2dec->info.display_fbuf = mpeg2dec->fbuf[1];
		}
	    }
	    if (!low_delay + !mpeg2dec->convert)
		mpeg2dec->info.discard_fbuf =
		    mpeg2dec->fbuf[!low_delay + !mpeg2dec->convert];
	}
	if (mpeg2dec->convert) {
	    mpeg2_convert_init_t convert_init;
	    if (!mpeg2dec->convert_start) {
		int y_size, uv_size;

		mpeg2dec->decoder.convert_id =
		    mpeg2_malloc (mpeg2dec->convert_id_size,
				  MPEG2_ALLOC_CONVERT_ID);
		mpeg2dec->convert (MPEG2_CONVERT_START,
				   mpeg2dec->decoder.convert_id,
				   &(mpeg2dec->sequence),
				   mpeg2dec->convert_stride, accels,
				   mpeg2dec->convert_arg, &convert_init);
		mpeg2dec->convert_start = convert_init.start;
		mpeg2dec->decoder.convert = convert_init.copy;

		y_size = decoder->stride_frame * mpeg2dec->sequence.height;
		uv_size = y_size >> (2 - mpeg2dec->decoder.chroma_format);
		mpeg2dec->yuv_buf[0][0] =
		    (uint8_t *) mpeg2_malloc (y_size, MPEG2_ALLOC_YUV);
		mpeg2dec->yuv_buf[0][1] =
		    (uint8_t *) mpeg2_malloc (uv_size, MPEG2_ALLOC_YUV);
		mpeg2dec->yuv_buf[0][2] =
		    (uint8_t *) mpeg2_malloc (uv_size, MPEG2_ALLOC_YUV);
		mpeg2dec->yuv_buf[1][0] =
		    (uint8_t *) mpeg2_malloc (y_size, MPEG2_ALLOC_YUV);
		mpeg2dec->yuv_buf[1][1] =
		    (uint8_t *) mpeg2_malloc (uv_size, MPEG2_ALLOC_YUV);
		mpeg2dec->yuv_buf[1][2] =
		    (uint8_t *) mpeg2_malloc (uv_size, MPEG2_ALLOC_YUV);
		y_size = decoder->stride_frame * 32;
		uv_size = y_size >> (2 - mpeg2dec->decoder.chroma_format);
		mpeg2dec->yuv_buf[2][0] =
		    (uint8_t *) mpeg2_malloc (y_size, MPEG2_ALLOC_YUV);
		mpeg2dec->yuv_buf[2][1] =
		    (uint8_t *) mpeg2_malloc (uv_size, MPEG2_ALLOC_YUV);
		mpeg2dec->yuv_buf[2][2] =
		    (uint8_t *) mpeg2_malloc (uv_size, MPEG2_ALLOC_YUV);
	    }
	    if (!mpeg2dec->custom_fbuf) {
		while (mpeg2dec->alloc_index < 3) {
		    mpeg2_fbuf_t * fbuf;

		    fbuf = &mpeg2dec->fbuf_alloc[mpeg2dec->alloc_index++].fbuf;
		    fbuf->id = NULL;
		    fbuf->buf[0] =
			(uint8_t *) mpeg2_malloc (convert_init.buf_size[0],
						  MPEG2_ALLOC_CONVERTED);
		    fbuf->buf[1] =
			(uint8_t *) mpeg2_malloc (convert_init.buf_size[1],
						  MPEG2_ALLOC_CONVERTED);
		    fbuf->buf[2] =
			(uint8_t *) mpeg2_malloc (convert_init.buf_size[2],
						  MPEG2_ALLOC_CONVERTED);
		}
		mpeg2_set_fbuf (mpeg2dec, (decoder->coding_type == B_TYPE));
	    }
	} else if (!mpeg2dec->custom_fbuf) {
	    while (mpeg2dec->alloc_index < 3) {
		mpeg2_fbuf_t * fbuf;
		int y_size, uv_size;

		fbuf = &(mpeg2dec->fbuf_alloc[mpeg2dec->alloc_index++].fbuf);
		fbuf->id = NULL;
		y_size = decoder->stride_frame * mpeg2dec->sequence.height;
		uv_size = y_size >> (2 - decoder->chroma_format);
		fbuf->buf[0] = (uint8_t *) mpeg2_malloc (y_size,
							 MPEG2_ALLOC_YUV);
		fbuf->buf[1] = (uint8_t *) mpeg2_malloc (uv_size,
							 MPEG2_ALLOC_YUV);
		fbuf->buf[2] = (uint8_t *) mpeg2_malloc (uv_size,
							 MPEG2_ALLOC_YUV);
	    }
	    mpeg2_set_fbuf (mpeg2dec, (decoder->coding_type == B_TYPE));
	}
    } else {
	decoder->second_field = 1;
	mpeg2dec->picture++;	/* second field picture */
	*(mpeg2dec->picture) = mpeg2dec->new_picture;
	mpeg2dec->info.current_picture_2nd = mpeg2dec->picture;
	if (low_delay || decoder->coding_type == B_TYPE)
	    mpeg2dec->info.display_picture_2nd = mpeg2dec->picture;
    }

    info_user_data (mpeg2dec);
}

static int copyright_ext (mpeg2dec_t * mpeg2dec)
{
    return 0;
}

static int quant_matrix_ext (mpeg2dec_t * mpeg2dec)
{
    uint8_t * buffer = mpeg2dec->chunk_start;
    int i, j;

    for (i = 0; i < 4; i++)
	if (buffer[0] & (8 >> i)) {
	    for (j = 0; j < 64; j++)
		mpeg2dec->new_quantizer_matrix[i][mpeg2_scan_norm[j]] =
		    (buffer[j] << (i+5)) | (buffer[j+1] >> (3-i));
	    mpeg2dec->copy_matrix |= 1 << i;
	    buffer += 64;
	}

    return 0;
}

int mpeg2_header_extension (mpeg2dec_t * mpeg2dec)
{
    static int (* parser[]) (mpeg2dec_t *) = {
	0, sequence_ext, sequence_display_ext, quant_matrix_ext,
	copyright_ext, 0, 0, picture_display_ext, picture_coding_ext
    };
    int ext, ext_bit;

    ext = mpeg2dec->chunk_start[0] >> 4;
    ext_bit = 1 << ext;

    if (!(mpeg2dec->ext_state & ext_bit))
	return 0;	/* ignore illegal extensions */
    mpeg2dec->ext_state &= ~ext_bit;
    return parser[ext] (mpeg2dec);
}

int mpeg2_header_user_data (mpeg2dec_t * mpeg2dec)
{
    mpeg2dec->user_data_len += mpeg2dec->chunk_ptr - 1 - mpeg2dec->chunk_start;
    mpeg2dec->chunk_start = mpeg2dec->chunk_ptr - 1;
    
    return 0;
}

static void prescale (mpeg2dec_t * mpeg2dec, int idx)
{
    static int non_linear_scale [] = {
	 0,  1,  2,  3,  4,  5,   6,   7,
	 8, 10, 12, 14, 16, 18,  20,  22,
	24, 28, 32, 36, 40, 44,  48,  52,
	56, 64, 72, 80, 88, 96, 104, 112
    };
    int i, j, k;
    mpeg2_decoder_t * decoder = &(mpeg2dec->decoder);

    if (mpeg2dec->scaled[idx] != decoder->q_scale_type) {
	mpeg2dec->scaled[idx] = decoder->q_scale_type;
	for (i = 0; i < 32; i++) {
	    k = decoder->q_scale_type ? non_linear_scale[i] : (i << 1);
	    decoder->quantizer_scales[i] = k;
	    for (j = 0; j < 64; j++)
		decoder->quantizer_prescale[idx][i][j] =
		    k * mpeg2dec->quantizer_matrix[idx][j];
	}
    }
}

mpeg2_state_t mpeg2_header_slice_start (mpeg2dec_t * mpeg2dec)
{
    mpeg2_decoder_t * decoder = &(mpeg2dec->decoder);

    mpeg2dec->info.user_data = NULL;	mpeg2dec->info.user_data_len = 0;
    mpeg2dec->state = ((mpeg2dec->picture->nb_fields > 1 ||
			mpeg2dec->state == STATE_PICTURE_2ND) ?
		       STATE_SLICE : STATE_SLICE_1ST);

    if (mpeg2dec->decoder.coding_type != D_TYPE) {
	prescale (mpeg2dec, 0);
	if (decoder->chroma_quantizer[0] == decoder->quantizer_prescale[2])
	    prescale (mpeg2dec, 2);
	if (mpeg2dec->decoder.coding_type != I_TYPE) {
	    prescale (mpeg2dec, 1);
	    if (decoder->chroma_quantizer[1] == decoder->quantizer_prescale[3])
		prescale (mpeg2dec, 3);
	}
    }

    if (!(mpeg2dec->nb_decode_slices))
	mpeg2dec->picture->flags |= PIC_FLAG_SKIP;
    else if (mpeg2dec->convert_start) {
	mpeg2dec->convert_start (decoder->convert_id, mpeg2dec->fbuf[0],
				 mpeg2dec->picture, mpeg2dec->info.gop);

	if (mpeg2dec->decoder.coding_type == B_TYPE)
	    mpeg2_init_fbuf (&(mpeg2dec->decoder), mpeg2dec->yuv_buf[2],
			     mpeg2dec->yuv_buf[mpeg2dec->yuv_index ^ 1],
			     mpeg2dec->yuv_buf[mpeg2dec->yuv_index]);
	else {
	    mpeg2_init_fbuf (&(mpeg2dec->decoder),
			     mpeg2dec->yuv_buf[mpeg2dec->yuv_index ^ 1],
			     mpeg2dec->yuv_buf[mpeg2dec->yuv_index],
			     mpeg2dec->yuv_buf[mpeg2dec->yuv_index]);
	    if (mpeg2dec->state == STATE_SLICE)
		mpeg2dec->yuv_index ^= 1;
	}
    } else {
	int b_type;

	b_type = (mpeg2dec->decoder.coding_type == B_TYPE);
	mpeg2_init_fbuf (&(mpeg2dec->decoder), mpeg2dec->fbuf[0]->buf,
			 mpeg2dec->fbuf[b_type + 1]->buf,
			 mpeg2dec->fbuf[b_type]->buf);
    }
    mpeg2dec->action = NULL;
    return STATE_INTERNAL_NORETURN;
}

static mpeg2_state_t seek_sequence (mpeg2dec_t * mpeg2dec)
{
    mpeg2_reset_info (&(mpeg2dec->info));
    mpeg2dec->info.sequence = NULL;
    mpeg2dec->info.gop = NULL;
    mpeg2_header_state_init (mpeg2dec);
    mpeg2dec->action = mpeg2_seek_header;
    return mpeg2_seek_header (mpeg2dec);
}

mpeg2_state_t mpeg2_header_end (mpeg2dec_t * mpeg2dec)
{
    mpeg2_picture_t * picture;
    int b_type;

    b_type = (mpeg2dec->decoder.coding_type == B_TYPE);
    picture = mpeg2dec->pictures;
    if ((mpeg2dec->picture >= picture + 2) ^ b_type)
	picture = mpeg2dec->pictures + 2;

    mpeg2_reset_info (&(mpeg2dec->info));
    if (!(mpeg2dec->sequence.flags & SEQ_FLAG_LOW_DELAY)) {
	mpeg2dec->info.display_picture = picture;
	if (picture->nb_fields == 1)
	    mpeg2dec->info.display_picture_2nd = picture + 1;
	mpeg2dec->info.display_fbuf = mpeg2dec->fbuf[b_type];
	if (!mpeg2dec->convert)
	    mpeg2dec->info.discard_fbuf = mpeg2dec->fbuf[b_type + 1];
    } else if (!mpeg2dec->convert)
	mpeg2dec->info.discard_fbuf = mpeg2dec->fbuf[b_type];
    mpeg2dec->action = seek_sequence;
    return STATE_END;
}
