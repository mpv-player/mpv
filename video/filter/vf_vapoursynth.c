/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
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
#include "filters/f_autoconvert.h"
#include "filters/f_utils.h"
#include "filters/filter.h"
#include "filters/filter_internal.h"
#include "filters/user_filters.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"

struct vapoursynth_opts {
    char *file;
    int maxbuffer;
    int maxrequests;

    const struct script_driver *drv;
};

struct priv {
    struct mp_log *log;
    struct vapoursynth_opts *opts;
    char *script_path;

    VSCore *vscore;
    const VSAPI *vsapi;
    VSNodeRef *out_node;
    VSNodeRef *in_node;

    const struct script_driver *drv;
    // drv_vss
    bool vs_initialized;
    struct VSScript *se;

    struct mp_filter *f;
    struct mp_pin *in_pin;

    // Format for which VS is currently configured.
    struct mp_image_params fmt_in;

    pthread_mutex_t lock;
    pthread_cond_t wakeup;

    // --- the following members are all protected by lock
    struct mp_image **buffered; // oldest image first
    int num_buffered;
    int in_frameno;             // frame number of buffered[0] (the oldest)
    int requested_frameno;      // last frame number for which we woke up core
    int out_frameno;            // frame number of first requested/ready frame
    double out_pts;             // pts corresponding to first requested/ready frame
    struct mp_image **requested;// frame callback results (can point to dummy_img)
                                // requested[0] is the frame to return first
    int max_requests;           // upper bound for requested[] array
    bool failed;                // frame callback returned with an error
    bool shutdown;              // ask node to return
    bool eof;                   // drain remaining data
    int64_t frames_sent;        // total nr. of frames ever added to input queue
    bool initializing;          // filters are being built
    bool in_node_active;        // node might still be called
};

// priv->requested[n] points to this if a request for frame n is in-progress
static const struct mp_image dummy_img;
// or if a request failed during EOF/reinit draining
static const struct mp_image dummy_img_eof;

static void destroy_vs(struct priv *p);
static int reinit_vs(struct priv *p, struct mp_image *input);

struct script_driver {
    int (*init)(struct priv *p);                // first time init
    void (*uninit)(struct priv *p);             // last time uninit
    int (*load_core)(struct priv *p);           // make vsapi/vscore available
    int (*load)(struct priv *p, VSMap *vars);   // also sets p->out_node
    void (*unload)(struct priv *p);             // unload script and maybe vs
};

struct mpvs_fmt {
    VSPresetFormat vs;
    int bits, xs, ys;
};

static const struct mpvs_fmt mpvs_fmt_table[] = {
    {pfYUV420P8,  8,  1, 1},
    {pfYUV420P9,  9,  1, 1},
    {pfYUV420P10, 10, 1, 1},
    {pfYUV420P16, 16, 1, 1},
    {pfYUV422P8,  8,  1, 0},
    {pfYUV422P9,  9,  1, 0},
    {pfYUV422P10, 10, 1, 0},
    {pfYUV422P16, 16, 1, 0},
    {pfYUV410P8,  8,  2, 2},
    {pfYUV411P8,  8,  2, 0},
    {pfYUV440P8,  8,  0, 1},
    {pfYUV444P8,  8,  0, 0},
    {pfYUV444P9,  9,  0, 0},
    {pfYUV444P10, 10, 0, 0},
    {pfYUV444P16, 16, 0, 0},
    {pfNone}
};

static bool compare_fmt(int imgfmt, const struct mpvs_fmt *vs)
{
    struct mp_regular_imgfmt rfmt;
    if (!mp_get_regular_imgfmt(&rfmt, imgfmt))
        return false;
    if (rfmt.component_pad > 0)
        return false;
    if (rfmt.chroma_xs != vs->xs || rfmt.chroma_ys != vs->ys)
        return false;
    if (rfmt.component_size * 8 + rfmt.component_pad != vs->bits)
        return false;
    if (rfmt.num_planes != 3)
        return false;
    for (int n = 0; n < 3; n++) {
        if (rfmt.planes[n].num_components != 1)
            return false;
        if (rfmt.planes[n].components[0] != n + 1)
            return false;
    }
    return true;
}

static VSPresetFormat mp_to_vs(int imgfmt)
{
    for (int n = 0; mpvs_fmt_table[n].bits; n++) {
        const struct mpvs_fmt *vsentry = &mpvs_fmt_table[n];
        if (compare_fmt(imgfmt, vsentry))
            return vsentry->vs;
    }
    return pfNone;
}

static int mp_from_vs(VSPresetFormat vs)
{
    for (int n = 0; mpvs_fmt_table[n].bits; n++) {
        const struct mpvs_fmt *vsentry = &mpvs_fmt_table[n];
        if (vsentry->vs == vs) {
            for (int imgfmt = IMGFMT_START; imgfmt < IMGFMT_END; imgfmt++) {
                if (compare_fmt(imgfmt, vsentry))
                    return imgfmt;
            }
            break;
        }
    }
    return 0;
}

static void copy_mp_to_vs_frame_props_map(struct priv *p, VSMap *map,
                                          struct mp_image *img)
{
    struct mp_image_params *params = &img->params;
    p->vsapi->propSetInt(map, "_SARNum", params->p_w, 0);
    p->vsapi->propSetInt(map, "_SARDen", params->p_h, 0);
    if (params->color.levels) {
        p->vsapi->propSetInt(map, "_ColorRange",
                params->color.levels == MP_CSP_LEVELS_TV, 0);
    }
    // The docs explicitly say it uses libavcodec values.
    p->vsapi->propSetInt(map, "_ColorSpace",
            mp_csp_to_avcol_spc(params->color.space), 0);
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

static int set_vs_frame_props(struct priv *p, VSFrameRef *frame,
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

static VSFrameRef *alloc_vs_frame(struct priv *p, struct mp_image_params *fmt)
{
    const VSFormat *vsfmt =
        p->vsapi->getFormatPreset(mp_to_vs(fmt->imgfmt), p->vscore);
    return p->vsapi->newVideoFrame(vsfmt, fmt->w, fmt->h, NULL, p->vscore);
}

static struct mp_image map_vs_frame(struct priv *p, const VSFrameRef *ref,
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

static void drain_oldest_buffered_frame(struct priv *p)
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
    struct priv *p = userData;

    struct mp_image *res = NULL;
    if (f) {
        struct mp_image img = map_vs_frame(p, f, false);
        struct mp_image dummy = {.params = p->fmt_in};
        mp_image_copy_attributes(&img, &dummy);
        img.pkt_duration = -1;
        const VSMap *map = p->vsapi->getFramePropsRO(f);
        if (map) {
            int err1, err2;
            int num = p->vsapi->propGetInt(map, "_DurationNum", 0, &err1);
            int den = p->vsapi->propGetInt(map, "_DurationDen", 0, &err2);
            if (!err1 && !err2)
                img.pkt_duration = num / (double)den;
        }
        if (img.pkt_duration < 0)
            MP_ERR(p, "No PTS after filter at frame %d!\n", n);
        res = mp_image_new_copy(&img);
        p->vsapi->freeFrame(f);
    }

    pthread_mutex_lock(&p->lock);

    // If these assertions fail, n is an unrequested frame (or filtered twice).
    assert(n >= p->out_frameno && n < p->out_frameno + p->max_requests);
    int index = n - p->out_frameno;
    MP_TRACE(p, "filtered frame %d (%d)\n", n, index);
    assert(p->requested[index] == &dummy_img);

    if (!res && !p->shutdown) {
        if (p->eof) {
            res = (struct mp_image *)&dummy_img_eof;
        } else {
            p->failed = true;
            MP_ERR(p, "Filter error at frame %d: %s\n", n, errorMsg);
        }
    }
    p->requested[index] = res;
    pthread_cond_broadcast(&p->wakeup);
    pthread_mutex_unlock(&p->lock);
    mp_filter_wakeup(p->f);
}

static void vf_vapoursynth_process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    pthread_mutex_lock(&p->lock);

    if (p->failed) {
        // Not sure what we do on errors, but at least don't deadlock.
        MP_ERR(f, "failed, no action taken\n");
        mp_filter_internal_mark_failed(f);
        goto done;
    }

    // Read input and pass it to the input queue VS reads.
    while (p->num_buffered < MP_TALLOC_AVAIL(p->buffered) && !p->eof) {
        // Note: this requests new input frames even if no output was ever
        // requested. Normally this is not how mp_filter works, but since VS
        // works asynchronously, it's probably ok.
        struct mp_frame frame = mp_pin_out_read(p->in_pin);
        if (frame.type == MP_FRAME_EOF) {
            if (p->out_node && !p->eof) {
                MP_VERBOSE(p, "initiate EOF\n");
                p->eof = true;
                pthread_cond_broadcast(&p->wakeup);
            }
            if (!p->out_node && mp_pin_in_needs_data(f->ppins[1])) {
                MP_VERBOSE(p, "return EOF\n");
                mp_pin_in_write(f->ppins[1], frame);
            } else {
                // Keep it until we can propagate it.
                mp_pin_out_unread(p->in_pin, frame);
                break;
            }
        } else if (frame.type == MP_FRAME_VIDEO) {
            struct mp_image *mpi = frame.data;
            // Init VS script, or reinit it to change video format. (This
            // includes derived parameters we pass manually to the script.)
            if (!p->out_node || mpi->imgfmt != p->fmt_in.imgfmt ||
                mpi->w != p->fmt_in.w || mpi->h != p->fmt_in.h ||
                mpi->params.p_w != p->fmt_in.p_w ||
                mpi->params.p_h != p->fmt_in.p_h)
            {
                if (p->out_node) {
                    // Drain still buffered frames.
                    MP_VERBOSE(p, "draining VS for format change\n");
                    mp_pin_out_unread(p->in_pin, frame);
                    p->eof = true;
                    pthread_cond_broadcast(&p->wakeup);
                    mp_filter_internal_mark_progress(f);
                    goto done;
                }
                pthread_mutex_unlock(&p->lock);
                if (p->out_node)
                    destroy_vs(p);
                p->fmt_in = mpi->params;
                if (reinit_vs(p, mpi) < 0) {
                    MP_ERR(p, "could not init VS\n");
                    mp_frame_unref(&frame);
                    mp_filter_internal_mark_failed(f);
                    return;
                }
                pthread_mutex_lock(&p->lock);
            }
            if (p->out_pts == MP_NOPTS_VALUE)
                p->out_pts = mpi->pts;
            p->frames_sent++;
            p->buffered[p->num_buffered++] = mpi;
            pthread_cond_broadcast(&p->wakeup);
        } else if (frame.type != MP_FRAME_NONE) {
            MP_ERR(p, "discarding unknown frame type\n");
            mp_frame_unref(&frame);
            goto done;
        } else {
            break; // no new data available
        }
    }

    // Read output and return them from the VS output queue.
    if (mp_pin_in_needs_data(f->ppins[1]) && p->requested[0] &&
        p->requested[0] != &dummy_img &&
        p->requested[0] != &dummy_img_eof)
    {
        struct mp_image *out = p->requested[0];

        out->pts = p->out_pts;
        if (p->out_pts != MP_NOPTS_VALUE && out->pkt_duration >= 0)
            p->out_pts += out->pkt_duration;

        mp_pin_in_write(f->ppins[1], MAKE_FRAME(MP_FRAME_VIDEO, out));

        for (int n = 0; n < p->max_requests - 1; n++)
            p->requested[n] = p->requested[n + 1];
        p->requested[p->max_requests - 1] = NULL;
        p->out_frameno++;
    }

    // This happens on EOF draining and format changes.
    if (p->requested[0] == &dummy_img_eof) {
        MP_VERBOSE(p, "finishing up\n");
        assert(p->eof);
        pthread_mutex_unlock(&p->lock);
        destroy_vs(p);
        mp_filter_internal_mark_progress(f);
        return;
    }

    // Don't request frames if we haven't sent any input yet.
    if (p->frames_sent && p->out_node) {
        // Request new future frames as far as possible.
        for (int n = 0; n < p->max_requests; n++) {
            if (!p->requested[n]) {
                // Note: this assumes getFrameAsync() will never call
                //       infiltGetFrame (if it does, we would deadlock)
                p->requested[n] = (struct mp_image *)&dummy_img;
                p->failed = false;
                MP_TRACE(p, "requesting frame %d (%d)\n", p->out_frameno + n, n);
                p->vsapi->getFrameAsync(p->out_frameno + n, p->out_node,
                                        vs_frame_done, p);
            }
        }
    }

done:
    pthread_mutex_unlock(&p->lock);
}

static void VS_CC infiltInit(VSMap *in, VSMap *out, void **instanceData,
                             VSNode *node, VSCore *core, const VSAPI *vsapi)
{
    struct priv *p = *instanceData;
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
    struct priv *p = *instanceData;
    VSFrameRef *ret = NULL;

    pthread_mutex_lock(&p->lock);
    MP_TRACE(p, "VS asking for frame %d (at %d)\n", frameno, p->in_frameno);
    while (1) {
        if (p->shutdown) {
            p->vsapi->setFilterError("EOF or filter reset/uninit", frameCtx);
            MP_DBG(p, "returning error on reset/uninit\n");
            break;
        }
        if (p->initializing) {
            MP_WARN(p, "Frame requested during init! This is unsupported.\n"
                        "Returning black dummy frame with 0 duration.\n");
            ret = alloc_vs_frame(p, &p->fmt_in);
            if (!ret) {
                p->vsapi->setFilterError("Could not allocate VS frame", frameCtx);
                break;
            }
            struct mp_image vsframe = map_vs_frame(p, ret, true);
            mp_image_clear(&vsframe, 0, 0, p->fmt_in.w, p->fmt_in.h);
            struct mp_image dummy = {0};
            mp_image_set_params(&dummy, &p->fmt_in);
            set_vs_frame_props(p, ret, &dummy, 0, 1);
            break;
        }
        if (frameno < p->in_frameno) {
            char msg[180];
            snprintf(msg, sizeof(msg),
                "Frame %d requested, but only have frames starting from %d. "
                "Try increasing the buffered-frames suboption.",
                frameno, p->in_frameno);
            MP_FATAL(p, "%s\n", msg);
            p->vsapi->setFilterError(msg, frameCtx);
            break;
        }
        if (frameno >= p->in_frameno + MP_TALLOC_AVAIL(p->buffered)) {
            // Too far in the future. Remove frames, so that the main thread can
            // queue new frames.
            if (p->num_buffered) {
                drain_oldest_buffered_frame(p);
                pthread_cond_broadcast(&p->wakeup);
                mp_filter_wakeup(p->f);
                continue;
            }
        }
        if (frameno >= p->in_frameno + p->num_buffered) {
            // If there won't be any new frames, abort the request.
            if (p->eof) {
                p->vsapi->setFilterError("EOF or filter EOF/reinit", frameCtx);
                MP_DBG(p, "returning error on EOF/reinit\n");
                break;
            }
            // Request more frames.
            if (p->requested_frameno <= p->in_frameno + p->num_buffered) {
                p->requested_frameno = p->in_frameno + p->num_buffered + 1;
                mp_filter_wakeup(p->f);
            }
        } else {
            struct mp_image *img = p->buffered[frameno - p->in_frameno];
            ret = alloc_vs_frame(p, &img->params);
            if (!ret) {
                p->vsapi->setFilterError("Could not allocate VS frame", frameCtx);
                break;
            }

            pthread_mutex_unlock(&p->lock);
            struct mp_image vsframe = map_vs_frame(p, ret, true);
            mp_image_copy(&vsframe, img);
            int res = 1e6;
            int dur = img->pkt_duration * res + 0.5;
            set_vs_frame_props(p, ret, img, dur, res);
            pthread_mutex_lock(&p->lock);
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
    struct priv *p = instanceData;

    pthread_mutex_lock(&p->lock);
    p->in_node_active = false;
    pthread_cond_broadcast(&p->wakeup);
    pthread_mutex_unlock(&p->lock);
}

// number of getAsyncFrame calls in progress
// must be called with p->lock held
static int num_requested(struct priv *p)
{
    int r = 0;
    for (int n = 0; n < p->max_requests; n++)
        r += p->requested[n] == &dummy_img;
    return r;
}

static void destroy_vs(struct priv *p)
{
    if (!p->out_node && !p->initializing)
        return;

    MP_DBG(p, "destroying VS filters\n");

    // Wait until our frame callbacks return.
    pthread_mutex_lock(&p->lock);
    p->initializing = false;
    p->shutdown = true;
    pthread_cond_broadcast(&p->wakeup);
    while (num_requested(p))
        pthread_cond_wait(&p->wakeup, &p->lock);
    pthread_mutex_unlock(&p->lock);

    MP_DBG(p, "all requests terminated\n");

    if (p->in_node)
        p->vsapi->freeNode(p->in_node);
    if (p->out_node)
        p->vsapi->freeNode(p->out_node);
    p->in_node = p->out_node = NULL;

    p->drv->unload(p);

    assert(!p->in_node_active);
    assert(num_requested(p) == 0); // async callback didn't return?

    p->shutdown = false;
    p->eof = false;
    p->frames_sent = 0;
    // Kill filtered images that weren't returned yet
    for (int n = 0; n < p->max_requests; n++) {
        if (p->requested[n] != &dummy_img_eof)
            mp_image_unrefp(&p->requested[n]);
        p->requested[n] = NULL;
    }
    // Kill queued frames too
    for (int n = 0; n < p->num_buffered; n++)
        talloc_free(p->buffered[n]);
    p->num_buffered = 0;
    p->out_frameno = p->in_frameno = 0;
    p->requested_frameno = 0;
    p->failed = false;

    MP_DBG(p, "uninitialized.\n");
}

static int reinit_vs(struct priv *p, struct mp_image *input)
{
    VSMap *vars = NULL, *in = NULL, *out = NULL;
    int res = -1;

    destroy_vs(p);

    MP_DBG(p, "initializing...\n");

    struct mp_imgfmt_desc desc = mp_imgfmt_get_desc(p->fmt_in.imgfmt);
    if (p->fmt_in.w % desc.align_x || p->fmt_in.h % desc.align_y) {
        MP_FATAL(p, "VapourSynth does not allow unaligned/cropped video sizes.\n");
        return -1;
    }

    p->initializing = true;
    p->out_pts = MP_NOPTS_VALUE;

    if (p->drv->load_core(p) < 0 || !p->vsapi || !p->vscore) {
        MP_FATAL(p, "Could not get vapoursynth API handle.\n");
        goto error;
    }

    in = p->vsapi->createMap();
    out = p->vsapi->createMap();
    vars = p->vsapi->createMap();
    if (!in || !out || !vars)
        goto error;

    p->vsapi->createFilter(in, out, "Input", infiltInit, infiltGetFrame,
                           infiltFree, fmSerial, 0, p, p->vscore);
    int vserr;
    p->in_node = p->vsapi->propGetNode(out, "clip", 0, &vserr);
    if (!p->in_node) {
        MP_FATAL(p, "Could not get our own input node.\n");
        goto error;
    }

    if (p->vsapi->propSetNode(vars, "video_in", p->in_node, 0))
        goto error;

    int d_w, d_h;
    mp_image_params_get_dsize(&p->fmt_in, &d_w, &d_h);

    p->vsapi->propSetInt(vars, "video_in_dw", d_w, 0);
    p->vsapi->propSetInt(vars, "video_in_dh", d_h, 0);

    struct mp_stream_info *info = mp_filter_find_stream_info(p->f);
    double container_fps = input->nominal_fps;
    double display_fps = 0;
    if (info) {
        if (info->get_display_fps)
            display_fps = info->get_display_fps(info);
    }
    p->vsapi->propSetFloat(vars, "container_fps", container_fps, 0);
    p->vsapi->propSetFloat(vars, "display_fps", display_fps, 0);

    if (p->drv->load(p, vars) < 0)
        goto error;
    if (!p->out_node) {
        MP_FATAL(p, "Could not get script output node.\n");
        goto error;
    }

    const VSVideoInfo *vi = p->vsapi->getVideoInfo(p->out_node);
    if (!mp_from_vs(vi->format->id)) {
        MP_FATAL(p, "Unsupported output format.\n");
        goto error;
    }

    pthread_mutex_lock(&p->lock);
    p->initializing = false;
    pthread_mutex_unlock(&p->lock);
    MP_DBG(p, "initialized.\n");
    res = 0;
error:
    if (p->vsapi) {
        p->vsapi->freeMap(in);
        p->vsapi->freeMap(out);
        p->vsapi->freeMap(vars);
    }
    if (res < 0)
        destroy_vs(p);
    return res;
}

static void vf_vapoursynth_reset(struct mp_filter *f)
{
    struct priv *p = f->priv;

    destroy_vs(p);
}

static void vf_vapoursynth_destroy(struct mp_filter *f)
{
    struct priv *p = f->priv;

    destroy_vs(p);
    p->drv->uninit(p);

    pthread_cond_destroy(&p->wakeup);
    pthread_mutex_destroy(&p->lock);

    mp_filter_free_children(f);
}

static const struct mp_filter_info vf_vapoursynth_filter = {
    .name = "vapoursynth",
    .process = vf_vapoursynth_process,
    .reset = vf_vapoursynth_reset,
    .destroy = vf_vapoursynth_destroy,
    .priv_size = sizeof(struct priv),
};

static struct mp_filter *vf_vapoursynth_create(struct mp_filter *parent,
                                               void *options)
{
    struct mp_filter *f = mp_filter_create(parent, &vf_vapoursynth_filter);
    if (!f) {
        talloc_free(options);
        return NULL;
    }

    // In theory, we could allow multiple inputs and outputs, but since this
    // wrapper is for --vf only, we don't.
    mp_filter_add_pin(f, MP_PIN_IN, "in");
    mp_filter_add_pin(f, MP_PIN_OUT, "out");

    struct priv *p = f->priv;
    p->opts = talloc_steal(p, options);
    p->log = f->log;
    p->drv = p->opts->drv;
    p->f = f;

    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->wakeup, NULL);

    if (!p->opts->file || !p->opts->file[0]) {
        MP_FATAL(p, "'file' parameter must be set.\n");
        goto error;
    }
    p->script_path = mp_get_user_path(p, f->global, p->opts->file);

    p->max_requests = p->opts->maxrequests;
    if (p->max_requests < 0)
        p->max_requests = av_cpu_count();
    MP_VERBOSE(p, "using %d concurrent requests.\n", p->max_requests);
    int maxbuffer = p->opts->maxbuffer * p->max_requests;
    p->buffered = talloc_array(p, struct mp_image *, maxbuffer);
    p->requested = talloc_zero_array(p, struct mp_image *, p->max_requests);

    struct mp_autoconvert *conv = mp_autoconvert_create(f);
    if (!conv)
        goto error;

    for (int n = 0; mpvs_fmt_table[n].bits; n++) {
        int imgfmt = mp_from_vs(mpvs_fmt_table[n].vs);
        if (imgfmt)
            mp_autoconvert_add_imgfmt(conv, imgfmt, 0);
    }

    struct mp_filter *dur = mp_compute_frame_duration_create(f);
    if (!dur)
        goto error;

    mp_pin_connect(conv->f->pins[0], f->ppins[0]);
    mp_pin_connect(dur->pins[0], conv->f->pins[1]);
    p->in_pin = dur->pins[1];

    if (p->drv->init(p) < 0)
        goto error;

    return f;

error:
    talloc_free(f);
    return NULL;
}


#define OPT_BASE_STRUCT struct vapoursynth_opts
static const m_option_t vf_opts_fields[] = {
    {"file", OPT_STRING(file), .flags = M_OPT_FILE},
    {"buffered-frames", OPT_INT(maxbuffer), M_RANGE(1, 9999),
        OPTDEF_INT(4)},
    {"concurrent-frames", OPT_CHOICE(maxrequests, {"auto", -1}),
        M_RANGE(1, 99), OPTDEF_INT(-1)},
    {0}
};

#include <VSScript.h>

static int drv_vss_init(struct priv *p)
{
    if (!vsscript_init()) {
        MP_FATAL(p, "Could not initialize VapourSynth scripting.\n");
        return -1;
    }
    p->vs_initialized = true;
    return 0;
}

static void drv_vss_uninit(struct priv *p)
{
    if (p->vs_initialized)
        vsscript_finalize();
    p->vs_initialized = false;
}

static int drv_vss_load_core(struct priv *p)
{
    // First load an empty script to get a VSScript, so that we get the vsapi
    // and vscore.
    if (vsscript_createScript(&p->se))
        return -1;
    p->vsapi = vsscript_getVSApi();
    p->vscore = vsscript_getCore(p->se);
    return 0;
}

static int drv_vss_load(struct priv *p, VSMap *vars)
{
    vsscript_setVariable(p->se, vars);

    if (vsscript_evaluateFile(&p->se, p->script_path, 0)) {
        MP_FATAL(p, "Script evaluation failed:\n%s\n", vsscript_getError(p->se));
        return -1;
    }
    p->out_node = vsscript_getOutput(p->se, 0);
    return 0;
}

static void drv_vss_unload(struct priv *p)
{
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

const struct mp_user_filter_entry vf_vapoursynth = {
    .desc = {
        .description = "VapourSynth bridge",
        .name = "vapoursynth",
        .priv_size = sizeof(OPT_BASE_STRUCT),
        .priv_defaults = &(const OPT_BASE_STRUCT){
            .drv = &drv_vss,
        },
        .options = vf_opts_fields,
    },
    .create = vf_vapoursynth_create,
};
