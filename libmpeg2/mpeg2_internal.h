/*
 * mpeg2_internal.h
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

/* macroblock modes */
#define MACROBLOCK_INTRA 1
#define MACROBLOCK_PATTERN 2
#define MACROBLOCK_MOTION_BACKWARD 4
#define MACROBLOCK_MOTION_FORWARD 8
#define MACROBLOCK_QUANT 16
#define DCT_TYPE_INTERLACED 32
/* motion_type */
#define MOTION_TYPE_MASK (3*64)
#define MOTION_TYPE_BASE 64
#define MC_FIELD (1*64)
#define MC_FRAME (2*64)
#define MC_16X8 (2*64)
#define MC_DMV (3*64)

/* picture structure */
#define TOP_FIELD 1
#define BOTTOM_FIELD 2
#define FRAME_PICTURE 3

/* picture coding type */
#define I_TYPE 1
#define P_TYPE 2
#define B_TYPE 3
#define D_TYPE 4

typedef struct {
    uint8_t * ref[2][3];
    uint8_t ** ref2[2];
    int pmv[2][2];
    int f_code[2];
} motion_t;

struct decoder_s {
    /* first, state that carries information from one macroblock to the */
    /* next inside a slice, and is never used outside of mpeg2_slice() */

    /* DCT coefficients - should be kept aligned ! */
    int16_t DCTblock[64];

    /* bit parsing stuff */
    uint32_t bitstream_buf;		/* current 32 bit working set */
    int bitstream_bits;			/* used bits in working set */
    const uint8_t * bitstream_ptr;	/* buffer with stream data */

    uint8_t * dest[3];
    uint8_t * picture_dest[3];
    void (* convert) (void * fbuf_id, uint8_t * const * src,
		      unsigned int v_offset);
    void * fbuf_id;

    int offset;
    int stride;
    int uv_stride;
    unsigned int limit_x;
    unsigned int limit_y_16;
    unsigned int limit_y_8;
    unsigned int limit_y;

    /* Motion vectors */
    /* The f_ and b_ correspond to the forward and backward motion */
    /* predictors */
    motion_t b_motion;
    motion_t f_motion;

    /* predictor for DC coefficients in intra blocks */
    int16_t dc_dct_pred[3];

    int quantizer_scale;	/* remove */
    int dmv_offset;		/* remove */
    unsigned int v_offset;	/* remove */

    /* now non-slice-specific information */

    /* sequence header stuff */
    uint8_t intra_quantizer_matrix [64];
    uint8_t non_intra_quantizer_matrix [64];

    /* The width and height of the picture snapped to macroblock units */
    int width;
    int height;
    int vertical_position_extension;

    /* picture header stuff */

    /* what type of picture this is (I, P, B, D) */
    int coding_type;

    /* picture coding extension stuff */

    /* quantization factor for intra dc coefficients */
    int intra_dc_precision;
    /* top/bottom/both fields */
    int picture_structure;
    /* bool to indicate all predictions are frame based */
    int frame_pred_frame_dct;
    /* bool to indicate whether intra blocks have motion vectors */
    /* (for concealment) */
    int concealment_motion_vectors;
    /* bit to indicate which quantization table to use */
    int q_scale_type;
    /* bool to use different vlc tables */
    int intra_vlc_format;
    /* used for DMV MC */
    int top_field_first;

    /* stuff derived from bitstream */

    /* pointer to the zigzag scan we're supposed to be using */
    const uint8_t * scan;

    int second_field;

    int mpeg1;
};

typedef struct {
    fbuf_t fbuf;
} fbuf_alloc_t;

struct mpeg2dec_s {
    decoder_t decoder;

    mpeg2_info_t info;

    uint32_t shift;
    int is_display_initialized;
    int (* action) (struct mpeg2dec_s * mpeg2dec);
    int state;
    uint32_t ext_state;

    /* allocated in init - gcc has problems allocating such big structures */
    uint8_t * chunk_buffer;
    /* pointer to start of the current chunk */
    uint8_t * chunk_start;
    /* pointer to current position in chunk_buffer */
    uint8_t * chunk_ptr;
    /* last start code ? */
    uint8_t code;

    /* PTS */
    uint32_t pts_current, pts_previous;
    int num_pts;
    int bytes_since_pts;

    int first;
    int alloc_index_user;
    int alloc_index;
    uint8_t first_decode_slice;
    uint8_t nb_decode_slices;

    sequence_t new_sequence;
    sequence_t sequence;
    picture_t pictures[4];
    picture_t * picture;
    /*const*/ fbuf_t * fbuf[3];	/* 0: current fbuf, 1-2: prediction fbufs */

    fbuf_alloc_t fbuf_alloc[3];
    int custom_fbuf;

    uint8_t * yuv_buf[3][3];
    int yuv_index;
    void * convert_id;
    int convert_size[3];
    void (* convert_start) (void * id, uint8_t * const * dest, int flags);
    void (* convert_copy) (void * id, uint8_t * const * src,
			   unsigned int v_offset);

    uint8_t * buf_start;
    uint8_t * buf_end;

    int16_t display_offset_x, display_offset_y;
};

typedef struct {
#ifdef ARCH_PPC
    uint8_t regv[12*16];
#endif
    int dummy;
} cpu_state_t;

/* alloc.c */
#define ALLOC_MPEG2DEC 0
#define ALLOC_CHUNK 1
#define ALLOC_YUV 2
#define ALLOC_CONVERT_ID 3
#define ALLOC_CONVERTED 4
void * mpeg2_malloc (int size, int reason);
void mpeg2_free (void * buf);

/* cpu_accel.c */
uint32_t mpeg2_detect_accel (void);

/* cpu_state.c */
void mpeg2_cpu_state_init (uint32_t accel);

/* decode.c */
int mpeg2_seek_sequence (mpeg2dec_t * mpeg2dec);
int mpeg2_seek_header (mpeg2dec_t * mpeg2dec);
int mpeg2_parse_header (mpeg2dec_t * mpeg2dec);

/* header.c */
void mpeg2_header_state_init (mpeg2dec_t * mpeg2dec);
int mpeg2_header_sequence (mpeg2dec_t * mpeg2dec);
int mpeg2_header_gop (mpeg2dec_t * mpeg2dec);
int mpeg2_header_picture_start (mpeg2dec_t * mpeg2dec);
int mpeg2_header_picture (mpeg2dec_t * mpeg2dec);
int mpeg2_header_extension (mpeg2dec_t * mpeg2dec);
int mpeg2_header_user_data (mpeg2dec_t * mpeg2dec);
void mpeg2_header_sequence_finalize (mpeg2dec_t * mpeg2dec);
int mpeg2_header_slice_start (mpeg2dec_t * mpeg2dec);
int mpeg2_header_end (mpeg2dec_t * mpeg2dec);
void mpeg2_set_fbuf (mpeg2dec_t * mpeg2dec, int coding_type);

/* idct.c */
void mpeg2_idct_init (uint32_t accel);

/* idct_mlib.c */
void mpeg2_idct_add_mlib (int last, int16_t * block,
			  uint8_t * dest, int stride);
void mpeg2_idct_copy_mlib_non_ieee (int16_t * block, uint8_t * dest,
				    int stride);
void mpeg2_idct_add_mlib_non_ieee (int last, int16_t * block,
				   uint8_t * dest, int stride);

/* idct_mmx.c */
void mpeg2_idct_copy_mmxext (int16_t * block, uint8_t * dest, int stride);
void mpeg2_idct_add_mmxext (int last, int16_t * block,
			    uint8_t * dest, int stride);
void mpeg2_idct_copy_mmx (int16_t * block, uint8_t * dest, int stride);
void mpeg2_idct_add_mmx (int last, int16_t * block,
			 uint8_t * dest, int stride);
void mpeg2_idct_mmx_init (void);

/* idct_altivec.c */
void mpeg2_idct_copy_altivec (int16_t * block, uint8_t * dest, int stride);
void mpeg2_idct_add_altivec (int last, int16_t * block,
			     uint8_t * dest, int stride);
void mpeg2_idct_altivec_init (void);

/* idct_alpha.c */
void mpeg2_idct_copy_mvi (int16_t * block, uint8_t * dest, int stride);
void mpeg2_idct_add_mvi (int last, int16_t * block,
			 uint8_t * dest, int stride);
void mpeg2_idct_copy_alpha (int16_t * block, uint8_t * dest, int stride);
void mpeg2_idct_add_alpha (int last, int16_t * block,
			   uint8_t * dest, int stride);
void mpeg2_idct_alpha_init(int no_mvi);

/* motion_comp.c */
void mpeg2_mc_init (uint32_t accel);

typedef void mpeg2_mc_fct (uint8_t *, const uint8_t *, int, int);

typedef struct {
    mpeg2_mc_fct * put [8];
    mpeg2_mc_fct * avg [8];
} mpeg2_mc_t;

#define MPEG2_MC_EXTERN(x) mpeg2_mc_t mpeg2_mc_##x = {			  \
    {MC_put_o_16_##x, MC_put_x_16_##x, MC_put_y_16_##x, MC_put_xy_16_##x, \
     MC_put_o_8_##x,  MC_put_x_8_##x,  MC_put_y_8_##x,  MC_put_xy_8_##x}, \
    {MC_avg_o_16_##x, MC_avg_x_16_##x, MC_avg_y_16_##x, MC_avg_xy_16_##x, \
     MC_avg_o_8_##x,  MC_avg_x_8_##x,  MC_avg_y_8_##x,  MC_avg_xy_8_##x}  \
};

extern mpeg2_mc_t mpeg2_mc_c;
extern mpeg2_mc_t mpeg2_mc_mmx;
extern mpeg2_mc_t mpeg2_mc_mmxext;
extern mpeg2_mc_t mpeg2_mc_3dnow;
extern mpeg2_mc_t mpeg2_mc_altivec;
extern mpeg2_mc_t mpeg2_mc_alpha;
extern mpeg2_mc_t mpeg2_mc_mlib;
