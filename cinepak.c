/* ------------------------------------------------------------------------
 * Radius Cinepak Video Decoder
 *
 * Dr. Tim Ferguson, 2001.
 * For more details on the algorithm:
 *         http://www.csse.monash.edu.au/~timf/videocodec.html
 *
 * This is basically a vector quantiser with adaptive vector density.  The
 * frame is segmented into 4x4 pixel blocks, and each block is coded using
 * either 1 or 4 vectors.
 *
 * There are still some issues with this code yet to be resolved.  In
 * particular with decoding in the strip boundaries.
 * ------------------------------------------------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "config.h"
#include "mp_msg.h"
#include "bswap.h"

#include "libvo/img_format.h"
#include "mp_image.h"

#define DBUG	0
#define MAX_STRIPS 32

/* ------------------------------------------------------------------------ */
typedef struct
{
	unsigned char y0, y1, y2, y3;
	char u, v;

	// These variables are for YV12 output: The v1 vars are for
	// when the vector is doublesized and used by itself to paint a
	// 4x4 block.
	// This quad (y0 y0 y1 y1) is used on the 2 upper rows.
	unsigned long yv12_v1_u;
	// This quad (y2 y2 y3 y3) is used on the 2 lower rows.
	unsigned long yv12_v1_l;
	// The v4 vars are for when the vector is used as 1 of 4 vectors
	// to paint a 4x4 block.
	// Upper pair (y0 y1):
	unsigned short yv12_v4_u;
	// Lower pair (y2 y3):
	unsigned short yv12_v4_l;

	// These longs are for YUY2 output: The v1 vars are for when the
	// vector is doublesized and used by itself to paint a 4x4 block.
	// The names stand for the upper-left, upper-right,
	// lower-left, and lower-right YUY2 pixel pairs.
        unsigned long yuy2_v1_ul, yuy2_v1_ur;
        unsigned long yuy2_v1_ll, yuy2_v1_lr;
	// The v4 vars are for when the vector is used as 1 of 4 vectors
	// to paint a 4x4 block. The names stand for upper and lower
	// YUY2 pixel pairs.
        unsigned long yuy2_v4_u, yuy2_v4_l;

	// These longs are for BGR32 output
	unsigned long rgb0, rgb1, rgb2, rgb3;

	// These char arrays are for BGR24 output
	unsigned char r[4], g[4], b[4];
} cvid_codebook;

typedef struct {
	cvid_codebook *v4_codebook[MAX_STRIPS];
	cvid_codebook *v1_codebook[MAX_STRIPS];
	unsigned long strip_num;
} cinepak_info;


/* ------------------------------------------------------------------------ */
static unsigned char *in_buffer, uiclip[1024], *uiclp = NULL;

#define SCALEBITS 16
#define ONE_HALF  ((long) 1 << (SCALEBITS-1))
#define FIX(x)    ((long) ((x) * (1L<<SCALEBITS) + 0.5))
static long CU_Y_tab[256], CV_Y_tab[256], CU_Cb_tab[256], CV_Cb_tab[256],
	CU_Cr_tab[256], CV_Cr_tab[256];

#define get_byte() *(in_buffer++)
#define skip_byte() in_buffer++
#define get_word() ((unsigned short)(in_buffer += 2, \
	(in_buffer[-2] << 8 | in_buffer[-1])))
#define get_long() ((unsigned long)(in_buffer += 4, \
	(in_buffer[-4] << 24 | in_buffer[-3] << 16 | in_buffer[-2] << 8 | in_buffer[-1])))


/* ---------------------------------------------------------------------- */

// This PACKing macro packs the luminance bytes as y1-y1-y0-y0, which is
// stored on a little endian machine as y0-y0-y1-y1. Therefore, treat it as
// a little endian number and rearrange the bytes on big endian machines
// using the built-in byte order macros.
#define PACK_YV12_V1_Y(cb,y0,y1) le2me_32((((unsigned char)cb->y1)<<24)|(cb->y1<<16)|(((unsigned char)cb->y0)<<8)|(cb->y0))
#define PACK_YV12_V4_Y(cb,y0,y1) le2me_16((((unsigned char)cb->y1)<<8)|(cb->y0))

static inline void read_codebook_yv12(cvid_codebook *c, int mode)
{
unsigned char y0, y1, y2, y3, u, v;
int y_uv;

	if(mode)		/* black and white */
		{
		c->y0 = get_byte();
		c->y1 = get_byte();
		c->y2 = get_byte();
		c->y3 = get_byte();
		c->u = c->v = 128;
		}
	else			/* colour */
		{
		y0 = get_byte();  /* luma */
		y1 = get_byte();
		y2 = get_byte();
		y3 = get_byte();
		u = 128+get_byte(); /* chroma */
		v = 128+get_byte();

		/*             YUV * inv(CinYUV)
		 *  | Y  |   | 1 -0.0655  0.0110 | | CY |
		 *  | Cb | = | 0  1.1656 -0.0062 | | CU |
		 *  | Cr |   | 0  0.0467  1.4187 | | CV |
		 */
		y_uv = (int)((CU_Y_tab[u] + CV_Y_tab[v]) >> SCALEBITS);
		c->y0 = uiclp[y0 + y_uv];
		c->y1 = uiclp[y1 + y_uv];
		c->y2 = uiclp[y2 + y_uv];
		c->y3 = uiclp[y3 + y_uv];
		c->u = uiclp[(int)((CU_Cb_tab[u] + CV_Cb_tab[v]) >> SCALEBITS)];
		c->v = uiclp[(int)((CU_Cr_tab[u] + CV_Cr_tab[v]) >> SCALEBITS)];

		c->yv12_v1_u = PACK_YV12_V1_Y(c, y0, y1);
		c->yv12_v1_l = PACK_YV12_V1_Y(c, y2, y3);
		c->yv12_v4_u = PACK_YV12_V4_Y(c, y0, y1);
		c->yv12_v4_l = PACK_YV12_V4_Y(c, y2, y3);
		}
}

/* ---------------------------------------------------------------------- */

inline void cvid_v1_yv12(mp_image_t *mpi, unsigned int x, unsigned int y, cvid_codebook *cb)
{
unsigned char *p;
int stride;

	if(y+3>=(unsigned int)mpi->height) return; // avoid sig11

	// take care of the luminance
	stride = mpi->stride[0];
	p = mpi->planes[0]+y*stride+x;
	*((unsigned int*)p)=cb->yv12_v1_u;
	*((unsigned int*)(p+stride))=cb->yv12_v1_u;
	*((unsigned int*)(p+stride*2))=cb->yv12_v1_l;
	*((unsigned int*)(p+stride*3))=cb->yv12_v1_l;

	// now for the chrominance
	x/=2; y/=2;

	stride = mpi->stride[1];
	p = mpi->planes[1]+y*stride+x;
	p[0]=p[1]=p[stride]=p[stride+1]=cb->u;

	stride = mpi->stride[2];
	p = mpi->planes[2]+y*stride+x;
	p[0]=p[1]=p[stride]=p[stride+1]=cb->v;

}

/* ---------------------------------------------------------------------- */
inline void cvid_v4_yv12(mp_image_t *mpi, unsigned int x, unsigned int y, cvid_codebook *cb0,
	cvid_codebook *cb1, cvid_codebook *cb2, cvid_codebook *cb3)
{
unsigned char *p;
int stride;

	if(y+3>=(unsigned int)mpi->height) return; // avoid sig11

	// take care of the luminance
	stride = mpi->stride[0];
	p = mpi->planes[0]+y*stride+x;
	((unsigned short*)p)[0]=cb0->yv12_v4_u;
	((unsigned short*)p)[1]=cb1->yv12_v4_u;
	((unsigned short*)(p+stride))[0]=cb0->yv12_v4_l;
	((unsigned short*)(p+stride))[1]=cb1->yv12_v4_l;
	((unsigned short*)(p+stride*2))[0]=cb2->yv12_v4_u;
	((unsigned short*)(p+stride*2))[1]=cb3->yv12_v4_u;
	((unsigned short*)(p+stride*3))[0]=cb2->yv12_v4_l;
	((unsigned short*)(p+stride*3))[1]=cb3->yv12_v4_l;

	// now for the chrominance
	x/=2; y/=2;

	stride = mpi->stride[1];
	p = mpi->planes[1]+y*stride+x;
	p[0]=cb0->u; p[1]=cb1->u;
	p[stride]=cb2->u; p[stride+1]=cb3->u;

	stride = mpi->stride[2];
	p = mpi->planes[2]+y*stride+x;
	p[0]=cb0->v; p[1]=cb1->v;
	p[stride]=cb2->v; p[stride+1]=cb3->v;

}

/* ---------------------------------------------------------------------- */

#define PACK_YUY2(cb,y0,y1,u,v) le2me_32(((((unsigned char)cb->v)<<24)|(cb->y1<<16)|(((unsigned char)cb->u)<<8)|(cb->y0)))

static inline void read_codebook_yuy2(cvid_codebook *c, int mode)
{
unsigned char y0, y1, y2, y3, u, v;
int y_uv;

	if(mode)		/* black and white */
		{
		c->y0 = get_byte();
		c->y1 = get_byte();
		c->y2 = get_byte();
		c->y3 = get_byte();
		c->u = c->v = 128;
		}
	else			/* colour */
		{
		y0 = get_byte();  /* luma */
		y1 = get_byte();
		y2 = get_byte();
		y3 = get_byte();
		u = 128+get_byte(); /* chroma */
		v = 128+get_byte();

		/*             YUV * inv(CinYUV)
		 *  | Y  |   | 1 -0.0655  0.0110 | | CY |
		 *  | Cb | = | 0  1.1656 -0.0062 | | CU |
		 *  | Cr |   | 0  0.0467  1.4187 | | CV |
		 */
		y_uv = (int)((CU_Y_tab[u] + CV_Y_tab[v]) >> SCALEBITS);
		c->y0 = uiclp[y0 + y_uv];
		c->y1 = uiclp[y1 + y_uv];
		c->y2 = uiclp[y2 + y_uv];
		c->y3 = uiclp[y3 + y_uv];
		c->u = uiclp[(int)((CU_Cb_tab[u] + CV_Cb_tab[v]) >> SCALEBITS)];
		c->v = uiclp[(int)((CU_Cr_tab[u] + CV_Cr_tab[v]) >> SCALEBITS)];

		c->yuy2_v4_u = PACK_YUY2(c, y0, y1, u, v);
		c->yuy2_v4_l = PACK_YUY2(c, y2, y3, u, v);
		c->yuy2_v1_ul = PACK_YUY2(c, y0, y0, u, v);
		c->yuy2_v1_ur = PACK_YUY2(c, y1, y1, u, v);
		c->yuy2_v1_ll = PACK_YUY2(c, y2, y2, u, v);
		c->yuy2_v1_lr = PACK_YUY2(c, y3, y3, u, v);
		}
}

/* ------------------------------------------------------------------------ */
inline void cvid_v1_yuy2(mp_image_t *mpi, unsigned int x, unsigned int y, cvid_codebook *cb)
{
int stride = mpi->stride[0] / 2;
unsigned long *vptr = (unsigned long *)mpi->planes[0];

	if(y+3>=(unsigned int)mpi->height) return; // avoid sig11

	vptr += (y * mpi->stride[0] + x) / 2;

	vptr[0] = cb->yuy2_v1_ul;
	vptr[1] = cb->yuy2_v1_ur;

	vptr += stride;
	vptr[0] = cb->yuy2_v1_ul;
	vptr[1] = cb->yuy2_v1_ur;

	vptr += stride;
	vptr[0] = cb->yuy2_v1_ll;
	vptr[1] = cb->yuy2_v1_lr;

	vptr += stride;
	vptr[0] = cb->yuy2_v1_ll;
	vptr[1] = cb->yuy2_v1_lr;
}


/* ------------------------------------------------------------------------ */
inline void cvid_v4_yuy2(mp_image_t *mpi, unsigned int x, unsigned int y, cvid_codebook *cb0,
	cvid_codebook *cb1, cvid_codebook *cb2, cvid_codebook *cb3)
{
int stride = mpi->stride[0] / 2;
unsigned long *vptr = (unsigned long *)mpi->planes[0];

	if(y+3>=(unsigned int)mpi->height) return; // avoid sig11

	vptr += (y * mpi->stride[0] + x) / 2;

	vptr[0] = cb0->yuy2_v4_u;
	vptr[1] = cb1->yuy2_v4_u;

	vptr += stride;
	vptr[0] = cb0->yuy2_v4_l;
	vptr[1] = cb1->yuy2_v4_l;

	vptr += stride;
	vptr[0] = cb2->yuy2_v4_u;
	vptr[1] = cb3->yuy2_v4_u;

	vptr += stride;
	vptr[0] = cb2->yuy2_v4_l;
	vptr[1] = cb3->yuy2_v4_l;
}

/* ---------------------------------------------------------------------- */
static inline void read_codebook_32(cvid_codebook *c, int mode)
{
int uvr, uvg, uvb;

	if(mode)		/* black and white */
		{
		c->y0 = get_byte();
		c->y1 = get_byte();
		c->y2 = get_byte();
		c->y3 = get_byte();
		c->u = c->v = 0;

		c->rgb0 = (c->y0 << 16) | (c->y0 << 8) | c->y0;
		c->rgb1 = (c->y1 << 16) | (c->y1 << 8) | c->y1;
		c->rgb2 = (c->y2 << 16) | (c->y2 << 8) | c->y2;
		c->rgb3 = (c->y3 << 16) | (c->y3 << 8) | c->y3;
		}
	else			/* colour */
		{
		c->y0 = get_byte();  /* luma */
		c->y1 = get_byte();
		c->y2 = get_byte();
		c->y3 = get_byte();
		c->u = get_byte(); /* chroma */
		c->v = get_byte();

		uvr = c->v << 1;
		uvg = -((c->u+1) >> 1) - c->v;
		uvb = c->u << 1;

		c->rgb0 = le2me_32((uiclp[c->y0 + uvr] << 16) | (uiclp[c->y0 + uvg] << 8) | uiclp[c->y0 + uvb]);
		c->rgb1 = le2me_32((uiclp[c->y1 + uvr] << 16) | (uiclp[c->y1 + uvg] << 8) | uiclp[c->y1 + uvb]);
		c->rgb2 = le2me_32((uiclp[c->y2 + uvr] << 16) | (uiclp[c->y2 + uvg] << 8) | uiclp[c->y2 + uvb]);
		c->rgb3 = le2me_32((uiclp[c->y3 + uvr] << 16) | (uiclp[c->y3 + uvg] << 8) | uiclp[c->y3 + uvb]);
		}
}


/* ------------------------------------------------------------------------ */
inline void cvid_v1_32(mp_image_t *mpi, unsigned int x, unsigned int y, cvid_codebook *cb)
{
int stride = mpi->stride[0];
unsigned long *vptr = (unsigned long *)mpi->planes[0];
unsigned long rgb;

	if(y+3>=(unsigned int)mpi->height) return; // avoid sig11

	vptr += (y * stride + x);

	vptr[0] = rgb = cb->rgb0; vptr[1] = rgb;
	vptr[2] = rgb = cb->rgb1; vptr[3] = rgb;
	vptr += stride;
	vptr[0] = rgb = cb->rgb0; vptr[1] = rgb;
	vptr[2] = rgb = cb->rgb1; vptr[3] = rgb;
	vptr += stride;
	vptr[0] = rgb = cb->rgb2; vptr[1] = rgb;
	vptr[2] = rgb = cb->rgb3; vptr[3] = rgb;
	vptr += stride;
	vptr[0] = rgb = cb->rgb2; vptr[1] = rgb;
	vptr[2] = rgb = cb->rgb3; vptr[3] = rgb;
}


/* ------------------------------------------------------------------------ */
inline void cvid_v4_32(mp_image_t *mpi, unsigned int x, unsigned int y, cvid_codebook *cb0,
	cvid_codebook *cb1, cvid_codebook *cb2, cvid_codebook *cb3)
{
int stride = mpi->stride[0];
unsigned long *vptr = (unsigned long *)mpi->planes[0];

	if(y+3>=(unsigned int)mpi->height) return; // avoid sig11

	vptr += (y * stride + x);

	vptr[0] = cb0->rgb0;
	vptr[1] = cb0->rgb1;
	vptr[2] = cb1->rgb0;
	vptr[3] = cb1->rgb1;
	vptr += stride;
	vptr[0] = cb0->rgb2;
	vptr[1] = cb0->rgb3;
	vptr[2] = cb1->rgb2;
	vptr[3] = cb1->rgb3;
	vptr += stride;
	vptr[0] = cb2->rgb0;
	vptr[1] = cb2->rgb1;
	vptr[2] = cb3->rgb0;
	vptr[3] = cb3->rgb1;
	vptr += stride;
	vptr[0] = cb2->rgb2;
	vptr[1] = cb2->rgb3;
	vptr[2] = cb3->rgb2;
	vptr[3] = cb3->rgb3;
}


/* ---------------------------------------------------------------------- */
static inline void read_codebook_24(cvid_codebook *c, int mode)
{
int uvr, uvg, uvb;

	if(mode)		/* black and white */
		{
		c->y0 = get_byte();
		c->y1 = get_byte();
		c->y2 = get_byte();
		c->y3 = get_byte();
		c->u = c->v = 0;

		c->r[0] = c->g[0] = c->b[0] = c->y0;
		c->r[1] = c->g[1] = c->b[1] = c->y1;
		c->r[2] = c->g[2] = c->b[2] = c->y2;
		c->r[3] = c->g[3] = c->b[3] = c->y3;
		}
	else			/* colour */
		{
		c->y0 = get_byte();  /* luma */
		c->y1 = get_byte();
		c->y2 = get_byte();
		c->y3 = get_byte();
		c->u = get_byte(); /* chroma */
		c->v = get_byte();

		uvr = c->v << 1;
		uvg = -((c->u+1) >> 1) - c->v;
		uvb = c->u << 1;

		c->r[0] = uiclp[c->y0 + uvr]; c->g[0] = uiclp[c->y0 + uvg]; c->b[0] = uiclp[c->y0 + uvb];
		c->r[1] = uiclp[c->y1 + uvr]; c->g[1] = uiclp[c->y1 + uvg]; c->b[1] = uiclp[c->y1 + uvb];
		c->r[2] = uiclp[c->y2 + uvr]; c->g[2] = uiclp[c->y2 + uvg]; c->b[2] = uiclp[c->y2 + uvb];
		c->r[3] = uiclp[c->y3 + uvr]; c->g[3] = uiclp[c->y3 + uvg]; c->b[3] = uiclp[c->y3 + uvb];
		}
}


/* ------------------------------------------------------------------------ */
inline void cvid_v1_24(mp_image_t *mpi, unsigned int x, unsigned int y, cvid_codebook *cb)
{
unsigned char r, g, b;
int stride = (mpi->stride[0]-4)*3;
unsigned char *vptr = mpi->planes[0] + (y * mpi->stride[0] + x) * 3;

	if(y+3>=(unsigned int)mpi->height) return; // avoid sig11

	*vptr++ = b = cb->b[0]; *vptr++ = g = cb->g[0]; *vptr++ = r = cb->r[0];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	*vptr++ = b = cb->b[1]; *vptr++ = g = cb->g[1]; *vptr++ = r = cb->r[1];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	vptr += stride;
	*vptr++ = b = cb->b[0]; *vptr++ = g = cb->g[0]; *vptr++ = r = cb->r[0];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	*vptr++ = b = cb->b[1]; *vptr++ = g = cb->g[1]; *vptr++ = r = cb->r[1];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	vptr += stride;
	*vptr++ = b = cb->b[2]; *vptr++ = g = cb->g[2]; *vptr++ = r = cb->r[2];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	*vptr++ = b = cb->b[3]; *vptr++ = g = cb->g[3]; *vptr++ = r = cb->r[3];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	vptr += stride;
	*vptr++ = b = cb->b[2]; *vptr++ = g = cb->g[2]; *vptr++ = r = cb->r[2];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	*vptr++ = b = cb->b[3]; *vptr++ = g = cb->g[3]; *vptr++ = r = cb->r[3];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
}


/* ------------------------------------------------------------------------ */
inline void cvid_v4_24(mp_image_t *mpi, unsigned int x, unsigned int y, cvid_codebook *cb0,
	cvid_codebook *cb1, cvid_codebook *cb2, cvid_codebook *cb3)
{
int stride = (mpi->stride[0]-4)*3;
unsigned char *vptr = mpi->planes[0] + (y * mpi->stride[0] + x) * 3;

	if(y+3>=(unsigned int)mpi->height) return; // avoid sig11

	*vptr++ = cb0->b[0]; *vptr++ = cb0->g[0]; *vptr++ = cb0->r[0];
	*vptr++ = cb0->b[1]; *vptr++ = cb0->g[1]; *vptr++ = cb0->r[1];
	*vptr++ = cb1->b[0]; *vptr++ = cb1->g[0]; *vptr++ = cb1->r[0];
	*vptr++ = cb1->b[1]; *vptr++ = cb1->g[1]; *vptr++ = cb1->r[1];
	vptr += stride;
	*vptr++ = cb0->b[2]; *vptr++ = cb0->g[2]; *vptr++ = cb0->r[2];
	*vptr++ = cb0->b[3]; *vptr++ = cb0->g[3]; *vptr++ = cb0->r[3];
	*vptr++ = cb1->b[2]; *vptr++ = cb1->g[2]; *vptr++ = cb1->r[2];
	*vptr++ = cb1->b[3]; *vptr++ = cb1->g[3]; *vptr++ = cb1->r[3];
	vptr += stride;
	*vptr++ = cb2->b[0]; *vptr++ = cb2->g[0]; *vptr++ = cb2->r[0];
	*vptr++ = cb2->b[1]; *vptr++ = cb2->g[1]; *vptr++ = cb2->r[1];
	*vptr++ = cb3->b[0]; *vptr++ = cb3->g[0]; *vptr++ = cb3->r[0];
	*vptr++ = cb3->b[1]; *vptr++ = cb3->g[1]; *vptr++ = cb3->r[1];
	vptr += stride;
	*vptr++ = cb2->b[2]; *vptr++ = cb2->g[2]; *vptr++ = cb2->r[2];
	*vptr++ = cb2->b[3]; *vptr++ = cb2->g[3]; *vptr++ = cb2->r[3];
	*vptr++ = cb3->b[2]; *vptr++ = cb3->g[2]; *vptr++ = cb3->r[2];
	*vptr++ = cb3->b[3]; *vptr++ = cb3->g[3]; *vptr++ = cb3->r[3];
}

/* ------------------------------------------------------------------------
 * Call this function once at the start of the sequence and save the
 * returned context for calls to decode_cinepak().
 */
void *decode_cinepak_init(void)
{
cinepak_info *cvinfo;
int i, x;

	if((cvinfo = calloc(sizeof(cinepak_info), 1)) == NULL) return NULL;
	cvinfo->strip_num = 0;

	if(uiclp == NULL)
		{
		uiclp = uiclip+512;
		for(i = -512; i < 512; i++)
			uiclp[i] = (i < 0 ? 0 : (i > 255 ? 255 : i));
		}

	for(i = 0, x = -128; i < 256; i++, x++)
		{
		CU_Y_tab[i] = (-FIX(0.0655)) * x;
		CV_Y_tab[i] = (FIX(0.0110)) * x + ONE_HALF;
		CU_Cb_tab[i] = (FIX(1.1656)) * x;
		CV_Cb_tab[i] = (-FIX(0.0062)) * x + ONE_HALF + FIX(128);
		CU_Cr_tab[i] = (FIX(0.0467)) * x;
		CV_Cr_tab[i] = (FIX(1.4187)) * x + ONE_HALF + FIX(128);
		}

	return (void *)cvinfo;
}


/* ------------------------------------------------------------------------
 * This function decodes a buffer containing a Cinepak encoded frame.
 *
 * context - the context created by decode_cinepak_init().
 * buf - the input buffer to be decoded
 * size - the size of the input buffer
 * frame - the output frame buffer
 * width - the width of the output frame
 * height - the height of the output frame
 * bit_per_pixel - the number of bits per pixel allocated to the output
 *   frame; depths support:
 *     32: BGR32
 *     24: BGR24
 *     16: YUY2
 *     12: YV12
 */
void decode_cinepak(void *context, unsigned char *buf, int size, mp_image_t *mpi)
{
cinepak_info *cvinfo = (cinepak_info *)context;
cvid_codebook *v4_codebook, *v1_codebook, *codebook = NULL;
unsigned long x, y, y_bottom, frame_flags, strips, cv_width, cv_height, cnum,
	strip_id, chunk_id, x0, y0, x1, y1, ci, flag, mask;
long len, top_size, chunk_size;
unsigned int i, cur_strip, d0, d1, d2, d3;
int modulo;
void (*read_codebook)(cvid_codebook *c, int mode) = NULL;
void (*cvid_v1)(mp_image_t *mpi, unsigned int x, unsigned int y, cvid_codebook *cb) = NULL;
void (*cvid_v4)(mp_image_t *mpi, unsigned int x, unsigned int y, cvid_codebook *cb0,
	cvid_codebook *cb1, cvid_codebook *cb2, cvid_codebook *cb3) = NULL;

	x = y = 0;
	y_bottom = 0;
	in_buffer = buf;

	frame_flags = get_byte();
	len = get_byte() << 16;
	len |= get_byte() << 8;
	len |= get_byte();

	switch(mpi->imgfmt)
		{
		case IMGFMT_YV12:  // YV12
			read_codebook = read_codebook_yv12;
			cvid_v1 = cvid_v1_yv12;
			cvid_v4 = cvid_v4_yv12;
			break;
		case IMGFMT_YUY2:  // YUY2
			read_codebook = read_codebook_yuy2;
			cvid_v1 = cvid_v1_yuy2;
			cvid_v4 = cvid_v4_yuy2;
			break;
		case IMGFMT_BGR24:  // BGR24
			read_codebook = read_codebook_24;
			cvid_v1 = cvid_v1_24;
			cvid_v4 = cvid_v4_24;
			break;
		case IMGFMT_BGR32:  // BGR32
			read_codebook = read_codebook_32;
			cvid_v1 = cvid_v1_32;
			cvid_v4 = cvid_v4_32;
			break;
		}

	if(len != size)
		{
		if(len & 0x01) len++; /* AVIs tend to have a size mismatch */
		if(len != size)
			{
			mp_msg(MSGT_DECVIDEO, MSGL_WARN, "CVID: corruption %d (QT/AVI) != %ld (CV)\n", size, len);
			// return;
			}
		}

	cv_width = get_word();
	cv_height = get_word();
	strips = get_word();

	if(strips > cvinfo->strip_num)
		{
		if(strips >= MAX_STRIPS) 
			{
			mp_msg(MSGT_DECVIDEO, MSGL_WARN, "CVID: strip overflow (more than %d)\n", MAX_STRIPS);
			return;
			}

		for(i = cvinfo->strip_num; i < strips; i++)
			{
			if((cvinfo->v4_codebook[i] = (cvid_codebook *)calloc(sizeof(cvid_codebook), 260)) == NULL)
				{
				mp_msg(MSGT_DECVIDEO, MSGL_WARN, "CVID: codebook v4 alloc err\n");
				return;
				}

			if((cvinfo->v1_codebook[i] = (cvid_codebook *)calloc(sizeof(cvid_codebook), 260)) == NULL)
				{
				mp_msg(MSGT_DECVIDEO, MSGL_WARN, "CVID: codebook v1 alloc err\n");
				return;
				}
			}
		}
	cvinfo->strip_num = strips;

#if DBUG
	mp_msg(MSGT_DECVIDEO, MSGL_WARN, "CVID: <%ld,%ld> strips %ld\n", cv_width, cv_height, strips);
#endif

	for(cur_strip = 0; cur_strip < strips; cur_strip++)
		{
		v4_codebook = cvinfo->v4_codebook[cur_strip];
		v1_codebook = cvinfo->v1_codebook[cur_strip];

		if((cur_strip > 0) && (!(frame_flags & 0x01)))
			{
			memcpy(cvinfo->v4_codebook[cur_strip], cvinfo->v4_codebook[cur_strip-1], 260 * sizeof(cvid_codebook));
			memcpy(cvinfo->v1_codebook[cur_strip], cvinfo->v1_codebook[cur_strip-1], 260 * sizeof(cvid_codebook));
			}

		strip_id = get_word();		/* 1000 = key strip, 1100 = iter strip */
		top_size = get_word();
		y0 = get_word();		/* FIXME: most of these are ignored at the moment */
		x0 = get_word();
		y1 = get_word();
		x1 = get_word();

		y_bottom += y1;
		top_size -= 12;
		x = 0;
//		if(x1 != (unsigned int)mpi->width) 
//			mp_msg(MSGT_DECVIDEO, MSGL_WARN, "CVID: Warning x1 (%ld) != width (%d)\n", x1, mpi->width);

//x1 = mpi->width;
#if DBUG
	mp_msg(MSGT_DECVIDEO, MSGL_WARN, "   %d) %04lx %04ld <%ld,%ld> <%ld,%ld> yt %ld  %d\n",
		cur_strip, strip_id, top_size, x0, y0, x1, y1, y_bottom);
#endif

		while(top_size > 0)
			{
			chunk_id  = get_word();
			chunk_size = get_word();

#if DBUG
	mp_msg(MSGT_DECVIDEO, MSGL_WARN, "        %04lx %04ld\n", chunk_id, chunk_size);
#endif
			top_size -= chunk_size;
			chunk_size -= 4;

			switch(chunk_id)
				{
					/* -------------------- Codebook Entries -------------------- */
				case 0x2000:
				case 0x2200:
					modulo = chunk_size % 6;
					codebook = (chunk_id == 0x2200 ? v1_codebook : v4_codebook);
					cnum = (chunk_size - modulo) / 6;
					for(i = 0; i < cnum; i++) read_codebook(codebook+i, 0);
					while (modulo--)
						in_buffer++;
					break;

				case 0x2400:
				case 0x2600:		/* 8 bit per pixel */
					codebook = (chunk_id == 0x2600 ? v1_codebook : v4_codebook);
					cnum = chunk_size/4;  
					for(i = 0; i < cnum; i++) read_codebook(codebook+i, 1);
					break;

				case 0x2100:
				case 0x2300:
					codebook = (chunk_id == 0x2300 ? v1_codebook : v4_codebook);

					ci = 0;
					while(chunk_size > 3)
						{
						flag = get_long();
						chunk_size -= 4;

						for(i = 0; i < 32; i++)
							{
							if(flag & 0x80000000)
								{
								chunk_size -= 6;
								read_codebook(codebook+ci, 0);
								}

							ci++;
							flag <<= 1;
							}
						}
					while(chunk_size > 0) { skip_byte(); chunk_size--; }
					break;

				case 0x2500:
				case 0x2700:		/* 8 bit per pixel */
					codebook = (chunk_id == 0x2700 ? v1_codebook : v4_codebook);

					ci = 0;
					while(chunk_size > 0)
						{
						flag = get_long();
						chunk_size -= 4;

						for(i = 0; i < 32; i++)
							{
							if(flag & 0x80000000)
								{
								chunk_size -= 4;
								read_codebook(codebook+ci, 1);
								}

							ci++;
							flag <<= 1;
							}
						}
					while(chunk_size > 0) { skip_byte(); chunk_size--; }
					break;

					/* -------------------- Frame -------------------- */
				case 0x3000: 
					while((chunk_size > 0) && (y < y_bottom))
						{
						flag = get_long();
						chunk_size -= 4;
						
						for(i = 0; i < 32; i++)
							{ 
							if(y >= y_bottom) break;
							if(flag & 0x80000000)	/* 4 bytes per block */
								{
								d0 = get_byte();
								d1 = get_byte();
								d2 = get_byte();
								d3 = get_byte();
								chunk_size -= 4;
								cvid_v4(mpi, x, y, v4_codebook+d0, v4_codebook+d1, v4_codebook+d2, v4_codebook+d3);
								}
							else		/* 1 byte per block */
								{
								cvid_v1(mpi, x, y, v1_codebook + get_byte());
								chunk_size--;
								}

							x += 4;
							if(x >= (unsigned int)x1)
								{
								x = 0;
								y += 4;
								}
							flag <<= 1;
							}
						}
					while(chunk_size > 0) { skip_byte(); chunk_size--; }
					break;

				case 0x3100:
					while((chunk_size > 0) && (y < y_bottom))
						{
							/* ---- flag bits: 0 = SKIP, 10 = V1, 11 = V4 ---- */
						flag = (unsigned long)get_long();
						chunk_size -= 4;
						mask = 0x80000000;

						while((mask) && (y < y_bottom))
							{
							if(flag & mask)
								{
								if(mask == 1)
									{
									if(chunk_size < 0) break;
									flag = (unsigned long)get_long();
									chunk_size -= 4;
									mask = 0x80000000;
									}
								else mask >>= 1;
								
								if(flag & mask)		/* V4 */
									{
									d0 = get_byte();
									d1 = get_byte();
									d2 = get_byte();
									d3 = get_byte();
									chunk_size -= 4;
									cvid_v4(mpi, x, y, v4_codebook+d0, v4_codebook+d1, v4_codebook+d2, v4_codebook+d3);
									}
								else		/* V1 */
									{
									chunk_size--;
									cvid_v1(mpi, x, y, v1_codebook + get_byte());
									}
								}		/* else SKIP */

							mask >>= 1;
							x += 4;
							if(x >= (unsigned int)x1)
								{
								x = 0;
								y += 4;
								}
							}
						}

					while(chunk_size > 0) { skip_byte(); chunk_size--; }
					break;

				case 0x3200:		/* each byte is a V1 codebook */
					while((chunk_size > 0) && (y < y_bottom))
						{
						cvid_v1(mpi, x, y, v1_codebook + get_byte());
						chunk_size--;
						x += 4;
						if(x >= (unsigned int)x1)
							{
							x = 0;
							y += 4;
							}
						}
					while(chunk_size > 0) { skip_byte(); chunk_size--; }
					break;

				default:
					mp_msg(MSGT_DECVIDEO, MSGL_WARN, "CVID: unknown chunk_id %08lx\n", chunk_id);
					while(chunk_size > 0) { skip_byte(); chunk_size--; }
					break;
				}
			}
		}

	if(len != size)
		{
		if(len & 0x01) len++; /* AVIs tend to have a size mismatch */
		if(len != size)
			{
			long xlen;
			skip_byte();
			xlen = get_byte() << 16;
			xlen |= get_byte() << 8;
			xlen |= get_byte(); /* Read Len */
			mp_msg(MSGT_DECVIDEO, MSGL_WARN, "CVID: END INFO chunk size %d cvid size1 %ld cvid size2 %ld\n", size, len, xlen);
			}
		}
}

