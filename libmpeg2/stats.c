/*
 * stats.c
 * Copyright (C) 1999-2001 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "mpeg2_internal.h"

static int debug_level = -1;

/* Determine is debug output is required. */
/* We could potentially have multiple levels of debug info */
static int debug_is_on (void)
{
    char * env_var;
	
    if (debug_level < 0) {
	env_var = getenv ("MPEG2_DEBUG");

	if (env_var)
	    debug_level = 1;
	else
	    debug_level = 0;
    }
	
    return debug_level;
}

static void stats_picture (uint8_t * buffer)
{
    static char * picture_coding_type_str [8] = {
	"Invalid picture type",
	"I-type",
	"P-type",
	"B-type",
	"D (very bad)",
	"Invalid","Invalid","Invalid"
    };

    int picture_coding_type;
    int temporal_reference;
    int vbv_delay;

    temporal_reference = (buffer[0] << 2) | (buffer[1] >> 6);
    picture_coding_type = (buffer [1] >> 3) & 7;
    vbv_delay = ((buffer[1] << 13) | (buffer[2] << 5) |
		 (buffer[3] >> 3)) & 0xffff;

    fprintf (stderr, " (picture) %s temporal_reference %d, vbv_delay %d\n",
	     picture_coding_type_str [picture_coding_type],
	     temporal_reference, vbv_delay);
}

static void stats_user_data (uint8_t * buffer)
{
    fprintf (stderr, " (user_data)\n");
}

static void stats_sequence (uint8_t * buffer)
{
    static char * aspect_ratio_information_str[8] = {
	"Invalid Aspect Ratio",
	"1:1",
	"4:3",
	"16:9",
	"2.21:1",
	"Invalid Aspect Ratio",
	"Invalid Aspect Ratio",
	"Invalid Aspect Ratio"
    };
    static char * frame_rate_str[16] = {
	"Invalid frame_rate_code",
	"23.976", "24", "25" , "29.97",
	"30" , "50", "59.94", "60" ,
	"Invalid frame_rate_code", "Invalid frame_rate_code",
	"Invalid frame_rate_code", "Invalid frame_rate_code",
	"Invalid frame_rate_code", "Invalid frame_rate_code",
	"Invalid frame_rate_code"
    };

    int horizontal_size;
    int vertical_size;
    int aspect_ratio_information;
    int frame_rate_code;
    int bit_rate_value;
    int vbv_buffer_size_value;
    int constrained_parameters_flag;
    int load_intra_quantizer_matrix;
    int load_non_intra_quantizer_matrix;

    vertical_size = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
    horizontal_size = vertical_size >> 12;
    vertical_size &= 0xfff;
    aspect_ratio_information = buffer[3] >> 4;
    frame_rate_code = buffer[3] & 15;
    bit_rate_value = (buffer[4] << 10) | (buffer[5] << 2) | (buffer[6] >> 6);
    vbv_buffer_size_value = ((buffer[6] << 5) | (buffer[7] >> 3)) & 0x3ff;
    constrained_parameters_flag = buffer[7] & 4;
    load_intra_quantizer_matrix = buffer[7] & 2;
    if (load_intra_quantizer_matrix)
	buffer += 64;
    load_non_intra_quantizer_matrix = buffer[7] & 1;

    fprintf (stderr, " (seq) %dx%d %s, %s fps, %5.0f kbps, VBV %d kB%s%s%s\n",
	     horizontal_size, vertical_size,
	     aspect_ratio_information_str [aspect_ratio_information],
	     frame_rate_str [frame_rate_code],
	     bit_rate_value * 400.0 / 1000.0,
	     2 * vbv_buffer_size_value,
	     constrained_parameters_flag ? " , CP":"",
	     load_intra_quantizer_matrix ? " , Custom Intra Matrix":"",
	     load_non_intra_quantizer_matrix ? " , Custom Non-Intra Matrix":"");
}

static void stats_sequence_error (uint8_t * buffer)
{
    fprintf (stderr, " (sequence_error)\n");
}

static void stats_sequence_end (uint8_t * buffer)
{
    fprintf (stderr, " (sequence_end)\n");
}

static void stats_group (uint8_t * buffer)
{
    fprintf (stderr, " (group)%s%s\n",
	     (buffer[4] & 0x40) ? " closed_gop" : "",
	     (buffer[4] & 0x20) ? " broken_link" : "");
}

static void stats_slice (uint8_t code, uint8_t * buffer)
{
    /* fprintf (stderr, " (slice %d)\n", code); */
}

static void stats_sequence_extension (uint8_t * buffer)
{
    static char * chroma_format_str[4] = {
	"Invalid Chroma Format",
	"4:2:0 Chroma",
	"4:2:2 Chroma",
	"4:4:4 Chroma"
    };

    int progressive_sequence;
    int chroma_format;

    progressive_sequence = (buffer[1] >> 3) & 1;
    chroma_format = (buffer[1] >> 1) & 3;

    fprintf (stderr, " (seq_ext) progressive_sequence %d, %s\n",
	     progressive_sequence, chroma_format_str [chroma_format]);
}

static void stats_sequence_display_extension (uint8_t * buffer)
{
    fprintf (stderr, " (sequence_display_extension)\n");
}

static void stats_quant_matrix_extension (uint8_t * buffer)
{
    fprintf (stderr, " (quant_matrix_extension)\n");
}

static void stats_copyright_extension (uint8_t * buffer)
{
    fprintf (stderr, " (copyright_extension)\n");
}


static void stats_sequence_scalable_extension (uint8_t * buffer)
{
    fprintf (stderr, " (sequence_scalable_extension)\n");
}

static void stats_picture_display_extension (uint8_t * buffer)
{
    fprintf (stderr, " (picture_display_extension)\n");
}

static void stats_picture_coding_extension (uint8_t * buffer)
{
    static char * picture_structure_str[4] = {
	"Invalid Picture Structure",
	"Top field",
	"Bottom field",
	"Frame Picture"
    };

    int f_code[2][2];
    int intra_dc_precision;
    int picture_structure;
    int top_field_first;
    int frame_pred_frame_dct;
    int concealment_motion_vectors;
    int q_scale_type;
    int intra_vlc_format;
    int alternate_scan;
    int repeat_first_field;
    int progressive_frame;

    f_code[0][0] = buffer[0] & 15;
    f_code[0][1] = buffer[1] >> 4;
    f_code[1][0] = buffer[1] & 15;
    f_code[1][1] = buffer[2] >> 4;
    intra_dc_precision = (buffer[2] >> 2) & 3;
    picture_structure = buffer[2] & 3;
    top_field_first = buffer[3] >> 7;
    frame_pred_frame_dct = (buffer[3] >> 6) & 1;
    concealment_motion_vectors = (buffer[3] >> 5) & 1;
    q_scale_type = (buffer[3] >> 4) & 1;
    intra_vlc_format = (buffer[3] >> 3) & 1;
    alternate_scan = (buffer[3] >> 2) & 1;
    repeat_first_field = (buffer[3] >> 1) & 1;
    progressive_frame = buffer[4] >> 7;

    fprintf (stderr,
	     " (pic_ext) %s\n", picture_structure_str [picture_structure]);
    fprintf (stderr,
	     " (pic_ext) forward horizontal f_code % d, forward vertical f_code % d\n",
	     f_code[0][0], f_code[0][1]);
    fprintf (stderr,
	     " (pic_ext) backward horizontal f_code % d, backward vertical f_code % d\n", 
	     f_code[1][0], f_code[1][1]);
    fprintf (stderr,
	     " (pic_ext) intra_dc_precision %d, top_field_first %d, frame_pred_frame_dct %d\n",
	     intra_dc_precision, top_field_first, frame_pred_frame_dct);
    fprintf (stderr,
	     " (pic_ext) concealment_motion_vectors %d, q_scale_type %d, intra_vlc_format %d\n",
	     concealment_motion_vectors, q_scale_type, intra_vlc_format);
    fprintf (stderr,
	     " (pic_ext) alternate_scan %d, repeat_first_field %d, progressive_frame %d\n",
	     alternate_scan, repeat_first_field, progressive_frame);
}

void stats_header (uint8_t code, uint8_t * buffer)
{
    if (! (debug_is_on ()))
	return;

    switch (code) {
    case 0x00:
	stats_picture (buffer);
	break;
    case 0xb2:
	stats_user_data (buffer);
	break;
    case 0xb3:
	stats_sequence (buffer);
	break;
    case 0xb4:
	stats_sequence_error (buffer);
	break;
    case 0xb5:
	switch (buffer[0] >> 4) {
	case 1:
	    stats_sequence_extension (buffer);
	    break;
	case 2:
	    stats_sequence_display_extension (buffer);
	    break;
	case 3:
	    stats_quant_matrix_extension (buffer);
	    break;
	case 4:
	    stats_copyright_extension (buffer);
	    break;
	case 5:
	    stats_sequence_scalable_extension (buffer);
	    break;
	case 7:
	    stats_picture_display_extension (buffer);
	    break;
	case 8:
	    stats_picture_coding_extension (buffer);
	    break;
	default:
	    fprintf (stderr, " (unknown extension %#x)\n", buffer[0] >> 4);
	}
	break;
    case 0xb7:
	stats_sequence_end (buffer);
	break;
    case 0xb8:
	stats_group (buffer);
	break;
    default:
	if (code < 0xb0)
	    stats_slice (code, buffer);
	else
	    fprintf (stderr, " (unknown start code %#02x)\n", code);
    }
}
