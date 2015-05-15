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
#include <limits.h>
#include <assert.h>

#include <VapourSynth.h>
#include <VSHelper.h>

#include <libavutil/rational.h>
#include <libavutil/cpu.h>

#include "config.h"

#include "common/msg.h"
#include "options/m_option.h"
#include "options/path.h"

#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"
#include "vf.h"

struct vf_priv_s {
    VSCore *vscore;
    const VSAPI *vsapi;
    VSNodeRef *out_node;
    VSNodeRef *in_node;

    const struct script_driver *drv;
    // drv_vss
    struct VSScript *se;
    // drv_lazy
    struct lua_State *ls;
    VSNodeRef **gc_noderef;
    int num_gc_noderef;
    VSMap **gc_map;
    int num_gc_map;

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
    bool eof;                   // drain remaining data
    int64_t frames_sent;
    bool initializing;          // filters are being built
    bool in_node_active;        // node might still be called

    // --- options
    char *cfg_file;
    int cfg_maxbuffer;
    int cfg_maxrequests;
};

// priv->requested[n] points to this if a request for frame n is in-progress
static const struct mp_image dummy_img;

struct script_driver {
    int (*init)(struct vf_instance *vf);        // first time init
    void (*uninit)(struct vf_instance *vf);     // last time uninit
    int (*load_core)(struct vf_instance *vf);   // make vsapi/vscore available
    int (*load)(struct vf_instance *vf, VSMap *vars); // also set p->out_node
    void (*unload)(struct vf_instance *vf);     // unload script and maybe vs
};

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

static void copy_mp_to_vs_frame_props_map(struct vf_priv_s *p, VSMap *map,
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
    if (params->chroma_location) {
        p->vsapi->propSetInt(map, "_ChromaLocation",
                params->chroma_location == MP_CHROMA_CENTER, 0);
    }
    char pict_type = 0;
    switch (img->pict_type) {
    case 1: pict_type = 'I'; break;
    case 2: pict_type = 'P'; break;
    case 3: pict_type = 'B'; break;
    }
    if (pict_type)
        p->vsapi->propSetData(map, "_PictType", &pict_type, 1, 0);
    int field = 0;
    if (img->fields & MP_IMGFIELD_INTERLACED)
        field = img->fields & MP_IMGFIELD_TOP_FIRST ? 2 : 1;
    p->vsapi->propSetInt(map, "_FieldBased", field, 0);
}

static int set_vs_frame_props(struct vf_priv_s *p, VSFrameRef *frame,
                              struct mp_image *img, int dur_num, int dur_den)
{
    VSMap *map = p->vsapi->getFramePropsRW(frame);
    if (!map)
        return -1;
    p->vsapi->propSetInt(map, "_DurationNum", dur_num, 0);
    p->vsapi->propSetInt(map, "_DurationDen", dur_den, 0);
    copy_mp_to_vs_frame_props_map(p, map, img);
    return 0;
}

static VSFrameRef *alloc_vs_frame(struct vf_priv_s *p, struct mp_image_params *fmt)
{
    const VSFormat *vsfmt =
        p->vsapi->getFormatPreset(mp_to_vs(fmt->imgfmt), p->vscore);
    return p->vsapi->newVideoFrame(vsfmt, fmt->w, fmt->h, NULL, p->vscore);
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
    }
    if (!res) {
        p->failed = true;
        MP_ERR(vf, "Filter error at frame %d: %s\n", n, errorMsg);
    }
    p->requested[index] = res;
    pthread_cond_broadcast(&p->wakeup);
    pthread_mutex_unlock(&p->lock);
}

static bool locked_need_input(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    return p->num_buffered < MP_TALLOC_AVAIL(p->buffered);
}

// Return true if progress was made.
static bool locked_read_output(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    bool r = false;

    // Move finished frames from the request slots to the vf output queue.
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
        r = true;
    }

    // Don't request frames if we haven't sent any input yet.
    if (p->num_buffered + p->in_frameno == 0)
        return r;

    // Request new future frames as far as possible.
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

    return r;
}

static int filter_ext(struct vf_instance *vf, struct mp_image *mpi)
{
    struct vf_priv_s *p = vf->priv;
    int ret = 0;
    bool eof = !mpi;

    if (!p->out_node) {
        talloc_free(mpi);
        return -1;
    }

    MPSWAP(struct mp_image *, p->next_image, mpi);

    if (mpi) {
        // Turn PTS into frame duration (the pts field is abused for storing it)
        if (p->out_pts == MP_NOPTS_VALUE)
            p->out_pts = mpi->pts;
        mpi->pts = p->next_image ? p->next_image->pts - mpi->pts : 0;
    }

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

        // Make the input frame available to infiltGetFrame().
        if (mpi && locked_need_input(vf)) {
            p->frames_sent++;
            p->buffered[p->num_buffered++] = talloc_steal(p->buffered, mpi);
            mpi = NULL;
            pthread_cond_broadcast(&p->wakeup);
        }

        locked_read_output(vf);

        if (!mpi) {
            if (eof && p->frames_sent && !p->eof) {
                MP_VERBOSE(vf, "input EOF\n");
                p->eof = true;
                pthread_cond_broadcast(&p->wakeup);
            }
            break;
        }
        pthread_cond_wait(&p->wakeup, &p->lock);
    }
    pthread_mutex_unlock(&p->lock);
    return ret;
}

// Fetch 1 outout frame, or 0 if we probably need new input.
static int filter_out(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    int ret = 0;
    pthread_mutex_lock(&p->lock);
    while (1) {
        if (p->failed) {
            ret = -1;
            break;
        }
        if (locked_read_output(vf))
            break;
        // If the VS filter wants new input, there's no guarantee that we can
        // actually finish any time soon without feeding new input.
        if (!p->eof && locked_need_input(vf))
            break;
        pthread_cond_wait(&p->wakeup, &p->lock);
    }
    pthread_mutex_unlock(&p->lock);
    return ret;
}

static bool needs_input(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    bool r = false;
    pthread_mutex_lock(&p->lock);
    locked_read_output(vf);
    r = vf->num_out_queued < p->max_requests && locked_need_input(vf);
    pthread_mutex_unlock(&p->lock);
    return r;
}

static void VS_CC infiltInit(VSMap *in, VSMap *out, void **instanceData,
                             VSNode *node, VSCore *core, const VSAPI *vsapi)
{
    struct vf_instance *vf = *instanceData;
    struct vf_priv_s *p = vf->priv;
    // The number of frames of our input node is obviously unknown. The user
    // could for example seek any time, randomly "ending" the clip.
    // This specific value was suggested by the VapourSynth developer.
    int enough_for_everyone = INT_MAX / 16;

    // Note: this is called from createFilter, so no need for locking.

    VSVideoInfo fmt = {
        .format = p->vsapi->getFormatPreset(mp_to_vs(p->fmt_in.imgfmt), p->vscore),
        .width = p->fmt_in.w,
        .height = p->fmt_in.h,
        .numFrames = enough_for_everyone,
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
            MP_DBG(vf, "returning error on EOF/reset\n");
            break;
        }
        if (p->initializing) {
            MP_WARN(vf, "Frame requested during init! This is unsupported.\n"
                        "Returning black dummy frame with 0 duration.\n");
            ret = alloc_vs_frame(p, &vf->fmt_in);
            if (!ret) {
                p->vsapi->setFilterError("Could not allocate VS frame", frameCtx);
                break;
            }
            struct mp_image vsframe = map_vs_frame(p, ret, true);
            mp_image_clear(&vsframe, 0, 0, vf->fmt_in.w, vf->fmt_in.h);
            struct mp_image dummy = {0};
            mp_image_set_params(&dummy, &vf->fmt_in);
            set_vs_frame_props(p, ret, &dummy, 0, 1);
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
        if (frameno >= p->in_frameno + MP_TALLOC_AVAIL(p->buffered)) {
            // Too far in the future. Remove frames, so that the main thread can
            // queue new frames.
            if (p->num_buffered) {
                drain_oldest_buffered_frame(p);
                pthread_cond_broadcast(&p->wakeup);
                if (vf->chain->wakeup_callback)
                    vf->chain->wakeup_callback(vf->chain->wakeup_callback_ctx);
                continue;
            }
        }
        if (frameno >= p->in_frameno + p->num_buffered) {
            // If we think EOF was reached, don't wait for new input, and assume
            // the VS filter has reached EOF.
            if (p->eof) {
                p->shutdown = true;
                continue;
            }
        }
        if (frameno < p->in_frameno + p->num_buffered) {
            struct mp_image *img = p->buffered[frameno - p->in_frameno];
            ret = alloc_vs_frame(p, &img->params);
            if (!ret) {
                p->vsapi->setFilterError("Could not allocate VS frame", frameCtx);
                break;
            }
            struct mp_image vsframe = map_vs_frame(p, ret, true);
            mp_image_copy(&vsframe, img);
            int res = 1e6;
            int dur = img->pts * res + 0.5;
            set_vs_frame_props(p, ret, img, dur, res);
            break;
        }
        pthread_cond_wait(&p->wakeup, &p->lock);
    }
    pthread_cond_broadcast(&p->wakeup);
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
    p->initializing = false;
    p->shutdown = true;
    pthread_cond_broadcast(&p->wakeup);
    while (num_requested(p))
        pthread_cond_wait(&p->wakeup, &p->lock);
    pthread_mutex_unlock(&p->lock);

    MP_DBG(vf, "all requests terminated\n");

    if (p->in_node)
        p->vsapi->freeNode(p->in_node);
    if (p->out_node)
        p->vsapi->freeNode(p->out_node);
    p->in_node = p->out_node = NULL;

    p->drv->unload(vf);

    assert(!p->in_node_active);
    assert(num_requested(p) == 0); // async callback didn't return?

    p->shutdown = false;
    p->eof = false;
    p->frames_sent = 0;
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
    p->failed = false;

    MP_DBG(vf, "uninitialized.\n");
}

static int reinit_vs(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    VSMap *vars = NULL, *in = NULL, *out = NULL;
    int res = -1;

    destroy_vs(vf);

    MP_DBG(vf, "initializing...\n");
    p->initializing = true;

    if (p->drv->load_core(vf) < 0 || !p->vsapi || !p->vscore) {
        MP_FATAL(vf, "Could not get vapoursynth API handle.\n");
        goto error;
    }

    in = p->vsapi->createMap();
    out = p->vsapi->createMap();
    vars = p->vsapi->createMap();
    if (!in || !out || !vars)
        goto error;

    p->vsapi->createFilter(in, out, "Input", infiltInit, infiltGetFrame,
                           infiltFree, fmSerial, 0, vf, p->vscore);
    int vserr;
    p->in_node = p->vsapi->propGetNode(out, "clip", 0, &vserr);
    if (!p->in_node) {
        MP_FATAL(vf, "Could not get our own input node.\n");
        goto error;
    }

    if (p->vsapi->propSetNode(vars, "video_in", p->in_node, 0))
        goto error;

    p->vsapi->propSetInt(vars, "video_in_dw", p->fmt_in.d_w, 0);
    p->vsapi->propSetInt(vars, "video_in_dh", p->fmt_in.d_h, 0);
    p->vsapi->propSetFloat(vars, "container_fps", vf->chain->container_fps, 0);
    p->vsapi->propSetFloat(vars, "display_fps", vf->chain->display_fps, 0);

    if (p->drv->load(vf, vars) < 0)
        goto error;
    if (!p->out_node) {
        MP_FATAL(vf, "Could not get script output node.\n");
        goto error;
    }

    const VSVideoInfo *vi = p->vsapi->getVideoInfo(p->out_node);
    if (!isConstantFormat(vi)) {
        MP_FATAL(vf, "Video format is required to be constant.\n");
        goto error;
    }

    pthread_mutex_lock(&p->lock);
    p->initializing = false;
    pthread_mutex_unlock(&p->lock);
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
        .d_w = d_width,
        .d_h = d_height,
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

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(fmt);
    if (width % desc.align_x || height % desc.align_y) {
        MP_FATAL(vf, "VapourSynth does not allow unaligned/cropped video sizes.\n");
        destroy_vs(vf);
        return 0;
    }

    vf_rescale_dsize(&d_width, &d_height, width, height, vi->width, vi->height);

    return vf_next_config(vf, vi->width, vi->height, d_width, d_height, flags, fmt);
}

static int query_format(struct vf_instance *vf, unsigned int fmt)
{
    return mp_to_vs(fmt) != pfNone;
}

static int control(vf_instance_t *vf, int request, void *data)
{
    struct vf_priv_s *p = vf->priv;
    switch (request) {
    case VFCTRL_SEEK_RESET:
        if (p->out_node && reinit_vs(vf) < 0)
            return CONTROL_ERROR;
        return CONTROL_OK;
    }
    return CONTROL_UNKNOWN;
}

static void uninit(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    destroy_vs(vf);
    p->drv->uninit(vf);

    pthread_cond_destroy(&p->wakeup);
    pthread_mutex_destroy(&p->lock);
}
static int vf_open(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;
    if (p->drv->init(vf) < 0)
        return 0;
    if (!p->cfg_file || !p->cfg_file[0]) {
        MP_FATAL(vf, "'file' parameter must be set.\n");
        return 0;
    }
    talloc_steal(vf, p->cfg_file);
    p->cfg_file = mp_get_user_path(vf, vf->chain->global, p->cfg_file);

    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->wakeup, NULL);
    vf->reconfig = NULL;
    vf->config = config;
    vf->filter_ext = filter_ext;
    vf->filter_out = filter_out;
    vf->needs_input = needs_input;
    vf->query_format = query_format;
    vf->control = control;
    vf->uninit = uninit;
    p->max_requests = p->cfg_maxrequests;
    if (p->max_requests < 0)
        p->max_requests = av_cpu_count();
    MP_VERBOSE(vf, "using %d concurrent requests.\n", p->max_requests);
    int maxbuffer = p->cfg_maxbuffer * p->max_requests;
    p->buffered = talloc_array(vf, struct mp_image *, maxbuffer);
    p->requested = talloc_zero_array(vf, struct mp_image *, p->max_requests);
    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
static const m_option_t vf_opts_fields[] = {
    OPT_STRING("file", cfg_file, 0),
    OPT_INTRANGE("buffered-frames", cfg_maxbuffer, 0, 1, 9999, OPTDEF_INT(4)),
    OPT_CHOICE_OR_INT("concurrent-frames", cfg_maxrequests, 0, 1, 99,
                      ({"auto", -1}), OPTDEF_INT(-1)),
    {0}
};

#if HAVE_VAPOURSYNTH

#include <VSScript.h>

static int drv_vss_init(struct vf_instance *vf)
{
    if (!vsscript_init()) {
        MP_FATAL(vf, "Could not initialize VapourSynth scripting.\n");
        return -1;
    }
    return 0;
}

static void drv_vss_uninit(struct vf_instance *vf)
{
    vsscript_finalize();
}

static int drv_vss_load_core(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    // First load an empty script to get a VSScript, so that we get the vsapi
    // and vscore.
    if (vsscript_createScript(&p->se))
        return -1;
    p->vsapi = vsscript_getVSApi();
    p->vscore = vsscript_getCore(p->se);
    return 0;
}

static int drv_vss_load(struct vf_instance *vf, VSMap *vars)
{
    struct vf_priv_s *p = vf->priv;

    vsscript_setVariable(p->se, vars);

    if (vsscript_evaluateFile(&p->se, p->cfg_file, 0)) {
        MP_FATAL(vf, "Script evaluation failed:\n%s\n", vsscript_getError(p->se));
        return -1;
    }
    p->out_node = vsscript_getOutput(p->se, 0);
    return 0;
}

static void drv_vss_unload(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    if (p->se)
        vsscript_freeScript(p->se);
    p->se = NULL;
    p->vsapi = NULL;
    p->vscore = NULL;
}

static const struct script_driver drv_vss = {
    .init = drv_vss_init,
    .uninit = drv_vss_uninit,
    .load_core = drv_vss_load_core,
    .load = drv_vss_load,
    .unload = drv_vss_unload,
};

static int vf_open_vss(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;
    p->drv = &drv_vss;
    return vf_open(vf);
}

const vf_info_t vf_info_vapoursynth = {
    .description = "VapourSynth bridge (Python)",
    .name = "vapoursynth",
    .open = vf_open_vss,
    .priv_size = sizeof(struct vf_priv_s),
    .options = vf_opts_fields,
};

#endif

#if HAVE_VAPOURSYNTH_LAZY

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#if LUA_VERSION_NUM <= 501
#define mp_cpcall lua_cpcall
#define FUCKYOUOHGODWHY(L) lua_pushvalue(L, LUA_GLOBALSINDEX)
#else
// Curse whoever had this stupid idea. Curse whoever thought it would be a good
// idea not to include an emulated lua_cpcall() even more.
static int mp_cpcall (lua_State *L, lua_CFunction func, void *ud)
{
    lua_pushcfunction(L, func); // doesn't allocate in 5.2 (but does in 5.1)
    lua_pushlightuserdata(L, ud);
    return lua_pcall(L, 1, 0, 0);
}
// Hey, let's replace old mechanisms with something slightly different!
#define FUCKYOUOHGODWHY lua_pushglobaltable
#endif

static int drv_lazy_init(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    p->ls = luaL_newstate();
    if (!p->ls)
        return -1;
    luaL_openlibs(p->ls);
    p->vsapi = getVapourSynthAPI(VAPOURSYNTH_API_VERSION);
    p->vscore = p->vsapi ? p->vsapi->createCore(0) : NULL;
    if (!p->vscore) {
        MP_FATAL(vf, "Could not load VapourSynth.\n");
        lua_close(p->ls);
        return -1;
    }
    return 0;
}

static void drv_lazy_uninit(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;
    lua_close(p->ls);
    p->vsapi->freeCore(p->vscore);
}

static int drv_lazy_load_core(struct vf_instance *vf)
{
    // not needed
    return 0;
}

static struct vf_instance *get_vf(lua_State *L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "p"); // p
    struct vf_instance *vf = lua_touserdata(L, -1); // p
    lua_pop(L, 1); // -
    return vf;
}

static void vsmap_to_table(lua_State *L, int index, VSMap *map)
{
    struct vf_instance *vf = get_vf(L);
    struct vf_priv_s *p = vf->priv;
    const VSAPI *vsapi = p->vsapi;
    for (int n = 0; n < vsapi->propNumKeys(map); n++) {
        const char *key = vsapi->propGetKey(map, n);
        VSPropTypes t = vsapi->propGetType(map, key);
        switch (t) {
        case ptInt:
            lua_pushnumber(L, vsapi->propGetInt(map, key, 0, NULL));
            break;
        case ptFloat:
            lua_pushnumber(L, vsapi->propGetFloat(map, key, 0, NULL));
            break;
        case ptNode: {
            VSNodeRef *r = vsapi->propGetNode(map, key, 0, NULL);
            MP_TARRAY_APPEND(p, p->gc_noderef, p->num_gc_noderef, r);
            lua_pushlightuserdata(L, r);
            break;
        }
        default:
            luaL_error(L, "unknown map type");
        }
        lua_setfield(L, index, key);
    }
}

static VSMap *table_to_vsmap(lua_State *L, int index)
{
    struct vf_instance *vf = get_vf(L);
    struct vf_priv_s *p = vf->priv;
    const VSAPI *vsapi = p->vsapi;
    assert(index > 0);
    VSMap *map = vsapi->createMap();
    MP_TARRAY_APPEND(p, p->gc_map, p->num_gc_map, map);
    if (!map)
        luaL_error(L, "out of memory");
    lua_pushnil(L); // nil
    while (lua_next(L, index) != 0) { // key value
        if (lua_type(L, -2) != LUA_TSTRING) {
            luaL_error(L, "key must be a string, but got %s",
                       lua_typename(L, -2));
        }
        const char *key = lua_tostring(L, -2);
        switch (lua_type(L, -1)) {
        case LUA_TNUMBER: {
            // gross hack because we hate everything
            if (strncmp(key, "i_", 2) == 0) {
                vsapi->propSetInt(map, key + 2, lua_tointeger(L, -1), 0);
            } else {
                vsapi->propSetFloat(map, key, lua_tonumber(L, -1), 0);
            }
            break;
        }
        case LUA_TSTRING: {
            const char *s = lua_tostring(L, -1);
            vsapi->propSetData(map, key, s, strlen(s), 0);
            break;
        }
        case LUA_TLIGHTUSERDATA: { // assume it's VSNodeRef*
            VSNodeRef *node = lua_touserdata(L, -1);
            vsapi->propSetNode(map, key, node, 0);
            break;
        }
        default:
            luaL_error(L, "unknown type");
            break;
        }
        lua_pop(L, 1); // key
    }
    return map;
}

static int l_invoke(lua_State *L)
{
    struct vf_instance *vf = get_vf(L);
    struct vf_priv_s *p = vf->priv;
    const VSAPI *vsapi = p->vsapi;

    VSPlugin *plugin = vsapi->getPluginByNs(luaL_checkstring(L, 1), p->vscore);
    if (!plugin)
        luaL_error(L, "plugin not found");
    VSMap *map = table_to_vsmap(L, 3);
    VSMap *r = vsapi->invoke(plugin, luaL_checkstring(L, 2), map);
    MP_TARRAY_APPEND(p, p->gc_map, p->num_gc_map, r);
    if (!r)
        luaL_error(L, "?");
    const char *err = vsapi->getError(r);
    if (err)
        luaL_error(L, "error calling invoke(): %s", err);
    int err2 = 0;
    VSNodeRef *node = vsapi->propGetNode(r, "clip", 0, &err2);
    MP_TARRAY_APPEND(p, p->gc_noderef, p->num_gc_noderef, node);
    if (node)
        lua_pushlightuserdata(L, node);
    return 1;
}

struct load_ctx {
    struct vf_instance *vf;
    VSMap *vars;
    int status;
};

static int load_stuff(lua_State *L)
{
    struct load_ctx *ctx = lua_touserdata(L, -1);
    lua_pop(L, 1); // -
    struct vf_instance *vf = ctx->vf;
    struct vf_priv_s *p = vf->priv;

    // setup stuff; should be idempotent
    lua_pushlightuserdata(L, vf);
    lua_setfield(L, LUA_REGISTRYINDEX, "p"); // -
    lua_pushcfunction(L, l_invoke);
    lua_setglobal(L, "invoke");

    FUCKYOUOHGODWHY(L);
    vsmap_to_table(L, lua_gettop(L), ctx->vars);
    if (luaL_dofile(L, p->cfg_file))
        lua_error(L);
    lua_pop(L, 1);

    lua_getglobal(L, "video_out"); // video_out
    if (!lua_islightuserdata(L, -1))
        luaL_error(L, "video_out not set or has wrong type");
    p->out_node = p->vsapi->cloneNodeRef(lua_touserdata(L, -1));
    return 0;
}

static int drv_lazy_load(struct vf_instance *vf, VSMap *vars)
{
    struct vf_priv_s *p = vf->priv;
    struct load_ctx ctx = {vf, vars, 0};
    if (mp_cpcall(p->ls, load_stuff, &ctx)) {
        MP_FATAL(vf, "filter creation failed: %s\n", lua_tostring(p->ls, -1));
        lua_pop(p->ls, 1);
        ctx.status = -1;
    }
    assert(lua_gettop(p->ls) == 0);
    return ctx.status;
}

static void drv_lazy_unload(struct vf_instance *vf)
{
    struct vf_priv_s *p = vf->priv;

    for (int n = 0; n < p->num_gc_noderef; n++) {
        VSNodeRef *ref = p->gc_noderef[n];
        if (ref)
            p->vsapi->freeNode(ref);
    }
    p->num_gc_noderef = 0;
    for (int n = 0; n < p->num_gc_map; n++) {
        VSMap *map = p->gc_map[n];
        if (map)
            p->vsapi->freeMap(map);
    }
    p->num_gc_map = 0;
}

static const struct script_driver drv_lazy = {
    .init = drv_lazy_init,
    .uninit = drv_lazy_uninit,
    .load_core = drv_lazy_load_core,
    .load = drv_lazy_load,
    .unload = drv_lazy_unload,
};

static int vf_open_lazy(vf_instance_t *vf)
{
    struct vf_priv_s *p = vf->priv;
    p->drv = &drv_lazy;
    return vf_open(vf);
}

const vf_info_t vf_info_vapoursynth_lazy = {
    .description = "VapourSynth bridge (Lua)",
    .name = "vapoursynth-lazy",
    .open = vf_open_lazy,
    .priv_size = sizeof(struct vf_priv_s),
    .options = vf_opts_fields,
};

#endif
