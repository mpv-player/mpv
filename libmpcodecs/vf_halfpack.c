#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "config.h"
#include "mp_msg.h"
#include "cpudetect.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "libvo/fastmemcpy.h"
#include "libswscale/rgb2rgb.h"

struct vf_priv_s {
	int field;
};

#ifdef HAVE_MMX
static void halfpack_MMX(unsigned char *dst, unsigned char *src[3],
		     int dststride, int srcstride[3],
		     int w, int h)
{
	int j;
	unsigned char *y1, *y2, *u, *v;
	int dstinc, yinc, uinc, vinc;

	y1 = src[0];
	y2 = src[0] + srcstride[0];
	u = src[1];
	v = src[2];

	dstinc = dststride - 2*w;
	yinc = 2*srcstride[0] - w;
	uinc = srcstride[1] - w/2;
	vinc = srcstride[2] - w/2;

	for (h/=2; h; h--) {
		asm (
			"pxor %%mm0, %%mm0 \n\t"
			ASMALIGN(4)
			"1: \n\t"
			"movq (%0), %%mm1 \n\t"
			"movq (%0), %%mm2 \n\t"
			"movq (%1), %%mm3 \n\t"
			"movq (%1), %%mm4 \n\t"
			"punpcklbw %%mm0, %%mm1 \n\t"
			"punpckhbw %%mm0, %%mm2 \n\t"
			"punpcklbw %%mm0, %%mm3 \n\t"
			"punpckhbw %%mm0, %%mm4 \n\t"
			"paddw %%mm3, %%mm1 \n\t"
			"paddw %%mm4, %%mm2 \n\t"
			"psrlw $1, %%mm1 \n\t"
			"psrlw $1, %%mm2 \n\t"
			
			"movq (%2), %%mm3 \n\t"
			"movq (%3), %%mm5 \n\t"
			"punpcklbw %%mm0, %%mm3 \n\t"
			"punpcklbw %%mm0, %%mm5 \n\t"
			"movq %%mm3, %%mm4 \n\t"
			"movq %%mm5, %%mm6 \n\t"
			"punpcklwd %%mm0, %%mm3 \n\t"
			"punpckhwd %%mm0, %%mm4 \n\t"
			"punpcklwd %%mm0, %%mm5 \n\t"
			"punpckhwd %%mm0, %%mm6 \n\t"
			"pslld $8, %%mm3 \n\t"
			"pslld $8, %%mm4 \n\t"
			"pslld $24, %%mm5 \n\t"
			"pslld $24, %%mm6 \n\t"

			"por %%mm3, %%mm1 \n\t"
			"por %%mm4, %%mm2 \n\t"
			"por %%mm5, %%mm1 \n\t"
			"por %%mm6, %%mm2 \n\t"

			"add $8, %0 \n\t"
			"add $8, %1 \n\t"
			"add $4, %2 \n\t"
			"add $4, %3 \n\t"
			"movq %%mm1, (%8) \n\t"
			"movq %%mm2, 8(%8) \n\t"
			"add $16, %8 \n\t"
			"decl %9 \n\t"
			"jnz 1b \n\t"
			: "=r" (y1), "=r" (y2), "=r" (u), "=r" (v)
			: "0" (y1), "1" (y2), "2" (u), "3" (v), "r" (dst), "r" (w/8)
			: "memory"
		);
		for (j = (w&7)/2; j; j--) {
			*dst++ = (*y1++ + *y2++)/2;
			*dst++ = *u++;
			*dst++ = (*y1++ + *y2++)/2;
			*dst++ = *v++;
		}
		y1 += yinc;
		y2 += yinc;
		u += uinc;
		v += vinc;
		dst += dstinc;
	}
	asm volatile ( "emms \n\t" ::: "memory" );
}
#endif



static void halfpack_C(unsigned char *dst, unsigned char *src[3],
		     int dststride, int srcstride[3],
		     int w, int h)
{
	int i, j;
	unsigned char *y1, *y2, *u, *v;
	int dstinc, yinc, uinc, vinc;

	y1 = src[0];
	y2 = src[0] + srcstride[0];
	u = src[1];
	v = src[2];

	dstinc = dststride - 2*w;
	yinc = 2*srcstride[0] - w;
	uinc = srcstride[1] - w/2;
	vinc = srcstride[2] - w/2;

	for (i = h/2; i; i--) {
		for (j = w/2; j; j--) {
			*dst++ = (*y1++ + *y2++)>>1;
			*dst++ = *u++;
			*dst++ = (*y1++ + *y2++)>>1;
			*dst++ = *v++;
		}
		y1 += yinc;
		y2 += yinc;
		u += uinc;
		v += vinc;
		dst += dstinc;
	}
}

static void (*halfpack)(unsigned char *dst, unsigned char *src[3],
	int dststride, int srcstride[3], int w, int h);


static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts)
{
	mp_image_t *dmpi;

	// hope we'll get DR buffer:
	dmpi=vf_get_image(vf->next, IMGFMT_YUY2,
			  MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
			  mpi->w, mpi->h/2);

	switch(vf->priv->field) {
	case 0:
	case 1:
		yuv422ptoyuy2(mpi->planes[0] + mpi->stride[0]*vf->priv->field,
			mpi->planes[1], mpi->planes[2], dmpi->planes[0],
			mpi->w, mpi->h/2, mpi->stride[0]*2, mpi->stride[1], dmpi->stride[0]);
		break;
	default:
		halfpack(dmpi->planes[0], mpi->planes, dmpi->stride[0],
			mpi->stride, mpi->w, mpi->h);
	}

	return vf_next_put_image(vf,dmpi, pts);
}

static int config(struct vf_instance_s* vf,
		  int width, int height, int d_width, int d_height,
		  unsigned int flags, unsigned int outfmt)
{
	/* FIXME - also support UYVY output? */
	return vf_next_config(vf, width, height/2, d_width, d_height, flags, IMGFMT_YUY2);
}


static int query_format(struct vf_instance_s* vf, unsigned int fmt)
{
	/* FIXME - really any YUV 4:2:0 input format should work */
	switch (fmt) {
	case IMGFMT_YV12:
	case IMGFMT_IYUV:
	case IMGFMT_I420:
		return vf_next_query_format(vf,IMGFMT_YUY2);
	}
	return 0;
}

static void uninit(struct vf_instance_s* vf)
{
	free(vf->priv);
}

static int open(vf_instance_t *vf, char* args)
{
	vf->config=config;
	vf->query_format=query_format;
	vf->put_image=put_image;
	vf->uninit=uninit;
	
	vf->priv = calloc(1, sizeof (struct vf_priv_s));
	vf->priv->field = 2;
	if (args) sscanf(args, "%d", &vf->priv->field);
	
	halfpack = halfpack_C;
#ifdef HAVE_MMX
	if(gCpuCaps.hasMMX) halfpack = halfpack_MMX;
#endif
	return 1;
}

vf_info_t vf_info_halfpack = {
	"yuv planar 4:2:0 -> packed 4:2:2, half height",
	"halfpack",
	"Richard Felker",
	"",
	open,
	NULL
};

