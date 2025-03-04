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

#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/opt.h>

#include "mpv_talloc.h"
#include "common/msg.h"
#include "common/av_common.h"
#include "demux/stheader.h"
#include "options/options.h"
#include "video/mp_image.h"
#include "video/out/bitmap_packer.h"
#include "img_convert.h"
#include "sd.h"
#include "dec_sub.h"

#define MAX_QUEUE 4

struct sub {
    bool valid;
    AVSubtitle avsub;
    struct sub_bitmap *inbitmaps;
    int count;
    struct mp_image *data;
    int bound_w, bound_h;
    int src_w, src_h;
    double pts;
    double endpts;
    int64_t id;
};

struct seekpoint {
    double pts;
    double endpts;
};

struct sd_lavc_priv {
    struct mp_codec_params *codec;
    AVCodecContext *avctx;
    AVPacket *avpkt;
    AVRational pkt_timebase;
    struct sub subs[MAX_QUEUE]; // most recent event first
    struct sub_bitmap *outbitmaps;
    struct sub_bitmap *prevret;
    int prevret_num;
    int64_t displayed_id;
    int64_t new_id;
    struct mp_image_params video_params;
    double current_pts;
    struct seekpoint *seekpoints;
    int num_seekpoints;
    struct bitmap_packer *packer;
};

static int init(struct sd *sd)
{
    enum AVCodecID cid = mp_codec_to_av_codec_id(sd->codec->codec);

    // Supported codecs must be known to decode to paletted bitmaps
    switch (cid) {
    case AV_CODEC_ID_DVB_SUBTITLE:
    case AV_CODEC_ID_DVB_TELETEXT:
    case AV_CODEC_ID_HDMV_PGS_SUBTITLE:
    case AV_CODEC_ID_XSUB:
    case AV_CODEC_ID_DVD_SUBTITLE:
    case AV_CODEC_ID_ARIB_CAPTION:
        break;
    default:
        return -1;
    }

    struct sd_lavc_priv *priv = talloc_zero(NULL, struct sd_lavc_priv);
    AVCodecContext *ctx = NULL;
    const AVCodec *sub_codec = avcodec_find_decoder(cid);
    if (!sub_codec)
        goto error_probe;
    ctx = avcodec_alloc_context3(sub_codec);
    if (!ctx)
        goto error_probe;

    mp_set_avopts(sd->log, ctx, sd->opts->sub_avopts);

    switch (cid) {
    case AV_CODEC_ID_DVB_TELETEXT: {
        int64_t format;
        int ret = av_opt_get_int(ctx, "txt_format", AV_OPT_SEARCH_CHILDREN, &format);
        // format == 0 is bitmap
        if (!ret && format)
            goto error_probe;
        break;
    }
    case AV_CODEC_ID_ARIB_CAPTION: {
        int64_t format;
        int ret = av_opt_get_int(ctx, "sub_type", AV_OPT_SEARCH_CHILDREN, &format);
        if (!ret && format != SUBTITLE_BITMAP)
            goto error_probe;
        break;
    }
    }

    priv->avpkt = av_packet_alloc();
    priv->codec = sd->codec;
    if (!priv->avpkt)
        goto error;
    if (mp_set_avctx_codec_headers(ctx, sd->codec) < 0)
        goto error;
    priv->pkt_timebase = mp_get_codec_timebase(sd->codec);
    ctx->pkt_timebase = priv->pkt_timebase;
    if (avcodec_open2(ctx, sub_codec, NULL) < 0)
        goto error;
    priv->avctx = ctx;
    sd->priv = priv;
    priv->displayed_id = -1;
    priv->current_pts = MP_NOPTS_VALUE;
    priv->packer = talloc_zero(priv, struct bitmap_packer);
    return 0;

error:
    MP_FATAL(sd, "Could not open libavcodec subtitle decoder\n");
error_probe:
    avcodec_free_context(&ctx);
    mp_free_av_packet(&priv->avpkt);
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
    priv->subs[0].src_w = 0;
    priv->subs[0].src_h = 0;
    priv->subs[0].id = priv->new_id++;
}

static void convert_pal(uint32_t *colors, size_t count, bool gray)
{
    for (int n = 0; n < count; n++) {
        uint32_t c = colors[n];
        uint32_t b = c & 0xFF;
        uint32_t g = (c >> 8) & 0xFF;
        uint32_t r = (c >> 16) & 0xFF;
        uint32_t a = (c >> 24) & 0xFF;
        if (gray)
            r = g = b = (r + g + b) / 3;
        // from straight to pre-multiplied alpha
        b = b * a / 255;
        g = g * a / 255;
        r = r * a / 255;
        colors[n] = b | (g << 8) | (r << 16) | (a << 24);
    }
}

// Initialize sub from sub->avsub.
static void read_sub_bitmaps(struct sd *sd, struct sub *sub)
{
    struct mp_subtitle_opts *opts = sd->opts;
    struct sd_lavc_priv *priv = sd->priv;
    AVSubtitle *avsub = &sub->avsub;

    MP_TARRAY_GROW(priv, sub->inbitmaps, avsub->num_rects);

    packer_set_size(priv->packer, avsub->num_rects);

    // If we blur, we want a transparent region around the bitmap data to
    // avoid "cut off" artifacts on the borders.
    bool apply_blur = opts->sub_gauss != 0.0f;
    int extend = apply_blur ? 5 : 0;
    // Assume consumers may use bilinear scaling on it (2x2 filter)
    int padding = 1 + extend;

    priv->packer->padding = padding;

    // For the sake of libswscale, which in some cases takes sub-rects as
    // source images, and wants 16 byte start pointer and stride alignment.
    int align = 4;

    for (int i = 0; i < avsub->num_rects; i++) {
        struct AVSubtitleRect *r = avsub->rects[i];
        struct sub_bitmap *b = &sub->inbitmaps[sub->count];

        if (r->type != SUBTITLE_BITMAP) {
            MP_ERR(sd, "unsupported subtitle type from libavcodec\n");
            continue;
        }
        if (!(r->flags & AV_SUBTITLE_FLAG_FORCED) && opts->sub_forced_events_only)
            continue;
        if (r->w <= 0 || r->h <= 0)
            continue;

        b->bitmap = r; // save for later (dumb hack to avoid more complexity)

        priv->packer->in[sub->count] = (struct pos){r->w + (align - 1), r->h};
        sub->count++;
    }

    priv->packer->count = sub->count;

    if (packer_pack(priv->packer) < 0) {
        MP_ERR(sd, "Unable to pack subtitle bitmaps.\n");
        sub->count = 0;
    }

    if (!sub->count)
        return;

    struct pos bb[2];
    packer_get_bb(priv->packer, bb);

    sub->bound_w = bb[1].x;
    sub->bound_h = bb[1].y;

    if (!sub->data || sub->data->w < sub->bound_w || sub->data->h < sub->bound_h) {
        talloc_free(sub->data);
        sub->data = mp_image_alloc(IMGFMT_BGRA, priv->packer->w, priv->packer->h);
        if (!sub->data) {
            sub->count = 0;
            return;
        }
        talloc_steal(priv, sub->data);
    }

    if (!mp_image_make_writeable(sub->data)) {
        sub->count = 0;
        return;
    }

    for (int i = 0; i < sub->count; i++) {
        struct sub_bitmap *b = &sub->inbitmaps[i];
        struct pos pos = priv->packer->result[i];
        struct AVSubtitleRect *r = b->bitmap;
        uint8_t **data = r->data;
        int *linesize = r->linesize;
        b->w = r->w;
        b->h = r->h;
        b->x = r->x;
        b->y = r->y;

        // Choose such that the extended start position is aligned.
        pos.x = MP_ALIGN_UP(pos.x - extend, align) + extend;

        b->src_x = pos.x;
        b->src_y = pos.y;
        b->stride = sub->data->stride[0];
        b->bitmap = sub->data->planes[0] + pos.y * b->stride + pos.x * 4;

        sub->src_w = MPMAX(sub->src_w, b->x + b->w);
        sub->src_h = MPMAX(sub->src_h, b->y + b->h);

        mp_assert(r->nb_colors > 0);
        mp_assert(r->nb_colors <= 256);
        uint32_t pal[256] = {0};
        memcpy(pal, data[1], r->nb_colors * 4);
        convert_pal(pal, 256, opts->sub_gray);

        for (int y = -padding; y < b->h + padding; y++) {
            uint32_t *out = (uint32_t*)((char*)b->bitmap + y * b->stride);
            int start = 0;
            for (int x = -padding; x < 0; x++)
                out[x] = 0;
            if (y >= 0 && y < b->h) {
                uint8_t *in = data[0] + y * linesize[0];
                for (int x = 0; x < b->w; x++)
                    *out++ = pal[*in++];
                start = b->w;
            }
            for (int x = start; x < b->w + padding; x++)
                *out++ = 0;
        }

        b->bitmap = (char*)b->bitmap - extend * b->stride - extend * 4;
        b->src_x -= extend;
        b->src_y -= extend;
        b->x -= extend;
        b->y -= extend;
        b->w += extend * 2;
        b->h += extend * 2;

        if (apply_blur)
            mp_blur_rgba_sub_bitmap(b, opts->sub_gauss);
    }
}

static void decode(struct sd *sd, struct demux_packet *packet)
{
    struct mp_subtitle_opts *opts = sd->opts;
    struct sd_lavc_priv *priv = sd->priv;
    AVCodecContext *ctx = priv->avctx;
    double pts = packet->pts;
    double endpts = MP_NOPTS_VALUE;
    AVSubtitle sub;

    if (pts == MP_NOPTS_VALUE)
        MP_WARN(sd, "Subtitle with unknown start time.\n");

    mp_set_av_packet(priv->avpkt, packet, &priv->pkt_timebase);

    if (ctx->codec_id == AV_CODEC_ID_DVB_TELETEXT) {
        if (!opts->teletext_page) {
            av_opt_set(ctx, "txt_page", "subtitle", AV_OPT_SEARCH_CHILDREN);
        } else if (opts->teletext_page == -1) {
            av_opt_set(ctx, "txt_page", "*", AV_OPT_SEARCH_CHILDREN);
        } else {
            char page[4];
            snprintf(page, sizeof(page), "%d", opts->teletext_page);
            av_opt_set(ctx, "txt_page", page, AV_OPT_SEARCH_CHILDREN);
        }
    }

    int got_sub;
    int res = avcodec_decode_subtitle2(ctx, &sub, &got_sub, priv->avpkt);
    if (res < 0 || !got_sub)
        return;

    mp_codec_info_from_av(ctx, priv->codec);

    packet->sub_duration = sub.end_display_time;

    if (sub.pts != AV_NOPTS_VALUE)
        pts = sub.pts / (double)AV_TIME_BASE;

    if (pts != MP_NOPTS_VALUE) {
        if (sub.end_display_time > sub.start_display_time &&
            sub.end_display_time != UINT32_MAX)
        {
            endpts = pts + sub.end_display_time / 1000.0;
        }
        pts += sub.start_display_time / 1000.0;

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

    read_sub_bitmaps(sd, current);

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

static struct sub *get_current(struct sd_lavc_priv *priv, double pts)
{
    struct sub *current = NULL;
    for (int n = 0; n < MAX_QUEUE; n++) {
        struct sub *sub = &priv->subs[n];
        if (!sub->valid)
            continue;
        if (pts == MP_NOPTS_VALUE ||
            ((sub->pts == MP_NOPTS_VALUE || pts + 1e-6 >= sub->pts) &&
             (sub->endpts == MP_NOPTS_VALUE || pts + 1e-6 < sub->endpts)))
        {
            // Ignore "trailing" subtitles with unknown length after 1 minute.
            if (sub->endpts == MP_NOPTS_VALUE && pts >= sub->pts + 60)
                break;
            current = sub;
            break;
        }
    }
    return current;
}

static struct sub_bitmaps *get_bitmaps(struct sd *sd, struct mp_osd_res d,
                                       int format, double pts)
{
    struct sd_lavc_priv *priv = sd->priv;
    struct mp_subtitle_opts *opts = sd->opts;
    struct mp_subtitle_shared_opts *shared_opts = sd->shared_opts;

    priv->current_pts = pts;

    struct sub *current = get_current(priv, pts);

    if (!current)
        return NULL;

    MP_TARRAY_GROW(priv, priv->outbitmaps, current->count);
    for (int n = 0; n < current->count; n++)
        priv->outbitmaps[n] = current->inbitmaps[n];

    struct sub_bitmaps *res = &(struct sub_bitmaps){0};
    res->parts = priv->outbitmaps;
    res->num_parts = current->count;
    if (priv->displayed_id != current->id)
        res->change_id++;
    priv->displayed_id = current->id;
    res->packed = current->data;
    res->packed_w = current->bound_w;
    res->packed_h = current->bound_h;
    res->format = SUBBITMAP_BGRA;

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
    {
        // For Blu-ray subs on SD video, try to match the video PAR.
        if (priv->video_params.w == 720 &&
            (priv->video_params.h == 480 ||
             priv->video_params.h == 576))
        {
            double par = priv->video_params.p_w / (double)priv->video_params.p_h;
            if (isnormal(par))
                video_par = par * -1;
            else
                video_par = -1;
        }
        else
        {
            // Force letter-boxing on all other Blu-ray subtitles
            video_par = -1;
        }
    }
    if (opts->stretch_image_subs)
        d.ml = d.mr = d.mt = d.mb = 0;
    int w = priv->avctx->width;
    int h = priv->avctx->height;
    if (w <= 0 || h <= 0 || opts->image_subs_video_res) {
        w = priv->video_params.w;
        h = priv->video_params.h;
    }
    if (current->src_w > w || current->src_h > h) {
        w = MPMAX(priv->video_params.w, current->src_w);
        h = MPMAX(priv->video_params.h, current->src_h);
    }

    if (shared_opts->sub_pos[sd->order] != 100.0f && shared_opts->ass_style_override[sd->order]) {
        float offset = (100.0f - shared_opts->sub_pos[sd->order]) / 100.0f * h;

        for (int n = 0; n < res->num_parts; n++) {
            struct sub_bitmap *sub = &res->parts[n];

            // Decide by heuristic whether this is a sub-title or something
            // else (top-title, covering whole screen).
            if (sub->y < h / 2)
                continue;

            // Allow moving up the subtitle, but only until it clips.
            sub->y = MPMAX(sub->y - offset, 0);
            sub->y = MPMIN(sub->y + sub->h, h) - sub->h;
        }
    }

    osd_rescale_bitmaps(res, w, h, d, video_par);

    if (opts->sub_scale != 1.0 && shared_opts->ass_style_override[sd->order]) {
        for (int n = 0; n < res->num_parts; n++) {
            struct sub_bitmap *sub = &res->parts[n];

            float shit = (opts->sub_scale - 1.0f) / 2;

            // Fortunately VO isn't supposed to give a FUCKING FUCK about
            // whether the sub might e.g. go outside of the screen.
            sub->x -= sub->dw * shit;
            sub->y -= sub->dh * shit;
            sub->dw += sub->dw * shit * 2;
            sub->dh += sub->dh * shit * 2;
        }
    }

    if (priv->prevret_num != res->num_parts)
        res->change_id++;

    if (!res->change_id) {
        mp_assert(priv->prevret_num == res->num_parts);
        for (int n = 0; n < priv->prevret_num; n++) {
            struct sub_bitmap *a = &res->parts[n];
            struct sub_bitmap *b = &priv->prevret[n];

            if (a->x != b->x || a->y != b->y ||
                a->dw != b->dw || a->dh != b->dh)
            {
                res->change_id++;
                break;
            }
        }
    }

    priv->prevret_num = res->num_parts;
    MP_TARRAY_GROW(priv, priv->prevret, priv->prevret_num);
    memcpy(priv->prevret, res->parts, res->num_parts * sizeof(priv->prevret[0]));

    return sub_bitmaps_copy(NULL, res);
}

static struct sd_times get_times(struct sd *sd, double pts)
{
    struct sd_lavc_priv *priv = sd->priv;
    struct sd_times res = { .start = MP_NOPTS_VALUE, .end = MP_NOPTS_VALUE };

    if (pts == MP_NOPTS_VALUE)
        return res;

    struct sub *current = get_current(priv, pts);

    if (!current)
        return res;

    res.start = current->pts;
    res.end = current->endpts;

    return res;
}

static bool accepts_packet(struct sd *sd, double min_pts)
{
    struct sd_lavc_priv *priv = sd->priv;

    double pts = priv->current_pts;
    if (min_pts != MP_NOPTS_VALUE) {
        // guard against bogus rendering PTS in the future.
        if (pts == MP_NOPTS_VALUE || min_pts < pts)
            pts = min_pts;
        // Heuristic: we assume rendering cannot lag behind more than 1 second
        // behind decoding.
        if (pts + 1 < min_pts)
            pts = min_pts;
    }

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
    avcodec_free_context(&priv->avctx);
    mp_free_av_packet(&priv->avpkt);
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
    int direction = (movement > 0 ? 1 : -1) * !!movement;

    if (priv->num_seekpoints == 0)
        return MP_NOPTS_VALUE;

    qsort(priv->seekpoints, priv->num_seekpoints, sizeof(priv->seekpoints[0]),
          compare_seekpoint);

    do {
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
            } else if (direction > 0) {
                if (start > target) {
                    if (closest < 0 || start < closest_time) {
                        closest = i;
                        closest_time = start;
                    }
                }
            } else {
                if (start < target) {
                    if (closest < 0 || start >= closest_time) {
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
    } while (movement);

    return best < 0 ? now : priv->seekpoints[best].pts;
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
    default:
        return CONTROL_UNKNOWN;
    }
}

const struct sd_functions sd_lavc = {
    .name = "lavc",
    .init = init,
    .decode = decode,
    .get_bitmaps = get_bitmaps,
    .get_times = get_times,
    .accepts_packet = accepts_packet,
    .control = control,
    .reset = reset,
    .uninit = uninit,
};
