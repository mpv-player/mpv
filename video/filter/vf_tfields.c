/*
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "options/options.h"
#include "common/msg.h"
#include "common/cpudetect.h"

#include "video/img_format.h"
#include "video/memcpy_pic.h"
#include "video/mp_image.h"
#include "vf.h"

static struct vf_priv_s {
	int mode;
	int parity;
	int buffered_i;
	double buffered_pts;
	double buffered_pts_delta;
	double buffered_tff;
} const vf_priv_default = {
	.mode = 4,
	.parity = -1,
};

static void deint(unsigned char *dest, int ds, unsigned char *src, int ss, int w, int h, int field)
{
	int x, y;
	src += ss;
	dest += ds;
	h--;
	if (field) {
		memcpy(dest - ds, src - ss, w);
		src += ss;
		dest += ds;
		h--;
	}
	for (y=h/2; y > 0; y--) {
		dest[0] = src[0];
		for (x=1; x<w-1; x++) {
			if (((src[x-ss] < src[x]) && (src[x+ss] < src[x])) ||
				((src[x-ss] > src[x]) && (src[x+ss] > src[x]))) {
				//dest[x] = (src[x+ss] + src[x-ss])>>1;
				dest[x] = ((src[x+ss]<<1) + (src[x-ss]<<1)
					+ src[x+ss+1] + src[x-ss+1]
					+ src[x+ss-1] + src[x-ss-1])>>3;
			}
			else dest[x] = src[x];
		}
		dest[w-1] = src[w-1];
		dest += ds<<1;
		src += ss<<1;
	}
	if (h & 1)
		memcpy(dest, src, w);
}

#if HAVE_MMX2
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
		__asm__ volatile(
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
	__asm__ volatile("emms \n\t" : : : "memory");
}
#endif

#if HAVE_MMX
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
		__asm__ volatile(
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
	__asm__ volatile("emms \n\t" : : : "memory");
}

#if HAVE_EBX_AVAILABLE
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
		__asm__ volatile(
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
	__asm__ volatile("emms \n\t" : : : "memory");
}
#endif /* HAVE_EBX_AVAILABLE */
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

static int continue_buffered_image(struct vf_instance *vf, struct mp_image *mpi);

static int filter_image(struct vf_instance *vf, struct mp_image *mpi)
{
	double delta;
	int tff;

	if (vf->priv->parity < 0) {
		if (mpi->fields & MP_IMGFIELD_ORDERED)
			tff = !!(mpi->fields & MP_IMGFIELD_TOP_FIRST);
		else
			tff = 1;
	}
	else tff = (vf->priv->parity&1)^1;

	if (vf->priv->buffered_pts == MP_NOPTS_VALUE)
		delta = 1001.0/60000.0; /* delta = field time distance */
	else
		delta = (mpi->pts - vf->priv->buffered_pts) / 2;

	if (delta <= 0.0 || delta >= 0.5)
		delta = 0.0;

	vf->priv->buffered_i = 0;
	vf->priv->buffered_pts = mpi->pts;
	vf->priv->buffered_pts_delta = delta;
	vf->priv->buffered_tff = tff;

	while (continue_buffered_image(vf, mpi)) {
	}

	talloc_free(mpi);

	return 0;
}

static int continue_buffered_image(struct vf_instance *vf, struct mp_image *mpi)
{
	int tff = vf->priv->buffered_tff;
	double pts = vf->priv->buffered_pts;
	int i = vf->priv->buffered_i;
	int ret = 0;
	struct mp_image *dmpi;
	void (*qpel)(unsigned char *, unsigned char *, int, int, int, int, int) = NULL;
	int bpp = 1;

	if (i == 0)
		ret = 1; /* more images to come */

	pts += i * vf->priv->buffered_pts_delta;

	switch (vf->priv->mode) {
	case 0:
		for (; i<2; i++) {
			dmpi = vf_alloc_out_image(vf);
			dmpi->planes[0] = mpi->planes[0] + (i^!tff)*mpi->stride[0];
			dmpi->stride[0] = 2*mpi->stride[0];
			if (mpi->flags & MP_IMGFLAG_PLANAR) {
				dmpi->planes[1] = mpi->planes[1] + (i^!tff)*mpi->stride[1];
				dmpi->planes[2] = mpi->planes[2] + (i^!tff)*mpi->stride[2];
				dmpi->stride[1] = 2*mpi->stride[1];
				dmpi->stride[2] = 2*mpi->stride[2];
			}
			dmpi->pts = pts;
			vf_add_output_frame(vf, dmpi);
			break;
		}
		break;
	case 1:
		for (; i<2; i++) {
			dmpi = vf_alloc_out_image(vf);
			memcpy_pic(dmpi->planes[0] + (i^!tff)*dmpi->stride[0],
				mpi->planes[0] + (i^!tff)*mpi->stride[0],
				mpi->w*bpp, mpi->h/2, dmpi->stride[0]*2, mpi->stride[0]*2);
			deint(dmpi->planes[0], dmpi->stride[0], mpi->planes[0], mpi->stride[0], mpi->w, mpi->h, (i^!tff));
			if (mpi->flags & MP_IMGFLAG_PLANAR) {
				memcpy_pic(dmpi->planes[1] + (i^!tff)*dmpi->stride[1],
					mpi->planes[1] + (i^!tff)*mpi->stride[1],
					mpi->chroma_width, mpi->chroma_height/2,
					dmpi->stride[1]*2, mpi->stride[1]*2);
				memcpy_pic(dmpi->planes[2] + (i^!tff)*dmpi->stride[2],
					mpi->planes[2] + (i^!tff)*mpi->stride[2],
					mpi->chroma_width, mpi->chroma_height/2,
					dmpi->stride[2]*2, mpi->stride[2]*2);
				deint(dmpi->planes[1], dmpi->stride[1], mpi->planes[1], mpi->stride[1],
					mpi->chroma_width, mpi->chroma_height, (i^!tff));
				deint(dmpi->planes[2], dmpi->stride[2], mpi->planes[2], mpi->stride[2],
					mpi->chroma_width, mpi->chroma_height, (i^!tff));
			}
			dmpi->pts = pts;
			vf_add_output_frame(vf, dmpi);
			break;
		}
		break;
	case 2:
		qpel = qpel_li;
	case 3:
		// TODO: add 3tap filter
		if (!qpel)
			qpel = qpel_4tap;
	case 4:
		if (!qpel)
			qpel = qpel_4tap;

		for (; i<2; i++) {
			dmpi = vf_alloc_out_image(vf);
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
			dmpi->pts = pts;
			vf_add_output_frame(vf, dmpi);
			break;
		}
		break;
	}
	vf->priv->buffered_i = 1;
	return ret;
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
	/* FIXME - figure out which formats exactly work */
	switch (fmt) {
	default:
		if (vf->priv->mode == 1)
			return 0;
	case IMGFMT_420P:
		return vf_next_query_format(vf, fmt);
	}
	return 0;
}

static int config(struct vf_instance *vf,
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

static int vf_open(vf_instance_t *vf)
{
	vf->config = config;
	vf->filter_ext = filter_image;
	vf->query_format = query_format;
	qpel_li = qpel_li_C;
	qpel_4tap = qpel_4tap_C;
#if HAVE_MMX
	if(gCpuCaps.hasMMX) qpel_li = qpel_li_MMX;
#if HAVE_EBX_AVAILABLE
	if(gCpuCaps.hasMMX) qpel_4tap = qpel_4tap_MMX;
#endif
#endif
#if HAVE_MMX2
	if(gCpuCaps.hasMMX2) qpel_li = qpel_li_MMX2;
#endif
	return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
	OPT_INTRANGE("mode", mode, 0, 0, 4),
	OPT_INTRANGE("parity", parity, 0, -1, 1),
	{0}
};

const vf_info_t vf_info_tfields = {
	.description = "temporal field separation",
	.name = "tfields",
	.open = vf_open,
	.priv_size = sizeof(struct vf_priv_s),
	.priv_defaults = &vf_priv_default,
	.options = vf_opts_fields,
};
