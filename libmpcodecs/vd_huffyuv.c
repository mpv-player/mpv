/*
 *
 * HuffYUV Decoder for Mplayer
 * (c) 2002 Roberto Togni
 *
 * Fourcc: HFYU
 *
 * Original Win32 codec copyright:
 *
 *** Huffyuv v2.1.1, by Ben Rudiak-Gould.
 *** http://www.math.berkeley.edu/~benrg/huffyuv.html
 ***
 *** This file is copyright 2000 Ben Rudiak-Gould, and distributed under
 *** the terms of the GNU General Public License, v2 or later.  See
 *** http://www.gnu.org/copyleft/gpl.html.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"

#include "vd_internal.h"


static vd_info_t info = {
	"HuffYUV Video decoder",
	"huffyuv",
	VFM_HUFFYUV,
	"Roberto Togni",
	"Roberto Togni",
	"native codec, original win32 by Ben Rudiak-Gould http://www.math.berkeley.edu/~benrg/huffyuv.html"
};

LIBVD_EXTERN(huffyuv)


/*
 * Bitmap types
 */
#define BMPTYPE_YUV -1
#define BMPTYPE_RGB -2
#define BMPTYPE_RGBA -3

/*
 * Compression methods
 */
#define METHOD_LEFT 0
#define METHOD_GRAD 1
#define METHOD_MEDIAN 2
#define DECORR_FLAG 64
#define METHOD_LEFT_DECORR (METHOD_LEFT | DECORR_FLAG)
#define METHOD_GRAD_DECORR (METHOD_GRAD | DECORR_FLAG)
#define METHOD_OLD -2 

#define FOURCC_HFYU mmioFOURCC('H','F','Y','U')

#define HUFFTABLE_CLASSIC_YUV ((unsigned char*) -1)
#define HUFFTABLE_CLASSIC_RGB ((unsigned char*) -2)
#define HUFFTABLE_CLASSIC_YUV_CHROMA ((unsigned char*) -3)


/*
 * Huffman table
 */
typedef struct {
	unsigned char* table_pointers[32];
	unsigned char table_data[129*25];
} DecodeTable;


/*
 * Decoder context
 */
typedef struct {
	// Real image depth
	int bitcount;
	// Prediction method
	int method;
	// Bitmap color type
	int bitmaptype;
	// Interlaced flag
	int interlaced;
	// Huffman tables
	unsigned char decode1_shift[256];
	unsigned char decode2_shift[256];
	unsigned char decode3_shift[256];
	DecodeTable decode1, decode2, decode3;
	// Above line buffers
	unsigned char *abovebuf1, *abovebuf2;
} huffyuv_context_t;


/*
 * Classic Huffman tables
 */
unsigned char classic_shift_luma[] = {
  34,36,35,69,135,232,9,16,10,24,11,23,12,16,13,10,14,8,15,8,
  16,8,17,20,16,10,207,206,205,236,11,8,10,21,9,23,8,8,199,70,
  69,68, 0
};

unsigned char classic_shift_chroma[] = {
  66,36,37,38,39,40,41,75,76,77,110,239,144,81,82,83,84,85,118,183,
  56,57,88,89,56,89,154,57,58,57,26,141,57,56,58,57,58,57,184,119,
  214,245,116,83,82,49,80,79,78,77,44,75,41,40,39,38,37,36,34, 0
};

unsigned char classic_add_luma[256] = {
    3,  9,  5, 12, 10, 35, 32, 29, 27, 50, 48, 45, 44, 41, 39, 37,
   73, 70, 68, 65, 64, 61, 58, 56, 53, 50, 49, 46, 44, 41, 38, 36,
   68, 65, 63, 61, 58, 55, 53, 51, 48, 46, 45, 43, 41, 39, 38, 36,
   35, 33, 32, 30, 29, 27, 26, 25, 48, 47, 46, 44, 43, 41, 40, 39,
   37, 36, 35, 34, 32, 31, 30, 28, 27, 26, 24, 23, 22, 20, 19, 37,
   35, 34, 33, 31, 30, 29, 27, 26, 24, 23, 21, 20, 18, 17, 15, 29,
   27, 26, 24, 22, 21, 19, 17, 16, 14, 26, 25, 23, 21, 19, 18, 16,
   15, 27, 25, 23, 21, 19, 17, 16, 14, 26, 25, 23, 21, 18, 17, 14,
   12, 17, 19, 13,  4,  9,  2, 11,  1,  7,  8,  0, 16,  3, 14,  6,
   12, 10,  5, 15, 18, 11, 10, 13, 15, 16, 19, 20, 22, 24, 27, 15,
   18, 20, 22, 24, 26, 14, 17, 20, 22, 24, 27, 15, 18, 20, 23, 25,
   28, 16, 19, 22, 25, 28, 32, 36, 21, 25, 29, 33, 38, 42, 45, 49,
   28, 31, 34, 37, 40, 42, 44, 47, 49, 50, 52, 54, 56, 57, 59, 60,
   62, 64, 66, 67, 69, 35, 37, 39, 40, 42, 43, 45, 47, 48, 51, 52,
   54, 55, 57, 59, 60, 62, 63, 66, 67, 69, 71, 72, 38, 40, 42, 43,
   46, 47, 49, 51, 26, 28, 30, 31, 33, 34, 18, 19, 11, 13,  7,  8,
};

unsigned char classic_add_chroma[256] = {
    3,  1,  2,  2,  2,  2,  3,  3,  7,  5,  7,  5,  8,  6, 11,  9,
    7, 13, 11, 10,  9,  8,  7,  5,  9,  7,  6,  4,  7,  5,  8,  7,
   11,  8, 13, 11, 19, 15, 22, 23, 20, 33, 32, 28, 27, 29, 51, 77,
   43, 45, 76, 81, 46, 82, 75, 55, 56,144, 58, 80, 60, 74,147, 63,
  143, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
   80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 27, 30, 21, 22,
   17, 14,  5,  6,100, 54, 47, 50, 51, 53,106,107,108,109,110,111,
  112,113,114,115,  4,117,118, 92, 94,121,122,  3,124,103,  2,  1,
    0,129,130,131,120,119,126,125,136,137,138,139,140,141,142,134,
  135,132,133,104, 64,101, 62, 57,102, 95, 93, 59, 61, 28, 97, 96,
   52, 49, 48, 29, 32, 25, 24, 46, 23, 98, 45, 44, 43, 20, 42, 41,
   19, 18, 99, 40, 15, 39, 38, 16, 13, 12, 11, 37, 10,  9,  8, 36,
    7,128,127,105,123,116, 35, 34, 33,145, 31, 79, 42,146, 78, 26,
   83, 48, 49, 50, 44, 47, 26, 31, 30, 18, 17, 19, 21, 24, 25, 13,
   14, 16, 17, 18, 20, 21, 12, 14, 15,  9, 10,  6,  9,  6,  5,  8,
    6, 12,  8, 10,  7,  9,  6,  4,  6,  2,  2,  3,  3,  3,  3,  2,
};


/*
 * Internal function prototypes
 */
unsigned char* InitializeDecodeTable(unsigned char* hufftable,
								unsigned char* shift, DecodeTable* decode_table);
unsigned char* InitializeShiftAddTables(unsigned char* hufftable,
								unsigned char* shift, unsigned* add_shifted);
unsigned char* DecompressHuffmanTable(unsigned char* hufftable,
																			unsigned char* dst);
unsigned char huff_decompress(unsigned int* in, unsigned int *pos,
														  DecodeTable *decode_table, unsigned char *decode_shift);




// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...)
{
	switch(cmd) {
		case VDCTRL_QUERY_FORMAT:
			if  (((huffyuv_context_t *)(sh->context))->bitmaptype == BMPTYPE_YUV) {
				if (*((int*)arg) == IMGFMT_YUY2)
					return CONTROL_TRUE;
				else
					return CONTROL_FALSE;
			} else {
				if ((*((int*)arg) == IMGFMT_BGR32) || (*((int*)arg) == IMGFMT_BGR24))
					return CONTROL_TRUE;
				else
					return CONTROL_FALSE;
			}
	}
	return CONTROL_UNKNOWN;
}


/*
 *
 * Init HuffYUV decoder
 *
 */
static int init(sh_video_t *sh)
{
	int vo_ret; // Video output init ret value
	huffyuv_context_t *hc; // Decoder context
	unsigned char *hufftable; // Compressed huffman tables
	BITMAPINFOHEADER *bih = sh->bih;
	
	if ((hc = malloc(sizeof(huffyuv_context_t))) == NULL) {
		mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Can't allocate memory for HuffYUV decoder context\n");
		return 0;
	}

	sh->context = (void *)hc;

	if (bih->biCompression != FOURCC_HFYU) {
		mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[HuffYUV] BITMAPHEADER fourcc != HFYU\n");
		return 0;
	}

	/* Get bitcount */
	hc->bitcount = 0;
	if (bih->biSize > sizeof(BITMAPINFOHEADER)+1)
		hc->bitcount = *((char*)bih + sizeof(BITMAPINFOHEADER) + 1);
	if (hc->bitcount == 0)
		hc->bitcount = bih->biBitCount;

	/* Get bitmap type */
	switch (hc->bitcount & ~7) {
		case 16:
			hc->bitmaptype = BMPTYPE_YUV; // -1
			mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Image type is YUV\n");
			break;
		case 24:
			hc->bitmaptype = BMPTYPE_RGB; // -2
			mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Image type is RGB\n");
			break;
		case 32:
			hc->bitmaptype = BMPTYPE_RGBA; //-3
			mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Image type is RGBA\n");
			break;
		default:
			hc->bitmaptype = 0; // ERR
			mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[HuffYUV] Image type is unknown\n");
	}

	/* Get method */
	switch (bih->biBitCount & 7) {
		case 0:
			if (bih->biSize > sizeof(BITMAPINFOHEADER)) {
				hc->method = *((unsigned char*)bih + sizeof(BITMAPINFOHEADER));
				mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Method stored in extra data\n");
			} else
				hc->method = METHOD_OLD;	// Is it really needed?
			break;
		case 1:
			hc->method = METHOD_LEFT;
			break;
		case 2:
			hc->method = METHOD_LEFT_DECORR;
			break;
		case 3:
			if (hc->bitmaptype == BMPTYPE_YUV) {
				hc->method = METHOD_GRAD;
			} else {
				hc->method = METHOD_GRAD_DECORR;
			}
			break;
		case 4:
			hc->method = METHOD_MEDIAN;
			break;
		default:
			mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Method: fallback to METHOD_OLD\n");
			hc->method = METHOD_OLD;
	}

	/* Print method info */
	switch (hc->method) {
		case METHOD_LEFT:
			mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Method: Predict Left\n");
			break;
		case METHOD_GRAD:
			mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Method: Predict Gradient\n");
			break;
		case METHOD_MEDIAN:
			mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Method: Predict Median\n");
			break;
		case METHOD_LEFT_DECORR:
			mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Method: Predict Left with decorrelation\n");
			break;
		case METHOD_GRAD_DECORR:
			mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Method: Predict Gradient with decorrelation\n");
			break;
		case METHOD_OLD:
			mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Method Old\n");
			break;
		default:
			mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[HuffYUV] Method unknown\n");
	}

	/* Take care of interlaced images */
	hc->interlaced = 0;
	if (bih->biHeight > 288) {
		// Image is interlaced (flag != 0), but we may not care
		hc->interlaced = 1;
		mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Image is interlaced\n");
	}

	/* Allocate buffers */
	hc->abovebuf1 = NULL;
	hc->abovebuf2 = NULL;
	if ((hc->method == METHOD_MEDIAN) || (hc->method == METHOD_GRAD) ||
			(hc->method == METHOD_GRAD_DECORR)) {
		// If inetrlaced flag will be 2
		(hc->interlaced)++;
		mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Allocating above line buffer\n");
		if ((hc->abovebuf1 = malloc(sizeof(char) * 4 * bih->biWidth * hc->interlaced)) == NULL) {
			mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Can't allocate memory for HuffYUV above buffer 1\n");
			return 0;
		}

		if ((hc->abovebuf2 = malloc(sizeof(char) * 4 * bih->biWidth * hc->interlaced)) == NULL) {
			mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Can't allocate memory for HuffYUV above buffer 2\n");
			return 0;
		}
	}

	/* Get compressed Huffman tables */
	if (bih->biSize == sizeof(BITMAPINFOHEADER) /*&& !(bih->biBitCount&7)*/) {
		hufftable = (hc->bitmaptype == BMPTYPE_YUV) ? HUFFTABLE_CLASSIC_YUV : HUFFTABLE_CLASSIC_RGB;
		mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Using classic static Huffman tables\n");
	} else {
		hufftable = (unsigned char*)bih + sizeof(BITMAPINFOHEADER) + ((bih->biBitCount&7) ? 0 : 4);
		mp_msg(MSGT_DECVIDEO, MSGL_V, "[HuffYUV] Using Huffman tables stored in file\n");
	}

	/* Initialize decoder Huffman tables */
	hufftable = InitializeDecodeTable(hufftable, hc->decode1_shift, &(hc->decode1));
	hufftable = InitializeDecodeTable(hufftable, hc->decode2_shift, &(hc->decode2));
	InitializeDecodeTable(hufftable, hc->decode3_shift, &(hc->decode3));

	/*
	 * Initialize video output device
	 */
	switch (hc->bitmaptype) {
		case BMPTYPE_YUV:
			vo_ret = mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_YUY2);
			break;
		case BMPTYPE_RGB:
			vo_ret = mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_BGR24);
			break;
		case BMPTYPE_RGBA:
			mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[HuffYUV] RGBA not supported yet.\n");
			return 0;
		default:
			mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[HuffYUV] BUG! Unknown bitmaptype in vo config.\n");
			return 0;
	}

	return vo_ret;
}




/*
 *
 * Uninit HuffYUV decoder
 *
 */
static void uninit(sh_video_t *sh)
{
	if (sh->context) {
		if (((huffyuv_context_t*)&sh->context)->abovebuf1)
			free(((huffyuv_context_t*)sh->context)->abovebuf1);
		if (((huffyuv_context_t*)&sh->context)->abovebuf2)
			free(((huffyuv_context_t*)sh->context)->abovebuf2);
		free(sh->context);
	}
}



#define HUFF_DECOMPRESS_YUYV() \
{ \
	y1 = huff_decompress((unsigned int *)encoded, &pos, &(hc->decode1), hc->decode1_shift); \
	u = huff_decompress((unsigned int *)encoded, &pos, &(hc->decode2), hc->decode2_shift); \
	y2 = huff_decompress((unsigned int *)encoded, &pos, &(hc->decode1), hc->decode1_shift); \
	v = huff_decompress((unsigned int *)encoded, &pos, &(hc->decode3), hc->decode3_shift); \
}



#define HUFF_DECOMPRESS_RGB_DECORR() \
{ \
	g = huff_decompress((unsigned int *)encoded, &pos, &(hc->decode2), hc->decode2_shift); \
	b = huff_decompress((unsigned int *)encoded, &pos, &(hc->decode1), hc->decode1_shift); \
	r = huff_decompress((unsigned int *)encoded, &pos, &(hc->decode3), hc->decode3_shift); \
}



#define HUFF_DECOMPRESS_RGB() \
{ \
	b = huff_decompress((unsigned int *)encoded, &pos, &(hc->decode1), hc->decode1_shift); \
	g = huff_decompress((unsigned int *)encoded, &pos, &(hc->decode2), hc->decode2_shift); \
	r = huff_decompress((unsigned int *)encoded, &pos, &(hc->decode3), hc->decode3_shift); \
}



#define MEDIAN(left, above, aboveleft) \
{ \
	if ((mi = (above)) > (left)) { \
		mx = mi; \
		mi = (left); \
	} else \
		mx = (left); \
	tmp = (above) + (left) - (aboveleft); \
	if (tmp < mi) \
		med = mi; \
	else if (tmp > mx) \
		med = mx; \
	else \
		med = tmp; \
}



#define YUV_STORE1ST_ABOVEBUF() \
{ \
	abovebuf[0] = outptr[0] = encoded[0]; \
	abovebuf[1] = left_u = outptr[1] = encoded[1]; \
	abovebuf[2] = left_y = outptr[2] = encoded[2]; \
	abovebuf[3] = left_v = outptr[3] = encoded[3]; \
	pixel_ptr = 4; \
}



#define YUV_STORE1ST() \
{ \
	outptr[0] = encoded[0]; \
	left_u = outptr[1] = encoded[1]; \
	left_y = outptr[2] = encoded[2]; \
	left_v = outptr[3] = encoded[3]; \
	pixel_ptr = 4; \
}



#define RGB_STORE1ST() \
{ \
	pixel_ptr = (height-1)*mpi->stride[0]; \
	left_b = outptr[pixel_ptr++] = encoded[1]; \
	left_g = outptr[pixel_ptr++] = encoded[2]; \
	left_r = outptr[pixel_ptr++] = encoded[3]; \
	pixel_ptr += bgr32; \
}



#define RGB_STORE1ST_ABOVEBUF() \
{ \
	pixel_ptr = (height-1)*mpi->stride[0]; \
	abovebuf[0] = left_b = outptr[pixel_ptr++] = encoded[1]; \
	abovebuf[1] = left_g = outptr[pixel_ptr++] = encoded[2]; \
	abovebuf[2] = left_r = outptr[pixel_ptr++] = encoded[3]; \
	pixel_ptr += bgr32; \
}




#define YUV_PREDLEFT() \
{ \
	outptr[pixel_ptr++] = left_y += y1; \
	outptr[pixel_ptr++] = left_u += u; \
	outptr[pixel_ptr++] = left_y += y2; \
	outptr[pixel_ptr++] = left_v += v; \
}



#define YUV_PREDLEFT_BUF(buf, offs) \
{ \
	(buf)[(offs)] = outptr[pixel_ptr++] = left_y += y1; \
	(buf)[(offs)+1] = outptr[pixel_ptr++] = left_u += u; \
	(buf)[(offs)+2] = outptr[pixel_ptr++] = left_y += y2; \
	(buf)[(offs)+3] = outptr[pixel_ptr++] = left_v += v; \
}



#define YUV_PREDMED() \
{ \
	MEDIAN (left_y, abovebuf[col], abovebuf[col-2]); \
	curbuf[col] = outptr[pixel_ptr++] = left_y = med + y1; \
	MEDIAN (left_u, abovebuf[col+1], abovebuf[col+1-4]); \
	curbuf[col+1] = outptr[pixel_ptr++] = left_u = med + u; \
	MEDIAN (left_y, abovebuf[col+2], abovebuf[col+2-2]); \
	curbuf[col+2] = outptr[pixel_ptr++] = left_y = med + y2; \
	MEDIAN (left_v, abovebuf[col+3], abovebuf[col+3-4]); \
	curbuf[col+3] = outptr[pixel_ptr++] = left_v = med + v; \
}



#define YUV_PREDMED_1ST() \
{ \
	MEDIAN (left_y, abovebuf[0], curbuf[width2*4-2]); \
	curbuf[0] = outptr[pixel_ptr++] = left_y = med + y1; \
	MEDIAN (left_u, abovebuf[1], curbuf[width2*4+1-4]); \
	curbuf[1] = outptr[pixel_ptr++] = left_u = med + u; \
	MEDIAN (left_y, abovebuf[2], abovebuf[0]); \
	curbuf[2] = outptr[pixel_ptr++] = left_y = med + y2; \
	MEDIAN (left_v, abovebuf[3], curbuf[width2*4+3-4]); \
	curbuf[3] = outptr[pixel_ptr++] = left_v = med + v; \
}
	
	
	
#define YUV_PREDGRAD() \
{ \
	curbuf[col] = outptr[pixel_ptr++] = left_y += y1 + abovebuf[col]-abovebuf[col-2]; \
	curbuf[col+1] = outptr[pixel_ptr++] = left_u += u + abovebuf[col+1]-abovebuf[col+1-4]; \
	curbuf[col+2] = outptr[pixel_ptr++] = left_y += y2 + abovebuf[col+2]-abovebuf[col+2-2]; \
	curbuf[col+3] = outptr[pixel_ptr++] = left_v += v + abovebuf[col+3]-abovebuf[col+3-4]; \
}



#define YUV_PREDGRAD_1ST() \
{ \
	curbuf[0] = outptr[pixel_ptr++] = left_y += y1 + abovebuf[0] - curbuf[width2*4-2]; \
	curbuf[1] = outptr[pixel_ptr++] = left_u += u + abovebuf[1] - curbuf[width2*4+1-4]; \
	curbuf[2] = outptr[pixel_ptr++] = left_y += y2 + abovebuf[2] - abovebuf[0]; \
	curbuf[3] = outptr[pixel_ptr++] = left_v += v + abovebuf[3] - curbuf[width2*4+3-4]; \
}



#define RGB_PREDLEFT_DECORR() \
{ \
	outptr[pixel_ptr++] = left_b += b + g; \
	outptr[pixel_ptr++] = left_g += g; \
	outptr[pixel_ptr++] = left_r += r + g; \
	pixel_ptr += bgr32; \
}



#define RGB_PREDLEFT_DECORR_BUF() \
{ \
	abovebuf[col] = outptr[pixel_ptr++] = left_b += b + g; \
	abovebuf[col+1] = outptr[pixel_ptr++] = left_g += g; \
	abovebuf[col+2] = outptr[pixel_ptr++] = left_r += r + g; \
	pixel_ptr += bgr32; \
}


#define RGB_PREDLEFT() \
{ \
	outptr[pixel_ptr++] = left_b += b; \
	outptr[pixel_ptr++] = left_g += g; \
	outptr[pixel_ptr++] = left_r += r; \
	pixel_ptr += bgr32; \
}



#define RGB_PREDGRAD_DECORR() \
{ \
	curbuf[col] = outptr[pixel_ptr++] = left_b += b + g + abovebuf[col]-abovebuf[col-3]; \
	curbuf[col+1] = outptr[pixel_ptr++] = left_g += g + abovebuf[col+1]-abovebuf[col+1-3]; \
	curbuf[col+2] = outptr[pixel_ptr++] = left_r += r + g + abovebuf[col+2]-abovebuf[col+2-3]; \
	pixel_ptr += bgr32; \
}



#define RGB_PREDGRAD_DECORR_1ST() \
{ \
	curbuf[0] = outptr[pixel_ptr++] = left_b += b + g + abovebuf[0] - curbuf[width2*3-3]; \
	curbuf[1] = outptr[pixel_ptr++] = left_g += g + abovebuf[1] - curbuf[width2*3+1-3]; \
	curbuf[2] = outptr[pixel_ptr++] = left_r += r + g + abovebuf[2] - curbuf[width2*3+2-3]; \
	pixel_ptr += bgr32; \
}



#define SWAPBUF() \
{ \
	swap = abovebuf; \
	abovebuf = curbuf; \
	curbuf = swap; \
}



/*
 *
 * Decode a HuffYUV frame
 *
 */
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags)
{
	mp_image_t* mpi;
	int pixel_ptr;
	unsigned char y1, y2, u, v, r, g, b, a;
	unsigned char left_y, left_u, left_v, left_r, left_g, left_b;
	unsigned char tmp, mi, mx, med;
	unsigned char *swap;
	int row, col;
	unsigned int pos = 32;
	unsigned char *encoded = (unsigned char *)data;
	huffyuv_context_t *hc = (huffyuv_context_t *) sh->context; // Decoder context
	unsigned char *abovebuf = hc->abovebuf1;
	unsigned char *curbuf = hc->abovebuf2;
	unsigned char *outptr;
	int width = sh->disp_w; // Real image width
	int height = sh->disp_h; // Real image height
	int width2, height2;
	int bgr32;
	int interlaced, oddlines;

	// Skipped frame
	if(len <= 0)
		return NULL;

	/* If image is interlaced and we care about it fix size */
	if (hc->interlaced == 2) {
		width2 = width*2; // Double image width
		height2 = height/2; // Half image height
		oddlines = height%2; // Set if line number is odd
		interlaced = 1; // Used also for row counter computation, must be exactly 1
	} else {
		width2 = width; // Real image width
		height2 = height; // Real image height
		interlaced = 0;  // Flag is 0: no need to deinterlaced image
		oddlines = 0; // Don't care about odd line number if not interlaced
	}


	/* Get output image buffer */
	mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE, sh->disp_w, sh->disp_h);
	if (!mpi) {
		mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Can't allocate mpi image for huffyuv codec.\n");
		return NULL;
	}

	outptr = mpi->planes[0]; // Output image pointer

	if (hc->bitmaptype == BMPTYPE_YUV) {
		width >>= 1; // Each cycle stores two pixels
		width2 >>= 1;
		if (hc->method == METHOD_GRAD) {
			/*
			 * YUV predict gradient
			 */
			/* Store 1st pixel */
			YUV_STORE1ST_ABOVEBUF();
			// Decompress 1st row (always stored with left prediction)
			for (col = 1*4; col < width*4; col += 4) {
				HUFF_DECOMPRESS_YUYV();
				YUV_PREDLEFT_BUF (abovebuf, col);
			}
			if (interlaced) {
				pixel_ptr = mpi->stride[0];
				for (col = width*4; col < width*8; col += 4) {
					HUFF_DECOMPRESS_YUYV();
					YUV_PREDLEFT_BUF (abovebuf, col);
				}
			}
			curbuf[width2*4-1] = curbuf[width2*4-2] = curbuf[width2*4-3] = 0;
			for (row = 1; row < height2; row++) {
				pixel_ptr = (interlaced + 1) * row * mpi->stride[0];
				HUFF_DECOMPRESS_YUYV();
				YUV_PREDGRAD_1ST();
				for (col = 1*4; col < width*4; col += 4) {
					HUFF_DECOMPRESS_YUYV();
					YUV_PREDGRAD();
				}
				if (interlaced) {
					pixel_ptr = (2 * row + 1) * mpi->stride[0];
					for (col = width*4; col < width*8; col += 4) {
						HUFF_DECOMPRESS_YUYV();
						YUV_PREDGRAD();
					}
				}
				SWAPBUF();
			}
			if (oddlines) {
				pixel_ptr = 2 * height * mpi->stride[0];
				HUFF_DECOMPRESS_YUYV();
				YUV_PREDGRAD_1ST();
				for (col = 1*4; col < width*4; col += 4) {
					HUFF_DECOMPRESS_YUYV();
					YUV_PREDGRAD();
				}
			}
		} else if (hc->method == METHOD_MEDIAN) {
			/*
			 * YUV predict median
			 */
			/* Store 1st pixel */
			YUV_STORE1ST_ABOVEBUF();
			// Decompress 1st row (always stored with left prediction)
			for (col = 1*4; col < width*4; col += 4) {
				HUFF_DECOMPRESS_YUYV();
				YUV_PREDLEFT_BUF (abovebuf, col);
			}
			if (interlaced) {
				pixel_ptr = mpi->stride[0];
				for (col = width*4; col < width*8; col += 4) {
					HUFF_DECOMPRESS_YUYV();
					YUV_PREDLEFT_BUF (abovebuf, col);
				}
			}
			// Decompress 1st two pixels of 2nd row
			pixel_ptr = mpi->stride[0] * (interlaced + 1);
			HUFF_DECOMPRESS_YUYV();
			YUV_PREDLEFT_BUF (curbuf, 0);
			HUFF_DECOMPRESS_YUYV();
			YUV_PREDLEFT_BUF (curbuf, 4);
			// Complete 2nd row
			for (col = 2*4; col < width*4; col += 4) {
				HUFF_DECOMPRESS_YUYV();
				YUV_PREDMED();
			}
			if (interlaced) {
				pixel_ptr = mpi->stride[0] * 3;
				for (col = width*4; col < width*8; col += 4) {
					HUFF_DECOMPRESS_YUYV();
					YUV_PREDMED();
				}
			}
			SWAPBUF();
			for (row = 2; row < height2; row++) {
				pixel_ptr = (interlaced + 1) * row * mpi->stride[0];
				HUFF_DECOMPRESS_YUYV();
				YUV_PREDMED_1ST();
				for (col = 1*4; col < width*4; col += 4) {
					HUFF_DECOMPRESS_YUYV();
					YUV_PREDMED();
				}
				if (interlaced) {
					pixel_ptr = (2 * row + 1) * mpi->stride[0];
					for (col = width*4; col < width*8; col += 4) {
						HUFF_DECOMPRESS_YUYV();
						YUV_PREDMED();
					}
				}
				SWAPBUF();
			}
			if (oddlines) {
				pixel_ptr = 2 * height2 * mpi->stride[0];
				HUFF_DECOMPRESS_YUYV();
				YUV_PREDMED_1ST();
				for (col = 1*4; col < width*4; col += 4) {
					HUFF_DECOMPRESS_YUYV();
					YUV_PREDMED();
				}
			}
		} else {
			/*
			 * YUV predict left and predict old
			 */
			/* Store 1st pixel */
			YUV_STORE1ST();
			// Decompress 1st row (always stored with left prediction)
			for (col = 1*4; col < width*4; col += 4) {
				HUFF_DECOMPRESS_YUYV();
				YUV_PREDLEFT();
			}
			for (row = 1; row < height; row++) {
				pixel_ptr = row * mpi->stride[0];
				for (col = 0; col < width*4; col += 4) {
					HUFF_DECOMPRESS_YUYV();
					YUV_PREDLEFT();
				}
			}
		}
	} else {
		bgr32 = (mpi->bpp) >> 5; // 1 if bpp = 32, 0 if bpp = 24
		if (hc->method == METHOD_LEFT_DECORR) {
			/*
			 * RGB predict left with decorrelation
			 */
			/* Store 1st pixel */
			RGB_STORE1ST();
			// Decompress 1st row
			for (col = 1; col < width; col ++) {
				HUFF_DECOMPRESS_RGB_DECORR();
				RGB_PREDLEFT_DECORR();
			}
			for (row = 1; row < height; row++) {
				pixel_ptr = (height - row - 1) * mpi->stride[0];
				for (col = 0; col < width; col++) {
					HUFF_DECOMPRESS_RGB_DECORR();
					RGB_PREDLEFT_DECORR();
				}
			}
		} else if (hc->method == METHOD_GRAD_DECORR) {
			/*
			 * RGB predict gradient with decorrelation
			 */
			/* Store 1st pixel */
			RGB_STORE1ST_ABOVEBUF();
			// Decompress 1st row (always stored with left prediction)
			for (col = 1*3; col < width*3; col += 3) {
				HUFF_DECOMPRESS_RGB_DECORR();
				RGB_PREDLEFT_DECORR_BUF();
			}
			if (interlaced) {
				pixel_ptr = (height-2)*mpi->stride[0];
				for (col = width*3; col < width*6; col += 3) {
					HUFF_DECOMPRESS_RGB_DECORR();
					RGB_PREDLEFT_DECORR_BUF();
				}
			}
			curbuf[width2*3-1] = curbuf[width2*3-2] = curbuf[width2*3-3] = 0;
			for (row = 1; row < height2; row++) {
				pixel_ptr = (height - (interlaced + 1) * row - 1) * mpi->stride[0];
				HUFF_DECOMPRESS_RGB_DECORR();
				RGB_PREDGRAD_DECORR_1ST();
				for (col = 1*3; col < width*3; col += 3) {
					HUFF_DECOMPRESS_RGB_DECORR();
					RGB_PREDGRAD_DECORR();
				}
				if (interlaced) {
					pixel_ptr = (height - 2 * row - 2) * mpi->stride[0];
					for (col = width*3; col < width*6; col += 3) {
						HUFF_DECOMPRESS_RGB_DECORR();
						RGB_PREDGRAD_DECORR();
					}
				}
				SWAPBUF();
			}
			if (oddlines) {
				pixel_ptr = mpi->stride[0];
				HUFF_DECOMPRESS_RGB_DECORR();
				RGB_PREDGRAD_DECORR_1ST();
				for (col = 1*3; col < width*3; col += 3) {
					HUFF_DECOMPRESS_RGB_DECORR();
					RGB_PREDGRAD_DECORR();
				}
			}
		} else {
			/*
			 * RGB predict left (no decorrelation) and predict old
			 */
			/* Store 1st pixel */
			RGB_STORE1ST();
			// Decompress 1st row
			for (col = 1; col < width; col++) {
				HUFF_DECOMPRESS_RGB();
				RGB_PREDLEFT();
			}
			for (row = 1; row < height; row++) {
				pixel_ptr = (height - row - 1) * mpi->stride[0];
				for (col = 0; col < width; col++) {
					HUFF_DECOMPRESS_RGB();
					RGB_PREDLEFT();
				}
			}
		}
	}
    
	return mpi;
}



unsigned char* InitializeDecodeTable(unsigned char* hufftable,
								unsigned char* shift, DecodeTable* decode_table)
{
	unsigned int add_shifted[256];
	char code_lengths[256];
	char code_firstbits[256];
	char table_lengths[32];
	int all_zero_code=-1;
	int i, j, k;
	int firstbit, length, val;
	unsigned char* p;
	unsigned char * table;

	/* Initialize shift[] and add_shifted[] */
	hufftable = InitializeShiftAddTables(hufftable, shift, add_shifted);

	memset(table_lengths, -1, 32);

	/* Fill code_firstbits[], code_legths[] and table_lengths[] */
	for (i = 0; i < 256; ++i) {
		if (add_shifted[i]) {
			for (firstbit = 31; firstbit >= 0; firstbit--) {
				if (add_shifted[i] & (1 << firstbit)) {
					code_firstbits[i] = firstbit;
					length = shift[i] - (32 - firstbit);
					code_lengths[i] = length;
					table_lengths[firstbit] = max(table_lengths[firstbit], length);
					break;
				}
			}
		} else {
			all_zero_code = i;
		}
	}

	p = decode_table->table_data;
	*p++ = 31;
	*p++ = all_zero_code;
	for (j = 0; j < 32; ++j) {
		if (table_lengths[j] == -1) {
			decode_table->table_pointers[j] = decode_table->table_data;
		} else {
			decode_table->table_pointers[j] = p;
			*p++ = j - table_lengths[j];
			p += 1 << table_lengths[j];
		}
	}

	for (k=0; k<256; ++k) {
		if (add_shifted[k]) {
			firstbit = code_firstbits[k];
			val = add_shifted[k] - (1 << firstbit);
			table = decode_table->table_pointers[firstbit];
			memset(&table[1 + (val >> table[0])], k,
						 1 << (table_lengths[firstbit] - code_lengths[k]));
		}
	}

	return hufftable;
}



unsigned char* InitializeShiftAddTables(unsigned char* hufftable,
								unsigned char* shift, unsigned* add_shifted)
{
	int i, j;
	unsigned int bits; // must be 32bit unsigned
	int min_already_processed;
	int max_not_processed;
	int bit;

	// special-case the old tables, since they don't fit the new rules
	if (hufftable == HUFFTABLE_CLASSIC_YUV || hufftable == HUFFTABLE_CLASSIC_RGB) {
		DecompressHuffmanTable(classic_shift_luma, shift);
		for (i = 0; i < 256; ++i)
			add_shifted[i] = classic_add_luma[i] << (32 - shift[i]);
		return (hufftable == HUFFTABLE_CLASSIC_YUV) ? HUFFTABLE_CLASSIC_YUV_CHROMA : hufftable;
	} else if (hufftable == HUFFTABLE_CLASSIC_YUV_CHROMA) {
		DecompressHuffmanTable(classic_shift_chroma, shift);
		for (i = 0; i < 256; ++i)
			add_shifted[i] = classic_add_chroma[i] << (32 - shift[i]);
		return hufftable;
	}

	hufftable = DecompressHuffmanTable(hufftable, shift);

	// derive the actual bit patterns from the code lengths
	min_already_processed = 32;
	bits = 0;
	do {
		max_not_processed = 0;
		for (i = 0; i < 256; ++i) {
			if (shift[i] < min_already_processed && shift[i] > max_not_processed)
				max_not_processed = shift[i];
		}
		bit = 1 << (32 - max_not_processed);
//		assert (!(bits & (bit - 1)));
		for (j = 0; j < 256; ++j) {
			if (shift[j] == max_not_processed) {
				add_shifted[j] = bits;
				bits += bit;
			}
		}
		min_already_processed = max_not_processed;
	} while (bits & 0xFFFFFFFF);

	return hufftable;
}



unsigned char* DecompressHuffmanTable(unsigned char* hufftable,
																			unsigned char* dst)
{
	int val;
	int repeat;
	int i = 0;
	
	do {
		val = *hufftable & 31;
		repeat = *hufftable++ >> 5;
		if (!repeat)
			repeat = *hufftable++;
		while (repeat--)
			dst[i++] = val;
	} while (i < 256);

	return hufftable;
}


unsigned char huff_decompress(unsigned int* in, unsigned int *pos, DecodeTable *decode_table,
															unsigned char *decode_shift)
{
	unsigned int word = *pos >> 5;
	unsigned int bit = *pos & 31;
	unsigned int val = in[word];
	unsigned char outbyte;
	unsigned char *tableptr;
	int i;
	
	if (bit)
		val = (val << bit) | (in[word + 1] >> (32 - bit));
  // figure out the appropriate lookup table based on the number of leading zeros
	i = 31;
	val |= 1;
	while ((val & (1 << i--)) == 0);
	val &= ~(1 << (i+1));
	tableptr = decode_table->table_pointers[i+1]; 
	val >>= *tableptr;

	outbyte = tableptr[val+1];
	*pos += decode_shift[outbyte];

	return outbyte;
}
