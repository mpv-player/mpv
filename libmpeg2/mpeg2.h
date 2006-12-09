/*
 * mpeg2.h
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
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
 * Modified for use with MPlayer, see libmpeg-0.4.1.diff for the exact changes.
 * detailed changelog at http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

#ifndef MPEG2_H
#define MPEG2_H

#define MPEG2_VERSION(a,b,c) (((a)<<16)|((b)<<8)|(c))
#define MPEG2_RELEASE MPEG2_VERSION (0, 4, 1)	/* 0.4.1 */

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

typedef struct mpeg2_sequence_s {
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
} mpeg2_sequence_t;

#define GOP_FLAG_DROP_FRAME 1
#define GOP_FLAG_BROKEN_LINK 2
#define GOP_FLAG_CLOSED_GOP 4

typedef struct mpeg2_gop_s {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t pictures;
    uint32_t flags;
} mpeg2_gop_t;

#define PIC_MASK_CODING_TYPE 7
#define PIC_FLAG_CODING_TYPE_I 1
#define PIC_FLAG_CODING_TYPE_P 2
#define PIC_FLAG_CODING_TYPE_B 3
#define PIC_FLAG_CODING_TYPE_D 4

#define PIC_FLAG_TOP_FIELD_FIRST 8
#define PIC_FLAG_PROGRESSIVE_FRAME 16
#define PIC_FLAG_COMPOSITE_DISPLAY 32
#define PIC_FLAG_SKIP 64
#define PIC_FLAG_TAGS 128
#define PIC_FLAG_REPEAT_FIRST_FIELD 256
#define PIC_MASK_COMPOSITE_DISPLAY 0xfffff000

typedef struct mpeg2_picture_s {
    unsigned int temporal_reference;
    unsigned int nb_fields;
    uint32_t tag, tag2;
    uint32_t flags;
    struct {
	int x, y;
    } display_offset[3];
} mpeg2_picture_t;

typedef struct mpeg2_fbuf_s {
    uint8_t * buf[3];
    void * id;
} mpeg2_fbuf_t;

typedef struct mpeg2_info_s {
    const mpeg2_sequence_t * sequence;
    const mpeg2_gop_t * gop;
    const mpeg2_picture_t * current_picture;
    const mpeg2_picture_t * current_picture_2nd;
    const mpeg2_fbuf_t * current_fbuf;
    const mpeg2_picture_t * display_picture;
    const mpeg2_picture_t * display_picture_2nd;
    const mpeg2_fbuf_t * display_fbuf;
    const mpeg2_fbuf_t * discard_fbuf;
    const uint8_t * user_data;
    unsigned int user_data_len;
} mpeg2_info_t;

typedef struct mpeg2dec_s mpeg2dec_t;
typedef struct mpeg2_decoder_s mpeg2_decoder_t;

typedef enum {
    STATE_BUFFER = 0,
    STATE_SEQUENCE = 1,
    STATE_SEQUENCE_REPEATED = 2,
    STATE_GOP = 3,
    STATE_PICTURE = 4,
    STATE_SLICE_1ST = 5,
    STATE_PICTURE_2ND = 6,
    STATE_SLICE = 7,
    STATE_END = 8,
    STATE_INVALID = 9,
    STATE_INVALID_END = 10
} mpeg2_state_t;

typedef struct mpeg2_convert_init_s {
    unsigned int id_size;
    unsigned int buf_size[3];
    void (* start) (void * id, const mpeg2_fbuf_t * fbuf,
		    const mpeg2_picture_t * picture, const mpeg2_gop_t * gop);
    void (* copy) (void * id, uint8_t * const * src, unsigned int v_offset);
} mpeg2_convert_init_t;
typedef enum {
    MPEG2_CONVERT_SET = 0,
    MPEG2_CONVERT_STRIDE = 1,
    MPEG2_CONVERT_START = 2
} mpeg2_convert_stage_t;
typedef int mpeg2_convert_t (int stage, void * id,
			     const mpeg2_sequence_t * sequence, int stride,
			     uint32_t accel, void * arg,
			     mpeg2_convert_init_t * result);
int mpeg2_convert (mpeg2dec_t * mpeg2dec, mpeg2_convert_t convert, void * arg);
int mpeg2_stride (mpeg2dec_t * mpeg2dec, int stride);
void mpeg2_set_buf (mpeg2dec_t * mpeg2dec, uint8_t * buf[3], void * id);
void mpeg2_custom_fbuf (mpeg2dec_t * mpeg2dec, int custom_fbuf);

#define MPEG2_ACCEL_X86_MMX 1
#define MPEG2_ACCEL_X86_3DNOW 2
#define MPEG2_ACCEL_X86_MMXEXT 4
#define MPEG2_ACCEL_X86_SSE2 8
#define MPEG2_ACCEL_PPC_ALTIVEC 1
#define MPEG2_ACCEL_ALPHA 1
#define MPEG2_ACCEL_ALPHA_MVI 2
#define MPEG2_ACCEL_SPARC_VIS 1
#define MPEG2_ACCEL_SPARC_VIS2 2
#define MPEG2_ACCEL_DETECT 0x80000000

uint32_t mpeg2_accel (uint32_t accel);
mpeg2dec_t * mpeg2_init (void);
const mpeg2_info_t * mpeg2_info (mpeg2dec_t * mpeg2dec);
void mpeg2_close (mpeg2dec_t * mpeg2dec);

void mpeg2_buffer (mpeg2dec_t * mpeg2dec, uint8_t * start, uint8_t * end);
int mpeg2_getpos (mpeg2dec_t * mpeg2dec);
mpeg2_state_t mpeg2_parse (mpeg2dec_t * mpeg2dec);

void mpeg2_reset (mpeg2dec_t * mpeg2dec, int full_reset);
void mpeg2_skip (mpeg2dec_t * mpeg2dec, int skip);
void mpeg2_slice_region (mpeg2dec_t * mpeg2dec, int start, int end);

void mpeg2_tag_picture (mpeg2dec_t * mpeg2dec, uint32_t tag, uint32_t tag2);

void mpeg2_init_fbuf (mpeg2_decoder_t * decoder, uint8_t * current_fbuf[3],
		      uint8_t * forward_fbuf[3], uint8_t * backward_fbuf[3]);
void mpeg2_slice (mpeg2_decoder_t * decoder, int code, const uint8_t * buffer);

typedef enum {
    MPEG2_ALLOC_MPEG2DEC = 0,
    MPEG2_ALLOC_CHUNK = 1,
    MPEG2_ALLOC_YUV = 2,
    MPEG2_ALLOC_CONVERT_ID = 3,
    MPEG2_ALLOC_CONVERTED = 4
} mpeg2_alloc_t;

void * mpeg2_malloc (unsigned size, mpeg2_alloc_t reason);
void mpeg2_free (void * buf);
void mpeg2_malloc_hooks (void * malloc (unsigned, mpeg2_alloc_t),
			 int free (void *));

#endif /* MPEG2_H */
