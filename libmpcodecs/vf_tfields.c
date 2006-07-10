#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "cpudetect.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "libvo/fastmemcpy.h"

struct vf_priv_s {
	int mode;
	int parity;
	int buffered_i;
	mp_image_t *buffered_mpi;
	double buffered_pts;
};

static inline void *my_memcpy_pic(void * dst, void * src, int bytesPerLine, int height, int dstStride, int srcStride)
{
	int i;
	void *retval=dst;

	for(i=0; i<height; i++)
	{
		memcpy(dst, src, bytesPerLine);
		src+= srcStride;
		dst+= dstStride;
	}

	return retval;
}

static void deint(unsigned char *dest, int ds, unsigned char *src, int ss, int w, int h, int field)
{
	int x, y;
	src += ss;
	dest += ds;
	if (field) {
		src += ss;
		dest += ds;
		h -= 2;
	}
	for (y=h/2; y; y--) {
		for (x=0; x<w; x++) {
			if (((src[x-ss] < src[x]) && (src[x+ss] < src[x])) ||
				((src[x-ss] > src[x]) && (src[x+ss] > src[x]))) {
				//dest[x] = (src[x+ss] + src[x-ss])>>1;
				dest[x] = ((src[x+ss]<<1) + (src[x-ss]<<1)
					+ src[x+ss+1] + src[x-ss+1]
					+ src[x+ss-1] + src[x-ss-1])>>3;
			}
			else dest[x] = src[x];
		}
		dest += ds<<1;
		src += ss<<1;
	}
}

#ifdef HAVE_3DNOW
static void qpel_li_3DNOW(unsigned char *d, unsigned char *s, int w, int h, int ds, int ss, int up)
{
	int i, j, ssd=ss;
	long crap1, crap2;
	if (up) {
		ssd = -ss;
		memcpy(d, s, w);
		d += ds;
		s += ss;
	}
	for (i=h-1; i; i--) {
		asm volatile(
			"1: \n\t"
			"movq (%%"REG_S"), %%mm0 \n\t"
			"movq (%%"REG_S",%%"REG_a"), %%mm1 \n\t"
			"pavgusb %%mm0, %%mm1 \n\t"
			"add $8, %%"REG_S" \n\t"
			"pavgusb %%mm0, %%mm1 \n\t"
			"movq %%mm1, (%%"REG_D") \n\t"
			"add $8, %%"REG_D" \n\t"
			"decl %%ecx \n\t"
			"jnz 1b \n\t"
			: "=S"(crap1), "=D"(crap2)
			: "c"(w>>3), "S"(s), "D"(d), "a"((long)ssd)
		);
		for (j=w-(w&7); j<w; j++)
			d[j] = (s[j+ssd] + 3*s[j])>>2;
		d += ds;
		s += ss;
	}
	if (!up) memcpy(d, s, w);
	asm volatile("emms \n\t" : : : "memory");
}
#endif

#ifdef HAVE_MMX2
static void qpel_li_MMX2(unsigned char *d, unsigned char *s, int w, int h, int ds, int ss, int up)
{
	int i, j, ssd=ss;
	long crap1, crap2;
	if (up) {
		ssd = -ss;
		memcpy(d, s, w);
		d += ds;
		s += ss;
	}
	for (i=h-1; i; i--) {
		asm volatile(
			"pxor %%mm7, %%mm7 \n\t"
			"2: \n\t"
			"movq (%%"REG_S"), %%mm0 \n\t"
			"movq (%%"REG_S",%%"REG_a"), %%mm1 \n\t"
			"pavgb %%mm0, %%mm1 \n\t"
			"add $8, %%"REG_S" \n\t"
			"pavgb %%mm0, %%mm1 \n\t"
			"movq %%mm1, (%%"REG_D") \n\t"
			"add $8, %%"REG_D" \n\t"
			"decl %%ecx \n\t"
			"jnz 2b \n\t"
			: "=S"(crap1), "=D"(crap2)
			: "c"(w>>3), "S"(s), "D"(d), "a"((long)ssd)
		);
		for (j=w-(w&7); j<w; j++)
			d[j] = (s[j+ssd] + 3*s[j])>>2;
		d += ds;
		s += ss;
	}
	if (!up) memcpy(d, s, w);
	asm volatile("emms \n\t" : : : "memory");
}
#endif

#ifdef HAVE_MMX
static void qpel_li_MMX(unsigned char *d, unsigned char *s, int w, int h, int ds, int ss, int up)
{
	int i, j, ssd=ss;
	int crap1, crap2;
	if (up) {
		ssd = -ss;
		memcpy(d, s, w);
		d += ds;
		s += ss;
	}
	for (i=h-1; i; i--) {
		asm volatile(
			"pxor %%mm7, %%mm7 \n\t"
			"3: \n\t"
			"movq (%%"REG_S"), %%mm0 \n\t"
			"movq (%%"REG_S"), %%mm1 \n\t"
			"movq (%%"REG_S",%%"REG_a"), %%mm2 \n\t"
			"movq (%%"REG_S",%%"REG_a"), %%mm3 \n\t"
			"add $8, %%"REG_S" \n\t"
			"punpcklbw %%mm7, %%mm0 \n\t"
			"punpckhbw %%mm7, %%mm1 \n\t"
			"punpcklbw %%mm7, %%mm2 \n\t"
			"punpckhbw %%mm7, %%mm3 \n\t"
			"paddw %%mm0, %%mm2 \n\t"
			"paddw %%mm1, %%mm3 \n\t"
			"paddw %%mm0, %%mm2 \n\t"
			"paddw %%mm1, %%mm3 \n\t"
			"paddw %%mm0, %%mm2 \n\t"
			"paddw %%mm1, %%mm3 \n\t"
			"psrlw $2, %%mm2 \n\t"
			"psrlw $2, %%mm3 \n\t"
			"packsswb %%mm3, %%mm2 \n\t"
			"movq %%mm2, (%%"REG_D") \n\t"
			"add $8, %%"REG_D" \n\t"
			"decl %%ecx \n\t"
			"jnz 3b \n\t"
			: "=S"(crap1), "=D"(crap2)
			: "c"(w>>3), "S"(s), "D"(d), "a"((long)ssd)
		);
		for (j=w-(w&7); j<w; j++)
			d[j] = (s[j+ssd] + 3*s[j])>>2;
		d += ds;
		s += ss;
	}
	if (!up) memcpy(d, s, w);
	asm volatile("emms \n\t" : : : "memory");
}

static void qpel_4tap_MMX(unsigned char *d, unsigned char *s, int w, int h, int ds, int ss, int up)
{
	int i, j, ssd=ss;
	static const short filter[] = {
		29, 29, 29, 29, 110, 110, 110, 110,
		9, 9, 9, 9, 3, 3, 3, 3,
		64, 64, 64, 64 };
	int crap1, crap2;
	if (up) {
		ssd = -ss;
		memcpy(d, s, w);
		d += ds; s += ss;
	}
	for (j=0; j<w; j++)
		d[j] = (s[j+ssd] + 3*s[j])>>2;
	d += ds; s += ss;
	for (i=h-3; i; i--) {
		asm volatile(
			"pxor %%mm0, %%mm0 \n\t"
			"movq (%%"REG_d"), %%mm4 \n\t"
			"movq 8(%%"REG_d"), %%mm5 \n\t"
			"movq 16(%%"REG_d"), %%mm6 \n\t"
			"movq 24(%%"REG_d"), %%mm7 \n\t"
			"4: \n\t"

			"movq (%%"REG_S",%%"REG_a"), %%mm1 \n\t"
			"movq (%%"REG_S"), %%mm2 \n\t"
			"movq (%%"REG_S",%%"REG_b"), %%mm3 \n\t"
			"punpcklbw %%mm0, %%mm1 \n\t"
			"punpcklbw %%mm0, %%mm2 \n\t"
			"pmullw %%mm4, %%mm1 \n\t"
			"punpcklbw %%mm0, %%mm3 \n\t"
			"pmullw %%mm5, %%mm2 \n\t"
			"paddusw %%mm2, %%mm1 \n\t"
			"pmullw %%mm6, %%mm3 \n\t"
			"movq (%%"REG_S",%%"REG_a",2), %%mm2 \n\t"
			"psubusw %%mm3, %%mm1 \n\t"
			"punpcklbw %%mm0, %%mm2 \n\t"	
			"pmullw %%mm7, %%mm2 \n\t"
			"psubusw %%mm2, %%mm1 \n\t"
			"psrlw $7, %%mm1 \n\t"

			"movq (%%"REG_S",%%"REG_a"), %%mm2 \n\t"
			"movq (%%"REG_S"), %%mm3 \n\t"
			"punpckhbw %%mm0, %%mm2 \n\t"
			"punpckhbw %%mm0, %%mm3 \n\t"
			"pmullw %%mm4, %%mm2 \n\t"
			"pmullw %%mm5, %%mm3 \n\t"
			"paddusw %%mm3, %%mm2 \n\t"
			"movq (%%"REG_S",%%"REG_b"), %%mm3 \n\t"
			"punpckhbw %%mm0, %%mm3 \n\t"
			"pmullw %%mm6, %%mm3 \n\t"
			"psubusw %%mm3, %%mm2 \n\t"
			"movq (%%"REG_S",%%"REG_a",2), %%mm3 \n\t"
			"punpckhbw %%mm0, %%mm3 \n\t"	
			"add $8, %%"REG_S" \n\t"
			"pmullw %%mm7, %%mm3 \n\t"
			"psubusw %%mm3, %%mm2 \n\t"
			"psrlw $7, %%mm2 \n\t"
			
			"packuswb %%mm2, %%mm1 \n\t"
			"movq %%mm1, (%%"REG_D") \n\t"
			"add $8, %%"REG_D" \n\t"
			"decl %%ecx \n\t"
			"jnz 4b \n\t"
			: "=S"(crap1), "=D"(crap2)
			: "c"(w>>3), "S"(s), "D"(d), "a"((long)ssd), "b"((long)-ssd), "d"(filter)
		);
		for (j=w-(w&7); j<w; j++)
			d[j] = (-9*s[j-ssd] + 111*s[j] + 29*s[j+ssd] - 3*s[j+ssd+ssd])>>7;
		d += ds;
		s += ss;
	}
	for (j=0; j<w; j++)
		d[j] = (s[j+ssd] + 3*s[j])>>2;
	d += ds; s += ss;
	if (!up) memcpy(d, s, w);
	asm volatile("emms \n\t" : : : "memory");
}
#endif

static inline int clamp(int a)
{
	// If a<512, this is equivalent to:
	// return (a<0) ? 0 : ( (a>255) ? 255 : a);
	return (~(a>>31)) & (a | ((a<<23)>>31));
}

static void qpel_li_C(unsigned char *d, unsigned char *s, int w, int h, int ds, int ss, int up)
{
	int i, j, ssd=ss;
	if (up) {
		ssd = -ss;
		memcpy(d, s, w);
		d += ds;
		s += ss;
	}
	for (i=h-1; i; i--) {
		for (j=0; j<w; j++)
			d[j] = (s[j+ssd] + 3*s[j])>>2;
		d += ds;
		s += ss;
	}
	if (!up) memcpy(d, s, w);
}

static void qpel_4tap_C(unsigned char *d, unsigned char *s, int w, int h, int ds, int ss, int up)
{
	int i, j, ssd=ss;
	if (up) {
		ssd = -ss;
		memcpy(d, s, w);
		d += ds; s += ss;
	}
	for (j=0; j<w; j++)
		d[j] = (s[j+ssd] + 3*s[j] + 2)>>2;
	d += ds; s += ss;
	for (i=h-3; i; i--) {
		for (j=0; j<w; j++)
			d[j] = clamp((-9*s[j-ssd] + 111*s[j] + 29*s[j+ssd] - 3*s[j+ssd+ssd] + 64)>>7);
		d += ds; s += ss;
	}
	for (j=0; j<w; j++)
		d[j] = (s[j+ssd] + 3*s[j] + 2)>>2;
	d += ds; s += ss;
	if (!up) memcpy(d, s, w);
}

static void (*qpel_li)(unsigned char *d, unsigned char *s, int w, int h, int ds, int ss, int up);
static void (*qpel_4tap)(unsigned char *d, unsigned char *s, int w, int h, int ds, int ss, int up);

static int continue_buffered_image(struct vf_instance_s *);
extern int correct_pts;

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts)
{
	vf->priv->buffered_mpi = mpi;
	vf->priv->buffered_pts = pts;
	vf->priv->buffered_i = 0;
	return continue_buffered_image(vf);
}

static int continue_buffered_image(struct vf_instance_s *vf)
{
	int i=vf->priv->buffered_i;
	double pts = vf->priv->buffered_pts;
	mp_image_t *mpi = vf->priv->buffered_mpi;
	int ret=0;
	mp_image_t *dmpi;
	void (*qpel)(unsigned char *, unsigned char *, int, int, int, int, int);
	int bpp=1;
	int tff;

	if (i == 0)
		vf_queue_frame(vf, continue_buffered_image);
	pts += i * .02;  // XXX not right

	if (!(mpi->flags & MP_IMGFLAG_PLANAR)) bpp = mpi->bpp/8;
	if (vf->priv->parity < 0) {
		if (mpi->fields & MP_IMGFIELD_ORDERED)
			tff = mpi->fields & MP_IMGFIELD_TOP_FIRST;
		else
			tff = 1;
	}
	else tff = (vf->priv->parity&1)^1;

	switch (vf->priv->mode) {
	case 2:
		qpel = qpel_li;
		break;
	case 3:
		// TODO: add 3tap filter
		qpel = qpel_4tap;
		break;
	case 4:
		qpel = qpel_4tap;
		break;
	}

	switch (vf->priv->mode) {
	case 0:
		for (; i<2; i++) {
			dmpi = vf_get_image(vf->next, mpi->imgfmt,
				MP_IMGTYPE_EXPORT, MP_IMGFLAG_ACCEPT_STRIDE,
				mpi->width, mpi->height/2);
			dmpi->planes[0] = mpi->planes[0] + (i^!tff)*mpi->stride[0];
			dmpi->stride[0] = 2*mpi->stride[0];
			if (mpi->flags & MP_IMGFLAG_PLANAR) {
				dmpi->planes[1] = mpi->planes[1] + (i^!tff)*mpi->stride[1];
				dmpi->planes[2] = mpi->planes[2] + (i^!tff)*mpi->stride[2];
				dmpi->stride[1] = 2*mpi->stride[1];
				dmpi->stride[2] = 2*mpi->stride[2];
			}
			ret |= vf_next_put_image(vf, dmpi, pts);
			if (correct_pts)
				break;
			else
				if (!i) vf_next_control(vf, VFCTRL_FLIP_PAGE, NULL);
		}
		break;
	case 1:
		for (; i<2; i++) {
			dmpi = vf_get_image(vf->next, mpi->imgfmt,
				MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
				mpi->width, mpi->height);
			my_memcpy_pic(dmpi->planes[0] + (i^!tff)*dmpi->stride[0],
				mpi->planes[0] + (i^!tff)*mpi->stride[0],
				mpi->w*bpp, mpi->h/2, dmpi->stride[0]*2, mpi->stride[0]*2);
			deint(dmpi->planes[0], dmpi->stride[0], mpi->planes[0], mpi->stride[0], mpi->w, mpi->h, (i^!tff));
			if (mpi->flags & MP_IMGFLAG_PLANAR) {
				my_memcpy_pic(dmpi->planes[1] + (i^!tff)*dmpi->stride[1],
					mpi->planes[1] + (i^!tff)*mpi->stride[1],
					mpi->chroma_width, mpi->chroma_height/2,
					dmpi->stride[1]*2, mpi->stride[1]*2);
				my_memcpy_pic(dmpi->planes[2] + (i^!tff)*dmpi->stride[2],
					mpi->planes[2] + (i^!tff)*mpi->stride[2],
					mpi->chroma_width, mpi->chroma_height/2,
					dmpi->stride[2]*2, mpi->stride[2]*2);
				deint(dmpi->planes[1], dmpi->stride[1], mpi->planes[1], mpi->stride[1],
					mpi->chroma_width, mpi->chroma_height, (i^!tff));
				deint(dmpi->planes[2], dmpi->stride[2], mpi->planes[2], mpi->stride[2],
					mpi->chroma_width, mpi->chroma_height, (i^!tff));
			}
			ret |= vf_next_put_image(vf, dmpi, pts);
			if (correct_pts)
				break;
			else
				if (!i) vf_next_control(vf, VFCTRL_FLIP_PAGE, NULL);
		}
		break;
	case 2:
	case 3:
	case 4:
		for (; i<2; i++) {
			dmpi = vf_get_image(vf->next, mpi->imgfmt,
				MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE,
				mpi->width, mpi->height/2);
			qpel(dmpi->planes[0], mpi->planes[0] + (i^!tff)*mpi->stride[0],
				mpi->w*bpp, mpi->h/2, dmpi->stride[0], mpi->stride[0]*2, (i^!tff));
			if (mpi->flags & MP_IMGFLAG_PLANAR) {
				qpel(dmpi->planes[1],
					mpi->planes[1] + (i^!tff)*mpi->stride[1],
					mpi->chroma_width, mpi->chroma_height/2,
					dmpi->stride[1], mpi->stride[1]*2, (i^!tff));
				qpel(dmpi->planes[2],
					mpi->planes[2] + (i^!tff)*mpi->stride[2],
					mpi->chroma_width, mpi->chroma_height/2,
					dmpi->stride[2], mpi->stride[2]*2, (i^!tff));
			}
			ret |= vf_next_put_image(vf, dmpi, pts);
			if (correct_pts)
				break;
			else
				if (!i) vf_next_control(vf, VFCTRL_FLIP_PAGE, NULL);
		}
		break;
	}
	vf->priv->buffered_i = 1;
	return ret;
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt)
{
	/* FIXME - figure out which other formats work */
	switch (fmt) {
	case IMGFMT_YV12:
	case IMGFMT_IYUV:
	case IMGFMT_I420:
		return vf_next_query_format(vf, fmt);
	}
	return 0;
}

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt)
{
	switch (vf->priv->mode) {
	case 0:
	case 2:
	case 3:
	case 4:
		return vf_next_config(vf,width,height/2,d_width,d_height,flags,outfmt);
	case 1:
		return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
	}
	return 0;
}

static void uninit(struct vf_instance_s* vf)
{
	free(vf->priv);
}

static int open(vf_instance_t *vf, char* args)
{
	struct vf_priv_s *p;
	vf->config = config;
	vf->put_image = put_image;
	//vf->query_format = query_format;
	vf->uninit = uninit;
	vf->default_reqs = VFCAP_ACCEPT_STRIDE;
	vf->priv = p = calloc(1, sizeof(struct vf_priv_s));
	vf->priv->mode = 4;
	vf->priv->parity = -1;
	if (args) sscanf(args, "%d:%d", &vf->priv->mode, &vf->priv->parity);
	qpel_li = qpel_li_C;
	qpel_4tap = qpel_4tap_C;
#ifdef HAVE_MMX
	if(gCpuCaps.hasMMX) qpel_li = qpel_li_MMX;
	if(gCpuCaps.hasMMX) qpel_4tap = qpel_4tap_MMX;
#endif
#ifdef HAVE_MMX2
	if(gCpuCaps.hasMMX2) qpel_li = qpel_li_MMX2;
#endif
#ifdef HAVE_3DNOW
	if(gCpuCaps.has3DNow) qpel_li = qpel_li_3DNOW;
#endif
	return 1;
}

vf_info_t vf_info_tfields = {
    "temporal field separation",
    "tfields",
    "Rich Felker",
    "",
    open,
    NULL
};


