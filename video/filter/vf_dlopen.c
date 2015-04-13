/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "config.h"
#include "common/msg.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "vf.h"

#include "options/m_option.h"

#include "vf_dlopen.h"

#ifdef _WIN32
# include <windows.h>
# define DLLOpen(name)           LoadLibraryA(name)
# define DLLClose(handle)        FreeLibrary(handle)
# define DLLSymbol(handle, name) ((void *)GetProcAddress(handle, name))
#else
# include <dlfcn.h>
# define DLLOpen(name)           dlopen(name, RTLD_NOW)
# define DLLClose(handle)        dlclose(handle)
# define DLLSymbol(handle, name) dlsym(handle, name)
#endif

struct vf_priv_s {
    char *cfg_dllname;
    int cfg_argc;
    char *cfg_argv[16];
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

    unsigned int outfmt;

    int argc;
};

struct fmtname {
    const char *name;
    enum mp_imgfmt fmt;
};

//===========================================================================//

static void set_imgprop(struct vf_dlopen_picdata *out, const mp_image_t *mpi)
{
    int i;
    out->planes = mpi->num_planes;
    for (i = 0; i < mpi->num_planes; ++i) {
        out->plane[i] = mpi->planes[i];
        out->planestride[i] = mpi->stride[i];
        out->planewidth[i] =
            i ? (/*mpi->chroma_width*/ mpi->w >> mpi->fmt.chroma_xs) : mpi->w;
        out->planeheight[i] =
            i ? (/*mpi->chroma_height*/ mpi->h >> mpi->fmt.chroma_ys) : mpi->h;
        out->planexshift[i] = i ? mpi->fmt.chroma_xs : 0;
        out->planeyshift[i] = i ? mpi->fmt.chroma_ys : 0;
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
    vf->priv->filter.in_fmt = talloc_strdup(vf, mp_imgfmt_to_name(fmt));
    vf->priv->filter.out_width = width;
    vf->priv->filter.out_height = height;
    vf->priv->filter.out_d_width = d_width;
    vf->priv->filter.out_d_height = d_height;
    vf->priv->filter.out_fmt = NULL;
    vf->priv->filter.out_cnt = 1;

    if (!vf->priv->filter.in_fmt) {
        MP_ERR(vf, "invalid input/output format\n");
        return 0;
    }
    if (vf->priv->filter.config && vf->priv->filter.config(&vf->priv->filter) < 0) {
        MP_ERR(vf, "filter config failed\n");
        return 0;
    }

    // copy away stuff to sanity island
    vf->priv->out_cnt = vf->priv->filter.out_cnt;
    vf->priv->out_width = vf->priv->filter.out_width;
    vf->priv->out_height = vf->priv->filter.out_height;

    if (vf->priv->filter.out_fmt)
        vf->priv->outfmt = mp_imgfmt_from_name(bstr0(vf->priv->filter.out_fmt), false);
    else {
        struct vf_dlopen_formatpair *p = vf->priv->filter.format_mapping;
        vf->priv->outfmt = 0;
        if (p) {
            for (; p->from; ++p) {
                // TODO support pixel format classes in matching
                if (!strcmp(p->from, vf->priv->filter.in_fmt)) {
                    if(p->to)
                        vf->priv->outfmt = mp_imgfmt_from_name(bstr0(p->to), false);
                    else
                        vf->priv->outfmt = mp_imgfmt_from_name(bstr0(p->from), false);
                    break;
                }
            }
        } else
            vf->priv->outfmt = fmt;
        vf->priv->filter.out_fmt =
            talloc_strdup(vf, mp_imgfmt_to_name(vf->priv->outfmt));
    }

    if (!vf->priv->outfmt) {
        MP_ERR(vf, "filter config wants an unsupported output format\n");
        return 0;
    }
    if (!vf->priv->out_cnt || vf->priv->out_cnt > FILTER_MAX_OUTCNT) {
        MP_ERR(vf, "filter config wants to yield zero or too many output frames\n");
        return 0;
    }

    for (int i = 0; i < vf->priv->out_cnt; ++i) {
        talloc_free(vf->priv->outpic[i]);
        vf->priv->outpic[i] =
            mp_image_alloc(vf->priv->outfmt,
                           vf->priv->out_width, vf->priv->out_height);
        if (!vf->priv->outpic[i])
            return 0; // OOM
        talloc_steal(vf, vf->priv->outpic[i]);
        set_imgprop(&vf->priv->filter.outpic[i], vf->priv->outpic[i]);
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
    memset(&vf->priv->filter, 0, sizeof(vf->priv->filter));
    if (vf->priv->dll) {
        DLLClose(vf->priv->dll);
        vf->priv->dll = NULL;
    }
}

static int filter(struct vf_instance *vf, struct mp_image *mpi)
{
    if (!mpi)
        return 0;

    set_imgprop(&vf->priv->filter.inpic, mpi);
    vf->priv->filter.inpic_qscale = NULL;
    vf->priv->filter.inpic_qscalestride = 0;
    vf->priv->filter.inpic_qscaleshift = 0;
    vf->priv->filter.inpic.pts = mpi->pts;

    struct mp_image *out[FILTER_MAX_OUTCNT] = {0};

    for (int n = 0; n < vf->priv->out_cnt; n++) {
        out[n] = vf_alloc_out_image(vf);
        if (!out[n]) {
            talloc_free(mpi);
            return -1;
        }
        mp_image_copy_attributes(out[n], mpi);
        set_imgprop(&vf->priv->filter.outpic[n], out[n]);
    }

    // more than one out pic
    int ret = vf->priv->filter.put_image(&vf->priv->filter);
    if (ret < 0)
        ret = 0;
    assert(ret <= vf->priv->out_cnt);

    for (int n = 0; n < ret; n++) {
        out[n]->pts = vf->priv->filter.outpic[n].pts;
        vf_add_output_frame(vf, out[n]);
    }
    for (int n = ret; n < FILTER_MAX_OUTCNT; n++) {
        talloc_free(out[n]);
    }

    talloc_free(mpi);
    return 0;
}

//===========================================================================//

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    if (IMGFMT_IS_HWACCEL(fmt))
        return 0;  // these can't really be filtered
    if (fmt == IMGFMT_PAL8)
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
                if (p->to)
                    outfmt = mp_imgfmt_from_name(bstr0(p->to), false);
                else
                    outfmt = mp_imgfmt_from_name(bstr0(p->from), false);
                break;
            }
        }
    } else {
        outfmt = fmt;
    }
    if (!outfmt)
        return 0;
    return vf_next_query_format(vf, outfmt);
}

static int vf_open(vf_instance_t *vf)
{
    int i;
    if (!vf->priv->cfg_dllname) {
        MP_ERR(vf, "usage: --vf=dlopen=/path/to/filename.so:args\n");
        return 0;
    }

    vf->priv->dll = DLLOpen(vf->priv->cfg_dllname);
    if (!vf->priv->dll) {
        MP_ERR(vf, "library not found: %s\n",
               vf->priv->cfg_dllname);
        return 0;
    }

    vf_dlopen_getcontext_func *func =
        (vf_dlopen_getcontext_func *) DLLSymbol(vf->priv->dll, "vf_dlopen_getcontext");
    if (!func) {
        MP_ERR(vf, "library is not a filter: %s\n",
               vf->priv->cfg_dllname);
        return 0;
    }

    memset(&vf->priv->filter, 0, sizeof(vf->priv->filter));
    vf->priv->filter.major_version = VF_DLOPEN_MAJOR_VERSION;
    vf->priv->filter.minor_version = VF_DLOPEN_MINOR_VERSION;

    // count arguments
    for (vf->priv->cfg_argc = sizeof(vf->priv->cfg_argv) / sizeof(vf->priv->cfg_argv[0]);
         vf->priv->cfg_argc > 0 && !vf->priv->cfg_argv[vf->priv->cfg_argc - 1];
         --vf->priv->cfg_argc)
        ;

    // fix empty arguments
    for (i = 0; i < vf->priv->cfg_argc; ++i)
        if (vf->priv->cfg_argv[i] == NULL)
            vf->priv->cfg_argv[i] = talloc_strdup (vf->priv, "");

    if (func(&vf->priv->filter, vf->priv->cfg_argc,
             (const char **)vf->priv->cfg_argv)  < 0)
    {
        MP_ERR(vf, "function did not create a filter: %s\n",
               vf->priv->cfg_dllname);
        return 0;
    }

    if (!vf->priv->filter.put_image) {
        MP_ERR(vf, "function did not create a filter that can put images: %s\n",
               vf->priv->cfg_dllname);
        return 0;
    }

    vf->filter_ext = filter;
    vf->query_format = query_format;
    vf->config = config;
    vf->uninit = uninit;

    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_STRING("dll", cfg_dllname, 0),
    OPT_STRING("a0", cfg_argv[0], 0),
    OPT_STRING("a1", cfg_argv[1], 0),
    OPT_STRING("a2", cfg_argv[2], 0),
    OPT_STRING("a3", cfg_argv[3], 0),
    OPT_STRING("a4", cfg_argv[4], 0),
    OPT_STRING("a5", cfg_argv[5], 0),
    OPT_STRING("a6", cfg_argv[6], 0),
    OPT_STRING("a7", cfg_argv[7], 0),
    OPT_STRING("a8", cfg_argv[8], 0),
    OPT_STRING("a9", cfg_argv[9], 0),
    OPT_STRING("a10", cfg_argv[10], 0),
    OPT_STRING("a11", cfg_argv[11], 0),
    OPT_STRING("a12", cfg_argv[12], 0),
    OPT_STRING("a13", cfg_argv[13], 0),
    OPT_STRING("a14", cfg_argv[14], 0),
    OPT_STRING("a15", cfg_argv[15], 0),
    {0}
};

const vf_info_t vf_info_dlopen = {
    .description = "Dynamic library filter",
    .name = "dlopen",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = vf_opts_fields,
};

//===========================================================================//
