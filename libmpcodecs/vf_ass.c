// -*- c-basic-offset: 8; indent-tabs-mode: t -*-
// vim:ts=8:sw=8:noet:ai:
/*
 * Copyright (C) 2006 Evgeniy Stepanov <eugeni.stepanov@gmail.com>
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "libvo/fastmemcpy.h"

#include "m_option.h"
#include "m_struct.h"

#include "libass/ass.h"
#include "libass/ass_mp.h"

#define _r(c)  ((c)>>24)
#define _g(c)  (((c)>>16)&0xFF)
#define _b(c)  (((c)>>8)&0xFF)
#define _a(c)  ((c)&0xFF)
#define rgba2y(c)  ( (( 263*_r(c)  + 516*_g(c) + 100*_b(c)) >> 10) + 16  )
#define rgba2u(c)  ( ((-152*_r(c) - 298*_g(c) + 450*_b(c)) >> 10) + 128 )
#define rgba2v(c)  ( (( 450*_r(c) - 376*_g(c) -  73*_b(c)) >> 10) + 128 )


static const struct vf_priv_s {
	int outh, outw;

	unsigned int outfmt;

	// 1 = auto-added filter: insert only if chain does not support EOSD already
	// 0 = insert always
	int auto_insert;

	ass_renderer_t* ass_priv;

	unsigned char* planes[3];
	unsigned char* dirty_rows;
} vf_priv_dflt;

extern int opt_screen_size_x;
extern int opt_screen_size_y;

extern ass_track_t* ass_track;
extern float sub_delay;
extern int sub_visibility;

static int config(struct vf_instance_s* vf,
	int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt)
{
	if (outfmt == IMGFMT_IF09) return 0;

	vf->priv->outh = height + ass_top_margin + ass_bottom_margin;
	vf->priv->outw = width;

	if(!opt_screen_size_x && !opt_screen_size_y){
		d_width = d_width * vf->priv->outw / width;
		d_height = d_height * vf->priv->outh / height;
	} 

	vf->priv->planes[1] = malloc(vf->priv->outw * vf->priv->outh);
	vf->priv->planes[2] = malloc(vf->priv->outw * vf->priv->outh);
	vf->priv->dirty_rows = malloc(vf->priv->outh);
	
	if (vf->priv->ass_priv) {
		ass_configure(vf->priv->ass_priv, vf->priv->outw, vf->priv->outh, 0);
		ass_set_aspect_ratio(vf->priv->ass_priv, ((double)d_width) / d_height);
	}

	return vf_next_config(vf, vf->priv->outw, vf->priv->outh, d_width, d_height, flags, outfmt);
}

static void get_image(struct vf_instance_s* vf, mp_image_t *mpi)
{
	if(mpi->type == MP_IMGTYPE_IPB) return;
	if(mpi->flags & MP_IMGFLAG_PRESERVE) return;
	if(mpi->imgfmt != vf->priv->outfmt) return; // colorspace differ
	    
	// width never changes, always try full DR
	mpi->priv = vf->dmpi = vf_get_image(vf->next, mpi->imgfmt,
			mpi->type, mpi->flags | MP_IMGFLAG_READABLE, 
			vf->priv->outw,
			vf->priv->outh);

	if((vf->dmpi->flags & MP_IMGFLAG_DRAW_CALLBACK) &&
			!(vf->dmpi->flags & MP_IMGFLAG_DIRECT)){
		mp_msg(MSGT_ASS, MSGL_INFO, MSGTR_MPCODECS_FullDRNotPossible);
		return;
	}

	// set up mpi as a cropped-down image of dmpi:
	if(mpi->flags&MP_IMGFLAG_PLANAR){
		mpi->planes[0]=vf->dmpi->planes[0] + ass_top_margin * vf->dmpi->stride[0];
		mpi->planes[1]=vf->dmpi->planes[1] + (ass_top_margin >> mpi->chroma_y_shift) * vf->dmpi->stride[1];
		mpi->planes[2]=vf->dmpi->planes[2] + (ass_top_margin >> mpi->chroma_y_shift) * vf->dmpi->stride[2];
		mpi->stride[1]=vf->dmpi->stride[1];
		mpi->stride[2]=vf->dmpi->stride[2];
	} else {
		mpi->planes[0]=vf->dmpi->planes[0] + ass_top_margin * vf->dmpi->stride[0];
	}
	mpi->stride[0]=vf->dmpi->stride[0];
	mpi->width=vf->dmpi->width;
	mpi->flags|=MP_IMGFLAG_DIRECT;
	mpi->flags&=~MP_IMGFLAG_DRAW_CALLBACK;
//	vf->dmpi->flags&=~MP_IMGFLAG_DRAW_CALLBACK;
}

static void blank(mp_image_t *mpi, int y1, int y2)
{
	int color[3] = {16, 128, 128}; // black (YUV)
	int y;
	unsigned char* dst;
	int chroma_rows = (y2 - y1) >> mpi->chroma_y_shift;

	dst = mpi->planes[0] + y1 * mpi->stride[0];
	for (y = 0; y < y2 - y1; ++y) {
		memset(dst, color[0], mpi->w);
		dst += mpi->stride[0];
	}
	dst = mpi->planes[1] + (y1 >> mpi->chroma_y_shift) * mpi->stride[1];
	for (y = 0; y < chroma_rows ; ++y) {
		memset(dst, color[1], mpi->chroma_width);
		dst += mpi->stride[1];
	}
	dst = mpi->planes[2] + (y1 >> mpi->chroma_y_shift) * mpi->stride[2];
	for (y = 0; y < chroma_rows ; ++y) {
		memset(dst, color[2], mpi->chroma_width);
		dst += mpi->stride[2];
	}
}

static int prepare_image(struct vf_instance_s* vf, mp_image_t *mpi)
{
	if(mpi->flags&MP_IMGFLAG_DIRECT || mpi->flags&MP_IMGFLAG_DRAW_CALLBACK){
		vf->dmpi = mpi->priv;
		if (!vf->dmpi) { mp_msg(MSGT_ASS, MSGL_WARN, MSGTR_MPCODECS_FunWhydowegetNULL); return 0; }
		mpi->priv = NULL;
		// we've used DR, so we're ready...
		if (ass_top_margin)
			blank(vf->dmpi, 0, ass_top_margin);
		if (ass_bottom_margin)
			blank(vf->dmpi, vf->priv->outh - ass_bottom_margin, vf->priv->outh);
		if(!(mpi->flags&MP_IMGFLAG_PLANAR))
			vf->dmpi->planes[1] = mpi->planes[1]; // passthrough rgb8 palette
		return 0;
	}

	// hope we'll get DR buffer:
	vf->dmpi = vf_get_image(vf->next, vf->priv->outfmt,
			MP_IMGTYPE_TEMP, MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_READABLE,
			vf->priv->outw, vf->priv->outh);

	// copy mpi->dmpi...
	if(mpi->flags&MP_IMGFLAG_PLANAR){
		memcpy_pic(vf->dmpi->planes[0] + ass_top_margin * vf->dmpi->stride[0],
				mpi->planes[0], mpi->w, mpi->h,
				vf->dmpi->stride[0], mpi->stride[0]);
		memcpy_pic(vf->dmpi->planes[1] + (ass_top_margin >> mpi->chroma_y_shift) * vf->dmpi->stride[1],
				mpi->planes[1], mpi->w >> mpi->chroma_x_shift, mpi->h >> mpi->chroma_y_shift,
				vf->dmpi->stride[1], mpi->stride[1]);
		memcpy_pic(vf->dmpi->planes[2] + (ass_top_margin >> mpi->chroma_y_shift) * vf->dmpi->stride[2],
				mpi->planes[2], mpi->w >> mpi->chroma_x_shift, mpi->h >> mpi->chroma_y_shift,
				vf->dmpi->stride[2], mpi->stride[2]);
	} else {
		memcpy_pic(vf->dmpi->planes[0] + ass_top_margin * vf->dmpi->stride[0],
				mpi->planes[0], mpi->w*(vf->dmpi->bpp/8), mpi->h,
				vf->dmpi->stride[0], mpi->stride[0]);
		vf->dmpi->planes[1] = mpi->planes[1]; // passthrough rgb8 palette
	}
	if (ass_top_margin)
		blank(vf->dmpi, 0, ass_top_margin);
	if (ass_bottom_margin)
		blank(vf->dmpi, vf->priv->outh - ass_bottom_margin, vf->priv->outh);
	return 0;
}

/**
 * \brief Copy specified rows from render_context.dmpi to render_context.planes, upsampling to 4:4:4
 */
static void copy_from_image(struct vf_instance_s* vf, int first_row, int last_row)
{
	int pl;
	int i, j, k;
	unsigned char val;
	int chroma_rows;

	first_row -= (first_row % 2);
	last_row += (last_row % 2);
	chroma_rows = (last_row - first_row) / 2;

	for (pl = 1; pl < 3; ++pl) {
		int dst_stride = vf->priv->outw;
		int src_stride = vf->dmpi->stride[pl];
		
		unsigned char* src = vf->dmpi->planes[pl] + (first_row/2) * src_stride;
		unsigned char* dst = vf->priv->planes[pl] + first_row * dst_stride;
		unsigned char* dst_next = dst + dst_stride;
		for(i = 0; i < chroma_rows; ++i)
		{
			if ((vf->priv->dirty_rows[first_row + i*2] == 0) ||
				(vf->priv->dirty_rows[first_row + i*2 + 1] == 0)) {
				for (j = 0, k = 0; j < vf->dmpi->chroma_width; ++j, k+=2) {
					val = *(src + j);
					*(dst + k) = val;
					*(dst + k + 1) = val;
					*(dst_next + k) = val;
					*(dst_next + k + 1) = val;
				}
			}
			src += src_stride;
			dst = dst_next + dst_stride;
			dst_next = dst + dst_stride;
		}
	}
	for (i = first_row; i < last_row; ++i)
		vf->priv->dirty_rows[i] = 1;
}

/**
 * \brief Copy all previously copied rows back to render_context.dmpi
 */
static void copy_to_image(struct vf_instance_s* vf)
{
	int pl;
	int i, j, k;
	for (pl = 1; pl < 3; ++pl) {
		int dst_stride = vf->dmpi->stride[pl];
		int src_stride = vf->priv->outw;
		
		unsigned char* dst = vf->dmpi->planes[pl];
		unsigned char* src = vf->priv->planes[pl];
		unsigned char* src_next = vf->priv->planes[pl] + src_stride;
		for(i = 0; i < vf->dmpi->chroma_height; ++i)
		{
			if ((vf->priv->dirty_rows[i*2] == 1)) {
				assert(vf->priv->dirty_rows[i*2 + 1] == 1);
				for (j = 0, k = 0; j < vf->dmpi->chroma_width; ++j, k+=2) {
					unsigned val = 0;
					val += *(src + k);
					val += *(src + k + 1);
					val += *(src_next + k);
					val += *(src_next + k + 1);
					*(dst + j) = val >> 2;
				}
			}
			dst += dst_stride;
			src = src_next + src_stride;
			src_next = src + src_stride;
		}
	}
}

static void my_draw_bitmap(struct vf_instance_s* vf, unsigned char* bitmap, int bitmap_w, int bitmap_h, int stride, int dst_x, int dst_y, unsigned color)
{
	unsigned char y = rgba2y(color);
	unsigned char u = rgba2u(color);
	unsigned char v = rgba2v(color);
	unsigned char opacity = 255 - _a(color);
	unsigned char *src, *dsty, *dstu, *dstv;
	int i, j;
	mp_image_t* dmpi = vf->dmpi;

	src = bitmap;
	dsty = dmpi->planes[0] + dst_x + dst_y * dmpi->stride[0];
	dstu = vf->priv->planes[1] + dst_x + dst_y * vf->priv->outw;
	dstv = vf->priv->planes[2] + dst_x + dst_y * vf->priv->outw;
	for (i = 0; i < bitmap_h; ++i) {
		for (j = 0; j < bitmap_w; ++j) {
			unsigned k = ((unsigned)src[j]) * opacity / 255;
			dsty[j] = (k*y + (255-k)*dsty[j]) / 255;
			dstu[j] = (k*u + (255-k)*dstu[j]) / 255;
			dstv[j] = (k*v + (255-k)*dstv[j]) / 255;
		}
		src += stride;
		dsty += dmpi->stride[0];
		dstu += vf->priv->outw;
		dstv += vf->priv->outw;
	} 
}

static int render_frame(struct vf_instance_s* vf, mp_image_t *mpi, const ass_image_t* img)
{
	if (img) {
		memset(vf->priv->dirty_rows, 0, vf->priv->outh); // reset dirty rows
		while (img) {
			copy_from_image(vf, img->dst_y, img->dst_y + img->h);
			my_draw_bitmap(vf, img->bitmap, img->w, img->h, img->stride,
					img->dst_x, img->dst_y, img->color);
			img = img->next;
		}
		copy_to_image(vf);
	}
	return 0;
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts)
{
	ass_image_t* images = 0;
	if (sub_visibility && vf->priv->ass_priv && ass_track && (pts != MP_NOPTS_VALUE))
		images = ass_mp_render_frame(vf->priv->ass_priv, ass_track, (pts+sub_delay) * 1000 + .5, NULL);
	
	prepare_image(vf, mpi);
	if (images) render_frame(vf, mpi, images);

	return vf_next_put_image(vf, vf->dmpi, pts);
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt)
{
	switch(fmt){
	case IMGFMT_YV12:
	case IMGFMT_I420:
	case IMGFMT_IYUV:
		return vf_next_query_format(vf, vf->priv->outfmt);
	}
	return 0;
}

static int control(vf_instance_t *vf, int request, void *data)
{
	switch (request) {
	case VFCTRL_INIT_EOSD:
		vf->priv->ass_priv = ass_renderer_init((ass_library_t*)data);
		if (!vf->priv->ass_priv) return CONTROL_FALSE;
		ass_configure_fonts(vf->priv->ass_priv);
		return CONTROL_TRUE;
	case VFCTRL_DRAW_EOSD:
		if (vf->priv->ass_priv) return CONTROL_TRUE;
		break;
	}
	return vf_next_control(vf, request, data);
}

static void uninit(struct vf_instance_s* vf)
{
	if (vf->priv->ass_priv)
		ass_renderer_done(vf->priv->ass_priv);
	if (vf->priv->planes[1])
		free(vf->priv->planes[1]);
	if (vf->priv->planes[2])
		free(vf->priv->planes[2]);
	if (vf->priv->dirty_rows)
		free(vf->priv->dirty_rows);
}

static const unsigned int fmt_list[]={
	IMGFMT_YV12,
	IMGFMT_I420,
	IMGFMT_IYUV,
	0
};

static int open(vf_instance_t *vf, char* args)
{
	int flags;
	vf->priv->outfmt = vf_match_csp(&vf->next,fmt_list,IMGFMT_YV12);
	if (vf->priv->outfmt)
		flags = vf_next_query_format(vf, vf->priv->outfmt);
	if (!vf->priv->outfmt || (vf->priv->auto_insert && flags&VFCAP_EOSD))
	{
		uninit(vf);
		return 0;
	}
	
	if (vf->priv->auto_insert)
		mp_msg(MSGT_ASS, MSGL_INFO, "[ass] auto-open\n");
	
	vf->config = config;
	vf->query_format = query_format;
	vf->uninit = uninit;
	vf->control = control;
	vf->get_image = get_image;
	vf->put_image = put_image;
	vf->default_caps=VFCAP_EOSD;
	return 1;
}

#define ST_OFF(f) M_ST_OFF(struct vf_priv_s,f)
static const m_option_t vf_opts_fields[] = {
	{"auto", ST_OFF(auto_insert), CONF_TYPE_FLAG, 0 , 0, 1, NULL},
	{ NULL, NULL, 0, 0, 0, 0,  NULL }
};

static const m_struct_t vf_opts = {
	"ass",
	sizeof(struct vf_priv_s),
	&vf_priv_dflt,
	vf_opts_fields
};

const vf_info_t vf_info_ass = {
	"Render ASS/SSA subtitles",
	"ass",
	"Evgeniy Stepanov",
	"",
	open,
	&vf_opts
};

