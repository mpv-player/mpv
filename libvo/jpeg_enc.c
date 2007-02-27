/* Straightforward (to be) optimized JPEG encoder for the YUV422 format 
 * based on mjpeg code from ffmpeg. 
 *
 * Copyright (c) 2002, Rik Snel
 * Parts from ffmpeg Copyright (c) 2000-2002 Fabrice Bellard
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * For an excellent introduction to the JPEG format, see:
 * http://www.ece.purdue.edu/~bouman/grad-labs/lab8/pdf/lab.pdf
 */



#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#ifdef USE_FASTMEMCPY
#include "fastmemcpy.h"
#endif
#include "mp_msg.h"
/* We need this #define because we need ../libavcodec/common.h to #define 
 * be2me_32, otherwise the linker will complain that it doesn't exist */
#define HAVE_AV_CONFIG_H
#include "libavcodec/avcodec.h"
#include "libavcodec/dsputil.h"
#include "libavcodec/mpegvideo.h"

#include "jpeg_enc.h"

extern int avcodec_inited;

/* zr_mjpeg_encode_mb needs access to these tables for the black & white 
 * option */
typedef struct MJpegContext {
    uint8_t huff_size_dc_luminance[12];
    uint16_t huff_code_dc_luminance[12];
    uint8_t huff_size_dc_chrominance[12];
    uint16_t huff_code_dc_chrominance[12];

    uint8_t huff_size_ac_luminance[256];
    uint16_t huff_code_ac_luminance[256];
    uint8_t huff_size_ac_chrominance[256];
    uint16_t huff_code_ac_chrominance[256];
} MJpegContext;


/* Begin excessive code duplication ************************************/
/* Code coming from mpegvideo.c and mjpeg.c in ../libavcodec ***********/

static const unsigned short aanscales[64] = {
    /* precomputed values scaled up by 14 bits */
    16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
    22725, 31521, 29692, 26722, 22725, 17855, 12299,  6270,
    21407, 29692, 27969, 25172, 21407, 16819, 11585,  5906,
    19266, 26722, 25172, 22654, 19266, 15137, 10426,  5315,
    16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
    12873, 17855, 16819, 15137, 12873, 10114,  6967,  3552,
    8867, 12299, 11585, 10426,  8867,  6967,  4799,  2446,
    4520,  6270,  5906,  5315,  4520,  3552,  2446,  1247
};

static void convert_matrix(MpegEncContext *s, int (*qmat)[64], 
		uint16_t (*qmat16)[2][64], const uint16_t *quant_matrix,
		int bias, int qmin, int qmax)
{
    int qscale;

    for(qscale=qmin; qscale<=qmax; qscale++){
        int i;
	if (s->dsp.fdct == ff_jpeg_fdct_islow) {
		for (i = 0; i < 64; i++) {
			const int j = s->dsp.idct_permutation[i];
			/* 16    <= qscale * quant_matrix[i] <= 7905 
			 * 19952 <= aanscales[i] *  \
			 * 	        qscale * quant_matrix[i]     <= 205026 
			 * (1<<36)/19952 >= (1<<36)/(aanscales[i] * \
			 * 	qscale * quant_matrix[i]) >= (1<<36)/249205025
			 * 3444240       >= (1<<36)/(aanscales[i] *
			 *      qscale * quant_matrix[i]) >= 275              */
			qmat[qscale][i] = (int)((UINT64_C(1) << (QMAT_SHIFT-3))/
					(qscale * quant_matrix[j]));
		}
	} else if (s->dsp.fdct == fdct_ifast) {
            for(i=0;i<64;i++) {
                const int j = s->dsp.idct_permutation[i];
                /* 16 <= qscale * quant_matrix[i] <= 7905 */
                /* 19952         <= aanscales[i] * qscale * quant_matrix[i]           <= 249205026 */
                /* (1<<36)/19952 >= (1<<36)/(aanscales[i] * qscale * quant_matrix[i]) >= (1<<36)/249205026 */
                /* 3444240       >= (1<<36)/(aanscales[i] * qscale * quant_matrix[i]) >= 275 */
                
                qmat[qscale][i] = (int)((UINT64_C(1) << (QMAT_SHIFT + 11)) / 
                                (aanscales[i] * qscale * quant_matrix[j]));
            }
        } else {
            for(i=0;i<64;i++) {
		const int j = s->dsp.idct_permutation[i];
                /* We can safely suppose that 16 <= quant_matrix[i] <= 255
                   So 16           <= qscale * quant_matrix[i]             <= 7905
                   so (1<<19) / 16 >= (1<<19) / (qscale * quant_matrix[i]) >= (1<<19) / 7905
                   so 32768        >= (1<<19) / (qscale * quant_matrix[i]) >= 67
                */
                qmat  [qscale][i] = (int)((UINT64_C(1) << QMAT_SHIFT_MMX) / (qscale * quant_matrix[j]));
                qmat16[qscale][0][i] = (1 << QMAT_SHIFT_MMX) / (qscale * quant_matrix[j]);

                if(qmat16[qscale][0][i]==0 || qmat16[qscale][0][i]==128*256) qmat16[qscale][0][i]=128*256-1;
                qmat16[qscale][1][i]= ROUNDED_DIV(bias<<(16-QUANT_BIAS_SHIFT), qmat16[qscale][0][i]);
            }
        }
    }
}

static inline void encode_dc(MpegEncContext *s, int val, 
                             uint8_t *huff_size, uint16_t *huff_code)
{
    int mant, nbits;

    if (val == 0) {
        put_bits(&s->pb, huff_size[0], huff_code[0]);
    } else {
        mant = val;
        if (val < 0) {
            val = -val;
            mant--;
        }
        
        /* compute the log (XXX: optimize) */
        nbits = 0;
        while (val != 0) {
            val = val >> 1;
            nbits++;
        }
            
        put_bits(&s->pb, huff_size[nbits], huff_code[nbits]);
        
        put_bits(&s->pb, nbits, mant & ((1 << nbits) - 1));
    }
}

static void encode_block(MpegEncContext *s, DCTELEM *block, int n)
{
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
        encode_dc(s, val, m->huff_size_dc_luminance, m->huff_code_dc_luminance);
        huff_size_ac = m->huff_size_ac_luminance;
        huff_code_ac = m->huff_code_ac_luminance;
    } else {
        encode_dc(s, val, m->huff_size_dc_chrominance, m->huff_code_dc_chrominance);
        huff_size_ac = m->huff_size_ac_chrominance;
        huff_code_ac = m->huff_code_ac_chrominance;
    }
    s->last_dc[component] = dc;
    
    /* AC coefs */
    
    run = 0;
    last_index = s->block_last_index[n];
    for(i=1;i<=last_index;i++) {
        j = s->intra_scantable.permutated[i];
        val = block[j];
        if (val == 0) {
            run++;
        } else {
            while (run >= 16) {
                put_bits(&s->pb, huff_size_ac[0xf0], huff_code_ac[0xf0]);
                run -= 16;
            }
            mant = val;
            if (val < 0) {
                val = -val;
                mant--;
            }
            
            /* compute the log (XXX: optimize) */
            nbits = 0;
            while (val != 0) {
                val = val >> 1;
                nbits++;
            }
            code = (run << 4) | nbits;

            put_bits(&s->pb, huff_size_ac[code], huff_code_ac[code]);
        
            put_bits(&s->pb, nbits, mant & ((1 << nbits) - 1));
            run = 0;
        }
    }

    /* output EOB only if not already 64 values */
    if (last_index < 63 || run != 0)
        put_bits(&s->pb, huff_size_ac[0], huff_code_ac[0]);
}

static inline void clip_coeffs(MpegEncContext *s, DCTELEM *block, int last_index)
{
    int i;
    const int maxlevel= s->max_qcoeff;
    const int minlevel= s->min_qcoeff;

    for(i=0; i<=last_index; i++){
        const int j = s->intra_scantable.permutated[i];
        int level = block[j];
       
        if     (level>maxlevel) level=maxlevel;
        else if(level<minlevel) level=minlevel;
        block[j]= level;
    }
}

/* End excessive code duplication **************************************/

/* this function is a reproduction of the one in mjpeg, it includes two
 * changes, it allows for black&white encoding (it skips the U and V
 * macroblocks and it outputs the huffman code for 'no change' (dc) and
 * 'all zero' (ac)) and it takes 4 macroblocks (422) instead of 6 (420) */
static void zr_mjpeg_encode_mb(jpeg_enc_t *j) {

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

/* this function can take all kinds of YUV colorspaces
 * YV12, YVYU, UYVY. The necesary parameters must be set up by the caller
 * y_ps means "y pixel size", y_rs means "y row size".
 * For YUYV, for example, is u_buf = y_buf + 1, v_buf = y_buf + 3, 
 * y_ps = 2, u_ps = 4, v_ps = 4, y_rs = u_rs = v_rs.
 *
 *  The actual buffers must be passed with mjpeg_encode_frame, this is
 *  to make it possible to call encode on the buffer provided by the
 *  codec in draw_frame.
 *  
 * The data is straightened out at the moment it is put in DCT
 * blocks, there are therefore no spurious memcopies involved */
/* Notice that w must be a multiple of 16 and h must be a multiple of 8 */
/* We produce YUV422 jpegs, the colors must be subsampled horizontally,
 * if the colors are also subsampled vertically, then this function
 * performs cheap upsampling (better solution will be: a DCT that is
 * optimized in the case that every two rows are the same) */
/* cu = 0 means 'No cheap upsampling'
 * cu = 1 means 'perform cheap upsampling' */
/* The encoder doesn't know anything about interlacing, the halve height
 * needs to be passed and the double rowstride. Which field gets encoded
 * is decided by what buffers are passed to mjpeg_encode_frame */
jpeg_enc_t *jpeg_enc_init(int w, int h, int y_psize, int y_rsize, 
		int u_psize, int u_rsize, int v_psize, int v_rsize,
		int cu, int q, int b) {
	jpeg_enc_t *j;
	int i = 0;
	mp_msg(MSGT_VO, MSGL_V, "JPEnc init: %dx%d %d %d %d %d %d %d\n",
			w, h, y_psize, y_rsize, u_psize, 
			u_rsize, v_psize, v_rsize);

	j = av_malloc(sizeof(jpeg_enc_t));
	if (j == NULL) return NULL;

	j->s = av_malloc(sizeof(MpegEncContext));
	memset(j->s,0x00,sizeof(MpegEncContext));
	if (j->s == NULL) {
		av_free(j);
		return NULL;
	}

	/* info on how to access the pixels */
	j->y_ps = y_psize; 
	j->u_ps = u_psize; 
	j->v_ps = v_psize;
	j->y_rs = y_rsize; 
	j->u_rs = u_rsize; 
	j->v_rs = v_rsize;

	j->s->width = w;
	j->s->height = h;
	j->s->qscale = q;

	j->s->mjpeg_data_only_frames = 0;
	j->s->out_format = FMT_MJPEG;
	j->s->intra_only = 1;
	j->s->encoding = 1;
	j->s->pict_type = I_TYPE;
	j->s->y_dc_scale = 8;
	j->s->c_dc_scale = 8;

	j->s->mjpeg_write_tables = 1;
	j->s->mjpeg_vsample[0] = 1;
	j->s->mjpeg_vsample[1] = 1;
	j->s->mjpeg_vsample[2] = 1;
	j->s->mjpeg_hsample[0] = 2;
	j->s->mjpeg_hsample[1] = 1;
	j->s->mjpeg_hsample[2] = 1;

	j->cheap_upsample = cu;
	j->bw = b;

	/* if libavcodec is used by the decoder then we must not
	 * initialize again, but if it is not initialized then we must
	 * initialize it here. */
	if (!avcodec_inited) {
		/* we need to initialize libavcodec */
		avcodec_init();
		avcodec_register_all();
		avcodec_inited=1;
	}

	if (mjpeg_init(j->s) < 0) {
		av_free(j->s);
		av_free(j);
		return NULL;
	}

	/* alloc bogus avctx to keep MPV_common_init from segfaulting */
	j->s->avctx = calloc(sizeof(*j->s->avctx), 1);
	/* Set up to encode mjpeg */
	j->s->avctx->codec_id = CODEC_ID_MJPEG;

	/* make MPV_common_init allocate important buffers, like s->block */
	j->s->avctx->thread_count = 1;

	if (MPV_common_init(j->s) < 0) {
		av_free(j->s);
		av_free(j);
		return NULL;
	}

	/* correct the value for sc->mb_height */
	j->s->mb_height = j->s->height/8;
	j->s->mb_intra = 1;

	j->s->intra_matrix[0] = ff_mpeg1_default_intra_matrix[0];
	for (i = 1; i < 64; i++) 
		j->s->intra_matrix[i] = av_clip_uint8(
			(ff_mpeg1_default_intra_matrix[i]*j->s->qscale) >> 3);
	convert_matrix(j->s, j->s->q_intra_matrix, j->s->q_intra_matrix16, 
			j->s->intra_matrix, j->s->intra_quant_bias, 8, 8);
	return j;
}	

int jpeg_enc_frame(jpeg_enc_t *j, unsigned char *y_data, 
		unsigned char *u_data, unsigned char *v_data, char *bufr) {
	int i, k, mb_x, mb_y, overflow;
	short int *dest;
	unsigned char *source;
	/* initialize the buffer */

	init_put_bits(&j->s->pb, bufr, 1024*256);

	mjpeg_picture_header(j->s);

	j->s->header_bits = put_bits_count(&j->s->pb);

	j->s->last_dc[0] = 128; 
	j->s->last_dc[1] = 128; 
	j->s->last_dc[2] = 128;

	for (mb_y = 0; mb_y < j->s->mb_height; mb_y++) {
		for (mb_x = 0; mb_x < j->s->mb_width; mb_x++) {
			/* conversion 8 to 16 bit and filling of blocks
			 * must be mmx optimized */
			/* fill 2 Y macroblocks and one U and one V */
			source = mb_y * 8 * j->y_rs + 
				16 * j->y_ps * mb_x + y_data;
			dest = j->s->block[0];
			for (i = 0; i < 8; i++) {
				for (k = 0; k < 8; k++) {
					dest[k] = source[k*j->y_ps];
				}
				dest += 8;
				source += j->y_rs;
			}
			source = mb_y * 8 * j->y_rs + 
				(16*mb_x + 8)*j->y_ps + y_data;
			dest = j->s->block[1];
			for (i = 0; i < 8; i++) {
				for (k = 0; k < 8; k++) {
					dest[k] = source[k*j->y_ps];
				}
				dest += 8;
				source += j->y_rs;
			}
			if (!j->bw && j->cheap_upsample) {
				source = mb_y*4*j->u_rs + 
					8*mb_x*j->u_ps + u_data;
				dest = j->s->block[2];
				for (i = 0; i < 4; i++) {
					for (k = 0; k < 8; k++) {
						dest[k] = source[k*j->u_ps];
						dest[k+8] = source[k*j->u_ps];
					}
					dest += 16;
					source += j->u_rs;
				}
				source = mb_y*4*j->v_rs + 
					8*mb_x*j->v_ps + v_data;
				dest = j->s->block[3];
				for (i = 0; i < 4; i++) {
					for (k = 0; k < 8; k++) {
						dest[k] = source[k*j->v_ps];
						dest[k+8] = source[k*j->v_ps];
					}
					dest += 16;
					source += j->u_rs;
				}
			} else if (!j->bw && !j->cheap_upsample) {
				source = mb_y*8*j->u_rs + 
					8*mb_x*j->u_ps + u_data;
				dest = j->s->block[2];
				for (i = 0; i < 8; i++) {
					for (k = 0; k < 8; k++) 
						dest[k] = source[k*j->u_ps];
					dest += 8;
					source += j->u_rs;
				}
				source = mb_y*8*j->v_rs + 
					8*mb_x*j->v_ps + v_data;
				dest = j->s->block[3];
				for (i = 0; i < 8; i++) {
					for (k = 0; k < 8; k++) 
						dest[k] = source[k*j->v_ps];
					dest += 8;
					source += j->u_rs;
				}
			}
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
	mjpeg_picture_trailer(j->s);
	flush_put_bits(&j->s->pb);	

	if (j->s->mjpeg_write_tables == 1)
		j->s->mjpeg_write_tables = 0;
	
	return pbBufPtr(&(j->s->pb)) - j->s->pb.buf;
}

void jpeg_enc_uninit(jpeg_enc_t *j) {
	mjpeg_close(j->s);
	av_free(j->s);
	av_free(j);
}

#if 0

#define		W	32	
#define		H	32

int quant_store[MBR+1][MBC+1];
unsigned char buf[W*H*3/2];
char code[256*1024];


main() {
	int i, size;
	FILE *fp;

	memset(buf, 0, W*H);
	memset(buf+W*H, 255, W*H/4);
	memset(buf+5*W*H/4, 0, W*H/4);
	mjpeg_encoder_init(W, H, 1, W, 1, W/2, 1, W/2, 1, 1, 0);

	size = mjpeg_encode_frame(buf, buf+W*H, buf+5*W*H/4, code);
	fp = fopen("test.jpg", "w");
	fwrite(code, 1, size, fp);
	fclose(fp);
}
#endif
