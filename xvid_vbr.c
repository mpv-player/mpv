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

/* Standard Headers */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <math.h>

/* Local headers */
#include "xvid_vbr.h"

/******************************************************************************
 * Build time constants
 *****************************************************************************/

/*
 * Portability note
 * Perhaps the msvc headers define Pi with another constant name
 */
#define DEG2RAD (M_PI / 180.0)

/* Defaults settings will be computed with the help of these constants */
#define DEFAULT_DESIRED_SIZE    700
#define DEFAULT_AUDIO_BITRATE   128
#define DEFAULT_MOVIE_LENGTH      2
#define DEFAULT_TWOPASS_BOOST  1000
#define DEFAULT_FPS           25.0f
#define DEFAULT_CREDITS_SIZE      0

#define DEFAULT_XVID_DBG_FILE   "xvid.dbg"
#define DEFAULT_XVID_STATS_FILE "xvid.stats"


/******************************************************************************
 * Local prototypes
 *****************************************************************************/

/* Sub vbrInit cases functions */
static vbr_init_function vbr_init_dummy;
static vbr_init_function vbr_init_2pass1;
static vbr_init_function vbr_init_2pass2;
static vbr_init_function vbr_init_fixedquant;

/* Sub vbrGetQuant cases functions */
static vbr_get_quant_function vbr_getquant_1pass;
static vbr_get_quant_function vbr_getquant_2pass1;
static vbr_get_quant_function vbr_getquant_2pass2;
static vbr_get_quant_function vbr_getquant_fixedquant;

/* Sub vbrGetIntra cases functions */
static vbr_get_intra_function vbr_getintra_1pass;
static vbr_get_intra_function vbr_getintra_2pass1;
static vbr_get_intra_function vbr_getintra_2pass2;
static vbr_get_intra_function vbr_getintra_fixedquant;

/* Sub vbrUpdate prototypes */
static vbr_update_function vbr_update_dummy;
static vbr_update_function vbr_update_2pass1;
static vbr_update_function vbr_update_2pass2;

/* Sub vbrFinish cases functions */
static vbr_finish_function vbr_finish_dummy;
static vbr_finish_function vbr_finish_2pass1;
static vbr_finish_function vbr_finish_2pass2;

/* Is the encoder in the credits */
#define FRAME_TYPE_NORMAL_MOVIE     0x00
#define FRAME_TYPE_STARTING_CREDITS 0x01
#define FRAME_TYPE_ENDING_CREDITS   0x02

/******************************************************************************
 * Inline utility functions
 *****************************************************************************/

static inline int util_frametype(vbr_control_t *state)
{

	if(state->credits_start) {

		if(state->cur_frame >= state->credits_start_begin &&
		   state->cur_frame < state->credits_start_end)
			return(FRAME_TYPE_STARTING_CREDITS);

	}

	if(state->credits_end) {

		if(state->cur_frame >= state->credits_end_begin &&
		   state->cur_frame < state->credits_end_end)
			return(FRAME_TYPE_ENDING_CREDITS);

	}

	return(FRAME_TYPE_NORMAL_MOVIE);


}

static inline int util_creditsframes(vbr_control_t *state)
{

	int frames = 0;

	if(state->credits_start)
		frames += state->credits_start_end - state->credits_start_begin;
	if(state->credits_end)
		frames += state->credits_end_end - state->credits_end_begin;

	return(frames);

}

/******************************************************************************
 * Functions
 *****************************************************************************/

/*****************************************************************************
 * Function description :
 *
 * This function initialiazes the vbr_control_t with safe defaults for all
 * modes.
 *
 * Return Values :
 *   = 0
 ****************************************************************************/

int vbrSetDefaults(vbr_control_t *state)
{

	/* Set all the structure to zero */
	memset(state, 0, sizeof(state));

	/* Default mode is CBR */
	state->mode = VBR_MODE_1PASS;

	/* Default statistic filename */
	state->filename = DEFAULT_XVID_STATS_FILE;

	/*
	 * Default is a 2hour movie on 700Mo CD-ROM + 128kbit sound track
	 * This represents a target bitrate of 687kbit/s
	 */
	state->desired_size = DEFAULT_DESIRED_SIZE*1024*1024 -
		DEFAULT_MOVIE_LENGTH*3600*DEFAULT_AUDIO_BITRATE*1000/8;
	state->desired_bitrate = state->desired_size*8/(DEFAULT_MOVIE_LENGTH*3600);

	/* Credits */
	state->credits_mode = VBR_CREDITS_MODE_RATE;
	state->credits_start = 0;
	state->credits_start_begin = 0;
	state->credits_start_end = 0;
	state->credits_end = 0;
	state->credits_end_begin = 0;
	state->credits_end_end = 0;
	state->credits_quant_ratio = 20;
	state->credits_fixed_quant = 20;
	state->credits_quant_i = 20;
	state->credits_quant_p = 20;
	state->credits_start_size = DEFAULT_CREDITS_SIZE*1024*1024;
	state->credits_end_size = DEFAULT_CREDITS_SIZE*1024*1024;

	/* Keyframe boost */
	state->keyframe_boost = 0;
	state->kftreshold = 10;
	state->kfreduction = 30;
	state->min_key_interval = 1;
	state->max_key_interval = (int)DEFAULT_FPS*10;

	/* Normal curve treatment */
	state->curve_compression_high = 25;
	state->curve_compression_low = 10;

	/* Alt curve */
	state->use_alt_curve = 1;
	state->alt_curve_type = VBR_ALT_CURVE_LINEAR;
	state->alt_curve_low_dist = 90;
	state->alt_curve_high_dist = 500;
	state->alt_curve_min_rel_qual = 50;
	state->alt_curve_use_auto = 1;
	state->alt_curve_auto_str = 30;
	state->alt_curve_use_auto_bonus_bias = 1;
	state->alt_curve_bonus_bias = 50;
	state->bitrate_payback_method = VBR_PAYBACK_BIAS;
	state->bitrate_payback_delay = 250;
	state->twopass_max_bitrate = DEFAULT_TWOPASS_BOOST*state->desired_bitrate;
	state->twopass_max_overflow_improvement = 60;
	state->twopass_max_overflow_degradation = 60;
	state->max_iquant = 31;
	state->min_iquant = 2;
	state->max_pquant = 31;
	state->min_pquant = 2;
	state->fixed_quant = 3;

	state->max_framesize = (1.0/(float)DEFAULT_FPS) * state->twopass_max_bitrate / 8;

	state->fps = (float)DEFAULT_FPS;

	return(0);

}

/*****************************************************************************
 * Function description :
 *
 * This function initialiaze the vbr_control_t state passed in parameter.
 *
 * The initialization depends on state->mode, there are 4 modes allowed.
 * Each mode description is done in the README file shipped with the lib.
 *
 * Return values :
 *
 *    =  0 on success
 *    = -1 on error
 *****************************************************************************/

int vbrInit(vbr_control_t *state)
{

	if(state == NULL) return(-1);

	/* Function pointers safe initialization */
	state->init     = NULL;
	state->getquant = NULL;
	state->getintra = NULL;
	state->update   = NULL;
	state->finish   = NULL;

	if(state->debug) {

		state->debug_file = fopen(DEFAULT_XVID_DBG_FILE, "w+");

		if(state->debug_file == NULL)
			return(-1);

		fprintf(state->debug_file, "# XviD Debug output\n");
		fprintf(state->debug_file, "# quant | intra | header bytes"
			"| total bytes | kblocks | mblocks | ublocks"
			"| vbr overflow | vbr kf overflow"
			"| vbr kf partial overflow\n\n");
	}

	/* Function pointers sub case initialization */
	switch(state->mode) {
	case VBR_MODE_1PASS:
		state->init     = vbr_init_dummy;
		state->getquant = vbr_getquant_1pass;
		state->getintra = vbr_getintra_1pass;
		state->update   = vbr_update_dummy;
		state->finish   = vbr_finish_dummy;
		break;
	case VBR_MODE_2PASS_1:
		state->init     = vbr_init_2pass1;
		state->getquant = vbr_getquant_2pass1;
		state->getintra = vbr_getintra_2pass1;
		state->update   = vbr_update_2pass1;
		state->finish   = vbr_finish_2pass1;
		break;
	case VBR_MODE_FIXED_QUANT:
		state->init     = vbr_init_fixedquant;
		state->getquant = vbr_getquant_fixedquant;
		state->getintra = vbr_getintra_fixedquant;
		state->update   = vbr_update_dummy;
		state->finish   = vbr_finish_dummy;
		break;
	case VBR_MODE_2PASS_2:
		state->init     = vbr_init_2pass2;
		state->getintra = vbr_getintra_2pass2;
		state->getquant = vbr_getquant_2pass2;
		state->update   = vbr_update_2pass2;
		state->finish   = vbr_finish_2pass2;
		break;
	default:
		return(-1);
	}

	return(state->init(state));

}

/******************************************************************************
 * Function description :
 *
 * This function returns an adapted quantizer according to the current vbr
 * controler state
 *
 * Return values :
 *  the quantizer value (0 <= value <= 31)
 *  (0 is a special case, means : let XviD decide)
 *
 *****************************************************************************/

int vbrGetQuant(vbr_control_t *state)
{

	/* Returns Zero, so XviD decides alone */
	if(state == NULL || state->getquant == NULL) return(0);

	return(state->getquant(state));

}

/******************************************************************************
 * Function description :
 *
 * This function returns the type of the frame to be encoded next (I or P/B)
 *
 * Return values :
 *  = -1 let the XviD encoder decide wether or not the next frame is I
 *  =  0 no I frame
 *  =  1 force keyframe
 *
 *****************************************************************************/

int vbrGetIntra(vbr_control_t *state)
{

	/* Returns -1, means let XviD decide */
	if(state == NULL || state->getintra == NULL) return(-1);

	return(state->getintra(state));

}

/******************************************************************************
 * Function description :
 *
 * This function updates the vbr control state according to collected statistics
 * from XviD core
 *
 * Return values :
 *
 *    =  0 on success
 *    = -1 on error
 *****************************************************************************/

int vbrUpdate(vbr_control_t *state,
	      int quant,
	      int intra,
	      int header_bytes,
	      int total_bytes,
	      int kblocks,
	      int mblocks,
	      int ublocks)
{

	if(state == NULL || state->update == NULL) return(-1);

	if(state->debug && state->debug_file != NULL) {
		int idx;

		fprintf(state->debug_file, "%d %d %d %d %d %d %d %d %d %d\n",
			quant, intra, header_bytes, total_bytes, kblocks,
			mblocks, ublocks, state->overflow, state->KFoverflow,
			state->KFoverflow_partial);

		idx = quant;

		if(quant < 1)
			idx = 1;
		if(quant > 31)
			idx = 31;

		idx--;

		state->debug_quant_count[idx]++;

	}

	return(state->update(state, quant, intra, header_bytes, total_bytes,
			     kblocks, mblocks, ublocks));

}

/******************************************************************************
 * Function description :
 *
 * This function stops the vbr controller
 *
 * Return values :
 *
 *    =  0 on success
 *    = -1 on error
 *****************************************************************************/

int vbrFinish(vbr_control_t *state)
{

	if(state == NULL || state->finish == NULL) return(-1);

	if(state->debug && state->debug_file != NULL) {

		int i;

		fprintf(state->debug_file, "\n\n");

		for(i=0; i<79; i++)
			fprintf(state->debug_file, "#");

		fprintf(state->debug_file, "\n# Quantizer distribution :\n\n");

		for(i=0;i<32; i++) {

			fprintf(state->debug_file, "# quant %d : %d\n",
				i+1,
				state->debug_quant_count[i]);

		}

		fclose(state->debug_file);

	}

	return(state->finish(state));

}

/******************************************************************************
 * Dummy functions - Used when a mode does not need such a function
 *****************************************************************************/

static int vbr_init_dummy(void *sstate)
{

	vbr_control_t *state = sstate;

	state->cur_frame = 0;

	return(0);

}

static int vbr_update_dummy(void *state,
			    int quant,
			    int intra,
			    int header_bytes,
			    int total_bytes,
			    int kblocks,
			    int mblocks,
			    int ublocks)
{

	((vbr_control_t*)state)->cur_frame++;

	return(0);

}

static int vbr_finish_dummy(void *state)
{

	return(0);

}

/******************************************************************************
 * 1 pass mode - XviD will do its job alone.
 *****************************************************************************/

static int vbr_getquant_1pass(void *state)
{

	return(0);

}

static int vbr_getintra_1pass(void *state)
{

	return(-1);

}

/******************************************************************************
 * 2 pass mode - first pass functions
 *****************************************************************************/

static int vbr_init_2pass1(void *sstate)
{

	FILE *f;
	vbr_control_t *state = sstate;

	/* Check the filename */
	if(state->filename == NULL || state->filename[0] == '\0')
		return(-1);

	/* Initialize safe defaults for 2pass 1 */
	state->pass1_file = NULL;
	state->nb_frames = 0;
	state->nb_keyframes = 0;
	state->cur_frame = 0;

	/* Open the 1st pass file */
	if((f = fopen(state->filename, "w+")) == NULL)
		return(-1);

	/*
	 * The File Header
	 *
	 * The extra white spaces will be used during the vbrFinish to write
	 * the resulting number of frames and keyframes (10 spaces == maximum
	 * string length of an int on 32bit machines, i don't think anyone is
	 * encoding more than 4 billion frames :-)
	 */
	fprintf(f, "# ASCII XviD vbr stat file version %d\n#\n", VBR_VERSION);
	fprintf(f, "# frames    :           \n");
	fprintf(f, "# keyframes :           \n");
	fprintf(f, "#\n# quant | intra | header bytes | total bytes | kblocks |"
		" mblocks | ublocks\n\n");

	/* Save file pointer */
	state->pass1_file   = f;

	return(0);

}

static int vbr_getquant_2pass1(void *state)
{

	return(2);

}

static int vbr_getintra_2pass1(void *state)
{

	return(-1);

}

static int vbr_update_2pass1(void *sstate,
			     int quant,
			     int intra,
			     int header_bytes,
			     int total_bytes,
			     int kblocks,
			     int mblocks,
			     int ublocks)


{

	vbr_control_t *state = sstate;

	if(state->pass1_file == NULL)
		return(-1);

	/* Writes the resulting statistics */
	fprintf(state->pass1_file, "%d %d %d %d %d %d %d\n",
		quant,
		intra,
		header_bytes,
		total_bytes,
		kblocks,
		mblocks,
		ublocks);

	/* Update vbr control state */
	if(intra) state->nb_keyframes++;
	state->nb_frames++;
	state->cur_frame++;

	return(0);

}

static int vbr_finish_2pass1(void *sstate)
{

	int c, i;
	vbr_control_t *state = sstate;

	if(state->pass1_file == NULL)
		return(-1);

	/* Goto to the file beginning */
	fseek(state->pass1_file, 0, SEEK_SET);

	/* Skip the version line and the empty line */
	c = i = 0;
	do {
		c = fgetc(state->pass1_file);

		if(c == EOF) return(-1);
		if(c == '\n') i++;

	}while(i < 2);

	/* Prepare to write to the stream */
	fseek( state->pass1_file, 0L, SEEK_CUR );

	/* Overwrite the frame field - safe as we have written extra spaces */
	fprintf(state->pass1_file, "# frames    : %.10d\n", state->nb_frames);

	/* Overwrite the keyframe field */
	fprintf(state->pass1_file, "# keyframes : %.10d\n",
		state->nb_keyframes);

	/* Close the file */
	if(fclose(state->pass1_file) != 0)
		return(-1);

	return(0);

}

/******************************************************************************
 * 2 pass mode - 2nd pass functions (Need to be finished)
 *****************************************************************************/

static int vbr_init_2pass2(void *sstate)
{

	FILE *f;
	int c, n, pos_firstframe, credits_frames;
	long long credits1_bytes;
	long long credits2_bytes;
	long long desired;
	long long total_bytes;
	long long itotal_bytes;
	long long start_curved;
	long long end_curved;
	double total1;
	double total2;

	vbr_control_t *state = sstate;

	/* Check the filename */
	if(state->filename == NULL || state->filename[0] == '\0')
		return(-1);

	/* Initialize safe defaults for 2pass 2 */
	state->pass1_file = NULL;
	state->nb_frames = 0;
	state->nb_keyframes = 0;

	/* Open the 1st pass file */
	if((f = fopen(state->filename, "r")) == NULL)
		return(-1);

	state->pass1_file = f;

	/* Get the file version and check against current version */
	fscanf(state->pass1_file, "# ASCII XviD vbr stat file version %d\n", &n);

	if(n != VBR_VERSION) {
		fclose(state->pass1_file);
		state->pass1_file = NULL;
		return(-1);
	}

	/* Skip the blank commented line */
	c = n = 0;
	do {

		c = fgetc(state->pass1_file);

		if(c == EOF) {
			fclose(state->pass1_file);
			state->pass1_file = NULL;
			return(-1);
		}

		if(c == '\n') n++;

	}while(n < 1);


	/* Get the number of frames */
	fscanf(state->pass1_file, "# frames : %d\n", &state->nb_frames);

	/* Compute the desired size */
	state->desired_size = (long long)
		(((long long)state->nb_frames * (long long)state->desired_bitrate) /
		 (state->fps * 8.0));

	/* Get the number of keyframes */
	fscanf(state->pass1_file, "# keyframes : %d\n", &state->nb_keyframes);

	/* Allocate memory space for the keyframe_location array */
	if((state->keyframe_locations
	    = malloc((state->nb_keyframes+1)*sizeof(int))) == NULL) {
		fclose(state->pass1_file);
		state->pass1_file = NULL;
		return(-1);
	}

	/* Skip the blank commented line and the colum description */
	c = n = 0;
	do {

		c = fgetc(state->pass1_file);

		if(c == EOF) {
			fclose(state->pass1_file);
			state->pass1_file = NULL;
			return(-1);
		}

		if(c == '\n') n++;

	}while(n < 2);

	/* Save position for future use */
	pos_firstframe = ftell(state->pass1_file);

	/* Read and initialize some variables */
	credits1_bytes = credits2_bytes = 0;
	total_bytes = itotal_bytes = 0;
	start_curved = end_curved = 0;
	credits_frames = 0;

	for(state->cur_frame = c = 0; state->cur_frame<state->nb_frames; state->cur_frame++) {

		int quant, keyframe, frame_hbytes, frame_bytes;
		int kblocks, mblocks, ublocks;

		fscanf(state->pass1_file, "%d %d %d %d %d %d %d\n",
		       &quant, &keyframe, &frame_hbytes, &frame_bytes,
		       &kblocks, &mblocks, &ublocks);

		/* Is the frame in the beginning credits */
		if(util_frametype(state) == FRAME_TYPE_STARTING_CREDITS) {
			credits1_bytes += frame_bytes;
			credits_frames++;
			continue;
		}

		/* Is the frame in the eding credits */
		if(util_frametype(state) == FRAME_TYPE_ENDING_CREDITS) {
			credits2_bytes += frame_bytes;
			credits_frames++;
			continue;
		}

		/* We only care about Keyframes when not in credits */
		if(keyframe) {
			itotal_bytes +=	frame_bytes + frame_bytes *
				state->keyframe_boost / 100;
			total_bytes  += frame_bytes *
				state->keyframe_boost / 100;
			state->keyframe_locations[c++] = state->cur_frame;
		}

		total_bytes += frame_bytes;

	}

	/*
	 * Last frame is treated like an I Frame so we can dispatch overflow
	 * all other the last film segment
	 */
	state->keyframe_locations[c] = state->cur_frame;

	desired = state->desired_size;

	switch(state->credits_mode) {
	case VBR_CREDITS_MODE_QUANT :

		state->movie_curve = (double)
			(total_bytes - credits1_bytes - credits2_bytes) /
			(desired  - credits1_bytes - credits2_bytes);

		start_curved = credits1_bytes;
		end_curved   = credits2_bytes;

		break;
	case VBR_CREDITS_MODE_SIZE:

		/* start curve = (start / start desired size) */
		state->credits_start_curve = (double)
			(credits1_bytes / state->credits_start_size);

		/* end curve = (end / end desired size) */
		state->credits_end_curve = (double)
			(credits2_bytes / state->credits_end_size);

		start_curved = (long long)
			(credits1_bytes / state->credits_start_curve);

		end_curved   = (long long)
			(credits2_bytes / state->credits_end_curve);

		/* movie curve=(total-credits)/(desired_size-curved credits) */
		state->movie_curve = (double)
			(total_bytes - credits1_bytes - credits2_bytes) /
			(desired - start_curved - end_curved);

		break;
	case VBR_CREDITS_MODE_RATE:
	default:

		/* credits curve = (total/desired_size)*(100/credits_rate) */
		state->credits_start_curve = state->credits_end_curve =
			((double)total_bytes / desired) *
			((double)100 / state->credits_quant_ratio);

		start_curved =
			(long long)(credits1_bytes/state->credits_start_curve);

		end_curved   =
			(long long)(credits2_bytes/state->credits_end_curve);

		state->movie_curve = (double)
			(total_bytes - credits1_bytes - credits2_bytes) /
			(desired - start_curved - end_curved);

		break;
	}

	/*
	 * average frame size = (desired - curved credits - curved keyframes) /
	 *                      (frames - credits frames - keyframes)
	 */
	state->average_frame = (double)
		(desired - start_curved - end_curved -
		 (itotal_bytes / state->movie_curve)) /
		(state->nb_frames - util_creditsframes(state) -
		 state->nb_keyframes);

	/* Initialize alt curve parameters */
	if (state->use_alt_curve) {

		state->alt_curve_low =
			state->average_frame - state->average_frame *
			(double)(state->alt_curve_low_dist / 100.0);

		state->alt_curve_low_diff =
			state->average_frame - state->alt_curve_low;

		state->alt_curve_high =
			state->average_frame + state->average_frame *
			(double)(state->alt_curve_high_dist / 100.0);

		state->alt_curve_high_diff =
			state->alt_curve_high - state->average_frame;

		if (state->alt_curve_use_auto) {

			if (state->movie_curve > 1.0)	{

				state->alt_curve_min_rel_qual =
					(int)(100.0 - (100.0 - 100.0 / state->movie_curve) *
					      (double)state->alt_curve_auto_str / 100.0);

				if (state->alt_curve_min_rel_qual < 20)
					state->alt_curve_min_rel_qual = 20;
			}
			else {
				state->alt_curve_min_rel_qual = 100;
			}

		}

		state->alt_curve_mid_qual =
		(1.0 + (double)state->alt_curve_min_rel_qual / 100.0) / 2.0;

		state->alt_curve_qual_dev = 1.0 - state->alt_curve_mid_qual;

		if (state->alt_curve_low_dist > 100) {

			switch(state->alt_curve_type) {
			case VBR_ALT_CURVE_AGGRESIVE:
				/* Sine Curve (high aggressiveness) */
				state->alt_curve_qual_dev *=
					2.0 /
					(1.0 +  sin(DEG2RAD * (state->average_frame * 90.0 / state->alt_curve_low_diff)));

				state->alt_curve_mid_qual =
					1.0 - state->alt_curve_qual_dev *
					sin(DEG2RAD * (state->average_frame * 90.0 / state->alt_curve_low_diff));
				break;

			default:
			case VBR_ALT_CURVE_LINEAR:
				/* Linear (medium aggressiveness) */
				state->alt_curve_qual_dev *=
					2.0 /
					(1.0 + state->average_frame / state->alt_curve_low_diff);

				state->alt_curve_mid_qual =
					1.0 - state->alt_curve_qual_dev *
					state->average_frame / state->alt_curve_low_diff;

				break;

			case VBR_ALT_CURVE_SOFT:
				/* Cosine Curve (low aggressiveness) */
				state->alt_curve_qual_dev *=
					2.0 /
					(1.0 + (1.0 - cos(DEG2RAD * (state->average_frame * 90.0 / state->alt_curve_low_diff))));

				state->alt_curve_mid_qual =
					1.0 - state->alt_curve_qual_dev *
					(1.0 - cos(DEG2RAD * (state->average_frame * 90.0 / state->alt_curve_low_diff)));

				break;
			}
		}
	}

	/* Go to the first non credits frame stats line into file */
	fseek(state->pass1_file, pos_firstframe, SEEK_SET);

	/* Perform prepass to compensate for over/undersizing */
	total1 = total2 = 0.0;
	for(state->cur_frame=0; state->cur_frame<state->nb_frames; state->cur_frame++) {

		int quant, keyframe, frame_hbytes, frame_bytes;
		int kblocks, mblocks, ublocks;

		fscanf(state->pass1_file, "%d %d %d %d %d %d %d\n",
		       &quant, &keyframe, &frame_hbytes, &frame_bytes,
		       &kblocks, &mblocks, &ublocks);

		if(util_frametype(state) != FRAME_TYPE_NORMAL_MOVIE)
			continue;

		if(!keyframe) {

			double dbytes = frame_bytes / state->movie_curve;
			total1 += dbytes;

			if (state->use_alt_curve) {

				if (dbytes > state->average_frame) {

					if (dbytes >= state->alt_curve_high) {
						total2 += dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev);
					}
					else {

						switch(state->alt_curve_type) {
						case VBR_ALT_CURVE_AGGRESIVE:

							total2 +=
								dbytes *
								(state->alt_curve_mid_qual - state->alt_curve_qual_dev *
								 sin(DEG2RAD * ((dbytes - state->average_frame) * 90.0 / state->alt_curve_high_diff)));
							break;
						default:
						case VBR_ALT_CURVE_LINEAR:

							total2 +=
								dbytes *
								(state->alt_curve_mid_qual - state->alt_curve_qual_dev *
								 (dbytes - state->average_frame) / state->alt_curve_high_diff);
							break;
						case VBR_ALT_CURVE_SOFT:
							total2 +=
								dbytes *
								(state->alt_curve_mid_qual - state->alt_curve_qual_dev *
								 (1.0 - cos(DEG2RAD * ((dbytes - state->average_frame) * 90.0 / state->alt_curve_high_diff))));
						}
					}
				}
				else {

					if (dbytes <= state->alt_curve_low) {
						total2 += dbytes;
					}
					else {

						switch(state->alt_curve_type) {
						case VBR_ALT_CURVE_AGGRESIVE:
							total2 +=
								dbytes *
								(state->alt_curve_mid_qual - state->alt_curve_qual_dev *
								 sin(DEG2RAD * ((dbytes - state->average_frame) * 90.0 / state->alt_curve_low_diff)));
							break;
						default:
						case VBR_ALT_CURVE_LINEAR:
							total2 +=
								dbytes *
								(state->alt_curve_mid_qual - state->alt_curve_qual_dev *
								 (dbytes - state->average_frame) / state->alt_curve_low_diff);
							break;
						case VBR_ALT_CURVE_SOFT:
							total2 +=
								dbytes *
								(state->alt_curve_mid_qual + state->alt_curve_qual_dev *
								 (1.0 - cos(DEG2RAD * ((dbytes - state->average_frame) * 90.0 / state->alt_curve_low_diff))));
						}
					}
				}
			}
			else {
				if (dbytes > state->average_frame) {
					total2 +=
						((double)dbytes +
						 (state->average_frame - dbytes) *
						 state->curve_compression_high / 100.0);
				}
				else {
					total2 +=
						((double)dbytes +
						 (state->average_frame - dbytes) *
						 state->curve_compression_low / 100.0);
				}
			}
		}
	}

	state->curve_comp_scale = total1 / total2;

	if (state->use_alt_curve) {

		double curve_temp, dbytes;
		int newquant, percent;
		int oldquant = 1;

		if (state->alt_curve_use_auto_bonus_bias)
			state->alt_curve_bonus_bias = state->alt_curve_min_rel_qual;

		state->curve_bias_bonus =
			(total1 - total2) * (double)state->alt_curve_bonus_bias /
			(100.0 * (double)(state->nb_frames - util_creditsframes(state) - state->nb_keyframes));
		state->curve_comp_scale =
			((total1 - total2) * (1.0 - (double)state->alt_curve_bonus_bias / 100.0) + total2) /
			total2;


		for (n=1; n <= (int)(state->alt_curve_high*2) + 1; n++) {
			dbytes = n;
			if (dbytes > state->average_frame)
			{
				if (dbytes >= state->alt_curve_high) {
					curve_temp = dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev);
				}
				else {
					switch(state->alt_curve_type) {
					case VBR_ALT_CURVE_AGGRESIVE:
						curve_temp = dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev *
								       sin(DEG2RAD * ((dbytes - state->average_frame) * 90.0 / state->alt_curve_high_diff)));
						break;
					default:
					case VBR_ALT_CURVE_LINEAR:
						curve_temp = dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev *
								       (dbytes - state->average_frame) / state->alt_curve_high_diff);
						break;
					case VBR_ALT_CURVE_SOFT:
						curve_temp = dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev *
								       (1.0 - cos(DEG2RAD * ((dbytes - state->average_frame) * 90.0 / state->alt_curve_high_diff))));
					}
				}
			}
			else {
				if (dbytes <= state->alt_curve_low) {
					curve_temp = dbytes;
				}
				else {
					switch(state->alt_curve_type) {
					case VBR_ALT_CURVE_AGGRESIVE:
						curve_temp = dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev *
								       sin(DEG2RAD * ((dbytes - state->average_frame) * 90.0 / state->alt_curve_low_diff)));
						break;
					default:
					case VBR_ALT_CURVE_LINEAR:
						curve_temp = dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev *
								       (dbytes - state->average_frame) / state->alt_curve_low_diff);
						break;
					case VBR_ALT_CURVE_SOFT:
						curve_temp = dbytes * (state->alt_curve_mid_qual + state->alt_curve_qual_dev *
								       (1.0 - cos(DEG2RAD * ((dbytes - state->average_frame) * 90.0 / state->alt_curve_low_diff))));
					}
				}
			}

			if (state->movie_curve > 1.0)
				dbytes *= state->movie_curve;

			newquant = (int)(dbytes * 2.0 / (curve_temp * state->curve_comp_scale + state->curve_bias_bonus));
			if (newquant > 1)
			{
				if (newquant != oldquant)
				{
					oldquant = newquant;
					percent = (int)((n - state->average_frame) * 100.0 / state->average_frame);
				}

			}

		}

	}

	state->overflow = 0;
	state->KFoverflow = 0;
	state->KFoverflow_partial = 0;
	state->KF_idx = 1;

	for (n=0 ; n < 32 ; n++) {
		state->quant_error[n] = 0.0;
		state->quant_count[n] = 0;
	}

	state->curve_comp_error = 0.0;
	state->last_quant = 0;

	/*
	 * Above this frame size limit, normal vbr rules will not apply
	 * This means :
	 *      1 - Quant can de/increase more than -/+2 between 2 frames
	 *      2 - Leads to artifacts because of 1
	 */
	state->max_framesize = state->twopass_max_bitrate/state->fps;

	/* Get back to the beginning of frame statistics */
	fseek(state->pass1_file, pos_firstframe, SEEK_SET);

	/*
	 * Small hack : We have to get next frame stats before the
	 * getintra/quant calls
	 * User clients update the data when they call vbrUpdate
	 * we are just bypassing this because we don't have to update
	 * the overflow and so on...
	 */
	{

		/* Fake vars */
		int next_hbytes, next_kblocks, next_mblocks, next_ublocks;

		fscanf(state->pass1_file, "%d %d %d %d %d %d %d\n",
		       &state->pass1_quant, &state->pass1_intra, &next_hbytes,
		       &state->pass1_bytes, &next_kblocks, &next_mblocks,
		       &next_ublocks);

	}

	/* Initialize the frame counter */
	state->cur_frame = 0;
	state->last_keyframe = 0;

	return(0);

}

static int vbr_getquant_2pass2(void *sstate)
{

	int quant;
	int intra;
	int bytes1, bytes2;
	int overflow;
	int capped_to_max_framesize = 0;
	int KFdistance, KF_min_size;
	vbr_control_t *state = sstate;

	bytes1 = state->pass1_bytes;
	overflow = state->overflow / 8;
	/* To shut up gcc warning */
	bytes2 = bytes1;


	if (state->pass1_intra)
	{
		overflow = 0;
	}

	if (util_frametype(state) != FRAME_TYPE_NORMAL_MOVIE) {


		switch (state->credits_mode) {
		case VBR_CREDITS_MODE_QUANT :
			if (state->credits_quant_i != state->credits_quant_p) {
				quant = state->pass1_intra ?
					state->credits_quant_i:
					state->credits_quant_p;
			}
			else {
				quant = state->credits_quant_p;
			}

			state->bytes1 = bytes1;
			state->bytes2 = bytes1;
			state->desired_bytes2 = bytes1;
			return(quant);
		default:
		case VBR_CREDITS_MODE_RATE :
		case VBR_CREDITS_MODE_SIZE :
			if(util_frametype(state) == FRAME_TYPE_STARTING_CREDITS)
				bytes2 = (int)(bytes1 / state->credits_start_curve);
			else
				bytes2 = (int)(bytes1 / state->credits_end_curve);
			break;
		}
	}
	else {
		/* Foxer: apply curve compression outside credits */
		double dbytes, curve_temp;

		bytes2 = bytes1;

		if (state->pass1_intra)
			dbytes = ((int)(bytes2 + bytes2 * state->keyframe_boost / 100)) /
				state->movie_curve;
		else
			dbytes = bytes2 / state->movie_curve;

		/* spread the compression error accross payback_delay frames */
		if (state->bitrate_payback_method == VBR_PAYBACK_BIAS)	{
			bytes2 = (int)(state->curve_comp_error / state->bitrate_payback_delay);
		}
		else {
			bytes2 = (int)(state->curve_comp_error * dbytes /
				state->average_frame / state->bitrate_payback_delay);

			if (labs(bytes2) > fabs(state->curve_comp_error))
				bytes2 = (int)state->curve_comp_error;
		}

		state->curve_comp_error -= bytes2;

		if (state->use_alt_curve) {

			if (!state->pass1_intra) {

				if (dbytes > state->average_frame) {
					if (dbytes >= state->alt_curve_high)
						curve_temp = dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev);
					else {
						switch(state->alt_curve_type) {
						case VBR_ALT_CURVE_AGGRESIVE:
							curve_temp = dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev *
									       sin(DEG2RAD * ((dbytes - state->average_frame) * 90.0 / state->alt_curve_high_diff)));
							break;
						default:
						case VBR_ALT_CURVE_LINEAR:
							curve_temp = dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev *
									       (dbytes - state->average_frame) / state->alt_curve_high_diff);
							break;
						case VBR_ALT_CURVE_SOFT:
							curve_temp = dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev *
									       (1.0 - cos(DEG2RAD * ((dbytes - state->average_frame) * 90.0 / state->alt_curve_high_diff))));
						}
					}
				}
				else {
					if (dbytes <= state->alt_curve_low)
						curve_temp = dbytes;
					else {
						switch(state->alt_curve_type) {
						case VBR_ALT_CURVE_AGGRESIVE:
							curve_temp = dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev *
									       sin(DEG2RAD * ((dbytes - state->average_frame) * 90.0 / state->alt_curve_low_diff)));
							break;
						default:
						case VBR_ALT_CURVE_LINEAR:
							curve_temp = dbytes * (state->alt_curve_mid_qual - state->alt_curve_qual_dev *
									       (dbytes - state->average_frame) / state->alt_curve_low_diff);
							break;
						case VBR_ALT_CURVE_SOFT:
							curve_temp = dbytes * (state->alt_curve_mid_qual + state->alt_curve_qual_dev *
									       (1.0 - cos(DEG2RAD * ((dbytes - state->average_frame) * 90.0 / state->alt_curve_low_diff))));
						}
					}
				}

				curve_temp = curve_temp * state->curve_comp_scale + state->curve_bias_bonus;

				bytes2 += ((int)curve_temp);
				state->curve_comp_error += curve_temp - ((int)curve_temp);

			}
			else {
				state->curve_comp_error += dbytes - ((int)dbytes);
				bytes2 += ((int)dbytes);
			}
		}
		else if ((state->curve_compression_high + state->curve_compression_low) &&
			!state->pass1_intra) {

			if (dbytes > state->average_frame) {
				curve_temp = state->curve_comp_scale *
					((double)dbytes + (state->average_frame - dbytes) *
					state->curve_compression_high / 100.0);
			}
			else {
				curve_temp = state->curve_comp_scale *
					((double)dbytes + (state->average_frame - dbytes) *
					state->curve_compression_low / 100.0);
			}

			bytes2 += ((int)curve_temp);
			state->curve_comp_error += curve_temp - ((int)curve_temp);
		}
		else {
			state->curve_comp_error += dbytes - ((int)dbytes);
			bytes2 += ((int)dbytes);
		}

		/* cap bytes2 to first pass size, lowers number of quant=1 frames */
		if (bytes2 > bytes1) {
			state->curve_comp_error += bytes2 - bytes1;
			bytes2 = bytes1;
		}
		else if (bytes2 < 1) {
			state->curve_comp_error += --bytes2;
			bytes2 = 1;
		}
	}

	state->desired_bytes2 = bytes2;

	/* Ugly dependence between getquant and getintra */
	intra = state->getintra(state);

	if(intra) {

		KFdistance = state->keyframe_locations[state->KF_idx] -
			state->keyframe_locations[state->KF_idx - 1];

		if (KFdistance < state->kftreshold) {
			KFdistance = KFdistance - state->min_key_interval;

			if (KFdistance >= 0) {

				KF_min_size = bytes2 * (100 - state->kfreduction) / 100;
				if (KF_min_size < 1)
					KF_min_size = 1;

				bytes2 = KF_min_size + (bytes2 - KF_min_size) * KFdistance /
					(state->kftreshold - state->min_key_interval);

				if (bytes2 < 1)
					bytes2 = 1;
			}
		}
	}

	/*
	 * Foxer: scale overflow in relation to average size, so smaller frames don't get
	 * too much/little bitrate
	 */
	overflow = (int)((double)overflow * bytes2 / state->average_frame);

	/* Foxer: reign in overflow with huge frames */
	if (labs(overflow) > labs(state->overflow)) {
		overflow = state->overflow;
	}

	/* Foxer: make sure overflow doesn't run away */
	if(overflow > bytes2 * state->twopass_max_overflow_improvement / 100) {
		bytes2 += (overflow <= bytes2) ? bytes2 * state->twopass_max_overflow_improvement / 100 :
			overflow * state->twopass_max_overflow_improvement / 100;
	}
	else if(overflow < bytes2 * state->twopass_max_overflow_degradation / -100) {
		bytes2 += bytes2 * state->twopass_max_overflow_degradation / -100;
	}
	else {
		bytes2 += overflow;
	}

	if(bytes2 > state->max_framesize) {
		capped_to_max_framesize = 1;
		bytes2 = state->max_framesize;
	}

	if(bytes2 < 1) {
		bytes2 = 1;
	}

	state->bytes1 = bytes1;
	state->bytes2 = bytes2;

	/* very 'simple' quant<->filesize relationship */
	quant = state->pass1_quant * bytes1 / bytes2;

	if(quant < 1)
		quant = 1;
	else if(quant > 31)
		quant = 31;
	else if(!state->pass1_intra) {

		/* Foxer: aid desired quantizer precision by accumulating decision error */
		state->quant_error[quant] += ((double)(state->pass1_quant * bytes1) / bytes2) - quant;

		if (state->quant_error[quant] >= 1.0) {
			state->quant_error[quant] -= 1.0;
			quant++;
		}
	}

	/* we're done with credits */
	if(util_frametype(state) != FRAME_TYPE_NORMAL_MOVIE) {
		return(quant);
	}

	if(intra) {

		if (quant < state->min_iquant)
			quant = state->min_iquant;
		if (quant > state->max_iquant)
			quant = state->max_iquant;
	}
	else {

		if(quant > state->max_pquant)
			quant = state->max_pquant;
		if(quant < state->min_pquant)
			quant = state->min_pquant;

		/* subsequent frame quants can only be +- 2 */
		if(state->last_quant && capped_to_max_framesize == 0) {
			if (quant > state->last_quant + 2)
				quant = state->last_quant + 2;
			if (quant < state->last_quant - 2)
				quant = state->last_quant - 2;
		}
	}

	return(quant);

}

static int vbr_getintra_2pass2(void *sstate)
{

	int intra;
	vbr_control_t *state = sstate;


	/* Get next intra state (fetched by update) */
	intra = state->pass1_intra;

	/* During credits, XviD will decide itself */
	if(util_frametype(state) != FRAME_TYPE_NORMAL_MOVIE) {


		switch(state->credits_mode) {
		default:
		case VBR_CREDITS_MODE_RATE :
		case VBR_CREDITS_MODE_SIZE :
			intra = -1;
			break;
		case VBR_CREDITS_MODE_QUANT :
			/* Except in this case */
			if (state->credits_quant_i == state->credits_quant_p)
				intra = -1;
			break;
		}

	}

	/* Force I Frame when max_key_interval is reached */
	if((state->cur_frame - state->last_keyframe) > state->max_key_interval)
		intra = 1;

	/*
	 * Force P or B Frames for frames whose distance is less than the
	 * requested minimum
	 */
	if((state->cur_frame - state->last_keyframe) < state->min_key_interval)
		intra = 0;


	/* Return the given intra mode except for first frame */
	return((state->cur_frame==0)?1:intra);

}

static int vbr_update_2pass2(void *sstate,
			     int quant,
			     int intra,
			     int header_bytes,
			     int total_bytes,
			     int kblocks,
			     int mblocks,
			     int ublocks)


{


	int next_hbytes, next_kblocks, next_mblocks, next_ublocks;
	int tempdiv;

	vbr_control_t *state = sstate;

	/*
	 * We do not depend on getintra/quant because we have the real results
	 * from the xvid core
	 */

	if (util_frametype(state) == FRAME_TYPE_NORMAL_MOVIE) {

		state->quant_count[quant]++;

		if (state->pass1_intra) {

			state->overflow += state->KFoverflow;
			state->KFoverflow = state->desired_bytes2 - total_bytes;

			tempdiv = (state->keyframe_locations[state->KF_idx] -
				   state->keyframe_locations[state->KF_idx - 1]);

			/* redistribute correctly (by koepi) */
			if (tempdiv > 1) {
				/* non-consecutive keyframes */
				state->KFoverflow_partial = state->KFoverflow /
					(tempdiv - 1);
			}
			else {
				state->overflow  += state->KFoverflow;
				state->KFoverflow = 0;
				state->KFoverflow_partial = 0;
			}
			state->KF_idx++;

		}
		else {
			state->overflow += state->desired_bytes2 - total_bytes +
				state->KFoverflow_partial;
			state->KFoverflow -= state->KFoverflow_partial;
		}
	}
	else {

		state->overflow += state->desired_bytes2 - total_bytes;
		state->overflow += state->KFoverflow;
		state->KFoverflow = 0;
		state->KFoverflow_partial = 0;
	}

	/* Save old quant */
	state->last_quant = quant;

	/* Update next frame data */
	fscanf(state->pass1_file, "%d %d %d %d %d %d %d\n",
	       &state->pass1_quant, &state->pass1_intra, &next_hbytes,
	       &state->pass1_bytes, &next_kblocks, &next_mblocks,
	       &next_ublocks);

	/* Save the last Keyframe pos */
	if(intra)
		state->last_keyframe = state->cur_frame;

	/* Ok next frame */
	state->cur_frame++;

	return(0);

}

static int vbr_finish_2pass2(void *sstate)
{

	vbr_control_t *state = sstate;

	if(state->pass1_file == NULL)
		return(-1);

	/* Close the file */
	if(fclose(state->pass1_file) != 0)
		return(-1);

	/* Free the memory */
	if(state->keyframe_locations)
		free(state->keyframe_locations);

	return(0);

}


/******************************************************************************
 * Fixed quant mode - Most of the functions will be dummy functions
 *****************************************************************************/

static int vbr_init_fixedquant(void *sstate)
{

	vbr_control_t *state = sstate;

	if(state->fixed_quant < 1)
		state->fixed_quant = 1;

	if(state->fixed_quant > 31)
		state->fixed_quant = 31;

	state->cur_frame = 0;

	return(0);

}

static int vbr_getquant_fixedquant(void *sstate)
{

	vbr_control_t *state = sstate;

	/* Credits' frame ? */
	if(util_frametype(state) != FRAME_TYPE_NORMAL_MOVIE) {

		int quant;

		switch(state->credits_mode) {
		case VBR_CREDITS_MODE_RATE:
			quant = state->fixed_quant * state->credits_quant_ratio;
			break;
		case VBR_CREDITS_MODE_QUANT:
			quant = state->credits_fixed_quant;
			break;
		default:
			quant = state->fixed_quant;

		}

		return(quant);

	}

	/* No credit frame - return fixed quant */
	return(state->fixed_quant);

}

static int vbr_getintra_fixedquant(void *state)
{

	return(-1);

}
