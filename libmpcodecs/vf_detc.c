#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "../mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "../libvo/fastmemcpy.h"

struct metrics {
	int even;
	int odd;
	int noise;
	int temp;
};

struct vf_priv_s {
	int frame;
	int autosync;
	int lastsync;
	int lastdrop;
	int oddfactor, noisefactor;
	int resync;
	struct metrics pm, hi, lo;
	int prevscore;
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

static void block_diffs(struct metrics *m, unsigned char *old, unsigned char *new, int os, int ns)
{
	int i, x, even=0, odd=0, noise=0, temp=0, sum=0;
	for (i = 8; i; i--) {
		for (x = 0; x < 16; x++) {
			even += abs(new[x]-old[x]);
			odd += abs(new[x+ns]-old[x+os]);
			sum += new[x];
			noise += new[x+ns];
			temp += old[x+os];
		}
		old += 2*os; new += 2*ns;
	}
	m->even = even;
	m->odd = odd;
	m->noise = abs(noise-sum);
	m->temp = abs(temp-sum);
}


static void diff_fields(struct metrics *m, unsigned char *old, unsigned char *new, int w, int h, int os, int ns)
{
	int x, y, me=0, mo=0, mn=0, mt=0;
	struct metrics l;
	for (y = 0; y < h-15; y += 16) {
		for (x = 0; x < w-15; x += 16) {
			block_diffs(&l, old+x+y*os, new+x+y*ns, os, ns);
			if (l.even > me) me = l.even;
			if (l.odd > mo) mo = l.odd;
			if (l.noise > mn) mn = l.noise;
			if (l.temp > mt) mt = l.temp;
		}
	}
	m->even = me;
	m->odd = mo;
	m->noise = mn;
	m->temp = mt;
}

static status(int f, struct metrics *m, int s)
{
	mp_msg(MSGT_VFILTER, MSGL_V, "frame %d: e=%d o=%d n=%d t=%d s=%d\n",
		f, m->even, m->odd, m->noise, m->temp, s);
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi)
{
	struct vf_priv_s *p = vf->priv;
	mp_image_t *dmpi;
	int i;
	struct metrics m;
	int isdup, notdup;
	int islaced, notlaced;
	int tcstart, tcend;
	int tcscore;

	if (p->frame >= 0)
		p->frame = (p->frame+1)%5;
	
	dmpi = vf_get_image(vf->next, mpi->imgfmt,
		MP_IMGTYPE_STATIC, MP_IMGFLAG_ACCEPT_STRIDE |
		MP_IMGFLAG_PRESERVE | MP_IMGFLAG_READABLE,
		mpi->width, mpi->height);

	diff_fields(&m, dmpi->planes[0], mpi->planes[0],
		mpi->w, mpi->h, dmpi->stride[0], mpi->stride[0]);

	isdup = m.even < p->lo.even;
	notdup = m.even > p->hi.even;

	tcscore = (m.odd > p->lo.odd) + (m.odd > p->hi.odd) + (m.odd > 4*m.even)
		+ (m.noise > p->lo.noise) + (m.noise > p->hi.noise)
		+ (m.noise > m.temp)
		+ (m.even * p->pm.odd > m.odd * p->pm.even);

	status(p->frame, &m, tcscore);
	
	vf->priv->pm = m;
	vf->priv->prevscore = tcscore;
	
	switch (vf->priv->frame) {
	case 0:
	case 1:
	case 2:
		if (isdup && (tcscore > 3)) {
			//status(p->frame, &m, tcscore);
			mp_msg(MSGT_VFILTER, MSGL_V, "heavy lacing, trying to resync with telecine!\n");
			vf->priv->frame = 3;
			return 0;
		} else if (tcscore > 5) {
			//status(p->frame, &m, tcscore);
			mp_msg(MSGT_VFILTER, MSGL_V, "laced scene change, trying to resync with telecine!\n");
			vf->priv->frame = 3;
			return 0;
		}
		break;
	case 3:
		if (notdup && (m.noise < p->hi.noise)) {
			//status(p->frame, &m, tcscore);
			mp_msg(MSGT_VFILTER, MSGL_V, "non-duplicate field; lost telecine tracking!\n");
			vf->priv->frame = -1;
		}
		break;
	case 4:
		if (m.temp > p->hi.temp) { /* bad match */
			//status(p->frame, &m, tcscore);
			if (m.noise < p->hi.noise) {
				mp_msg(MSGT_VFILTER, MSGL_V, "mismatched non-interlaced frame; lost telecine tracking!\n");
				vf->priv->frame = -1;
			} else {
				mp_msg(MSGT_VFILTER, MSGL_V, "mismatched interlaced frame; trying to resync!\n");
				vf->priv->frame = 3;
			}
		}
		break;
	default:
		if (!notdup && (tcscore > 2)) {
			//status(p->frame, &m, tcscore);
			mp_msg(MSGT_VFILTER, MSGL_V, "caught the telecine start!\n");
			vf->priv->frame = 3;
		}
		break;
	}

	if (vf->priv->frame < 3) {
		memcpy_pic(dmpi->planes[0], mpi->planes[0], mpi->w, mpi->h,
			dmpi->stride[0], mpi->stride[0]);
		if (mpi->flags & MP_IMGFLAG_PLANAR) {
			memcpy_pic(dmpi->planes[1], mpi->planes[1],
				mpi->chroma_width, mpi->chroma_height,
				dmpi->stride[1], mpi->stride[1]);
			memcpy_pic(dmpi->planes[2], mpi->planes[2],
				mpi->chroma_width, mpi->chroma_height,
				dmpi->stride[2], mpi->stride[2]);
		}
	} else if (vf->priv->frame == 3) {
		my_memcpy_pic(dmpi->planes[0]+dmpi->stride[0],
			mpi->planes[0]+mpi->stride[0], mpi->w, mpi->h/2,
			dmpi->stride[0]*2, mpi->stride[0]*2);
		if (mpi->flags & MP_IMGFLAG_PLANAR) {
			my_memcpy_pic(dmpi->planes[1]+dmpi->stride[1],
				mpi->planes[1]+mpi->stride[1],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[1]*2, mpi->stride[1]*2);
			my_memcpy_pic(dmpi->planes[2]+dmpi->stride[2],
				mpi->planes[2]+mpi->stride[2],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[2]*2, mpi->stride[2]*2);
		}
		p->lastdrop = 0;
		return 0;
	} else {
		my_memcpy_pic(dmpi->planes[0], mpi->planes[0], mpi->w, mpi->h/2,
			dmpi->stride[0]*2, mpi->stride[0]*2);
		if (mpi->flags & MP_IMGFLAG_PLANAR) {
			my_memcpy_pic(dmpi->planes[1], mpi->planes[1],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[1]*2, mpi->stride[1]*2);
			my_memcpy_pic(dmpi->planes[2], mpi->planes[2],
				mpi->chroma_width, mpi->chroma_height/2,
				dmpi->stride[2]*2, mpi->stride[2]*2);
		}
	}
	if (++p->lastdrop >= 5) {
		mp_msg(MSGT_VFILTER, MSGL_V, "dropping frame!\n");
		p->lastdrop = 0;
		return 0;
	}
	return vf_next_put_image(vf, dmpi);
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
	return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
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
	vf->query_format = query_format;
	vf->uninit = uninit;
	vf->default_reqs = VFCAP_ACCEPT_STRIDE;
	vf->priv = p = calloc(1, sizeof(struct vf_priv_s));
	if (args) sscanf(args, "%d:%d", &vf->priv->frame, &vf->priv->autosync);
	p->frame = -1;
	p->lastsync = 10;
	p->lo.even = 1760;
	p->hi.even = 2880;
	p->lo.odd = 2560;
	p->hi.odd = 10240;
	p->lo.noise = 4480;
	p->hi.noise = 10240;
	p->lo.temp = 6400;
	p->hi.temp = 12800;
	vf->priv->oddfactor = 3;
	vf->priv->noisefactor = 6;
	return 1;
}

vf_info_t vf_info_detc = {
    "de-telecine filter",
    "detc",
    "Rich Felker",
    "",
    open
};


