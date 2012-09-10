/*
 * This file is part of mplayer.
 *
 * mplayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "config.h"
#include "mp_msg.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "m_option.h"
#include "m_struct.h"

#include "vf_dlopen.h"

#ifdef _WIN32
# include <windows.h>
# define DLLOpen(name)           LoadLibrary(name)
# define DLLClose(handle)        FreeLibrary(handle)
# define DLLSymbol(handle, name) ((void *)GetProcAddress(handle, name))
#else
# include <dlfcn.h>
# define DLLOpen(name)           dlopen(name, RTLD_NOW)
# define DLLClose(handle)        dlclose(handle)
# define DLLSymbol(handle, name) dlsym(handle, name)
#endif

static struct vf_priv_s {
    const char *cfg_dllname;
    int cfg_argc;
    const char *cfg_argv[4];
    void *dll;
    struct vf_dlopen_context filter;

    // output mp_image_t stuff
    mp_image_t *outpic[FILTER_MAX_OUTCNT];

    // generic
    unsigned int out_cnt, out_width, out_height;

    // multi frame output
    unsigned int outbufferpos;
    unsigned int outbufferlen;
    mp_image_t *outbuffermpi;

    // qscale buffer
    unsigned char *qbuffer;
    size_t qbuffersize;

    unsigned int outfmt;

    int argc;
} const vf_priv_dflt = {};

//===========================================================================//

static void set_imgprop(struct vf_dlopen_picdata *out, const mp_image_t *mpi)
{
    int i;
    out->planes = mpi->num_planes;
    for (i = 0; i < mpi->num_planes; ++i) {
        out->plane[i] = mpi->planes[i];
        out->planestride[i] = mpi->stride[i];
        out->planewidth[i] =
            i ? (/*mpi->chroma_width*/ mpi->w >> mpi->chroma_x_shift) : mpi->w;
        out->planeheight[i] =
            i ? (/*mpi->chroma_height*/ mpi->h >> mpi->chroma_y_shift) : mpi->h;
        out->planexshift[i] = i ? mpi->chroma_x_shift : 0;
        out->planeyshift[i] = i ? mpi->chroma_y_shift : 0;
    }
}

static int config(struct vf_instance *vf,
                  int width, int height, int d_width, int d_height,
                  unsigned int flags, unsigned int fmt)
{
    vf->priv->filter.in_width = width;
    vf->priv->filter.in_height = height;
    vf->priv->filter.in_d_width = d_width;
    vf->priv->filter.in_d_height = d_height;
    vf->priv->filter.in_fmt = mp_imgfmt_to_name(fmt);
    vf->priv->filter.out_width = width;
    vf->priv->filter.out_height = height;
    vf->priv->filter.out_d_width = d_width;
    vf->priv->filter.out_d_height = d_height;
    vf->priv->filter.out_fmt = NULL;
    vf->priv->filter.out_cnt = 1;

    if (!vf->priv->filter.in_fmt) {
        mp_msg(MSGT_VFILTER, MSGL_ERR, "invalid input/output format\n");
        return 0;
    }
    if (vf->priv->filter.config && vf->priv->filter.config(&vf->priv->filter) < 0) {
        mp_msg(MSGT_VFILTER, MSGL_ERR, "filter config failed\n");
        return 0;
    }

    // copy away stuff to sanity island
    vf->priv->out_cnt = vf->priv->filter.out_cnt;
    vf->priv->out_width = vf->priv->filter.out_width;
    vf->priv->out_height = vf->priv->filter.out_height;

    if (vf->priv->filter.out_fmt)
        vf->priv->outfmt = mp_imgfmt_from_name(bstr0(vf->priv->filter.out_fmt),
                                               false);
    else {
        struct vf_dlopen_formatpair *p = vf->priv->filter.format_mapping;
        vf->priv->outfmt = 0;
        if (p) {
            for (; p->from; ++p) {
                // TODO support pixel format classes in matching
                if (!strcmp(p->from, vf->priv->filter.in_fmt)) {
                    vf->priv->outfmt = mp_imgfmt_from_name(bstr0(p->to), false);
                    break;
                }
            }
        } else
            vf->priv->outfmt = fmt;
        vf->priv->filter.out_fmt = mp_imgfmt_to_name(vf->priv->outfmt);
    }

    if (!vf->priv->outfmt) {
        mp_msg(MSGT_VFILTER, MSGL_ERR,
               "filter config wants an unsupported output format\n");
        return 0;
    }
    if (!vf->priv->out_cnt || vf->priv->out_cnt > FILTER_MAX_OUTCNT) {
        mp_msg(MSGT_VFILTER, MSGL_ERR,
               "filter config wants to yield zero or too many output frames\n");
        return 0;
    }

    if (vf->priv->out_cnt >= 2) {
        int i;
        for (i = 0; i < vf->priv->out_cnt; ++i) {
            vf->priv->outpic[i] =
                alloc_mpi(vf->priv->out_width, vf->priv->out_height,
                          vf->priv->outfmt);
            set_imgprop(&vf->priv->filter.outpic[i], vf->priv->outpic[i]);
        }
    }

    return vf_next_config(vf, vf->priv->out_width,
                          vf->priv->out_height,
                          vf->priv->filter.out_d_width,
                          vf->priv->filter.out_d_height,
                          flags, vf->priv->outfmt);
}

static void uninit(struct vf_instance *vf)
{
    if (vf->priv->filter.uninit)
        vf->priv->filter.uninit(&vf->priv->filter);
    memset(&vf->priv->filter, 0, sizeof(&vf->priv->filter));
    if (vf->priv->dll) {
        DLLClose(vf->priv->dll);
        vf->priv->dll = NULL;
    }
    if (vf->priv->out_cnt >= 2) {
        int i;
        for (i = 0; i < vf->priv->out_cnt; ++i) {
            free_mp_image(vf->priv->outpic[i]);
            vf->priv->outpic[i] = NULL;
        }
    }
    if (vf->priv->qbuffer) {
        free(vf->priv->qbuffer);
        vf->priv->qbuffer = NULL;
    }
}

// NOTE: only called if (vf->priv->out_cnt >= 2) {
static int continue_put_image(struct vf_instance *vf)
{
    int k;
    int ret = 0;

    mp_image_t *dmpi =
        vf_get_image(vf->next, vf->priv->outfmt, MP_IMGTYPE_EXPORT, 0,
                     vf->priv->outpic[vf->priv->outbufferpos]->w,
                     vf->priv->outpic[vf->priv->outbufferpos]->h);
    for (k = 0; k < vf->priv->outpic[vf->priv->outbufferpos]->num_planes;
         ++k) {
        dmpi->planes[k] = vf->priv->outpic[vf->priv->outbufferpos]->planes[k];
        dmpi->stride[k] = vf->priv->outpic[vf->priv->outbufferpos]->stride[k];
    }

    // pass through qscale if we can
    vf_clone_mpi_attributes(dmpi, vf->priv->outbuffermpi);

    ret =
        vf_next_put_image(vf, dmpi,
                          vf->priv->filter.outpic[vf->priv->outbufferpos].pts);

    ++vf->priv->outbufferpos;

    // more frames left?
    if (vf->priv->outbufferpos < vf->priv->outbufferlen)
        vf_queue_frame(vf, continue_put_image);

    return ret;
}

static int put_image(struct vf_instance *vf, mp_image_t *mpi, double pts)
{
    int i, k;

    set_imgprop(&vf->priv->filter.inpic, mpi);
    if (mpi->qscale) {
        if (mpi->qscale_type != 0) {
            k = mpi->qstride * ((mpi->height + 15) >> 4);
            if (vf->priv->qbuffersize != k) {
                vf->priv->qbuffer = realloc(vf->priv->qbuffer, k);
                vf->priv->qbuffersize = k;
            }
            for (i = 0; i < k; ++i)
                vf->priv->qbuffer[i] = norm_qscale(mpi->qscale[i],
                                                   mpi->qscale_type);
            vf->priv->filter.inpic_qscale = vf->priv->qbuffer;
        } else
            vf->priv->filter.inpic_qscale = mpi->qscale;
        vf->priv->filter.inpic_qscalestride = mpi->qstride;
        vf->priv->filter.inpic_qscaleshift = 4;
    } else {
        vf->priv->filter.inpic_qscale = NULL;
        vf->priv->filter.inpic_qscalestride = 0;
        vf->priv->filter.inpic_qscaleshift = 0;
    }
    vf->priv->filter.inpic.pts = pts;

    if (vf->priv->out_cnt >= 2) {
        // more than one out pic
        int ret = vf->priv->filter.put_image(&vf->priv->filter);
        if (ret <= 0)
            return ret;

        vf->priv->outbuffermpi = mpi;
        vf->priv->outbufferlen = ret;
        vf->priv->outbufferpos = 0;
        return continue_put_image(vf);
    } else {
        // efficient case: exactly one out pic
        mp_image_t *dmpi =
            vf_get_image(vf->next, vf->priv->outfmt,
                    MP_IMGTYPE_TEMP,
                    MP_IMGFLAG_ACCEPT_STRIDE | MP_IMGFLAG_PREFER_ALIGNED_STRIDE,
                    vf->priv->out_width, vf->priv->out_height);
        set_imgprop(&vf->priv->filter.outpic[0], dmpi);

        int ret = vf->priv->filter.put_image(&vf->priv->filter);
        if (ret <= 0)
            return ret;

        // pass through qscale if we can
        vf_clone_mpi_attributes(dmpi, mpi);

        return vf_next_put_image(vf, dmpi, vf->priv->filter.outpic[0].pts);
    }
}

//===========================================================================//

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (IMGFMT_IS_HWACCEL(fmt) || fmt == IMGFMT_MJPEG || fmt == IMGFMT_MPEGPES)
        return 0;  // these can't really be filtered
    if (fmt == IMGFMT_RGB8 || fmt == IMGFMT_BGR8)
        return 0;  // we don't have palette support, sorry
    const char *fmtname = mp_imgfmt_to_name(fmt);
    if (!fmtname)
        return 0;
    struct vf_dlopen_formatpair *p = vf->priv->filter.format_mapping;
    unsigned int outfmt = 0;
    if (p) {
        for (; p->from; ++p) {
            // TODO support pixel format classes in matching
            if (!strcmp(p->from, fmtname)) {
                outfmt = mp_imgfmt_from_name(bstr0(p->to), false);
                break;
            }
        }
    } else
        outfmt = fmt;
    if (!outfmt)
        return 0;
    return vf_next_query_format(vf, outfmt);
}

static int vf_open(vf_instance_t *vf, char *args)
{
    if (!vf->priv->cfg_dllname) {
        mp_msg(MSGT_VFILTER, MSGL_ERR,
               "usage: -vf dlopen=filename.so:function:args\n");
        return 0;
    }

    vf->priv->dll = DLLOpen(vf->priv->cfg_dllname);
    if (!vf->priv->dll) {
        mp_msg(MSGT_VFILTER, MSGL_ERR, "library not found: %s\n",
               vf->priv->cfg_dllname);
        return 0;
    }
    
    vf_dlopen_getcontext_func *func =
        (vf_dlopen_getcontext_func *) DLLSymbol(vf->priv->dll, "vf_dlopen_getcontext");
    if (!func) {
        mp_msg(MSGT_VFILTER, MSGL_ERR, "library is not a filter: %s\n",
               vf->priv->cfg_dllname);
        return 0;
    }

    memset(&vf->priv->filter, 0, sizeof(vf->priv->filter));
    vf->priv->filter.major_version = VF_DLOPEN_MAJOR_VERSION;
    vf->priv->filter.minor_version = VF_DLOPEN_MINOR_VERSION;

    // count arguments
    for (vf->priv->cfg_argc = 0;
         vf->priv->cfg_argc < sizeof(vf->priv->cfg_argv) / sizeof(vf->priv->cfg_argv[0]) && vf->priv->cfg_argv[vf->priv->cfg_argc];
         ++vf->priv->cfg_argc)
        ;

    if (func(&vf->priv->filter, vf->priv->cfg_argc, vf->priv->cfg_argv) < 0) {
        mp_msg(MSGT_VFILTER, MSGL_ERR,
               "function did not create a filter: %s\n",
               vf->priv->cfg_dllname);
        return 0;
    }

    if (!vf->priv->filter.put_image) {
        mp_msg(MSGT_VFILTER, MSGL_ERR,
               "function did not create a filter that can put images: %s\n",
               vf->priv->cfg_dllname);
        return 0;
    }

    vf->put_image = put_image;
    vf->query_format = query_format;
    vf->config = config;
    vf->uninit = uninit;

    return 1;
}

#define ST_OFF(f) M_ST_OFF(struct vf_priv_s, f)
static m_option_t vf_opts_fields[] = {
    {"dll", ST_OFF(cfg_dllname), CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"a0", ST_OFF(cfg_argv[0]), CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"a1", ST_OFF(cfg_argv[1]), CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"a2", ST_OFF(cfg_argv[2]), CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"a3", ST_OFF(cfg_argv[3]), CONF_TYPE_STRING, 0, 0, 0, NULL},
    { NULL, NULL, 0, 0, 0, 0, NULL }
};

static const m_struct_t vf_opts = {
    "dlopen",
    sizeof(struct vf_priv_s),
    &vf_priv_dflt,
    vf_opts_fields
};

const vf_info_t vf_info_dlopen = {
    "Dynamic library filter",
    "dlopen",
    "Rudolf Polzer",
    "",
    vf_open,
    &vf_opts
};

//===========================================================================//
