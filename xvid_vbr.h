/******************************************************************************
 *
 *   XviD VBR Library
 *   
 *   Copyright (C) 2002 Edouard Gomez <ed.gomez@wanadoo.fr>
 *
 *   The curve treatment algorithm is based on work done by Foxer <email?> and
 *   Dirk Knop <dknop@gwdg.de> for the XviD vfw dynamic library.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *****************************************************************************/

#ifndef __XVID_VBR_H__
#define __XVID_VBR_H__

#define VBR_VERSION 0

/******************************************************************************
 * Function types used in the vbr controler
 *****************************************************************************/

typedef int (vbr_init_function)(void *state);
typedef vbr_init_function *vbr_init_function_ptr;

typedef int (vbr_get_quant_function)(void *state);
typedef vbr_get_quant_function *vbr_get_quant_function_ptr;

typedef int (vbr_get_intra_function)(void *state);
typedef vbr_get_intra_function *vbr_get_intra_function_ptr;

typedef int (vbr_update_function)(void *state,
				  int quant,
				  int intra,
				  int header_bytes,
				  int total_bytes,
				  int kblocks,
				  int mblocks,
				  int ublocks);
typedef vbr_update_function *vbr_update_function_ptr;

typedef int (vbr_finish_function)(void *state);
typedef vbr_finish_function *vbr_finish_function_ptr;

/******************************************************************************
 * The VBR CONTROLER structure - the spin of the library
 *****************************************************************************/

typedef struct _vbr_control_t
{

	/* All modes - specifies what VBR algorithm has to be used */
	int mode;

	/* All modes - specifies what fps the movie uses */
	float fps;

	/* All modes */
	int debug;

	/*
	 * For VBR_MODE_2PASS_1/2 - specifies from/to what file the vbr
	 * controller has to write/read stats
	 */
	char *filename;

	/* For VBR_MODE_2PASS_2 - Target size */
	int desired_bitrate;

	/* For VBR_MODE_2PASS_2 - Credits parameters */
	int credits_mode;
	int credits_start;
	int credits_start_begin;
	int credits_start_end;
	int credits_end;
	int credits_end_begin;
	int credits_end_end;
	int credits_quant_ratio;
	int credits_fixed_quant;
	int credits_quant_i;
	int credits_quant_p;
	int credits_start_size;
	int credits_end_size;

	/* For VBR_MODE_2PASS_2 - keyframe parameters */
	int keyframe_boost;
	int kftreshold;
	int kfreduction;
	int min_key_interval;
	int max_key_interval;

	/* For VBR_MODE_2PASS_2 - Normal curve */
	int curve_compression_high;
	int curve_compression_low;

	/* For VBR_MODE_2PASS_2 - Alternate curve parameters */
	int use_alt_curve;
	int alt_curve_type;
	int alt_curve_low_dist;
	int alt_curve_high_dist;
	int alt_curve_min_rel_qual;
	int alt_curve_use_auto;
	int alt_curve_auto_str;
	int alt_curve_use_auto_bonus_bias;
	int alt_curve_bonus_bias;
	int bitrate_payback_method;
	int bitrate_payback_delay;
	int max_iquant;
	int min_iquant;
	int max_pquant;
	int min_pquant;
	int twopass_max_bitrate;
	int twopass_max_overflow_improvement;
	int twopass_max_overflow_degradation;

	/*
	 * For VBR_MODE_FIXED_QUANT - the quantizer that has to be used for all
	 * frames
	 */
	int fixed_quant;

	/* ----------- Internal data - Do not modify ----------- */
	void *debug_file;
	void *pass1_file;

	long long desired_size;

	int cur_frame;
	int nb_frames;
	int nb_keyframes;

	int *keyframe_locations;
	int last_keyframe;

	double credits_start_curve;
	double credits_end_curve;
	double movie_curve;
	double average_frame;
	double alt_curve_low;
	double alt_curve_low_diff;
	double alt_curve_high;
	double alt_curve_high_diff;
	double alt_curve_mid_qual;
	double alt_curve_qual_dev;
	double curve_bias_bonus;
	double curve_comp_scale;
	double curve_comp_error;

	int pass1_quant;
	int pass1_intra;
	int pass1_bytes;

	int bytes1;
	int bytes2;
	int desired_bytes2;
	int max_framesize;
	int last_quant;
	int quant_count[32];
	double quant_error[32];

	int overflow;
	int KFoverflow;
	int KFoverflow_partial;
	int KF_idx;

	int debug_quant_count[32];

	/* ----------- Internal data - do not modify ----------- */
	vbr_init_function_ptr      init;
	vbr_get_quant_function_ptr getquant;
	vbr_get_intra_function_ptr getintra;	
	vbr_update_function_ptr    update;
	vbr_finish_function_ptr    finish;

}vbr_control_t;

/******************************************************************************
 * Constants
 *****************************************************************************/

/* Constants for the mode member */
#define VBR_MODE_1PASS       0x01
#define VBR_MODE_2PASS_1     0x02
#define VBR_MODE_2PASS_2     0x04
#define VBR_MODE_FIXED_QUANT 0x08

/* Constants for the credits mode */
#define VBR_CREDITS_MODE_RATE  0x01
#define VBR_CREDITS_MODE_QUANT 0x02
#define VBR_CREDITS_MODE_SIZE  0x04

/* Alternate curve treatment types */
#define VBR_ALT_CURVE_SOFT      0x01
#define VBR_ALT_CURVE_LINEAR    0x02
#define VBR_ALT_CURVE_AGGRESIVE 0x04

/* Payback modes */
#define VBR_PAYBACK_BIAS            0x01
#define VBR_PAYBACK_PROPORTIONAL    0x02

/******************************************************************************
 * VBR API
 *****************************************************************************/

extern int vbrSetDefaults(vbr_control_t *state);
extern int vbrInit(vbr_control_t *state);
extern int vbrGetQuant(vbr_control_t *state);
extern int vbrGetIntra(vbr_control_t *state);
extern int vbrUpdate(vbr_control_t *state,
		     int quant,
		     int intra,
		     int header_bytes,
		     int total_bytes,
		     int kblocks,
		     int mblocks,
		     int ublocks);
extern int vbrFinish(vbr_control_t *state);

#endif
