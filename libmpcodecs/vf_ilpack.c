#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "../config.h"
#include "../mp_msg.h"
#include "../cpudetect.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "../libvo/fastmemcpy.h"
#include "../postproc/rgb2rgb.h"

#ifdef HAVE_MMX
static void pack_MMX(unsigned char *dst, unsigned char *y,
	unsigned char *u, unsigned char *v, int w)
{
	int j;
	asm (""
		"pxor %%mm0, %%mm0 \n\t"
		".balign 16 \n\t"
		"1: \n\t"
		"movq (%0), %%mm1 \n\t"
		"movq (%0), %%mm2 \n\t"
		"punpcklbw %%mm0, %%mm1 \n\t"
		"punpckhbw %%mm0, %%mm2 \n\t"
		
		"movq (%1), %%mm3 \n\t"
		"movq (%2), %%mm5 \n\t"
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
		
		"addl $8, %0 \n\t"
		"addl $4, %1 \n\t"
		"addl $4, %2 \n\t"
		"movq %%mm1, (%3) \n\t"
		"movq %%mm2, 8(%3) \n\t"
		"addl $16, %3 \n\t"
		"decl %4 \n\t"
		"jnz 1b \n\t"
		: 
		: "r" (y), "r" (u), "r" (v), "r" (dst), "r" (w/8)
		: "memory"
		);
	for (j = (w&7)/2; j; j--) {
		*dst++ = *y++;
		*dst++ = *u++;
		*dst++ = *y++;
		*dst++ = *v++;
	}
	asm volatile ( "emms \n\t" ::: "memory" );
}
#endif

static void pack_C(unsigned char *dst, unsigned char *y,
	unsigned char *u, unsigned char *v, int w)
{
	int j;
	for (j = w/2; j; j--) {
		*dst++ = *y++;
		*dst++ = *u++;
		*dst++ = *y++;
		*dst++ = *v++;
	}
}

static void (*pack)(unsigned char *dst, unsigned char *y,
	unsigned char *u, unsigned char *v, int w);

static void ilpack(unsigned char *dst, unsigned char *src[3],
	unsigned int dststride, unsigned int srcstride[3], int w, int h)
{
	int i;
	unsigned char *y, *u, *v;

	y = src[0];
	u = src[1];
	v = src[2];

	for (i=0; i<h; i++) {
		pack(dst, y, u, v, w);
		y += srcstride[0];
		if ((i&3) == 1) {
			u -= srcstride[1];
			v -= srcstride[2];
		} else {
			u += srcstride[1];
			v += srcstride[2];
		}
		dst += dststride;
	}
}


static int put_image(struct vf_instance_s* vf, mp_image_t *mpi)
{
	mp_image_t *dmpi;

	// hope we'll get DR buffer:
	dmpi=vf_get_image(vf->next, IMGFMT_YUY2,
			  MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
			  mpi->w, mpi->h);

	ilpack(dmpi->planes[0], mpi->planes, dmpi->stride[0], mpi->stride, mpi->w, mpi->h);

	return vf_next_put_image(vf,dmpi);
}

static int config(struct vf_instance_s* vf,
		  int width, int height, int d_width, int d_height,
		  unsigned int flags, unsigned int outfmt)
{
	/* FIXME - also support UYVY output? */
	return vf_next_config(vf, width, height, d_width, d_height, flags, IMGFMT_YUY2);
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

static int open(vf_instance_t *vf, char* args)
{
	vf->config=config;
	vf->query_format=query_format;
	vf->put_image=put_image;
	
	pack = pack_C;
#ifdef HAVE_MMX
	if(gCpuCaps.hasMMX) pack = pack_MMX;
#endif
	return 1;
}

vf_info_t vf_info_ilpack = {
	"4:2:0 planar -> 4:2:2 packed reinterlacer",
	"ilpack",
	"Richard Felker",
	"",
	open,
	NULL
};

