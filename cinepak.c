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

#define DBUG	0
#define MAX_STRIPS 32

/* ------------------------------------------------------------------------ */
typedef struct
{
	unsigned char y0, y1, y2, y3;
	char u, v;
	unsigned long rgb0, rgb1, rgb2, rgb3;		/* should be a union */
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
		}
}

//#define PACK_YUV(cb,y0,y1,u,v) ((cb->y0<<24)|(((unsigned char)cb->u)<<16)|(cb->y1<<8)|(((unsigned char)cb->v)))
#define PACK_YUV(cb,y0,y1,u,v) ((((unsigned char)cb->v)<<24)|(cb->y1<<16)|(((unsigned char)cb->u)<<8)|(cb->y0))

#define TO_YUY2_U0(cb) PACK_YUV(cb,y0,y0,u,v)
#define TO_YUY2_U1(cb) PACK_YUV(cb,y1,y1,u,v)
#define TO_YUY2_L0(cb) PACK_YUV(cb,y2,y2,u,v)
#define TO_YUY2_L1(cb) PACK_YUV(cb,y3,y3,u,v)

#define TO_YUY2_U(cb) PACK_YUV(cb,y0,y1,u,v)
#define TO_YUY2_L(cb) PACK_YUV(cb,y2,y3,u,v)

/* ------------------------------------------------------------------------ */
inline void cvid_v1_yuy2(unsigned char *frm, unsigned char *end, int stride, cvid_codebook *cb)
{
unsigned long *vptr = (unsigned long *)frm, rgb;
int row_inc = stride/4;

	vptr[0] = TO_YUY2_U0(cb);
	vptr[1] = TO_YUY2_U1(cb);
	vptr += row_inc; if(vptr > (unsigned long *)end) return;

	vptr[0] = TO_YUY2_U0(cb);
	vptr[1] = TO_YUY2_U1(cb);
	vptr += row_inc; if(vptr > (unsigned long *)end) return;

	vptr[0] = TO_YUY2_L0(cb);
	vptr[1] = TO_YUY2_L1(cb);
	vptr += row_inc; if(vptr > (unsigned long *)end) return;

	vptr[0] = TO_YUY2_L0(cb);
	vptr[1] = TO_YUY2_L1(cb);
}


/* ------------------------------------------------------------------------ */
inline void cvid_v4_yuy2(unsigned char *frm, unsigned char *end, int stride, cvid_codebook *cb0,
	cvid_codebook *cb1, cvid_codebook *cb2, cvid_codebook *cb3)
{
unsigned long *vptr = (unsigned long *)frm;
int row_inc = stride/4;

	vptr[0] = TO_YUY2_U(cb0);
	vptr[1] = TO_YUY2_U(cb1);
	vptr += row_inc; if(vptr > (unsigned long *)end) return;

	vptr[0] = TO_YUY2_L(cb0);
	vptr[1] = TO_YUY2_L(cb1);
	vptr += row_inc; if(vptr > (unsigned long *)end) return;

	vptr[0] = TO_YUY2_U(cb2);
	vptr[1] = TO_YUY2_U(cb3);
	vptr += row_inc; if(vptr > (unsigned long *)end) return;

	vptr[0] = TO_YUY2_L(cb2);
	vptr[1] = TO_YUY2_L(cb3);

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

		c->rgb0 = (uiclp[c->y0 + uvr] << 16) | (uiclp[c->y0 + uvg] << 8) | uiclp[c->y0 + uvb];
		c->rgb1 = (uiclp[c->y1 + uvr] << 16) | (uiclp[c->y1 + uvg] << 8) | uiclp[c->y1 + uvb];
		c->rgb2 = (uiclp[c->y2 + uvr] << 16) | (uiclp[c->y2 + uvg] << 8) | uiclp[c->y2 + uvb];
		c->rgb3 = (uiclp[c->y3 + uvr] << 16) | (uiclp[c->y3 + uvg] << 8) | uiclp[c->y3 + uvb];
		}
}


/* ------------------------------------------------------------------------ */
inline void cvid_v1_32(unsigned char *frm, unsigned char *end, int stride, cvid_codebook *cb)
{
unsigned long *vptr = (unsigned long *)frm, rgb;
int row_inc = stride/4;

	vptr[0] = rgb = cb->rgb0; vptr[1] = rgb;
	vptr[2] = rgb = cb->rgb1; vptr[3] = rgb;
	vptr += row_inc; if(vptr > (unsigned long *)end) return;
	vptr[0] = rgb = cb->rgb0; vptr[1] = rgb;
	vptr[2] = rgb = cb->rgb1; vptr[3] = rgb;
	vptr += row_inc; if(vptr > (unsigned long *)end) return;
	vptr[0] = rgb = cb->rgb2; vptr[1] = rgb;
	vptr[2] = rgb = cb->rgb3; vptr[3] = rgb;
	vptr += row_inc; if(vptr > (unsigned long *)end) return;
	vptr[0] = rgb = cb->rgb2; vptr[1] = rgb;
	vptr[2] = rgb = cb->rgb3; vptr[3] = rgb;
}


/* ------------------------------------------------------------------------ */
inline void cvid_v4_32(unsigned char *frm, unsigned char *end, int stride, cvid_codebook *cb0,
	cvid_codebook *cb1, cvid_codebook *cb2, cvid_codebook *cb3)
{
unsigned long *vptr = (unsigned long *)frm;
int row_inc = stride/4;

	vptr[0] = cb0->rgb0;
	vptr[1] = cb0->rgb1;
	vptr[2] = cb1->rgb0;
	vptr[3] = cb1->rgb1;
	vptr += row_inc; if(vptr > (unsigned long *)end) return;
	vptr[0] = cb0->rgb2;
	vptr[1] = cb0->rgb3;
	vptr[2] = cb1->rgb2;
	vptr[3] = cb1->rgb3;
	vptr += row_inc; if(vptr > (unsigned long *)end) return;
	vptr[0] = cb2->rgb0;
	vptr[1] = cb2->rgb1;
	vptr[2] = cb3->rgb0;
	vptr[3] = cb3->rgb1;
	vptr += row_inc; if(vptr > (unsigned long *)end) return;
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
void cvid_v1_24(unsigned char *vptr, unsigned char *end, int stride, cvid_codebook *cb)
{
unsigned char r, g, b;
int row_inc = stride-4*3;

	*vptr++ = b = cb->b[0]; *vptr++ = g = cb->g[0]; *vptr++ = r = cb->r[0];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	*vptr++ = b = cb->b[1]; *vptr++ = g = cb->g[1]; *vptr++ = r = cb->r[1];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	vptr += row_inc; if(vptr > end) return;
	*vptr++ = b = cb->b[0]; *vptr++ = g = cb->g[0]; *vptr++ = r = cb->r[0];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	*vptr++ = b = cb->b[1]; *vptr++ = g = cb->g[1]; *vptr++ = r = cb->r[1];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	vptr += row_inc; if(vptr > end) return;
	*vptr++ = b = cb->b[2]; *vptr++ = g = cb->g[2]; *vptr++ = r = cb->r[2];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	*vptr++ = b = cb->b[3]; *vptr++ = g = cb->g[3]; *vptr++ = r = cb->r[3];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	vptr += row_inc; if(vptr > end) return;
	*vptr++ = b = cb->b[2]; *vptr++ = g = cb->g[2]; *vptr++ = r = cb->r[2];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
	*vptr++ = b = cb->b[3]; *vptr++ = g = cb->g[3]; *vptr++ = r = cb->r[3];
	*vptr++ = b; *vptr++ = g; *vptr++ = r;
}


/* ------------------------------------------------------------------------ */
void cvid_v4_24(unsigned char *vptr, unsigned char *end, int stride, cvid_codebook *cb0,
	cvid_codebook *cb1, cvid_codebook *cb2, cvid_codebook *cb3)
{
int row_inc = stride-4*3;

	*vptr++ = cb0->b[0]; *vptr++ = cb0->g[0]; *vptr++ = cb0->r[0];
	*vptr++ = cb0->b[1]; *vptr++ = cb0->g[1]; *vptr++ = cb0->r[1];
	*vptr++ = cb1->b[0]; *vptr++ = cb1->g[0]; *vptr++ = cb1->r[0];
	*vptr++ = cb1->b[1]; *vptr++ = cb1->g[1]; *vptr++ = cb1->r[1];
	vptr += row_inc; if(vptr > end) return;
	*vptr++ = cb0->b[2]; *vptr++ = cb0->g[2]; *vptr++ = cb0->r[2];
	*vptr++ = cb0->b[3]; *vptr++ = cb0->g[3]; *vptr++ = cb0->r[3];
	*vptr++ = cb1->b[2]; *vptr++ = cb1->g[2]; *vptr++ = cb1->r[2];
	*vptr++ = cb1->b[3]; *vptr++ = cb1->g[3]; *vptr++ = cb1->r[3];
	vptr += row_inc; if(vptr > end) return;
	*vptr++ = cb2->b[0]; *vptr++ = cb2->g[0]; *vptr++ = cb2->r[0];
	*vptr++ = cb2->b[1]; *vptr++ = cb2->g[1]; *vptr++ = cb2->r[1];
	*vptr++ = cb3->b[0]; *vptr++ = cb3->g[0]; *vptr++ = cb3->r[0];
	*vptr++ = cb3->b[1]; *vptr++ = cb3->g[1]; *vptr++ = cb3->r[1];
	vptr += row_inc; if(vptr > end) return;
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
 * frame - the output frame buffer (24 or 32 bit per pixel)
 * width - the width of the output frame
 * height - the height of the output frame
 * bit_per_pixel - the number of bits per pixel allocated to the output
 *   frame (only 24 or 32 bpp are supported)
 */
void decode_cinepak(void *context, unsigned char *buf, int size, unsigned char *frame, int width, int height, int bit_per_pixel, int stride_)
{
cinepak_info *cvinfo = (cinepak_info *)context;
cvid_codebook *v4_codebook, *v1_codebook, *codebook = NULL;
unsigned long x, y, y_bottom, frame_flags, strips, cv_width, cv_height, cnum,
	strip_id, chunk_id, x0, y0, x1, y1, ci, flag, mask;
long len, top_size, chunk_size;
unsigned char *frm_ptr, *frm_end;
int i, cur_strip, d0, d1, d2, d3, frm_stride, bpp = 3;
int modulo;
void (*read_codebook)(cvid_codebook *c, int mode) = read_codebook_24;
void (*cvid_v1)(unsigned char *frm, unsigned char *end, int stride, cvid_codebook *cb) = cvid_v1_24;
void (*cvid_v4)(unsigned char *frm, unsigned char *end, int stride, cvid_codebook *cb0,
	cvid_codebook *cb1, cvid_codebook *cb2, cvid_codebook *cb3) = cvid_v4_24;

	x = y = 0;
	y_bottom = 0;
	in_buffer = buf;

	frame_flags = get_byte();
	len = get_byte() << 16;
	len |= get_byte() << 8;
	len |= get_byte();

	switch(bit_per_pixel)
		{
		case 16:
			bpp = 2;
			read_codebook = read_codebook_yuy2;
			cvid_v1 = cvid_v1_yuy2;
			cvid_v4 = cvid_v4_yuy2;
			break;
		case 24:
			bpp = 3;
			read_codebook = read_codebook_24;
			cvid_v1 = cvid_v1_24;
			cvid_v4 = cvid_v4_24;
			break;
		case 32:
			bpp = 4;
			read_codebook = read_codebook_32;
			cvid_v1 = cvid_v1_32;
			cvid_v4 = cvid_v4_32;
			break;
		}

	frm_stride = stride_ ? stride_ : width * bpp;
	frm_ptr = frame;
	frm_end = frm_ptr + width * height * bpp;

	if(len != size)
		{
		if(len & 0x01) len++; /* AVIs tend to have a size mismatch */
		if(len != size)
			{
			printf("CVID: corruption %d (QT/AVI) != %ld (CV)\n", size, len);
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
			printf("CVID: strip overflow (more than %d)\n", MAX_STRIPS);
			return;
			}

		for(i = cvinfo->strip_num; i < strips; i++)
			{
			if((cvinfo->v4_codebook[i] = (cvid_codebook *)calloc(sizeof(cvid_codebook), 260)) == NULL)
				{
				printf("CVID: codebook v4 alloc err\n");
				return;
				}

			if((cvinfo->v1_codebook[i] = (cvid_codebook *)calloc(sizeof(cvid_codebook), 260)) == NULL)
				{
				printf("CVID: codebook v1 alloc err\n");
				return;
				}
			}
		}
	cvinfo->strip_num = strips;

#if DBUG
	printf("CVID: <%ld,%ld> strips %ld\n", cv_width, cv_height, strips);
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
		if(x1 != width) 
			printf("CVID: Warning x1 (%ld) != width (%d)\n", x1, width);
x1 = width;
#if DBUG
	printf("   %d) %04lx %04ld <%ld,%ld> <%ld,%ld> yt %ld  %d\n",
		cur_strip, strip_id, top_size, x0, y0, x1, y1, y_bottom);
#endif

		while(top_size > 0)
			{
			chunk_id  = get_word();
			chunk_size = get_word();

#if DBUG
	printf("        %04lx %04ld\n", chunk_id, chunk_size);
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
						get_byte();
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
								cvid_v4(frm_ptr + (y * frm_stride + x * bpp), frm_end, frm_stride, v4_codebook+d0, v4_codebook+d1, v4_codebook+d2, v4_codebook+d3);
								}
							else		/* 1 byte per block */
								{
								cvid_v1(frm_ptr + (y * frm_stride + x * bpp), frm_end, frm_stride, v1_codebook + get_byte());
								chunk_size--;
								}

							x += 4;
							if(x >= width)
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
									cvid_v4(frm_ptr + (y * frm_stride + x * bpp), frm_end, frm_stride, v4_codebook+d0, v4_codebook+d1, v4_codebook+d2, v4_codebook+d3);
									}
								else		/* V1 */
									{
									chunk_size--;
									cvid_v1(frm_ptr + (y * frm_stride + x * bpp), frm_end, frm_stride, v1_codebook + get_byte());
									}
								}		/* else SKIP */

							mask >>= 1;
							x += 4;
							if(x >= width)
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
						cvid_v1(frm_ptr + (y * frm_stride + x * bpp), frm_end, frm_stride, v1_codebook + get_byte());
						chunk_size--;
						x += 4;
						if(x >= width)
							{
							x = 0;
							y += 4;
							}
						}
					while(chunk_size > 0) { skip_byte(); chunk_size--; }
					break;

				default:
					printf("CVID: unknown chunk_id %08lx\n", chunk_id);
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
			printf("CVID: END INFO chunk size %d cvid size1 %ld cvid size2 %ld\n", size, len, xlen);
			}
		}
}

