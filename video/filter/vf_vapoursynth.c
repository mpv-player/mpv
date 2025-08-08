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
#include <limits.h>
#include <assert.h>

#include <VapourSynth4.h>
#include <VSScript4.h>

#include <libavutil/rational.h>
#include <libavutil/cpu.h>
#include <libplacebo/utils/libav.h>

#include "common/msg.h"
#include "filters/f_autoconvert.h"
#include "filters/f_utils.h"
#include "filters/filter_internal.h"
#include "filters/filter.h"
#include "filters/user_filters.h"
#include "options/m_option.h"
#include "options/path.h"
#include "osdep/threads.h"
#include "video/img_format.h"
#include "video/mp_image.h"
#include "video/sws_utils.h"

struct vapoursynth_opts {
    char *file;
    int maxbuffer;
    int maxrequests;
    char *user_data;

    const struct script_driver *drv;
};

struct priv {
    struct mp_log *log;
    struct vapoursynth_opts *opts;
    char *script_path;

    VSCore *vscore;
    const VSAPI *vsapi;
    VSNode *out_node;
    VSNode *in_node;

    const struct script_driver *drv;
    // drv_vss
    const VSSCRIPTAPI *vs_script_api;
    VSScript *vs_script;

    struct mp_filter *f;
    struct mp_pin *in_pin;

    // Format for which VS is currently configured.
    struct mp_image_params fmt_in;

    mp_mutex lock;
    mp_cond wakeup;

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

static bool get_valid_mp_regular_imgfmt(struct mp_regular_imgfmt *reg_fmt, int imgfmt) {
    if (!mp_get_regular_imgfmt(reg_fmt, imgfmt))
        return false;
    if (reg_fmt->component_pad > 0)
        return false;
    if (reg_fmt->num_planes != 3)
        return false;
    for (int n = 0; n < 3; n++) {
        if (reg_fmt->planes[n].num_components != 1)
            return false;
        if (reg_fmt->planes[n].components[0] != n + 1)
            return false;
    }
    return true;
}

static bool mp_to_vs(struct priv *p, VSVideoFormat *vsfmt, int imgfmt)
{
    struct mp_regular_imgfmt reg_fmt;
    if (!get_valid_mp_regular_imgfmt(&reg_fmt, imgfmt))
        return false;

    int rfmt_bits = reg_fmt.component_size * 8 + reg_fmt.component_pad;
    return p->vsapi->queryVideoFormat(vsfmt, cfYUV,
                                      reg_fmt.component_type == MP_COMPONENT_TYPE_FLOAT ? stFloat : stInteger,
                                      rfmt_bits, reg_fmt.chroma_xs, reg_fmt.chroma_ys, p->vscore);
}

static int mp_from_vs(const VSVideoFormat *vsfmt)
{
    if (vsfmt->colorFamily == cfYUV) {
        for (int imgfmt = IMGFMT_START + 1; imgfmt < IMGFMT_END; imgfmt++) {
            struct mp_regular_imgfmt reg_fmt;
            if (!get_valid_mp_regular_imgfmt(&reg_fmt, imgfmt))
                continue;

            int rfmt_bits = reg_fmt.component_size * 8 + reg_fmt.component_pad;
            if ((reg_fmt.component_type == MP_COMPONENT_TYPE_FLOAT) == (vsfmt->sampleType == stFloat) &&
                rfmt_bits == vsfmt->bitsPerSample &&
                reg_fmt.chroma_xs == vsfmt->subSamplingW &&
                reg_fmt.chroma_ys == vsfmt->subSamplingH)
            {
                return imgfmt;
            }
        }
    }
    return 0;
}

static void copy_mp_to_vs_frame_props_map(struct priv *p, VSMap *map,
                                          struct mp_image *img)
{
    struct mp_image_params *params = &img->params;
    p->vsapi->mapSetInt(map, "_SARNum", params->p_w, 0);
    p->vsapi->mapSetInt(map, "_SARDen", params->p_h, 0);
    if (params->repr.levels) {
        p->vsapi->mapSetInt(map, "_ColorRange",
                params->repr.levels == PL_COLOR_LEVELS_LIMITED, 0);
    }
    // The docs explicitly say it uses libavcodec values.
    p->vsapi->mapSetInt(map, "_ColorSpace",
            pl_system_to_av(params->repr.sys), 0);
    if (params->chroma_location) {
        // 0=left, 1=center, 2=topleft, 3=top, 4=bottomleft, 5=bottom.
        p->vsapi->mapSetInt(map, "_ChromaLocation",
                params->chroma_location - 1, 0);
    }
    char pict_type = 0;
    switch (img->pict_type) {
    case 1: pict_type = 'I'; break;
    case 2: pict_type = 'P'; break;
    case 3: pict_type = 'B'; break;
    }
    if (pict_type)
        p->vsapi->mapSetData(map, "_PictType", &pict_type, 1, dtUtf8, 0);
    int field = 0;
    if (img->fields & MP_IMGFIELD_INTERLACED)
        field = img->fields & MP_IMGFIELD_TOP_FIRST ? 2 : 1;
    p->vsapi->mapSetInt(map, "_FieldBased", field, 0);

    // Don't increase the reference count. It is not intended to be read externally,
    // and we know it will be alive when we retrieve it.
    p->vsapi->mapSetData(map, "_MP_IMAGE", (const char *)img, sizeof(*img), dtBinary, 0);
}

static int set_vs_frame_props(struct priv *p, VSFrame *frame,
                              struct mp_image *img, int dur_num, int dur_den)
{
    VSMap *map = p->vsapi->getFramePropertiesRW(frame);
    if (!map)
        return -1;
    p->vsapi->mapSetInt(map, "_DurationNum", dur_num, 0);
    p->vsapi->mapSetInt(map, "_DurationDen", dur_den, 0);
    copy_mp_to_vs_frame_props_map(p, map, img);
    return 0;
}

static VSFrame *alloc_vs_frame(struct priv *p, struct mp_image_params *fmt)
{
    VSVideoFormat vsfmt;
    if (mp_to_vs(p, &vsfmt, fmt->imgfmt))
        return p->vsapi->newVideoFrame(&vsfmt, fmt->w, fmt->h, NULL, p->vscore);

    return NULL;
}

static struct mp_image map_vs_frame(struct priv *p, const VSFrame *ref,
                                    bool w, struct mp_image *ref_image)
{
    const VSVideoFormat *fmt = p->vsapi->getVideoFrameFormat(ref);

    struct mp_image img = {0};
    if (ref_image)
        img = *ref_image;
    mp_image_setfmt(&img, mp_from_vs(fmt));
    mp_image_set_size(&img, p->vsapi->getFrameWidth(ref, 0),
                            p->vsapi->getFrameHeight(ref, 0));

    for (int n = 0; n < img.num_planes; n++) {
        if (w) {
            img.planes[n] = p->vsapi->getWritePtr((VSFrame *)ref, n);
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

static void VS_CC vs_frame_done(void *userData, const VSFrame *f, int n,
                                VSNode *node, const char *errorMsg)
{
    struct priv *p = userData;

    struct mp_image *res = NULL;
    if (f) {
        const VSMap *map = p->vsapi->getFramePropertiesRO(f);
        if (!map)
            MP_ERR(p, "Failed to get frame properties!");
        struct mp_image *mpi = NULL;
        if (map) {
            mpi = (void *)p->vsapi->mapGetData(map, "_MP_IMAGE", 0, NULL);
            if (!mpi)
                MP_ERR(p, "Failed to get mp_image attributes!");
        }
        struct mp_image img = map_vs_frame(p, f, false, mpi);
        img.pkt_duration = -1;
        if (mpi && (mpi->params.w != img.w || mpi->params.h != img.h))
            img.params.crop = (struct mp_rect){0, 0, img.w, img.h};
        if (map) {
            int err1, err2;
            int num = p->vsapi->mapGetInt(map, "_DurationNum", 0, &err1);
            int den = p->vsapi->mapGetInt(map, "_DurationDen", 0, &err2);
            if (!err1 && !err2)
                img.pkt_duration = num / (double)den;
        }
        if (img.pkt_duration < 0) {
            MP_ERR(p, "No PTS after filter at frame %d!\n", n);
        } else {
            img.nominal_fps = 1.0 / img.pkt_duration;
        }
        res = mp_image_new_copy(&img);
        p->vsapi->freeFrame(f);
    }

    mp_mutex_lock(&p->lock);

    // If these assertions fail, n is an unrequested frame (or filtered twice).
    mp_assert(n >= p->out_frameno && n < p->out_frameno + p->max_requests);
    int index = n - p->out_frameno;
    MP_TRACE(p, "filtered frame %d (%d)\n", n, index);
    mp_assert(p->requested[index] == &dummy_img);

    if (!res && !p->shutdown) {
        if (p->eof) {
            res = (struct mp_image *)&dummy_img_eof;
        } else {
            p->failed = true;
            MP_ERR(p, "Filter error at frame %d: %s\n", n, errorMsg);
        }
    }
    p->requested[index] = res;
    mp_cond_broadcast(&p->wakeup);
    mp_mutex_unlock(&p->lock);
    mp_filter_wakeup(p->f);
}

static void vf_vapoursynth_process(struct mp_filter *f)
{
    struct priv *p = f->priv;

    mp_mutex_lock(&p->lock);

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
                mp_cond_broadcast(&p->wakeup);
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
                    mp_cond_broadcast(&p->wakeup);
                    mp_filter_internal_mark_progress(f);
                    goto done;
                }
                mp_mutex_unlock(&p->lock);
                if (p->out_node)
                    destroy_vs(p);
                p->fmt_in = mpi->params;
                if (reinit_vs(p, mpi) < 0) {
                    MP_ERR(p, "could not init VS\n");
                    mp_frame_unref(&frame);
                    mp_filter_internal_mark_failed(f);
                    return;
                }
                mp_mutex_lock(&p->lock);
            }
            if (p->out_pts == MP_NOPTS_VALUE)
                p->out_pts = mpi->pts;
            p->frames_sent++;
            p->buffered[p->num_buffered++] = mpi;
            mp_cond_broadcast(&p->wakeup);
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
        mp_assert(p->eof);
        mp_mutex_unlock(&p->lock);
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
    mp_mutex_unlock(&p->lock);
}

static const VSFrame *VS_CC infiltGetFrame(int frameno, int activationReason,
    void *instanceData, void **frameData,
    VSFrameContext *frameCtx, VSCore *core,
    const VSAPI *vsapi)
{
    struct priv *p = instanceData;
    VSFrame *ret = NULL;

    mp_mutex_lock(&p->lock);
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
            struct mp_image vsframe = map_vs_frame(p, ret, true, NULL);
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
                mp_cond_broadcast(&p->wakeup);
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

            mp_mutex_unlock(&p->lock);
            struct mp_image vsframe = map_vs_frame(p, ret, true, NULL);
            mp_image_copy(&vsframe, img);
            int res = 1e6;
            int dur = img->pkt_duration * res + 0.5;
            set_vs_frame_props(p, ret, img, dur, res);
            mp_mutex_lock(&p->lock);
            break;
        }
        mp_cond_wait(&p->wakeup, &p->lock);
    }
    mp_cond_broadcast(&p->wakeup);
    mp_mutex_unlock(&p->lock);
    return ret;
}

static void VS_CC infiltFree(void *instanceData, VSCore *core, const VSAPI *vsapi)
{
    struct priv *p = instanceData;

    mp_mutex_lock(&p->lock);
    p->in_node_active = false;
    mp_cond_broadcast(&p->wakeup);
    mp_mutex_unlock(&p->lock);
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
    mp_mutex_lock(&p->lock);
    p->initializing = false;
    p->shutdown = true;
    mp_cond_broadcast(&p->wakeup);
    while (num_requested(p))
        mp_cond_wait(&p->wakeup, &p->lock);
    mp_mutex_unlock(&p->lock);

    MP_DBG(p, "all requests terminated\n");

    if (p->in_node)
        p->vsapi->freeNode(p->in_node);
    if (p->out_node)
        p->vsapi->freeNode(p->out_node);
    p->in_node = p->out_node = NULL;

    p->drv->unload(p);

    mp_assert(!p->in_node_active);
    mp_assert(num_requested(p) == 0); // async callback didn't return?

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
    VSMap *vars = NULL;
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

    // The number of frames of our input node is obviously unknown. The user
    // could for example seek any time, randomly "ending" the clip.
    // This specific value was suggested by the VapourSynth developer.
    int enough_for_everyone = INT_MAX / 16;

    VSVideoInfo vi_in = {
        .width = p->fmt_in.w,
        .height = p->fmt_in.h,
        .numFrames = enough_for_everyone,
    };
    if (!mp_to_vs(p, &vi_in.format, p->fmt_in.imgfmt)) {
        MP_FATAL(p, "Unsupported input format.\n");
        goto error;
    }

    p->in_node = p->vsapi->createVideoFilter2("Input", &vi_in, infiltGetFrame, infiltFree,
                                              fmParallel, NULL, 0, p, p->vscore);
    if (!p->in_node) {
        MP_FATAL(p, "Could not get our own input node.\n");
        goto error;
    }
    p->in_node_active = true;

    vars = p->vsapi->createMap();
    if (!vars)
        goto error;

    if (p->vsapi->mapSetNode(vars, "video_in", p->in_node, 0))
        goto error;

    int d_w, d_h;
    mp_image_params_get_dsize(&p->fmt_in, &d_w, &d_h);

    p->vsapi->mapSetInt(vars, "video_in_dw", d_w, 0);
    p->vsapi->mapSetInt(vars, "video_in_dh", d_h, 0);

    struct mp_stream_info *info = mp_filter_find_stream_info(p->f);
    double container_fps = input->nominal_fps;
    double display_fps = 0;
    int64_t display_res[2] = {0};
    if (info) {
        if (info->get_display_fps)
            display_fps = info->get_display_fps(info);
        if (info->get_display_res) {
            int tmp[2] = {0};
            info->get_display_res(info, tmp);
            display_res[0] = tmp[0];
            display_res[1] = tmp[1];
        }
    }
    p->vsapi->mapSetFloat(vars, "container_fps", container_fps, 0);
    p->vsapi->mapSetFloat(vars, "display_fps", display_fps, 0);
    p->vsapi->mapSetIntArray(vars, "display_res", display_res, 2);
    p->vsapi->mapSetData(vars, "user_data", p->opts->user_data, -1, dtUtf8, 0);

    if (p->drv->load(p, vars) < 0)
        goto error;
    if (!p->out_node) {
        MP_FATAL(p, "Could not get script output node.\n");
        goto error;
    }

    const VSVideoInfo *vi_out = p->vsapi->getVideoInfo(p->out_node);
    if (!mp_from_vs(&vi_out->format)) {
        MP_FATAL(p, "Unsupported output format.\n");
        goto error;
    }

    mp_mutex_lock(&p->lock);
    p->initializing = false;
    mp_mutex_unlock(&p->lock);
    MP_DBG(p, "initialized.\n");
    res = 0;
error:
    if (p->vsapi) {
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

    mp_cond_destroy(&p->wakeup);
    mp_mutex_destroy(&p->lock);

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

    mp_mutex_init(&p->lock);
    mp_cond_init(&p->wakeup);

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

    for (int imgfmt = IMGFMT_START + 1; imgfmt < IMGFMT_END; imgfmt++) {
        // due to the lack of access to VapourSynth at this point, the formats
        // added to autoconvert is a superset of what's actually needed
        struct mp_regular_imgfmt reg_fmt;
        if (get_valid_mp_regular_imgfmt(&reg_fmt, imgfmt))
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
    {"user-data", OPT_STRING(user_data), OPTDEF_STR("")},
    {0}
};

static int drv_vss_init(struct priv *p)
{
    p->vs_script_api = getVSScriptAPI(VSSCRIPT_API_VERSION);
    if (!p->vs_script_api) {
        MP_FATAL(p, "Could not initialize VapourSynth scripting.\n");
        return -1;
    }
    return 0;
}

static void drv_vss_uninit(struct priv *p)
{
    p->vs_script_api = NULL;
}

static int drv_vss_load_core(struct priv *p)
{
    // First load an empty script to get a VSScript, so that we get the vsapi
    // and vscore.
    p->vs_script = p->vs_script_api->createScript(NULL);
    if (!p->vs_script)
        return -1;
    p->vsapi = p->vs_script_api->getVSAPI(VAPOURSYNTH_API_VERSION);
    p->vscore = p->vs_script_api->getCore(p->vs_script);
    return 0;
}

static int drv_vss_load(struct priv *p, VSMap *vars)
{
    p->vs_script_api->setVariables(p->vs_script, vars);

    if (p->vs_script_api->evaluateFile(p->vs_script, p->script_path)) {
        MP_FATAL(p, "Script evaluation failed:\n%s\n", p->vs_script_api->getError(p->vs_script));
        return -1;
    }
    p->out_node = p->vs_script_api->getOutputNode(p->vs_script, 0);
    return 0;
}

static void drv_vss_unload(struct priv *p)
{
    if (p->vs_script)
        p->vs_script_api->freeScript(p->vs_script);
    p->vsapi = NULL;
    p->vscore = NULL;
    p->vs_script = NULL;
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
