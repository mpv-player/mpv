#include <inttypes.h>
/*
 * mpeg2_internal.h
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
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

// hack mode - temporary
// 0 = decode B pictures in a small slice buffer, display slice per slice
// 1 = decode in a frame buffer, display slice per slice
// 2 = decode in a frame buffer, display whole frames
#define HACK_MODE 0

// macroblock modes
#define MACROBLOCK_INTRA 1
#define MACROBLOCK_PATTERN 2
#define MACROBLOCK_MOTION_BACKWARD 4
#define MACROBLOCK_MOTION_FORWARD 8
#define MACROBLOCK_QUANT 16
#define DCT_TYPE_INTERLACED 32
// motion_type
#define MOTION_TYPE_MASK (3*64)
#define MOTION_TYPE_BASE 64
#define MC_FIELD (1*64)
#define MC_FRAME (2*64)
#define MC_16X8 (2*64)
#define MC_DMV (3*64)

//picture structure
#define TOP_FIELD 1
#define BOTTOM_FIELD 2
#define FRAME_PICTURE 3

//picture coding type
#define I_TYPE 1
#define P_TYPE 2
#define B_TYPE 3
#define D_TYPE 4

//The picture struct contains all of the top level state
//information (ie everything except slice and macroblock
//state)
typedef struct picture_s {
    //-- sequence header stuff --
    uint8_t intra_quantizer_matrix [64];
    uint8_t non_intra_quantizer_matrix [64];

    //The width and height of the picture snapped to macroblock units
    int coded_picture_width;
    int coded_picture_height;

    //-- picture header stuff --

    //what type of picture this is (I,P,or B) D from MPEG-1 isn't supported
    int picture_coding_type;
	
    //-- picture coding extension stuff --
	
    //quantization factor for motion vectors
    int f_code[2][2];
    //quantization factor for intra dc coefficients
    int intra_dc_precision;
    //top/bottom/both fields
    int picture_structure;
    //bool to indicate all predictions are frame based
    int frame_pred_frame_dct;
    //bool to indicate whether intra blocks have motion vectors 
    // (for concealment)
    int concealment_motion_vectors;
    //bit to indicate which quantization table to use
    int q_scale_type;
    //bool to use different vlc tables
    int intra_vlc_format;

    //last macroblock in the picture
    int last_mba;
    //width of picture in macroblocks
    int mb_width;

    //stuff derived from bitstream

    //pointer to the zigzag scan we're supposed to be using
    uint8_t * scan;

    //Pointer to the current planar frame buffer (Y,Cr,CB)
    uint8_t * current_frame[3];    
    //storage for reference frames plus a b-frame
    uint8_t * forward_reference_frame[3];
    uint8_t * backward_reference_frame[3];
    uint8_t * throwaway_frame[3];
    uint8_t * pp_frame[3];  // postprocess
    //uint8_t * throwaway_frame;

    int pp_options; // postprocess

    int second_field;

    // MPEG1 - testing
    uint8_t mpeg1;

    //these things are not needed by the decoder
    //NOTICE : this is a temporary interface, we will build a better one later.
    int aspect_ratio_information;
    int frame_rate_code;
    int progressive_sequence;
    int top_field_first; // this one is actually used for DMV MC
    int repeat_first_field;
    int progressive_frame;
    // added by A'rpi/ESP-team:
    int repeat_count;
    int bitrate;
    int frame_rate;
    int display_picture_width;
    int display_picture_height;
} picture_t;

typedef struct motion_s {
    uint8_t * ref[2][3];
    int pmv[2][2];
    int f_code[2];
} motion_t;

// state that is carried from one macroblock to the next inside of a same slice
typedef struct slice_s {
    // bit parsing stuff
    uint32_t bitstream_buf;	// current 32 bit working set of buffer
    int bitstream_bits;		// used bits in working set
    uint8_t * bitstream_ptr;	// buffer with stream data

    //Motion vectors
    //The f_ and b_ correspond to the forward and backward motion
    //predictors
    motion_t b_motion;
    motion_t f_motion;

    // predictor for DC coefficients in intra blocks
    int16_t dc_dct_pred[3];

    uint16_t quantizer_scale;	// remove
} slice_t;

typedef struct mpeg2_config_s {
    //Bit flags that enable various things
    uint32_t flags;
} mpeg2_config_t;

//The only global variable,
//the config struct
extern mpeg2_config_t config;



// slice.c
void header_state_init (picture_t * picture);
int header_process_picture_header (picture_t * picture, uint8_t * buffer);
int header_process_sequence_header (picture_t * picture, uint8_t * buffer);
int header_process_extension (picture_t * picture, uint8_t * buffer);

// idct.c
void idct_init (void);

// idct_mlib.c
void idct_block_copy_mlib (int16_t * block, uint8_t * dest, int stride);
void idct_block_add_mlib (int16_t * block, uint8_t * dest, int stride);

// idct_mmx.c
void idct_block_copy_mmxext (int16_t *block, uint8_t * dest, int stride);
void idct_block_add_mmxext (int16_t *block, uint8_t * dest, int stride);
void idct_block_copy_mmx (int16_t *block, uint8_t * dest, int stride);
void idct_block_add_mmx (int16_t *block, uint8_t * dest, int stride);
void idct_mmx_init (void);

// motion_comp.c
void motion_comp_init (void);

typedef struct mc_functions_s
{
    void (* put [8]) (uint8_t *dst, uint8_t *, int32_t, int32_t);
    void (* avg [8]) (uint8_t *dst, uint8_t *, int32_t, int32_t);
} mc_functions_t;

#define MOTION_COMP_EXTERN(x) mc_functions_t mc_functions_##x =		\
{									\
    {MC_put_16_##x, MC_put_x16_##x, MC_put_y16_##x, MC_put_xy16_##x,	\
     MC_put_8_##x,  MC_put_x8_##x,  MC_put_y8_##x,  MC_put_xy8_##x},	\
    {MC_avg_16_##x, MC_avg_x16_##x, MC_avg_y16_##x, MC_avg_xy16_##x,	\
     MC_avg_8_##x,  MC_avg_x8_##x,  MC_avg_y8_##x,  MC_avg_xy8_##x}	\
};

extern mc_functions_t mc_functions_c;
extern mc_functions_t mc_functions_mmx;
extern mc_functions_t mc_functions_mmxext;
extern mc_functions_t mc_functions_3dnow;
extern mc_functions_t mc_functions_mlib;

// slice.c
int slice_process (picture_t *picture, uint8_t code, uint8_t * buffer);

// stats.c
void stats_header (uint8_t code, uint8_t * buffer);

#define MBC 45
#define MBR 36
extern int quant_store[MBR+1][MBC+1]; // [Review]

