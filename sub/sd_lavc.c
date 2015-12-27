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

#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavutil/intreadwrite.h>

#include "config.h"

#include "talloc.h"
#include "common/msg.h"
#include "common/av_common.h"
#include "demux/stheader.h"
#include "options/options.h"
#include "video/mp_image.h"
#include "sd.h"
#include "dec_sub.h"

#define MAX_QUEUE 4

struct sub {
    bool valid;
    AVSubtitle avsub;
    int count;
    struct sub_bitmap *inbitmaps;
    struct osd_bmp_indexed *imgs;
    double pts;
    double endpts;
    int64_t id;
};

struct seekpoint {
    double pts;
    double endpts;
};

struct sd_lavc_priv {
    AVCodecContext *avctx;
    struct sub subs[MAX_QUEUE]; // most recent event first
    struct sub_bitmap *outbitmaps;
    int64_t displayed_id;
    int64_t new_id;
    struct mp_image_params video_params;
    double current_pts;
    struct seekpoint *seekpoints;
    int num_seekpoints;
};

static void get_resolution(struct sd *sd, int wh[2])
{
    struct sd_lavc_priv *priv = sd->priv;
    enum AVCodecID codec = priv->avctx->codec_id;
    int *w = &wh[0], *h = &wh[1];
    *w = priv->avctx->width;
    *h = priv->avctx->height;
    if (codec == AV_CODEC_ID_DVD_SUBTITLE) {
        if (*w <= 0 || *h <= 0) {
            *w = priv->video_params.w;
            *h = priv->video_params.h;
        }
        /* XXX Although the video frame is some size, the SPU frame is
           always maximum size i.e. 720 wide and 576 or 480 high */
        // For HD files in MKV the VobSub resolution can be higher though,
        // see largeres_vobsub.mkv
        if (*w <= 720 && *h <= 576) {
            *w = 720;
            *h = (*h == 480 || *h == 240) ? 480 : 576;
        }
    } else {
        // Hope that PGS subs set these and 720/576 works for dvb subs
        if (!*w)
            *w = 720;
        if (!*h)
            *h = 576;
    }
}

static int init(struct sd *sd)
{
    enum AVCodecID cid = mp_codec_to_av_codec_id(sd->sh->codec);

    // Supported codecs must be known to decode to paletted bitmaps
    switch (cid) {
    case AV_CODEC_ID_DVB_SUBTITLE:
    case AV_CODEC_ID_HDMV_PGS_SUBTITLE:
    case AV_CODEC_ID_XSUB:
    case AV_CODEC_ID_DVD_SUBTITLE:
        break;
    default:
        return -1;
    }

    struct sd_lavc_priv *priv = talloc_zero(NULL, struct sd_lavc_priv);
    AVCodecContext *ctx = NULL;
    AVCodec *sub_codec = avcodec_find_decoder(cid);
    if (!sub_codec)
        goto error;
    ctx = avcodec_alloc_context3(sub_codec);
    if (!ctx)
        goto error;
    mp_lavc_set_extradata(ctx, sd->sh->extradata, sd->sh->extradata_size);
    if (avcodec_open2(ctx, sub_codec, NULL) < 0)
        goto error;
    priv->avctx = ctx;
    sd->priv = priv;
    priv->displayed_id = -1;
    priv->current_pts = MP_NOPTS_VALUE;
    return 0;

 error:
    MP_FATAL(sd, "Could not open libavcodec subtitle decoder\n");
    av_free(ctx);
    talloc_free(priv);
    return -1;
}

static void clear_sub(struct sub *sub)
{
    sub->count = 0;
    sub->pts = MP_NOPTS_VALUE;
    sub->endpts = MP_NOPTS_VALUE;
    if (sub->valid)
        avsubtitle_free(&sub->avsub);
    sub->valid = false;
}

static void alloc_sub(struct sd_lavc_priv *priv)
{
    clear_sub(&priv->subs[MAX_QUEUE - 1]);
    struct sub tmp = priv->subs[MAX_QUEUE - 1];
    for (int n = MAX_QUEUE - 1; n > 0; n--)
        priv->subs[n] = priv->subs[n - 1];
    priv->subs[0] = tmp;
    // clear only some fields; the memory allocs can be reused
    priv->subs[0].valid = false;
    priv->subs[0].count = 0;
    priv->subs[0].id = priv->new_id++;
}

static void decode(struct sd *sd, struct demux_packet *packet)
{
    struct MPOpts *opts = sd->opts;
    struct sd_lavc_priv *priv = sd->priv;
    AVCodecContext *ctx = priv->avctx;
    double pts = packet->pts;
    double endpts = MP_NOPTS_VALUE;
    double duration = packet->duration;
    AVSubtitle sub;
    AVPacket pkt;

    // libavformat sets duration==0, even if the duration is unknown.
    // Assume there are no bitmap subs that actually use duration==0 for
    // hidden subtitle events.
    if (duration == 0)
        duration = -1;

    if (pts == MP_NOPTS_VALUE)
        MP_WARN(sd, "Subtitle with unknown start time.\n");

    av_init_packet(&pkt);
    pkt.data = packet->buffer;
    pkt.size = packet->len;
    int got_sub;
    int res = avcodec_decode_subtitle2(ctx, &sub, &got_sub, &pkt);
    if (res < 0 || !got_sub)
        return;

    if (pts != MP_NOPTS_VALUE) {
        if (sub.end_display_time > sub.start_display_time &&
            sub.end_display_time != UINT32_MAX)
        {
            duration = (sub.end_display_time - sub.start_display_time) / 1000.0;
        }
        pts += sub.start_display_time / 1000.0;

        if (duration >= 0)
            endpts = pts + duration;

        // set end time of previous sub
        struct sub *prev = &priv->subs[0];
        if (prev->valid) {
            if (prev->endpts == MP_NOPTS_VALUE || prev->endpts > pts)
                prev->endpts = pts;

            if (opts->sub_fix_timing && pts - prev->endpts <= SUB_GAP_THRESHOLD)
                prev->endpts = pts;

            for (int n = 0; n < priv->num_seekpoints; n++) {
                if (priv->seekpoints[n].pts == prev->pts) {
                    priv->seekpoints[n].endpts = prev->endpts;
                    break;
                }
            }
        }

        // This subtitle packet only signals the end of subtitle display.
        if (!sub.num_rects) {
            avsubtitle_free(&sub);
            return;
        }
    }

    alloc_sub(priv);
    struct sub *current = &priv->subs[0];

    current->valid = true;
    current->pts = pts;
    current->endpts = endpts;
    current->avsub = sub;

    MP_TARRAY_GROW(priv, current->inbitmaps, sub.num_rects);
    MP_TARRAY_GROW(priv, current->imgs, sub.num_rects);

    for (int i = 0; i < sub.num_rects; i++) {
        struct AVSubtitleRect *r = sub.rects[i];
        struct sub_bitmap *b = &current->inbitmaps[current->count];
        struct osd_bmp_indexed *img = &current->imgs[current->count];
        if (r->type != SUBTITLE_BITMAP) {
            MP_ERR(sd, "unsupported subtitle type from libavcodec\n");
            continue;
        }
        if (!(r->flags & AV_SUBTITLE_FLAG_FORCED) && opts->forced_subs_only)
            continue;
        if (r->w <= 0 || r->h <= 0)
            continue;
#if HAVE_AV_SUBTITLE_NOPICT
        uint8_t **data = r->data;
        int *linesize = r->linesize;
#else
        uint8_t **data = r->pict.data;
        int *linesize = r->pict.linesize;
#endif
        img->bitmap = data[0];
        assert(r->nb_colors > 0);
        assert(r->nb_colors * 4 <= sizeof(img->palette));
        memcpy(img->palette, data[1], r->nb_colors * 4);
        b->bitmap = img;
        b->stride = linesize[0];
        b->w = r->w;
        b->h = r->h;
        b->x = r->x;
        b->y = r->y;
        current->count++;
    }

    if (pts != MP_NOPTS_VALUE) {
        for (int n = 0; n < priv->num_seekpoints; n++) {
            if (priv->seekpoints[n].pts == pts)
                goto skip;
        }
        // Set arbitrary limit as safe-guard against insane files.
        if (priv->num_seekpoints >= 10000)
            MP_TARRAY_REMOVE_AT(priv->seekpoints, priv->num_seekpoints, 0);
        MP_TARRAY_APPEND(priv, priv->seekpoints, priv->num_seekpoints,
                         (struct seekpoint){.pts = pts, .endpts = endpts});
        skip: ;
    }
}

static void get_bitmaps(struct sd *sd, struct mp_osd_res d, double pts,
                        struct sub_bitmaps *res)
{
    struct sd_lavc_priv *priv = sd->priv;
    struct MPOpts *opts = sd->opts;

    priv->current_pts = pts;

    struct sub *current = NULL;
    for (int n = MAX_QUEUE - 1; n >= 0; n--) {
        struct sub *sub = &priv->subs[n];
        if (!sub->valid)
            continue;
        if (pts == MP_NOPTS_VALUE ||
            ((sub->pts == MP_NOPTS_VALUE || pts >= sub->pts) &&
             (sub->endpts == MP_NOPTS_VALUE || pts < sub->endpts)))
        {
            // Ignore "trailing" subtitles with unknown length after 1 minute.
            if (sub->endpts == MP_NOPTS_VALUE && pts >= sub->pts + 60)
                break;
            current = sub;
            break;
        }
    }
    if (!current)
        return;

    MP_TARRAY_GROW(priv, priv->outbitmaps, current->count);
    for (int n = 0; n < current->count; n++)
        priv->outbitmaps[n] = current->inbitmaps[n];

    res->parts = priv->outbitmaps;
    res->num_parts = current->count;
    if (priv->displayed_id != current->id)
        res->change_id++;
    priv->displayed_id = current->id;
    res->format = SUBBITMAP_INDEXED;

    double video_par = 0;
    if (priv->avctx->codec_id == AV_CODEC_ID_DVD_SUBTITLE &&
        opts->stretch_dvd_subs)
    {
        // For DVD subs, try to keep the subtitle PAR at display PAR.
        double par = priv->video_params.p_w / (double)priv->video_params.p_h;
        if (isnormal(par))
            video_par = par;
    }
    if (priv->avctx->codec_id == AV_CODEC_ID_HDMV_PGS_SUBTITLE)
        video_par = -1;
    if (opts->stretch_image_subs)
        d.ml = d.mr = d.mt = d.mb = 0;
    int insize[2];
    get_resolution(sd, insize);
    for (int n = 0; n < res->num_parts; n++) {
        struct sub_bitmap *p = &res->parts[n];
        if ((p->x + p->w > insize[0] || p->y + p->h > insize[1]) &&
            priv->video_params.w > insize[0] && priv->video_params.h > insize[1])
        {
            insize[0] = priv->video_params.w;
            insize[1] = priv->video_params.h;
        }
    }
    osd_rescale_bitmaps(res, insize[0], insize[1], d, video_par);
}

static bool accepts_packet(struct sd *sd)
{
    struct sd_lavc_priv *priv = sd->priv;

    double pts = priv->current_pts;
    int last_needed = -1;
    for (int n = 0; n < MAX_QUEUE; n++) {
        struct sub *sub = &priv->subs[n];
        if (!sub->valid)
            continue;
        if (pts == MP_NOPTS_VALUE ||
            ((sub->pts == MP_NOPTS_VALUE || sub->pts >= pts) ||
             (sub->endpts == MP_NOPTS_VALUE || pts < sub->endpts)))
        {
            last_needed = n;
        }
    }
    // We can accept a packet if it wouldn't overflow the fixed subtitle queue.
    // We assume that get_bitmaps() never decreases the PTS.
    return last_needed + 1 < MAX_QUEUE;
}

static void reset(struct sd *sd)
{
    struct sd_lavc_priv *priv = sd->priv;

    for (int n = 0; n < MAX_QUEUE; n++)
        clear_sub(&priv->subs[n]);
    // lavc might not do this right for all codecs; may need close+reopen
    avcodec_flush_buffers(priv->avctx);

    priv->current_pts = MP_NOPTS_VALUE;
}

static void uninit(struct sd *sd)
{
    struct sd_lavc_priv *priv = sd->priv;

    for (int n = 0; n < MAX_QUEUE; n++)
        clear_sub(&priv->subs[n]);
    avcodec_close(priv->avctx);
    av_free(priv->avctx->extradata);
    av_free(priv->avctx);
    talloc_free(priv);
}

static int compare_seekpoint(const void *pa, const void *pb)
{
    const struct seekpoint *a = pa, *b = pb;
    return a->pts == b->pts ? 0 : (a->pts < b->pts ? -1 : +1);
}

// taken from ass_step_sub(), libass (ISC)
static double step_sub(struct sd *sd, double now, int movement)
{
    struct sd_lavc_priv *priv = sd->priv;
    int best = -1;
    double target = now;
    int direction = movement > 0 ? 1 : -1;

    if (movement == 0 || priv->num_seekpoints == 0)
        return MP_NOPTS_VALUE;

    qsort(priv->seekpoints, priv->num_seekpoints, sizeof(priv->seekpoints[0]),
          compare_seekpoint);

    while (movement) {
        int closest = -1;
        double closest_time = 0;
        for (int i = 0; i < priv->num_seekpoints; i++) {
            struct seekpoint *p = &priv->seekpoints[i];
            double start = p->pts;
            if (direction < 0) {
                double end = p->endpts == MP_NOPTS_VALUE ? INFINITY : p->endpts;
                if (end < target) {
                    if (closest < 0 || end > closest_time) {
                        closest = i;
                        closest_time = end;
                    }
                }
            } else {
                if (start > target) {
                    if (closest < 0 || start < closest_time) {
                        closest = i;
                        closest_time = start;
                    }
                }
            }
        }
        if (closest < 0)
            break;
        target = closest_time + direction;
        best = closest;
        movement -= direction;
    }

    return best < 0 ? 0 : priv->seekpoints[best].pts - now;
}

static int control(struct sd *sd, enum sd_ctrl cmd, void *arg)
{
    struct sd_lavc_priv *priv = sd->priv;
    switch (cmd) {
    case SD_CTRL_SUB_STEP: {
        double *a = arg;
        double res = step_sub(sd, a[0], a[1]);
        if (res == MP_NOPTS_VALUE)
            return false;
        a[0] = res;
        return true;
    }
    case SD_CTRL_SET_VIDEO_PARAMS:
        priv->video_params = *(struct mp_image_params *)arg;
        return CONTROL_OK;
    case SD_CTRL_GET_RESOLUTION:
        get_resolution(sd, arg);
        return CONTROL_OK;
    default:
        return CONTROL_UNKNOWN;
    }
}

const struct sd_functions sd_lavc = {
    .name = "lavc",
    .init = init,
    .decode = decode,
    .get_bitmaps = get_bitmaps,
    .accepts_packet = accepts_packet,
    .control = control,
    .reset = reset,
    .uninit = uninit,
};
