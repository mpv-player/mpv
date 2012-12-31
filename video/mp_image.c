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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libavutil/mem.h>
#include <libavutil/common.h>

#include "talloc.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "video/filter/vf.h"
#include "video/memcpy_pic.h"

struct m_refcount {
    void *arg;
    // free() is called if refcount reaches 0.
    void (*free)(void *arg);
    // External refcounted object (such as libavcodec DR buffers). This assumes
    // that the actual data is managed by the external object, not by
    // m_refcount. The .ext_* calls use that external object's refcount
    // primitives. It usually doesn't make sense to set both .free and .ext_*.
    void (*ext_ref)(void *arg);
    void (*ext_unref)(void *arg);
    bool (*ext_is_unique)(void *arg);
    // Native refcount (there may be additional references if .ext_* are set)
    int refcount;
};

// Only for checking API usage
static int m_refcount_destructor(void *ptr)
{
    struct m_refcount *ref = ptr;
    assert(ref->refcount == 0);
    return 0;
}

// Starts out with refcount==1, caller can set .arg and .free and .ext_*
static struct m_refcount *m_refcount_new(void)
{
    struct m_refcount *ref = talloc_ptrtype(NULL, ref);
    *ref = (struct m_refcount) { .refcount = 1 };
    talloc_set_destructor(ref, m_refcount_destructor);
    return ref;
}

static void m_refcount_ref(struct m_refcount *ref)
{
    ref->refcount++;
    if (ref->ext_ref)
        ref->ext_ref(ref->arg);
}

static void m_refcount_unref(struct m_refcount *ref)
{
    assert(ref->refcount > 0);
    if (ref->ext_unref)
        ref->ext_unref(ref->arg);
    ref->refcount--;
    if (ref->refcount == 0) {
        if (ref->free)
            ref->free(ref->arg);
        talloc_free(ref);
    }
}

static bool m_refcount_is_unique(struct m_refcount *ref)
{
    if (ref->refcount > 1)
        return false;
    if (ref->ext_is_unique)
        return ref->ext_is_unique(ref->arg); // referenced only by us
    return true;
}


void mp_image_alloc_planes(mp_image_t *mpi) {
  assert(!mpi->refcount);
  // IF09 - allocate space for 4. plane delta info - unused
  if (mpi->imgfmt == IMGFMT_IF09) {
    mpi->planes[0]=av_malloc(mpi->bpp*mpi->w*(mpi->h+2)/8+
                            mpi->chroma_width*mpi->chroma_height);
  } else
    mpi->planes[0]=av_malloc(mpi->bpp*mpi->w*(mpi->h+2)/8);
  if (!mpi->planes[0])
    abort(); //out of memory
  if (mpi->flags&MP_IMGFLAG_PLANAR) {
    // FIXME this code only supports same bpp for all planes, and bpp divisible
    // by 8. Currently the case for all planar formats.
    int bpp = MP_IMAGE_PLANAR_BITS_PER_PIXEL_ON_PLANE(mpi, 0) / 8;
    // YV12/I420/YVU9/IF09. feel free to add other planar formats here...
    mpi->stride[0]=mpi->stride[3]=bpp*mpi->w;
    if(mpi->num_planes > 2){
      mpi->stride[1]=mpi->stride[2]=bpp*mpi->chroma_width;
      if(mpi->flags&MP_IMGFLAG_SWAPPED){
        // I420/IYUV  (Y,U,V)
        mpi->planes[1]=mpi->planes[0]+mpi->stride[0]*mpi->h;
        mpi->planes[2]=mpi->planes[1]+mpi->stride[1]*mpi->chroma_height;
        if (mpi->num_planes > 3)
            mpi->planes[3]=mpi->planes[2]+mpi->stride[2]*mpi->chroma_height;
      } else {
        // YV12,YVU9,IF09  (Y,V,U)
        mpi->planes[2]=mpi->planes[0]+mpi->stride[0]*mpi->h;
        mpi->planes[1]=mpi->planes[2]+mpi->stride[1]*mpi->chroma_height;
        if (mpi->num_planes > 3)
            mpi->planes[3]=mpi->planes[1]+mpi->stride[1]*mpi->chroma_height;
      }
    } else {
      // NV12/NV21
      mpi->stride[1]=mpi->chroma_width;
      mpi->planes[1]=mpi->planes[0]+mpi->stride[0]*mpi->h;
    }
  } else {
    mpi->stride[0]=mpi->w*mpi->bpp/8;
    if (mpi->flags & MP_IMGFLAG_RGB_PALETTE)
      mpi->planes[1] = av_malloc(1024);
  }
  mpi->flags|=MP_IMGFLAG_ALLOCATED;
}

void mp_image_copy(struct mp_image *dmpi, struct mp_image *mpi)
{
  if(mpi->flags&MP_IMGFLAG_PLANAR){
    memcpy_pic(dmpi->planes[0],mpi->planes[0], MP_IMAGE_BYTES_PER_ROW_ON_PLANE(mpi, 0), mpi->h,
	       dmpi->stride[0],mpi->stride[0]);
    memcpy_pic(dmpi->planes[1],mpi->planes[1], MP_IMAGE_BYTES_PER_ROW_ON_PLANE(mpi, 1), mpi->chroma_height,
	       dmpi->stride[1],mpi->stride[1]);
    memcpy_pic(dmpi->planes[2], mpi->planes[2], MP_IMAGE_BYTES_PER_ROW_ON_PLANE(mpi, 2), mpi->chroma_height,
	       dmpi->stride[2],mpi->stride[2]);
  } else {
    memcpy_pic(dmpi->planes[0],mpi->planes[0],
	       MP_IMAGE_BYTES_PER_ROW_ON_PLANE(mpi, 0), mpi->h,
	       dmpi->stride[0],mpi->stride[0]);
  }
}

void mp_image_copy_attributes(struct mp_image *dmpi, struct mp_image *mpi)
{
    vf_clone_mpi_attributes(dmpi, mpi);
}

void mp_image_setfmt(struct mp_image *mpi, unsigned int out_fmt)
{
    mpi->flags &= ~MP_IMGFLAG_FMT_MASK;
    struct mp_imgfmt_desc fmt = mp_imgfmt_get_desc(out_fmt);
    mpi->fmt = fmt;
    mpi->flags |= fmt.flags;
    mpi->imgfmt = fmt.id;
    mpi->bpp = fmt.avg_bpp;
    mpi->chroma_x_shift = fmt.chroma_xs;
    mpi->chroma_y_shift = fmt.chroma_ys;
    mpi->num_planes = fmt.num_planes;
    mp_image_set_size(mpi, mpi->w, mpi->h);
}

static int mp_image_destructor(void *ptr)
{
    mp_image_t *mpi = ptr;

    if (mpi->refcount) {
        m_refcount_unref(mpi->refcount);
    }

    if (mpi->flags & MP_IMGFLAG_ALLOCATED) {
        /* because we allocate the whole image at once */
        av_free(mpi->planes[0]);
        if (mpi->flags & MP_IMGFLAG_RGB_PALETTE)
            av_free(mpi->planes[1]);
    }

    return 0;
}

// Image without format or allocated image data
struct mp_image *mp_image_new_empty(int w, int h)
{
    struct mp_image *mpi = talloc_zero(NULL, struct mp_image);
    talloc_set_destructor(mpi, mp_image_destructor);
    mp_image_set_size(mpi, w, h);
    return mpi;
}

// Caller has to make sure this doesn't exceed the allocated plane data/strides.
void mp_image_set_size(struct mp_image *mpi, int w, int h)
{
    mpi->w = w;
    mpi->h = h;
    for (int n = 0; n < mpi->num_planes; n++) {
        mpi->plane_w[n] = mpi->w >> mpi->fmt.xs[n];
        mpi->plane_h[n] = mpi->h >> mpi->fmt.ys[n];
    }
    mpi->chroma_width = mpi->plane_w[1];
    mpi->chroma_height = mpi->plane_h[1];
    mpi->display_w = mpi->display_h = 0;
}

void mp_image_set_display_size(struct mp_image *mpi, int dw, int dh)
{
    mpi->display_w = dw;
    mpi->display_h = dh;
}

struct mp_image *mp_image_alloc(unsigned int imgfmt, int w, int h)
{
    struct mp_image *mpi = mp_image_new_empty(w, h);

    mpi->w = FFALIGN(w, MP_STRIDE_ALIGNMENT);
    mp_image_setfmt(mpi, imgfmt);
    mp_image_alloc_planes(mpi);
    mpi->w = w;
    mp_image_setfmt(mpi, imgfmt); // reset chroma size

    mpi->flags &= ~MP_IMGFLAG_ALLOCATED;
    mpi->refcount = m_refcount_new();
    mpi->refcount->free = av_free;
    mpi->refcount->arg = mpi->planes[0];
    // NOTE: palette isn't free'd. Palette handling should be fixed instead.

    return mpi;
}

struct mp_image *mp_image_new_copy(struct mp_image *img)
{
    struct mp_image *new = mp_image_alloc(img->imgfmt, img->w, img->h);
    mp_image_copy(new, img);
    mp_image_copy_attributes(new, img);

    // Normally these are covered by the reference to the original image data
    // (like the AVFrame in vd_lavc.c), but we can't manage it on our own.
    new->qscale = NULL;
    new->qstride = 0;

    return new;
}

// Make dst take over the image data of src, and free src.
// This is basically a safe version of *dst = *src; free(src);
// Only works with ref-counted images, and can't change image size/format.
void mp_image_steal_data(struct mp_image *dst, struct mp_image *src)
{
    assert(dst->imgfmt == src->imgfmt && dst->w == src->w && dst->h == src->h);
    assert(dst->refcount && src->refcount);

    for (int p = 0; p < MP_MAX_PLANES; p++) {
        dst->planes[p] = src->planes[p];
        dst->stride[p] = src->stride[p];
    }
    mp_image_copy_attributes(dst, src);

    m_refcount_unref(dst->refcount);
    dst->refcount = src->refcount;
    talloc_set_destructor(src, NULL);
    talloc_free(src);
}

// Return a new reference to img. The returned reference is owned by the caller,
// while img is left untouched.
struct mp_image *mp_image_new_ref(struct mp_image *img)
{
    if (!img->refcount)
        return mp_image_new_copy(img);

    struct mp_image *new = talloc_ptrtype(NULL, new);
    talloc_set_destructor(new, mp_image_destructor);
    *new = *img;

    m_refcount_ref(new->refcount);
    return new;
}

// Return a reference counted reference to img. If the reference count reaches
// 0, call free(free_arg). The data passed by img must not be free'd before
// that. The new reference will be writeable.
struct mp_image *mp_image_new_custom_ref(struct mp_image *img, void *free_arg,
                                         void (*free)(void *arg))
{
    struct mp_image *new = talloc_ptrtype(NULL, new);
    talloc_set_destructor(new, mp_image_destructor);
    *new = *img;

    new->flags &= ~MP_IMGFLAG_ALLOCATED;
    new->refcount = m_refcount_new();
    new->refcount->free = free;
    new->refcount->arg = free_arg;
    return new;
}

// Return a reference counted reference to img. ref/unref/is_unique are used to
// connect to an external refcounting API. It is assumed that the new object
// has an initial reference to that external API.
struct mp_image *mp_image_new_external_ref(struct mp_image *img, void *arg,
                                           void (*ref)(void *arg),
                                           void (*unref)(void *arg),
                                           bool (*is_unique)(void *arg))
{
    struct mp_image *new = talloc_ptrtype(NULL, new);
    talloc_set_destructor(new, mp_image_destructor);
    *new = *img;

    new->flags &= ~MP_IMGFLAG_ALLOCATED;
    new->refcount = m_refcount_new();
    new->refcount->ext_ref = ref;
    new->refcount->ext_unref = unref;
    new->refcount->ext_is_unique = is_unique;
    new->refcount->arg = arg;
    return new;
}

bool mp_image_is_writeable(struct mp_image *img)
{
    // if non ref-counted, it's writeable if the caller allocated the image
    if (!img->refcount)
        return img->flags & MP_IMGFLAG_ALLOCATED;
    return m_refcount_is_unique(img->refcount);
}

// Make the image data referenced by img writeable. This allocates new data
// if the data wasn't already writeable, and img->planes[] and img->stride[]
// will be set to the copy.
void mp_image_make_writeable(struct mp_image *img)
{
    if (mp_image_is_writeable(img))
        return;

    mp_image_steal_data(img, mp_image_new_copy(img));
    assert(mp_image_is_writeable(img));
}

void mp_image_setrefp(struct mp_image **p_img, struct mp_image *new_value)
{
    if (*p_img != new_value) {
        talloc_free(*p_img);
        *p_img = new_value ? mp_image_new_ref(new_value) : NULL;
    }
}

// Mere helper function (mp_image can be directly free'd with talloc_free)
void mp_image_unrefp(struct mp_image **p_img)
{
    talloc_free(*p_img);
    *p_img = NULL;
}

enum mp_csp mp_image_csp(struct mp_image *img)
{
    if (img->colorspace != MP_CSP_AUTO)
        return img->colorspace;
    return (img->flags & MP_IMGFLAG_YUV) ? MP_CSP_BT_601 : MP_CSP_RGB;
}

enum mp_csp_levels mp_image_levels(struct mp_image *img)
{
    if (img->levels != MP_CSP_LEVELS_AUTO)
        return img->levels;
    return (img->flags & MP_IMGFLAG_YUV) ? MP_CSP_LEVELS_TV : MP_CSP_LEVELS_PC;
}

void mp_image_set_colorspace_details(struct mp_image *image,
                                     struct mp_csp_details *csp)
{
    if (image->flags & MP_IMGFLAG_YUV) {
        image->colorspace = csp->format;
        if (image->colorspace == MP_CSP_AUTO)
            image->colorspace = MP_CSP_BT_601;
        image->levels = csp->levels_in;
        if (image->levels == MP_CSP_LEVELS_AUTO)
            image->levels = MP_CSP_LEVELS_TV;
    } else {
        image->colorspace = MP_CSP_RGB;
        image->levels = MP_CSP_LEVELS_PC;
    }
}
