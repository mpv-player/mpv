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
#include <pthread.h>
#include <assert.h>

#include <VapourSynth.h>
#include <VSScript.h>
#include <VSHelper.h>

#include <libavutil/rational.h>

#include "common/msg.h"
#include "options/m_option.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "vf.h"

struct vf_priv_s {
    VSCore *vscore;
    const VSAPI *vsapi;
    VSScript *se;
    VSNodeRef *out_node;
    VSNodeRef *in_node;

    struct mp_image_params fmt_in;

    pthread_mutex_t lock;
    pthread_cond_t wakeup;

    // --- the following members are all protected by lock
    struct mp_image *next_image;// used to compute frame duration of oldest image
    struct mp_image **buffered; // oldest image first
    int num_buffered;
    int in_frameno;             // frame number of buffered[0] (the oldest)
    int out_frameno;            // frame number of first requested/ready frame
    double out_pts;             // pts corresponding to first requested/ready frame
    struct mp_image **requested;// frame callback results (can point to dummy_img)
                                // requested[0] is the frame to return first
    int max_requests;           // upper bound for requested[] array
    bool failed;                // frame callback returned with an error
    bool shutdown;              // ask node to return
    bool in_node_active;        // node might still be called

    // --- options
    char *cfg_file;
    int cfg_maxbuffer;
    int cfg_maxrequests;
};

// priv->requested[n] points to this if a request for frame n is in-progress
static const struct mp_image dummy_img;

struct mpvs_fmt {
    VSPresetFormat vs;
    int mp;
};

static const struct mpvs_fmt mpvs_fmt_table[] = {
    {pfYUV420P8, IMGFMT_420P},
    {pfYUV422P8, IMGFMT_422P},
    {pfYUV444P8, IMGFMT_444P},
    {pfYUV410P8, IMGFMT_410P},
    {pfYUV411P8, IMGFMT_411P},
    {pfYUV440P8, IMGFMT_440P},
    {pfYUV420P9, IMGFMT_420P9},
    {pfYUV422P9, IMGFMT_422P9},
    {pfYUV444P9, IMGFMT_444P9},
    {pfYUV420P10, IMGFMT_420P10},
    {pfYUV422P10, IMGFMT_422P10},
    {pfYUV444P10, IMGFMT_444P10},
    {pfYUV420P16, IMGFMT_420P16},
    {pfYUV422P16, IMGFMT_422P16},
    {pfYUV444P16, IMGFMT_444P16},
    {pfNone}
};

static VSPresetFormat mp_to_vs(int imgfmt)
{
    for (int n = 0; mpvs_fmt_table[n].mp; n++) {
        if (mpvs_fmt_table[n].mp == imgfmt)
            return mpvs_fmt_table[n].vs;
    }
    return pfNone;
}

static int mp_from_vs(VSPresetFormat vs)
{
    for (int n = 0; mpvs_fmt_table[n].mp; n++) {
        if (mpvs_fmt_table[n].vs == vs)
            return mpvs_fmt_table[n].mp;
    }
    return pfNone;
}

static void copy_mp_to_vs_frame_props(struct vf_priv_s *p, VSMap *map,
                                      struct mp_image *img)
{
    struct mp_image_params *params = &img->params;
    if (params->d_w > 0 && params->d_h > 0) {
        AVRational dar = {params->d_w, params->d_h};
        AVRational asp = {params->w, params->h};
        AVRational par = av_div_q(dar, asp);

        p->vsapi->propSetInt(map, "_SARNum", par.num, 0);
        p->vsapi->propSetInt(map, "_SARDen", par.den, 0);
    }
    if (params->colorlevels) {
        p->vsapi->propSetInt(map, "_ColorRange",
                params->colorlevels == MP_CSP_LEVELS_TV, 0);
    }
    // The docs explicitly say it uses libavcodec values.
    p->vsapi->propSetInt(map, "_ColorSpace",
            mp_csp_to_avcol_spc(params->colorspace), 0);
    char pict_type = 0;
    switch (img->pict_type) {
    case 1: pict_type = 'I'; break;
    case 2: pict_type = 'P'; break;
    case 3: pict_type = 'B'; break;
    }
    if (pict_type)
        p->vsapi->propSetData(map, "_PictType", &pict_type, 1, 0);
}

static struct mp_image map_vs_frame(struct vf_priv_s *p, const VSFrameRef *ref,
                                    bool w)
{
    const VSFormat *fmt = p->vsapi->getFrameFormat(ref);

    struct mp_image img = {0};
    mp_image_setfmt(&img, mp_from_vs(fmt->id));
    mp_image_set_size(&img, p->vsapi->getFrameWidth(ref, 0),
                            p->vsapi->getFrameHeight(ref, 0));

    for (int n = 0; n < img.num_planes; n++) {
        if (w) {
            img.planes[n] = p->vsapi->getWritePtr((VSFrameRef *)ref, n);
        } else {
            img.planes[n] = (uint8_t *)p->vsapi->getReadPtr(ref, n);
        }
        img.stride[n] = p->vsapi->getStride(ref, n);
    }

    return img;
}

static void drain_oldest_buffered_frame(struct vf_priv_s *p)
{
    if (!p->num_buffered)
        return;
    talloc_free(p->buffered[0]);
    for (int n = 0; n < p->num_buffered - 1; n++)
        p->buffered[n] = p->buffered[n + 1];
    p->num_buffered--;
    p->in_frameno++;
}

static void VS_CC vs_frame_done(void *userData, const VSFrameRef *f, int n,
                                VSNodeRef *node, const char *errorMsg)
{
    struct vf_instance *vf = userData;
    struct vf_priv_s *p = vf->priv;

    pthread_mutex_lock(&p->lock);

    // If these assertions fail, n is an unrequested frame (or filtered twice).
    assert(n >= p->out_frameno && n < p->out_frameno + p->max_requests);
    int index = n - p->out_frameno;
    MP_DBG(vf, "filtered frame %d (%d)\n", n, index);
    assert(p->requested[index] == &dummy_img);

    struct mp_image *res = NULL;
    if (f) {
        struct mp_image img = map_vs_frame(p, f, false);
        img.pts = MP_NOPTS_VALUE;
        const VSMap *map = p->vsapi->getFramePropsRO(f);
        if (map) {
            int err1, err2;
            int num = p->vsapi->propGetInt(map, "_DurationNum", 0, &err1);
            int den = p->vsapi->propGetInt(map, "_DurationDen", 0, &err2);
            if (!err1 && !err2)
                img.pts = num / (double)den; // abusing pts for frame length
        }
        if (img.pts == MP_NOPTS_VALUE)
            MP_ERR(vf, "No PTS after filter at frame %d!\n", n);
        res = mp_image_new_copy(&img);
        p->vsapi->freeFrame(f);
    } else {
        p->failed = true;
        MP_ERR(vf, "Filter error at frame %d: %s\n", n, errorMsg);
    }
    p->requested[index] = res;
    pthread_cond_broadcast(&p->wakeup);
    pthread_mutex_unlock(&p->lock);
}

static int filter_ext(struct vf_instance *vf, struct mp_image *mpi)
{
    struct vf_priv_s *p = vf->priv;
    int ret = 0;

    if (!p->out_node) {
        talloc_free(mpi);
        return -1;
    }

    MPSWAP(struct mp_image *, p->next_image, mpi);

    if (!mpi)
        return 0;

    // Turn PTS into frame duration (the pts field is abused for storing it)
    if (p->out_pts == MP_NOPTS_VALUE)
        p->out_pts = mpi->pts;
    mpi->pts = p->next_image ? p->next_image->pts - mpi->pts : 0;

    // Try to get new frames until we get rid of the input mpi.
    pthread_mutex_lock(&p->lock);
    while (1) {
        // Not sure what we do on errors, but at least don't deadlock.
        if (p->failed) {
            p->failed = false;
            talloc_free(mpi);
            ret = -1;
            break;
        }

        if (mpi && p->num_buffered < MP_TALLOC_ELEMS(p->buffered)) {
            p->buffered[p->num_buffered++] = talloc_steal(p->buffered, mpi);
            mpi = NULL;
            pthread_cond_broadcast(&p->wakeup);
        }

        while (p->requested[0] && p->requested[0] != &dummy_img) {
            struct mp_image *out = p->requested[0];
            if (out->pts != MP_NOPTS_VALUE) {
                double duration = out->pts;
                out->pts = p->out_pts;
                p->out_pts += duration;
            }
            vf_add_output_frame(vf, out);
            for (int n = 0; n < p->max_requests - 1; n++)
                p->requested[n] = p->requested[n + 1];
            p->requested[p->max_requests - 1] = NULL;
            p->out_frameno++;
        }

        for (int n = 0; n < p->max_requests; n++) {
            if (!p->requested[n]) {
                // Note: this assumes getFrameAsync() will never call
                //       infiltGetFrame (if it does, we would deadlock)
                p->requested[n] = (struct mp_image *)&dummy_img;
                p->failed = false;
                MP_DBG(vf, "requesting frame %d (%d)\n", p->out_frameno + n, n);
                p->vsapi->getFrameAsync(p->out_frameno + n, p->out_node,
                                        vs_frame_done, vf);
            }
        }

        if (!mpi)
            break;
        pthread_cond_wait(&p->wakeup, &p->lock);
    }
    pthread_mutex_unlock(&p->lock);

    return ret;
}

static void VS_CC infiltInit(VSMap *in, VSMap *out, void **instanceData,
                             VSNode *node, VSCore *core, const VSAPI *vsapi)
{
    struct vf_instance *vf = *instanceData;
    struct vf_priv_s *p = vf->priv;

    // Note: this is called from createFilter, so no need for locking.

    VSVideoInfo fmt = {
        .format = p->vsapi->getFormatPreset(mp_to_vs(p->fmt_in.imgfmt), p->vscore),
        .width = p->fmt_in.w,
        .height = p->fmt_in.h,
    };
    if (!fmt.format) {
        p->vsapi->setError(out, "Unsupported input format.\n");
        return;
    }

    p->vsapi->setVideoInfo(&fmt, 1, node);
    p->in_node_active = true;
}

static const VSFrameRef *VS_CC infiltGetFrame(int frameno, int activationReason,
    void **instanceData, void **frameData,
    VSFrameContext *frameCtx, VSCore *core,
    const VSAPI *vsapi)
{
    struct vf_instance *vf = *instanceData;
    struct vf_priv_s *p = vf->priv;
    VSFrameRef *ret = NULL;

    pthread_mutex_lock(&p->lock);
    MP_DBG(vf, "VS asking for frame %d (at %d)\n", frameno, p->in_frameno);
    while (1) {
        if (p->shutdown) {
            p->vsapi->setFilterError("EOF or filter reinit/uninit", frameCtx);
            break;
        }
        if (frameno < p->in_frameno) {
            char msg[180];
            snprintf(msg, sizeof(msg),
                "Frame %d requested, but only have frames starting from %d. "
                "Try increasing the buffered-frames suboption.",
                frameno, p->in_frameno);
            MP_FATAL(vf, "%s\n", msg);
            p->vsapi->setFilterError(msg, frameCtx);
            break;
        }
        if (frameno >= p->in_frameno + MP_TALLOC_ELEMS(p->buffered)) {
            // Too far in the future. Remove frames, so that the main thread can
            // queue new frames.
            if (p->num_buffered) {
                drain_oldest_buffered_frame(p);
                pthread_cond_broadcast(&p->wakeup);
                continue;
            }
        }
        if (frameno < p->in_frameno + p->num_buffered) {
            struct mp_image *img = p->buffered[frameno - p->in_frameno];
            const VSFormat *vsfmt =
                vsapi->getFormatPreset(mp_to_vs(img->imgfmt), core);
            ret = vsapi->newVideoFrame(vsfmt, img->w, img->h, NULL, core);
            if (!ret) {
                p->vsapi->setFilterError("Could not allocate VS frame", frameCtx);
                break;
            }
            struct mp_image vsframe = map_vs_frame(p, ret, true);
            mp_image_copy(&vsframe, img);
            VSMap *map = p->vsapi->getFramePropsRW(ret);
            if (map) {
                int res = 1e6;
                int dur = img->pts * res + 0.5;
                p->vsapi->propSetInt(map, "_DurationNum", dur, 0);
                p->vsapi->propSetInt(map, "_DurationDen", res, 0);
                copy_mp_to_vs_frame_props(p, map, img);
            }
            break;
        }
        pthread_cond_wait(&p->wakeup, &p->lock);
    }
    pthread_mutex_unlock(&p->lock);
    return ret;
}

static void VS_CC infiltFree(void *instanceData, VSCore *core, const VSAPI *vsapi)
{
    struct vf_instance *vf = instanceData;
    struct vf_priv_s *p = vf->priv;

    pthread_mutex_lock(&p->lock);
    p->in_node_active = false;
    pthread_cond_broadcast(&p->wakeup);
    pthread_mutex_unlock(&p->lock);
}

// number of getAsyncFrame calls in progress
// must be called with p->lock held
static int num_requested(struct vf_priv_s *p)
{
    int r = 0;
    for (int n = 0; n < p->max_requests; n++)
        r += p->requested[n] == &dummy_img;
    return r;
}

static void destroy_vs(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    MP_DBG(vf, "destroying VS filters\n");

    // Wait until our frame callbacks return.
    pthread_mutex_lock(&p->lock);
    p->shutdown = true;
    pthread_cond_broadcast(&p->wakeup);
    while (num_requested(p))
        pthread_cond_wait(&p->wakeup, &p->lock);
    pthread_mutex_unlock(&p->lock);

    if (p->in_node)
        p->vsapi->freeNode(p->in_node);
    if (p->out_node)
        p->vsapi->freeNode(p->out_node);
    p->in_node = p->out_node = NULL;

    if (p->se)
        vsscript_freeScript(p->se);

    p->se = NULL;
    p->vsapi = NULL;
    p->vscore = NULL;

    assert(!p->in_node_active);
    assert(num_requested(p) == 0); // async callback didn't return?

    p->shutdown = false;
    // Kill filtered images that weren't returned yet
    for (int n = 0; n < p->max_requests; n++)
        mp_image_unrefp(&p->requested[n]);
    // Kill queued frames too
    for (int n = 0; n < p->num_buffered; n++)
        talloc_free(p->buffered[n]);
    p->num_buffered = 0;
    talloc_free(p->next_image);
    p->next_image = NULL;
    p->out_pts = MP_NOPTS_VALUE;
    p->out_frameno = p->in_frameno = 0;

    MP_DBG(vf, "uninitialized.\n");
}

static int reinit_vs(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    VSMap *vars = NULL, *in = NULL, *out = NULL;
    int res = -1;

    destroy_vs(vf);

    MP_DBG(vf, "initializing...\n");

    // First load an empty script to get a VSScript, so that we get the vsapi
    // and vscore.
    if (vsscript_evaluateScript(&p->se, "", NULL, 0))
        goto error;
    p->vsapi = vsscript_getVSApi();
    p->vscore = vsscript_getCore(p->se);
    if (!p->vsapi || !p->vscore)
        goto error;

    in = p->vsapi->createMap();
    out = p->vsapi->createMap();
    vars = p->vsapi->createMap();
    if (!in || !out || !vars)
        goto error;

    p->vsapi->createFilter(in, out, "Input", infiltInit, infiltGetFrame,
                           infiltFree, fmSerial, 0, vf, p->vscore);
    int vserr;
    p->in_node = p->vsapi->propGetNode(out, "clip", 0, &vserr);
    if (!p->in_node)
        goto error;

    if (p->vsapi->propSetNode(vars, "video_in", p->in_node, 0))
        goto error;

    vsscript_setVariable(p->se, vars);

    if (vsscript_evaluateFile(&p->se, p->cfg_file, 0)) {
        MP_FATAL(vf, "Script evaluation failed:\n%s\n", vsscript_getError(p->se));
        goto error;
    }
    p->out_node = vsscript_getOutput(p->se, 0);
    if (!p->out_node)
        goto error;

    const VSVideoInfo *vi = p->vsapi->getVideoInfo(p->out_node);
    if (!isConstantFormat(vi)) {
        MP_FATAL(vf, "Video format is required to be constant.\n");
        goto error;
    }

    MP_DBG(vf, "initialized.\n");
    res = 0;
error:
    if (p->vsapi) {
        p->vsapi->freeMap(in);
        p->vsapi->freeMap(out);
        p->vsapi->freeMap(vars);
    }
    if (res < 0)
        destroy_vs(vf);
    return res;
}

static int config(struct vf_instance *vf, int width, int height,
                  int d_width, int d_height, unsigned int flags,
                  unsigned int fmt)
{
    struct vf_priv_s *p = vf->priv;

    p->fmt_in = (struct mp_image_params){
        .imgfmt = fmt,
        .w = width,
        .h = height,
    };

    if (reinit_vs(vf) < 0)
        return 0;

    const VSVideoInfo *vi = p->vsapi->getVideoInfo(p->out_node);
    fmt = mp_from_vs(vi->format->id);
    if (!fmt) {
        MP_FATAL(vf, "Unsupported output format.\n");
        destroy_vs(vf);
        return 0;
    }

    vf_rescale_dsize(&d_width, &d_height, width, height, vi->width, vi->height);

    return vf_next_config(vf, vi->width, vi->height, d_width, d_height, flags, fmt);
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    return mp_to_vs(fmt) != pfNone ? VFCAP_CSP_SUPPORTED : 0;
}

static int control(vf_instance_t *vf, int request, void *data)
{
    switch (request) {
    case VFCTRL_SEEK_RESET:
        if (reinit_vs(vf) < 0)
            return CONTROL_ERROR;
        return CONTROL_OK;
    }
    return CONTROL_UNKNOWN;
}

static void uninit(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    destroy_vs(vf);
    vsscript_finalize();

    pthread_cond_destroy(&p->wakeup);
    pthread_mutex_destroy(&p->lock);
}

static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;
    if (!vsscript_init()) {
        MP_FATAL(vf, "Could not initialize VapourSynth scripting.\n");
        return 0;
    }
    if (!p->cfg_file || !p->cfg_file[0]) {
        MP_FATAL(vf, "'file' parameter must be set.\n");
        return 0;
    }

    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->wakeup, NULL);
    vf->reconfig = NULL;
    vf->config = config;
    vf->filter_ext = filter_ext;
    vf->filter = NULL;
    vf->query_format = query_format;
    vf->control = control;
    vf->uninit = uninit;
    int maxbuffer = p->cfg_maxbuffer * p->cfg_maxrequests;
    p->buffered = talloc_array(vf, struct mp_image *, maxbuffer);
    p->max_requests = p->cfg_maxrequests;
    p->requested = talloc_zero_array(vf, struct mp_image *, p->max_requests);
    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_STRING("file", cfg_file, 0),
    OPT_INTRANGE("buffered-frames", cfg_maxbuffer, 0, 1, 9999, OPTDEF_INT(4)),
    OPT_INTRANGE("concurrent-frames", cfg_maxrequests, 0, 1, 99, OPTDEF_INT(2)),
    {0}
};

const vf_info_t vf_info_vapoursynth = {
    .description = "vapoursynth bridge",
    .name = "vapoursynth",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = vf_opts_fields,
};
