/*
 * mpeg2.h
 * Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
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
 */

#ifndef MPEG2_H
#define MPEG2_H

#define SEQ_FLAG_MPEG2 1
#define SEQ_FLAG_CONSTRAINED_PARAMETERS 2
#define SEQ_FLAG_PROGRESSIVE_SEQUENCE 4
#define SEQ_FLAG_LOW_DELAY 8
#define SEQ_FLAG_COLOUR_DESCRIPTION 16

#define SEQ_MASK_VIDEO_FORMAT 0xe0
#define SEQ_VIDEO_FORMAT_COMPONENT 0
#define SEQ_VIDEO_FORMAT_PAL 0x20
#define SEQ_VIDEO_FORMAT_NTSC 0x40
#define SEQ_VIDEO_FORMAT_SECAM 0x60
#define SEQ_VIDEO_FORMAT_MAC 0x80
#define SEQ_VIDEO_FORMAT_UNSPECIFIED 0xa0

typedef struct {
    unsigned int width, height;
    unsigned int chroma_width, chroma_height;
    unsigned int byte_rate;
    unsigned int vbv_buffer_size;
    uint32_t flags;

    unsigned int picture_width, picture_height;
    unsigned int display_width, display_height;
    unsigned int pixel_width, pixel_height;
    unsigned int frame_period;

    uint8_t profile_level_id;
    uint8_t colour_primaries;
    uint8_t transfer_characteristics;
    uint8_t matrix_coefficients;
} sequence_t;

#define PIC_MASK_CODING_TYPE 7
#define PIC_FLAG_CODING_TYPE_I 1
#define PIC_FLAG_CODING_TYPE_P 2
#define PIC_FLAG_CODING_TYPE_B 3
#define PIC_FLAG_CODING_TYPE_D 4

#define PIC_FLAG_TOP_FIELD_FIRST 8
#define PIC_FLAG_PROGRESSIVE_FRAME 16
#define PIC_FLAG_COMPOSITE_DISPLAY 32
#define PIC_FLAG_SKIP 64
#define PIC_FLAG_PTS 128
#define PIC_MASK_COMPOSITE_DISPLAY 0xfffff000

typedef struct {
    unsigned int temporal_reference;
    unsigned int nb_fields;
    uint32_t pts;
    uint32_t flags;
    struct {
	int x, y;
    } display_offset[3];
} picture_t;

typedef struct {
    uint8_t * buf[3];
    void * id;
} fbuf_t;

typedef struct {
    const sequence_t * sequence;
    const picture_t * current_picture;
    const picture_t * current_picture_2nd;
    const fbuf_t * current_fbuf;
    const picture_t * display_picture;
    const picture_t * display_picture_2nd;
    const fbuf_t * display_fbuf;
    const fbuf_t * discard_fbuf;
    const uint8_t * user_data;
    int user_data_len;
} mpeg2_info_t;

typedef struct mpeg2dec_s mpeg2dec_t;
typedef struct decoder_s decoder_t;

#define STATE_SEQUENCE 1
#define STATE_SEQUENCE_REPEATED 2
#define STATE_GOP 3
#define STATE_PICTURE 4
#define STATE_SLICE_1ST 5
#define STATE_PICTURE_2ND 6
#define STATE_SLICE 7
#define STATE_END 8
#define STATE_INVALID 9

struct convert_init_s;
void mpeg2_convert (mpeg2dec_t * mpeg2dec,
		    void (* convert) (int, int, uint32_t, void *,
				      struct convert_init_s *), void * arg);
void mpeg2_set_buf (mpeg2dec_t * mpeg2dec, uint8_t * buf[3], void * id);
void mpeg2_custom_fbuf (mpeg2dec_t * mpeg2dec, int custom_fbuf);
void mpeg2_init_fbuf (decoder_t * decoder, uint8_t * current_fbuf[3],
		      uint8_t * forward_fbuf[3], uint8_t * backward_fbuf[3]);

void mpeg2_slice (decoder_t * decoder, int code, const uint8_t * buffer);

#define MPEG2_ACCEL_X86_MMX 1
#define MPEG2_ACCEL_X86_3DNOW 2
#define MPEG2_ACCEL_X86_MMXEXT 4
#define MPEG2_ACCEL_PPC_ALTIVEC 1
#define MPEG2_ACCEL_ALPHA 1
#define MPEG2_ACCEL_ALPHA_MVI 2
#define MPEG2_ACCEL_MLIB 0x40000000
#define MPEG2_ACCEL_DETECT 0x80000000

uint32_t mpeg2_accel (uint32_t accel);
mpeg2dec_t * mpeg2_init (void);
const mpeg2_info_t * mpeg2_info (mpeg2dec_t * mpeg2dec);
void mpeg2_close (mpeg2dec_t * mpeg2dec);

void mpeg2_buffer (mpeg2dec_t * mpeg2dec, uint8_t * start, uint8_t * end);
int mpeg2_parse (mpeg2dec_t * mpeg2dec);

void mpeg2_skip (mpeg2dec_t * mpeg2dec, int skip);
void mpeg2_slice_region (mpeg2dec_t * mpeg2dec, int start, int end);

void mpeg2_pts (mpeg2dec_t * mpeg2dec, uint32_t pts);

#endif /* MPEG2_H */
