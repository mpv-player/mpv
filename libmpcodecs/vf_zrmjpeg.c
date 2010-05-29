/*
 * This files includes a straightforward (to be) optimized JPEG encoder for
 * the YUV422 format, based on mjpeg code from ffmpeg.
 *
 * For an excellent introduction to the JPEG format, see:
 * http://www.ece.purdue.edu/~bouman/grad-labs/lab8/pdf/lab.pdf
 *
 * Copyright (C) 2005 Rik Snel <rsnel@cube.dyndns.org>
 * - based on vd_lavc.c by A'rpi (C) 2002-2003
 * - parts from ffmpeg Copyright (c) 2000-2003 Fabrice Bellard
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * \file vf_zrmjpeg.c
 *
 * \brief Does mjpeg encoding as required by the zrmjpeg filter as well
 * as by the zr video driver.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "config.h"
#include "mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

/* We need this #define because we need ../libavcodec/common.h to #define
 * be2me_32, otherwise the linker will complain that it doesn't exist */
#define HAVE_AV_CONFIG_H
#include "libavcodec/avcodec.h"
#include "libavcodec/mjpegenc.h"

#undef malloc
#undef free

/* some convenient #define's, is this portable enough? */
/// Printout  with vf_zrmjpeg: prefix at VERBOSE level
#define VERBOSE(...) mp_msg(MSGT_DECVIDEO, MSGL_V, "vf_zrmjpeg: " __VA_ARGS__)
/// Printout with vf_zrmjpeg: prefix at ERROR level
#define ERROR(...) mp_msg(MSGT_DECVIDEO, MSGL_ERR, "vf_zrmjpeg: " __VA_ARGS__)
/// Printout with vf_zrmjpeg: prefix at WARNING level
#define WARNING(...) mp_msg(MSGT_DECVIDEO, MSGL_WARN, \
		"vf_zrmjpeg: " __VA_ARGS__)

// "local" flag in vd_ffmpeg.c. If not set, avcodec_init() et. al. need to be called
// set when init is done, so that initialization is not done twice.
extern int avcodec_initialized;

/// The get_pixels() routine to use. The real routine comes from dsputil
static void (*get_pixels)(DCTELEM *restrict block, const uint8_t *pixels, int line_size);

/* Begin excessive code duplication ************************************/
/* Code coming from mpegvideo.c and mjpeg.c in ../libavcodec ***********/

/// copy of the table in mpegvideo.c
static const unsigned short aanscales[64] = {
	/**< precomputed values scaled up by 14 bits */
	16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
	22725, 31521, 29692, 26722, 22725, 17855, 12299,  6270,
	21407, 29692, 27969, 25172, 21407, 16819, 11585,  5906,
	19266, 26722, 25172, 22654, 19266, 15137, 10426,  5315,
	16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
	12873, 17855, 16819, 15137, 12873, 10114,  6967,  3552,
	8867,  12299, 11585, 10426,  8867,  6967,  4799,  2446,
	4520,   6270,  5906,  5315,  4520,  3552,  2446,  1247
};

/// Precompute DCT quantizing matrix
/**
 * This routine will precompute the combined DCT matrix with qscale
 * and DCT renorm needed by the MPEG encoder here. It is basically the
 * same as the routine with the same name in mpegvideo.c, except for
 * some coefficient changes. The matrix will be computed in two variations,
 * depending on the DCT version used. The second used by the MMX version of DCT.
 *
 * \param s MpegEncContext pointer
 * \param qmat[OUT] pointer to where the matrix is stored
 * \param qmat16[OUT] pointer to where matrix for MMX is stored.
 *		  This matrix is not permutated
 *                and second 64 entries are bias
 * \param quant_matrix[IN] the quantizion matrix to use
 * \param bias bias for the quantizer
 * \param qmin minimum qscale value to set up for
 * \param qmax maximum qscale value to set up for
 *
 * Only rows between qmin and qmax will be populated in the matrix.
 * In this MJPEG encoder, only the value 8 for qscale is used.
 */
static void convert_matrix(MpegEncContext *s, int (*qmat)[64],
		uint16_t (*qmat16)[2][64], const uint16_t *quant_matrix,
		int bias, int qmin, int qmax) {
	int qscale;

	for(qscale = qmin; qscale <= qmax; qscale++) {
		int i;
		if (s->dsp.fdct == ff_jpeg_fdct_islow) {
			for (i = 0; i < 64; i++) {
				const int j = s->dsp.idct_permutation[i];
/* 16 <= qscale * quant_matrix[i] <= 7905
 * 19952         <= aanscales[i] * qscale * quant_matrix[i]      <= 249205026
 * (1<<36)/19952 >= (1<<36)/(aanscales[i] * qscale * quant_matrix[i])
 *                                                       >= (1<<36)/249205026
 * 3444240       >= (1<<36)/(aanscales[i] * qscale * quant_matrix[i])  >= 275 */
				qmat[qscale][i] = (int)((UINT64_C(1) <<
					(QMAT_SHIFT-3))/
					(qscale*quant_matrix[j]));
			}
		} else if (s->dsp.fdct == fdct_ifast) {
			for (i = 0; i < 64; i++) {
				const int j = s->dsp.idct_permutation[i];
/* 16 <= qscale * quant_matrix[i] <= 7905
 * 19952         <= aanscales[i] * qscale * quant_matrix[i]      <= 249205026
 * (1<<36)/19952 >= (1<<36)/(aanscales[i] * qscale * quant_matrix[i])
 *                                                       >= (1<<36)/249205026
 * 3444240       >= (1<<36)/(aanscales[i] * qscale * quant_matrix[i])  >= 275 */
				qmat[qscale][i] = (int)((UINT64_C(1) <<
					(QMAT_SHIFT + 11))/(aanscales[i]
					*qscale * quant_matrix[j]));
			}
		} else {
			for (i = 0; i < 64; i++) {
				const int j = s->dsp.idct_permutation[i];
/* We can safely assume that 16 <= quant_matrix[i] <= 255
 * So 16           <= qscale * quant_matrix[i]             <= 7905
 * so (1<<19) / 16 >= (1<<19) / (qscale * quant_matrix[i]) >= (1<<19) / 7905
 * so 32768        >= (1<<19) / (qscale * quant_matrix[i]) >= 67 */
				qmat[qscale][i] = (int)((UINT64_C(1) <<
						QMAT_SHIFT_MMX) / (qscale
							*quant_matrix[j]));
				qmat16[qscale][0][i] = (1 << QMAT_SHIFT_MMX)
						/(qscale * quant_matrix[j]);

				if (qmat16[qscale][0][i] == 0 ||
						qmat16[qscale][0][i] == 128*256)
					qmat16[qscale][0][i]=128*256-1;
				qmat16[qscale][1][i]=ROUNDED_DIV(bias
						<<(16-QUANT_BIAS_SHIFT),
						qmat16[qscale][0][i]);
			}
		}
	}
}

/// Emit the DC value into a MJPEG code sream
/**
 * This routine is only intended to be used from encode_block
 *
 * \param s pointer to MpegEncContext structure
 * \param val the DC value to emit
 * \param huff_size pointer to huffman code size array
 * \param huff_code pointer to the code array corresponding to \a huff_size
 *
 * This routine is a clone of mjpeg_encode_dc
 */
static inline void encode_dc(MpegEncContext *s, int val,
		uint8_t *huff_size, uint16_t *huff_code) {
	int mant, nbits;

	if (val == 0) {
		put_bits(&s->pb, huff_size[0], huff_code[0]);
	} else {
		mant = val;
		if (val < 0) {
			val = -val;
			mant--;
		}
		nbits= av_log2_16bit(val) + 1;
		put_bits(&s->pb, huff_size[nbits], huff_code[nbits]);
		put_bits(&s->pb, nbits, mant & ((1 << nbits) - 1));
	}
}

/// Huffman encode and emit one DCT block into the MJPEG code stream
/**
 * \param s pointer to MpegEncContext structure
 * \param block pointer to the DCT block to emit
 * \param n
 *
 * This routine is a duplicate of encode_block in mjpeg.c
 */
static void encode_block(MpegEncContext *s, DCTELEM *block, int n) {
	int mant, nbits, code, i, j;
	int component, dc, run, last_index, val;
	MJpegContext *m = s->mjpeg_ctx;
	uint8_t *huff_size_ac;
	uint16_t *huff_code_ac;

	/* DC coef */
	component = (n <= 3 ? 0 : n - 4 + 1);
	dc = block[0]; /* overflow is impossible */
	val = dc - s->last_dc[component];
	if (n < 4) {
		encode_dc(s, val, m->huff_size_dc_luminance,
				m->huff_code_dc_luminance);
		huff_size_ac = m->huff_size_ac_luminance;
		huff_code_ac = m->huff_code_ac_luminance;
	} else {
		encode_dc(s, val, m->huff_size_dc_chrominance,
				m->huff_code_dc_chrominance);
		huff_size_ac = m->huff_size_ac_chrominance;
		huff_code_ac = m->huff_code_ac_chrominance;
	}
	s->last_dc[component] = dc;

	/* AC coefs */

	run = 0;
	last_index = s->block_last_index[n];
	for (i = 1; i <= last_index; i++) {
		j = s->intra_scantable.permutated[i];
		val = block[j];
		if (val == 0) run++;
		else {
			while (run >= 16) {
				put_bits(&s->pb, huff_size_ac[0xf0],
						huff_code_ac[0xf0]);
				run -= 16;
			}
			mant = val;
			if (val < 0) {
				val = -val;
				mant--;
			}

			nbits= av_log2_16bit(val) + 1;
			code = (run << 4) | nbits;

			put_bits(&s->pb, huff_size_ac[code],
					huff_code_ac[code]);
			put_bits(&s->pb, nbits, mant & ((1 << nbits) - 1));
			run = 0;
		}
	}

	/* output EOB only if not already 64 values */
	if (last_index < 63 || run != 0)
		put_bits(&s->pb, huff_size_ac[0], huff_code_ac[0]);
}

/// clip overflowing DCT coefficients
/**
 * If the computed DCT coefficients in a block overflow, this routine
 * will go through them and clip them to be in the valid range.
 *
 * \param s pointer to MpegEncContext
 * \param block pointer to DCT block to process
 * \param last_index index of the last non-zero coefficient in block
 *
 * The max and min level, which are clipped to, are stored in
 * s->min_qcoeff and s->max_qcoeff respectively.
 */
static inline void clip_coeffs(MpegEncContext *s, DCTELEM *block,
		int last_index) {
	int i;
	const int maxlevel= s->max_qcoeff;
	const int minlevel= s->min_qcoeff;

	for (i = 0; i <= last_index; i++) {
		const int j = s->intra_scantable.permutated[i];
		int level = block[j];

		if (level > maxlevel) level=maxlevel;
		else if(level < minlevel) level=minlevel;
		block[j]= level;
	}
}

/* End excessive code duplication **************************************/

typedef struct {
	struct MpegEncContext *s;
	int cheap_upsample;
	int bw;
	int y_rs;
	int u_rs;
	int v_rs;
} jpeg_enc_t;

// Huffman encode and emit one MCU of MJPEG code
/**
 * \param j pointer to jpeg_enc_t structure
 *
 * This function huffman encodes one MCU, and emits the
 * resulting bitstream into the MJPEG code that is currently worked on.
 *
 * this function is a reproduction of the one in mjpeg, it includes two
 * changes, it allows for black&white encoding (it skips the U and V
 * macroblocks and it outputs the huffman code for 'no change' (dc) and
 * 'all zero' (ac)) and it takes 4 macroblocks (422) instead of 6 (420)
 */
static av_always_inline void zr_mjpeg_encode_mb(jpeg_enc_t *j) {

	MJpegContext *m = j->s->mjpeg_ctx;

	encode_block(j->s, j->s->block[0], 0);
	encode_block(j->s, j->s->block[1], 1);
	if (j->bw) {
		/* U */
		put_bits(&j->s->pb, m->huff_size_dc_chrominance[0],
				m->huff_code_dc_chrominance[0]);
		put_bits(&j->s->pb, m->huff_size_ac_chrominance[0],
				m->huff_code_ac_chrominance[0]);
		/* V */
		put_bits(&j->s->pb, m->huff_size_dc_chrominance[0],
				m->huff_code_dc_chrominance[0]);
		put_bits(&j->s->pb, m->huff_size_ac_chrominance[0],
				m->huff_code_ac_chrominance[0]);
	} else {
		/* we trick encode_block here so that it uses
		 * chrominance huffman tables instead of luminance ones
		 * (see the effect of second argument of encode_block) */
		encode_block(j->s, j->s->block[2], 4);
		encode_block(j->s, j->s->block[3], 5);
	}
}

/// Fill one DCT MCU from planar storage
/**
 * This routine will convert one MCU from YUYV planar storage into 4
 * DCT macro blocks, converting from 8-bit format in the planar
 * storage to 16-bit format used in the DCT.
 *
 * \param j pointer to jpeg_enc structure, and also storage for DCT macro blocks
 * \param x pixel x-coordinate for the first pixel
 * \param y pixel y-coordinate for the first pixel
 * \param y_data pointer to the Y plane
 * \param u_data pointer to the U plane
 * \param v_data pointer to the V plane
 */
static av_always_inline void fill_block(jpeg_enc_t *j, int x, int y,
		unsigned char *y_data, unsigned char *u_data,
		unsigned char *v_data)
{
	int i, k;
	short int *dest;
	unsigned char *source;

	// The first Y, Y0
	get_pixels(j->s->block[0], y*8*j->y_rs + 16*x + y_data, j->y_rs);
	// The second Y, Y1
	get_pixels(j->s->block[1], y*8*j->y_rs + 16*x + 8 + y_data, j->y_rs);

	if (!j->bw && j->cheap_upsample) {
		source = y * 4 * j->u_rs + 8*x + u_data;
		dest = j->s->block[2];
		for (i = 0; i < 4; i++) {
			for (k = 0; k < 8; k++) {
				dest[k] = source[k];   // First row
				dest[k+8] = source[k]; // Duplicate to next row

			}
			dest += 16;
			source += j->u_rs;
		}
		source = y * 4 * j->v_rs + 8*x + v_data;
		dest = j->s->block[3];
		for (i = 0; i < 4; i++) {
			for (k = 0; k < 8; k++) {
				dest[k] = source[k];
				dest[k+8] = source[k];
			}
			dest += 16;
			source += j->u_rs;
		}
	} else if (!j->bw && !j->cheap_upsample) {
		// U
		get_pixels(j->s->block[2], y*8*j->u_rs + 8*x + u_data, j->u_rs);
		// V
		get_pixels(j->s->block[3], y*8*j->v_rs + 8*x + v_data, j->v_rs);
	}
}

/**
 * \brief initialize mjpeg encoder
 *
 * This routine is to set up the parameters and initialize the mjpeg encoder.
 * It does all the initializations needed of lower level routines.
 * The formats accepted by this encoder is YUV422P and YUV420
 *
 * \param w width in pixels of the image to encode, must be a multiple of 16
 * \param h height in pixels of the image to encode, must be a multiple of 8
 * \param y_rsize size of each plane row Y component
 * \param y_rsize size of each plane row U component
 * \param v_rsize size of each plane row V component
 * \param cu "cheap upsample". Set to 0 for YUV422 format, 1 for YUV420 format
 *           when set to 1, the encoder will assume that there is only half th
 *           number of rows of chroma information, and every chroma row is
 *           duplicated.
 * \param q quality parameter for the mjpeg encode. Between 1 and 20 where 1
 *	    is best quality and 20 is the worst quality.
 * \param b monochrome flag. When set to 1, the mjpeg output is monochrome.
 *          In that case, the colour information is omitted, and actually the
 *          colour planes are not touched.
 *
 * \returns an appropriately set up jpeg_enc_t structure
 *
 * The actual plane buffer addreses are passed by jpeg_enc_frame().
 *
 * The encoder doesn't know anything about interlacing, the halve height
 * needs to be passed and the double rowstride. Which field gets encoded
 * is decided by what buffers are passed to mjpeg_encode_frame()
 */
static jpeg_enc_t *jpeg_enc_init(int w, int h, int y_rsize,
			  int u_rsize, int v_rsize,
		int cu, int q, int b) {
	jpeg_enc_t *j;
	int i = 0;
	VERBOSE("JPEG encoder init: %dx%d %d %d %d cu=%d q=%d bw=%d\n",
			w, h, y_rsize, u_rsize, v_rsize, cu, q, b);

	j = av_mallocz(sizeof(jpeg_enc_t));
	if (j == NULL) return NULL;

	j->s = av_mallocz(sizeof(MpegEncContext));
	if (j->s == NULL) {
		av_free(j);
		return NULL;
	}

	/* info on how to access the pixels */
	j->y_rs = y_rsize;
	j->u_rs = u_rsize;
	j->v_rs = v_rsize;

	j->s->width = w;		// image width and height
	j->s->height = h;
	j->s->qscale = q;		// Encoding quality

	j->s->out_format = FMT_MJPEG;
	j->s->intra_only = 1;		// Generate only intra pictures for jpeg
	j->s->encoding = 1;		// Set mode to encode
	j->s->pict_type = FF_I_TYPE;
	j->s->y_dc_scale = 8;
	j->s->c_dc_scale = 8;

	/*
	 * This sets up the MCU (Minimal Code Unit) number
	 * of appearances of the various component
	 * for the SOF0 table in the generated MJPEG.
	 * The values are not used for anything else.
	 * The current setup is simply YUV422, with two horizontal Y components
	 * for every UV component.
	 */
	//FIXME j->s->mjpeg_write_tables = 1;	// setup to write tables
	j->s->mjpeg_vsample[0] = 1;	// 1 appearance of Y vertically
	j->s->mjpeg_vsample[1] = 1;	// 1 appearance of U vertically
	j->s->mjpeg_vsample[2] = 1;	// 1 appearance of V vertically
	j->s->mjpeg_hsample[0] = 2;	// 2 appearances of Y horizontally
	j->s->mjpeg_hsample[1] = 1;	// 1 appearance of U horizontally
	j->s->mjpeg_hsample[2] = 1;	// 1 appearance of V horizontally

	j->cheap_upsample = cu;
	j->bw = b;

	// Is this needed?
	/* if libavcodec is used by the decoder then we must not
	 * initialize again, but if it is not initialized then we must
	 * initialize it here. */
	if (!avcodec_initialized) {
		avcodec_init();
		avcodec_register_all();
		avcodec_initialized=1;
	}

	// Build mjpeg huffman code tables, setting up j->s->mjpeg_ctx
	if (ff_mjpeg_encode_init(j->s) < 0) {
		av_free(j->s);
		av_free(j);
		return NULL;
	}

	/* alloc bogus avctx to keep MPV_common_init from segfaulting */
	j->s->avctx = avcodec_alloc_context();
	if (j->s->avctx == NULL) {
		av_free(j->s);
		av_free(j);
		return NULL;
	}

	// Set some a minimum amount of default values that are needed
	// Indicates that we should generated normal MJPEG
	j->s->avctx->codec_id = CODEC_ID_MJPEG;
	// Which DCT method to use. AUTO will select the fastest one
	j->s->avctx->dct_algo = FF_DCT_AUTO;
	j->s->intra_quant_bias= 1<<(QUANT_BIAS_SHIFT-1); //(a + x/2)/x
	// indicate we 'decode' to jpeg 4:2:2
	j->s->avctx->pix_fmt = PIX_FMT_YUVJ422P;

	j->s->avctx->thread_count = 1;

	/* make MPV_common_init allocate important buffers, like s->block
	 * Also initializes dsputil */
	if (MPV_common_init(j->s) < 0) {
		av_free(j->s);
		av_free(j);
		return NULL;
	}

	/* correct the value for sc->mb_height. MPV_common_init put other
	 * values there */
	j->s->mb_height = j->s->height/8;
	j->s->mb_intra = 1;

	// Init q matrix
	j->s->intra_matrix[0] = ff_mpeg1_default_intra_matrix[0];
	for (i = 1; i < 64; i++)
		j->s->intra_matrix[i] = av_clip_uint8(
			(ff_mpeg1_default_intra_matrix[i]*j->s->qscale) >> 3);

	// precompute matrix
	convert_matrix(j->s, j->s->q_intra_matrix, j->s->q_intra_matrix16,
			j->s->intra_matrix, j->s->intra_quant_bias, 8, 8);

	/* Pick up the selection of the optimal get_pixels() routine
	 * to use, which was done in  MPV_common_init() */
	get_pixels = j->s->dsp.get_pixels;

	return j;
}

/**
 * \brief mjpeg encode an image
 *
 * This routine will take a 3-plane YUV422 image and encoded it with MJPEG
 * base line format, as suitable as input for the Zoran hardare MJPEG chips.
 *
 * It requires that the \a j parameter points the structure set up by the
 * jpeg_enc_init() routine.
 *
 * \param j pointer to jpeg_enc_t structure as created by jpeg_enc_init()
 * \param y_data pointer to Y component plane, packed one byte/pixel
 * \param u_data pointer to U component plane, packed one byte per every
 *		 other pixel
 * \param v_data pointer to V component plane, packed one byte per every
 *		 other pixel
 * \param bufr pointer to the buffer where the mjpeg encoded code is stored
 *
 * \returns the number of bytes stored into \a bufr
 *
 * If \a j->s->mjpeg_write_tables is set, it will also emit the mjpeg tables,
 * otherwise it will just emit the data. The \a j->s->mjpeg_write_tables
 * variable will be reset to 0 by the routine.
 */
static int jpeg_enc_frame(jpeg_enc_t *j, uint8_t *y_data,
		   uint8_t *u_data, uint8_t *v_data, uint8_t *bufr) {
	int mb_x, mb_y, overflow;
	/* initialize the buffer */

	init_put_bits(&j->s->pb, bufr, 1024*256);

	// Emit the mjpeg header blocks
	ff_mjpeg_encode_picture_header(j->s);

	j->s->header_bits = put_bits_count(&j->s->pb);

	j->s->last_dc[0] = 128;
	j->s->last_dc[1] = 128;
	j->s->last_dc[2] = 128;

	for (mb_y = 0; mb_y < j->s->mb_height; mb_y++) {
		for (mb_x = 0; mb_x < j->s->mb_width; mb_x++) {
			/*
			 * Fill one DCT block (8x8 pixels) from
			 * 2 Y macroblocks and one U and one V
			 */
			fill_block(j, mb_x, mb_y, y_data, u_data, v_data);
			emms_c(); /* is this really needed? */

			j->s->block_last_index[0] =
				j->s->dct_quantize(j->s, j->s->block[0],
						0, 8, &overflow);
			if (overflow) clip_coeffs(j->s, j->s->block[0],
					j->s->block_last_index[0]);
			j->s->block_last_index[1] =
				j->s->dct_quantize(j->s, j->s->block[1],
						1, 8, &overflow);
			if (overflow) clip_coeffs(j->s, j->s->block[1],
					j->s->block_last_index[1]);

			if (!j->bw) {
				j->s->block_last_index[4] =
					j->s->dct_quantize(j->s, j->s->block[2],
							4, 8, &overflow);
				if (overflow) clip_coeffs(j->s, j->s->block[2],
						j->s->block_last_index[2]);
				j->s->block_last_index[5] =
					j->s->dct_quantize(j->s, j->s->block[3],
							5, 8, &overflow);
				if (overflow) clip_coeffs(j->s, j->s->block[3],
						j->s->block_last_index[3]);
			}
			zr_mjpeg_encode_mb(j);
		}
	}
	emms_c();
	ff_mjpeg_encode_picture_trailer(j->s);
	flush_put_bits(&j->s->pb);

	//FIXME
	//if (j->s->mjpeg_write_tables == 1)
	//	j->s->mjpeg_write_tables = 0;

	return put_bits_ptr(&(j->s->pb)) - j->s->pb.buf;
}

/// the real uninit routine
/**
 * This is the real routine that does the uninit of the ZRMJPEG filter
 *
 * \param j pointer to jpeg_enc structure
 */
static void jpeg_enc_uninit(jpeg_enc_t *j) {
	ff_mjpeg_encode_close(j->s);
	av_free(j->s);
	av_free(j);
}

/// Private structure for ZRMJPEG filter
struct vf_priv_s {
	jpeg_enc_t *j;
	unsigned char buf[256*1024];
	int bw, fd, hdec, vdec;
	int fields;
	int y_stride;
	int c_stride;
	int quality;
	int maxwidth;
	int maxheight;
};

/// vf CONFIGURE entry point for the ZRMJPEG filter
/**
 * \param vf video filter instance pointer
 * \param width image source width in pixels
 * \param height image source height in pixels
 * \param d_width width of requested window, just a hint
 * \param d_height height of requested window, just a hint
 * \param flags vf filter flags
 * \param outfmt
 *
 * \returns returns 0 on error
 *
 * This routine will make the necessary hardware-related decisions for
 * the ZRMJPEG filter, do the initialization of the MJPEG encoder, and
 * then select one of the ZRJMJPEGIT or ZRMJPEGNI filters and then
 * arrange to dispatch to the config() entry pointer for the one
 * selected.
 */
static int config(struct vf_instance *vf, int width, int height, int d_width,
		int d_height, unsigned int flags, unsigned int outfmt){
	struct vf_priv_s *priv = vf->priv;
	float aspect_decision;
	int stretchx, stretchy, err = 0, maxstretchx = 4;
	priv->fields = 1;

	VERBOSE("config() called\n");

	if (priv->j) {
		VERBOSE("re-configuring, resetting JPEG encoder\n");
		jpeg_enc_uninit(priv->j);
		priv->j = NULL;
	}

	aspect_decision = ((float)d_width/(float)d_height)/
		((float)width/(float)height);

	if (aspect_decision > 1.8 && aspect_decision < 2.2) {
		VERBOSE("should correct aspect by stretching x times 2, %d %d\n", 2*width, priv->maxwidth);
		if (2*width <= priv->maxwidth) {
			d_width = 2*width;
			d_height = height;
			maxstretchx = 2;
		} else {
			WARNING("unable to correct aspect by stretching, because resulting X will be too large, aspect correction by decimating y not yet implemented\n");
			d_width = width;
			d_height = height;
		}
		/* prestretch movie */
	} else {
		/* uncorrecting output for now */
		d_width = width;
		d_height = height;
	}
	/* make the scaling decision
	 * we are capable of stretching the image in the horizontal
	 * direction by factors 1, 2 and 4
	 * we can stretch the image in the vertical direction by a
	 * factor of 1 and 2 AND we must decide about interlacing */
	if (d_width > priv->maxwidth/2 || height > priv->maxheight/2
			|| maxstretchx == 1) {
		stretchx = 1;
		stretchy = 1;
		priv->fields = 2;
		if (priv->vdec == 2) {
			priv->fields = 1;
		} else if (priv->vdec == 4) {
			priv->fields = 1;
			stretchy = 2;
		}
		if (priv->hdec > maxstretchx) {
			if (priv->fd) {
				WARNING("horizontal decimation too high, "
						"changing to %d (use fd to keep"
						" hdec=%d)\n",
						maxstretchx, priv->hdec);
				priv->hdec = maxstretchx;
			}
		}
		stretchx = priv->hdec;
	} else if (d_width > priv->maxwidth/4 ||
			height > priv->maxheight/4 ||
			maxstretchx == 2) {
		stretchx = 2;
		stretchy = 1;
		priv->fields = 1;
		if (priv->vdec == 2) {
			stretchy = 2;
		} else if (priv->vdec == 4) {
			if (!priv->fd) {
				WARNING("vertical decimation too high, "
						"changing to 2 (use fd to keep "
						"vdec=4)\n");
				priv->vdec = 2;
			}
			stretchy = 2;
		}
		if (priv->hdec == 2) {
			stretchx = 4;
		} else if (priv->hdec == 4) {
			if (priv->fd) {
				WARNING("horizontal decimation too high, "
						"changing to 2 (use fd to keep "
						"hdec=4)\n");
				priv->hdec = 2;
			}
			stretchx = 4;
		}
	} else {
		/* output image is maximally stretched */
		stretchx = 4;
		stretchy = 2;
		priv->fields = 1;
		if (priv->vdec != 1 && !priv->fd) {
			WARNING("vertical decimation too high, changing to 1 "
					"(use fd to keep vdec=%d)\n",
					priv->vdec);
			priv->vdec = 1;
		}
		if (priv->hdec != 1 && !priv->fd) {
			WARNING("horizontal decimation too high, changing to 1 (use fd to keep hdec=%d)\n", priv->hdec);
			priv->hdec = 1;
		}
	}

	VERBOSE("generated JPEG's %dx%s%d%s, stretched to %dx%d\n",
			width/priv->hdec, (priv->fields == 2) ? "(" : "",
			height/(priv->vdec*priv->fields),
			(priv->fields == 2) ? "x2)" : "",
			(width/priv->hdec)*stretchx,
			(height/(priv->vdec*priv->fields))*
			stretchy*priv->fields);


	if ((width/priv->hdec)*stretchx > priv->maxwidth ||
			(height/(priv->vdec*priv->fields))*
			 stretchy*priv->fields  > priv->maxheight) {
		ERROR("output dimensions too large (%dx%d), max (%dx%d) "
				"insert crop to fix\n",
				(width/priv->hdec)*stretchx,
				(height/(priv->vdec*priv->fields))*
				stretchy*priv->fields,
				priv->maxwidth, priv->maxheight);
		err = 1;
	}

	if (width%(16*priv->hdec) != 0) {
		ERROR("width must be a multiple of 16*hdec (%d), use expand\n",
				priv->hdec*16);
		err = 1;
	}

	if (height%(8*priv->fields*priv->vdec) != 0) {
		ERROR("height must be a multiple of 8*fields*vdec (%d),"
				" use expand\n", priv->vdec*priv->fields*8);
		err = 1;
	}

	if (err) return 0;

	priv->y_stride = width;
	priv->c_stride = width/2;
	priv->j = jpeg_enc_init(width, height/priv->fields,
				priv->fields*priv->y_stride,
				priv->fields*priv->c_stride,
				priv->fields*priv->c_stride,
				1, priv->quality, priv->bw);

	if (!priv->j) return 0;
	return vf_next_config(vf, width, height, d_width, d_height, flags,
		(priv->fields == 2) ? IMGFMT_ZRMJPEGIT : IMGFMT_ZRMJPEGNI);
}

/// put_image entrypoint for the ZRMJPEG vf filter
/***
 * \param vf pointer to vf_instance
 * \param mpi pointer to mp_image_t structure
 * \param pts
 */
static int put_image(struct vf_instance *vf, mp_image_t *mpi, double pts){
	struct vf_priv_s *priv = vf->priv;
	int size = 0;
	int i;
	mp_image_t* dmpi;
	for (i = 0; i < priv->fields; i++)
		size += jpeg_enc_frame(priv->j,
				mpi->planes[0] + i*priv->y_stride,
				mpi->planes[1] + i*priv->c_stride,
				mpi->planes[2] + i*priv->c_stride,
				priv->buf + size);

	dmpi = vf_get_image(vf->next, IMGFMT_ZRMJPEGNI,
			MP_IMGTYPE_EXPORT, 0, mpi->w, mpi->h);
	dmpi->planes[0] = (uint8_t*)priv->buf;
	dmpi->planes[1] = (uint8_t*)size;
	return vf_next_put_image(vf,dmpi, pts);
}

/// query_format entrypoint for the ZRMJPEG vf filter
/***
 * \param vf pointer to vf_instance
 * \param fmt image format to query for
 *
 * \returns 0 if image format in fmt is not supported
 *
 * Given the image format specified by \a fmt, this routine is called
 * to ask if the format is supported or not.
 */
static int query_format(struct vf_instance *vf, unsigned int fmt){
	VERBOSE("query_format() called\n");

	switch (fmt) {
		case IMGFMT_YV12:
		case IMGFMT_YUY2:
			/* strictly speaking the output format of
			 * this filter will be known after config(),
			 * but everything that supports IMGFMT_ZRMJPEGNI
			 * should also support all other IMGFMT_ZRMJPEG* */
			return vf_next_query_format(vf, IMGFMT_ZRMJPEGNI);
	}

	return 0;
}

/// vf UNINIT entry point for the ZRMJPEG filter
/**
 * \param vf pointer to the vf instance structure
 */
static void uninit(vf_instance_t *vf) {
	struct vf_priv_s *priv = vf->priv;
	VERBOSE("uninit() called\n");
	if (priv->j) jpeg_enc_uninit(priv->j);
	free(priv);
}

/// vf OPEN entry point for the ZRMJPEG filter
/**
 * \param vf pointer to the vf instance structure
 * \param args the argument list string for the -vf zrmjpeg command
 *
 * \returns 0 for error, 1 for success
 *
 * This routine will do some basic initialization of local structures etc.,
 * and then parse the command line arguments specific for the ZRMJPEG filter.
 */
static int vf_open(vf_instance_t *vf, char *args){
	struct vf_priv_s *priv;
	VERBOSE("vf_open() called: args=\"%s\"\n", args);

	vf->config = config;
	vf->put_image = put_image;
	vf->query_format = query_format;
	vf->uninit = uninit;

	priv = vf->priv = calloc(sizeof(*priv), 1);
	if (!vf->priv) {
		ERROR("out of memory error\n");
		return 0;
	}

	/* maximum displayable size by zoran card, these defaults
	 * are for my own zoran card in PAL mode, these can be changed
	 * by filter options. But... in an ideal world these values would
	 * be queried from the vo device itself... */
	priv->maxwidth = 768;
	priv->maxheight = 576;

	priv->quality = 2;
	priv->hdec = 1;
	priv->vdec = 1;

	/* if libavcodec is already initialized, we must not initialize it
	 * again, but if it is not initialized then we mustinitialize it now. */
	if (!avcodec_initialized) {
		/* we need to initialize libavcodec */
		avcodec_init();
		avcodec_register_all();
		avcodec_initialized=1;
	}

	if (args) {
		char *arg, *tmp, *ptr, junk;
		int last = 0, input;

		/* save arguments, to be able to safely modify them */
		arg = strdup(args);
		if (!arg) {
			ERROR("out of memory, this is bad\n");
			return 0;
		}

		tmp = ptr = arg;
		do {
			while (*tmp != ':' && *tmp) tmp++;
			if (*tmp == ':') *tmp++ = '\0';
			else last = 1;
			VERBOSE("processing filter option \"%s\"\n", ptr);
			/* These options deal with the maximum output
			 * resolution of the zoran card. These should
			 * be queried from the vo device, but it is currently
			 * too difficult, so the user should tell the filter */
			if (!strncmp("maxheight=", ptr, 10)) {
				if (sscanf(ptr+10, "%d%c", &input, &junk) != 1)
						ERROR(
		"error parsing parameter to \"maxheight=\", \"%s\", ignoring\n"
								, ptr + 10);
				else {
					priv->maxheight = input;
					VERBOSE("setting maxheight to %d\n",
							priv->maxheight);
				}
			} else if (!strncmp("quality=", ptr, 8)) {
				if (sscanf(ptr+8, "%d%c", &input, &junk) != 1)
					ERROR(
		"error parsing parameter to \"quality=\", \"%s\", ignoring\n"
								, ptr + 8);
				else if (input < 1 || input > 20)
					ERROR(
		"parameter to \"quality=\" out of range (1..20), %d\n", input);
				else {
					priv->quality = input;
					VERBOSE("setting JPEG quality to %d\n",
							priv->quality);
				}
			} else if (!strncmp("maxwidth=", ptr, 9)) {
				if (sscanf(ptr+9, "%d%c", &input, &junk) != 1)
					ERROR(
		"error parsing parameter to \"maxwidth=\", \"%s\", ignoring\n"
								, ptr + 9);
				else {
					priv->maxwidth = input;
					VERBOSE("setting maxwidth to %d\n",
							priv->maxwidth);
				}
			} else if (!strncmp("hdec=", ptr, 5)) {
				if (sscanf(ptr+5, "%d%c", &input, &junk) != 1)
					ERROR(
		"error parsing parameter to \"hdec=\", \"%s\", ignoring\n"
								, ptr + 9);
				else if (input != 1 && input != 2 && input != 4)
					ERROR(
		"illegal parameter to \"hdec=\", %d, should be 1, 2 or 4",
								input);
				else {
					priv->hdec = input;
					VERBOSE(
		"setting horizontal decimation to %d\n", priv->maxwidth);
				}
			} else if (!strncmp("vdec=", ptr, 5)) {
				if (sscanf(ptr+5, "%d%c", &input, &junk) != 1)
					ERROR(
		"error parsing parameter to \"vdec=\", \"%s\", ignoring\n"
								, ptr + 9);
				else if (input != 1 && input != 2 && input != 4)
					ERROR(
		"illegal parameter to \"vdec=\", %d, should be 1, 2 or 4",
								input);
				else {
					priv->vdec = input;
					VERBOSE(
			"setting vertical decimation to %d\n", priv->maxwidth);
				}
			} else if (!strcasecmp("dc10+-PAL", ptr) ||
					!strcasecmp("dc10-PAL", ptr)) {
				priv->maxwidth = 768;
				priv->maxheight = 576;
				VERBOSE("setting DC10(+) PAL profile\n");
			} else if (!strcasecmp("fd", ptr)) {
				priv->fd = 1;
				VERBOSE("forcing decimation\n");
			} else if (!strcasecmp("nofd", ptr)) {
				priv->fd = 0;
				VERBOSE("decimate only if beautiful\n");
			} else if (!strcasecmp("bw", ptr)) {
				priv->bw = 1;
				VERBOSE("setting black and white encoding\n");
			} else if (!strcasecmp("color", ptr)) {
				priv->bw = 0;
				VERBOSE("setting color encoding\n");
			} else if (!strcasecmp("dc10+-NTSC", ptr) ||
					!strcasecmp("dc10-NTSC", ptr)) {
				priv->maxwidth = 640;
				priv->maxheight = 480;
				VERBOSE("setting DC10(+) NTSC profile\n");
			} else if (!strcasecmp("buz-PAL", ptr) ||
					!strcasecmp("lml33-PAL", ptr)) {
				priv->maxwidth = 720;
				priv->maxheight = 576;
				VERBOSE("setting buz/lml33 PAL profile\n");
			} else if (!strcasecmp("buz-NTSC", ptr) ||
					!strcasecmp("lml33-NTSC", ptr)) {
				priv->maxwidth = 720;
				priv->maxheight = 480;
				VERBOSE("setting buz/lml33 NTSC profile\n");
			} else {
				WARNING("ignoring unknown filter option "
						"\"%s\", or missing argument\n",
						ptr);
			}
			ptr = tmp;
		} while (!last);

		free(arg);
	}


	return 1;
}

const vf_info_t vf_info_zrmjpeg = {
    "realtime zoran MJPEG encoding",
    "zrmjpeg",
    "Rik Snel",
    "",
    vf_open,
    NULL
};
