/*
 * This file is part of mplayer2.
 *
 * mplayer2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mplayer2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mplayer2.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include <libavcodec/avcodec.h>

#include "mp_msg.h"
#include "libmpdemux/stheader.h"
#include "sd.h"
#include "spudec.h"
// Current code still pushes subs directly to global spudec
#include "sub.h"

static void avsub_to_spudec(AVSubtitleRect **rects, int num_rects,
                            double pts, double endpts)
{
    int i, xmin = INT_MAX, ymin = INT_MAX, xmax = INT_MIN, ymax = INT_MIN;
    struct spu_packet_t *packet;

    if (num_rects == 1) {
        spudec_set_paletted(vo_spudec,
                            rects[0]->pict.data[0],
                            rects[0]->pict.linesize[0],
                            rects[0]->pict.data[1],
                            rects[0]->x,
                            rects[0]->y,
                            rects[0]->w,
                            rects[0]->h,
                            pts,
                            endpts);
        return;
    }
    for (i = 0; i < num_rects; i++) {
        xmin = FFMIN(xmin, rects[i]->x);
        ymin = FFMIN(ymin, rects[i]->y);
        xmax = FFMAX(xmax, rects[i]->x + rects[i]->w);
        ymax = FFMAX(ymax, rects[i]->y + rects[i]->h);
    }
    packet = spudec_packet_create(xmin, ymin, xmax - xmin, ymax - ymin);
    if (!packet)
        return;
    spudec_packet_clear(packet);
    for (i = 0; i < num_rects; i++)
        spudec_packet_fill(packet,
                           rects[i]->pict.data[0],
                           rects[i]->pict.linesize[0],
                           rects[i]->pict.data[1],
                           rects[i]->x - xmin,
                           rects[i]->y - ymin,
                           rects[i]->w,
                           rects[i]->h);
    spudec_packet_send(vo_spudec, packet, pts, endpts);
}

static int init(struct sh_sub *sh, struct osd_state *osd)
{
    enum CodecID cid = CODEC_ID_NONE;
    switch (sh->type) {
    case 'b':
        cid = CODEC_ID_DVB_SUBTITLE; break;
    case 'p':
        cid = CODEC_ID_HDMV_PGS_SUBTITLE; break;
    case 'x':
        cid = CODEC_ID_XSUB; break;
    }
    AVCodecContext *ctx = NULL;
    AVCodec *sub_codec = avcodec_find_decoder(cid);
    if (!sub_codec)
        goto error;
    ctx = avcodec_alloc_context3(sub_codec);
    if (!ctx)
        goto error;
    if (avcodec_open2(ctx, sub_codec, NULL) < 0)
        goto error;
    sh->context = ctx;
    return 0;

 error:
    mp_msg(MSGT_SUBREADER, MSGL_ERR,
           "Could not open libavcodec subtitle decoder\n");
    av_free(ctx);
    return -1;
}

static void decode(struct sh_sub *sh, struct osd_state *osd, void *data,
                   int data_len, double pts, double duration)
{
    AVCodecContext *ctx = sh->context;
    AVSubtitle sub;
    AVPacket pkt;

    av_init_packet(&pkt);
    pkt.data = data;
    pkt.size = data_len;
    pkt.pts = pts * 1000;
    if (duration >= 0)
        pkt.convergence_duration = duration * 1000;
    int got_sub;
    int res = avcodec_decode_subtitle2(ctx, &sub, &got_sub, &pkt);
    if (res < 0 || !got_sub)
        return;
    if (pts != MP_NOPTS_VALUE) {
        if (sub.end_display_time > sub.start_display_time)
            duration = (sub.end_display_time - sub.start_display_time) / 1000.0;
        pts += sub.start_display_time / 1000.0;
    }
    double endpts = MP_NOPTS_VALUE;
    if (pts != MP_NOPTS_VALUE && duration >= 0)
        endpts = pts + duration;
    if (vo_spudec && sub.num_rects == 0)
        spudec_set_paletted(vo_spudec, NULL, 0, NULL, 0, 0, 0, 0, pts, endpts);
    if (sub.num_rects > 0) {
        switch (sub.rects[0]->type) {
        case SUBTITLE_BITMAP:
            if (!vo_spudec)
                vo_spudec = spudec_new_scaled(NULL, ctx->width, ctx->height, NULL, 0);
            avsub_to_spudec(sub.rects, sub.num_rects, pts, endpts);
            vo_osd_changed(OSDTYPE_SPU);
            break;
        default:
            mp_msg(MSGT_SUBREADER, MSGL_ERR, "sd_lavc: unsupported subtitle "
                   "type from libavcodec\n");
            break;
        }
    }
    avsubtitle_free(&sub);
}

static void reset(struct sh_sub *sh, struct osd_state *osd)
{
    // lavc might not do this right for all codecs; may need close+reopen
    avcodec_flush_buffers(sh->context);
}

static void uninit(struct sh_sub *sh)
{
    avcodec_close(sh->context);
    av_free(sh->context);
}

const struct sd_functions sd_lavc = {
    .init = init,
    .decode = decode,
    .reset = reset,
    .switch_off = reset,
    .uninit = uninit,
};
