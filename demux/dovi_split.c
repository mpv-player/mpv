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

#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavutil/dovi_meta.h>
#include <libavutil/opt.h>

#include "common/av_common.h"
#include "common/common.h"
#include "common/msg.h"
#include "demux.h"
#include "demux/packet.h"
#include "demux/packet_pool.h"
#include "demux/stheader.h"
#include "dovi_split.h"
#include "mpv_talloc.h"

struct mp_dovi_split {
    struct mp_log *log;
    struct demuxer *demuxer;
    struct sh_stream *bl;
    struct sh_stream *el;
    AVBSFContext *bsf;
    AVPacket *staging;
};

static void mp_dovi_split_destructor(void *p)
{
    struct mp_dovi_split *s = p;
    av_packet_free(&s->staging);
    av_bsf_free(&s->bsf);
}

struct mp_dovi_split *mp_dovi_split_create(struct demuxer *demuxer,
                                           struct sh_stream *bl)
{
    if (!bl || bl->type != STREAM_VIDEO || !bl->codec ||
        !bl->codec->codec || strcmp(bl->codec->codec, "hevc") != 0)
        return NULL;

    const AVBitStreamFilter *def = av_bsf_get_by_name("dovi_split");
    if (!def) {
        MP_WARN(demuxer, "Dolby Vision EL: 'dovi_split' BSF not available in "
                         "libavcodec; rendering base layer only.\n");
        return NULL;
    }

    struct mp_dovi_split *s = talloc_zero(demuxer, struct mp_dovi_split);
    talloc_set_destructor(s, mp_dovi_split_destructor);
    s->log = demuxer->log;
    s->demuxer = demuxer;
    s->bl = bl;

    s->staging = av_packet_alloc();
    if (!s->staging)
        goto fail;

    int ret = av_bsf_alloc(def, &s->bsf);
    if (ret < 0)
        goto fail;

    AVCodecParameters *par = mp_codec_params_to_av(bl->codec);
    if (par) {
        avcodec_parameters_copy(s->bsf->par_in, par);
        avcodec_parameters_free(&par);
    }
    s->bsf->time_base_in = mp_get_codec_timebase(bl->codec);

    if (av_opt_set(s->bsf, "mode", "el", AV_OPT_SEARCH_CHILDREN) < 0)
        goto fail;
    if (av_bsf_init(s->bsf) < 0)
        goto fail;

    const AVCodecParameters *par_out = s->bsf->par_out;

    // Allocate the virtual EL sh_stream.
    struct sh_stream *el = demux_alloc_sh_stream(STREAM_VIDEO);
    el->codec->codec = "hevc";
    el->codec->native_tb_num = bl->codec->native_tb_num;
    el->codec->native_tb_den = bl->codec->native_tb_den;
    el->codec->fps = bl->codec->fps;
    el->codec->disp_w = par_out->width;
    el->codec->disp_h = par_out->height;
    if (par_out->extradata_size > 0) {
        el->codec->extradata = talloc_memdup(el, par_out->extradata,
                                             par_out->extradata_size);
        el->codec->extradata_size = par_out->extradata_size;
    }
    for (int i = 0; i < par_out->nb_coded_side_data; i++) {
        const AVPacketSideData *sd = &par_out->coded_side_data[i];
        if (sd->type != AV_PKT_DATA_DOVI_CONF)
            continue;
        const AVDOVIDecoderConfigurationRecord *cfg = (const void *)sd->data;
        el->codec->dovi = true;
        el->codec->dv_profile = cfg->dv_profile;
        el->codec->dv_level = cfg->dv_level;
        el->codec->dv_el_present = cfg->el_present_flag;
        break;
    }
    el->title = talloc_strdup(el, "Dolby Vision enhancement layer");
    el->dependent_track = true;

    demux_add_sh_stream(demuxer, el);
    s->el = el;

    // Bind BL and EL into a sh_stream_group.
    struct sh_stream_group *group = talloc_zero(bl, struct sh_stream_group);
    MP_TARRAY_APPEND(group, group->members, group->num_members, bl);
    MP_TARRAY_APPEND(group, group->members, group->num_members, el);
    bl->group = group;
    el->group = group;

    MP_VERBOSE(demuxer, "Dolby Vision Profile 7 splitter: BL stream %d, "
               "virtual EL stream %d (dependent_track).\n",
               bl->index, el->index);
    return s;

fail:
    talloc_free(s);
    return NULL;
}

void mp_dovi_split_reset(struct mp_dovi_split *s)
{
    if (!s || !s->bsf)
        return;
    av_bsf_flush(s->bsf);
}

struct sh_stream *mp_dovi_split_el_stream(struct mp_dovi_split *s)
{
    return s ? s->el : NULL;
}

struct demux_packet *mp_dovi_split_dispatch(struct mp_dovi_split *s,
                                            struct demux_packet *bl_dp)
{
    if (!s || !s->bsf || !bl_dp || !bl_dp->buffer || bl_dp->len <= 0)
        return NULL;

    // av_bsf_send_packet takes ownership of the packet's buffer, so copy it,
    // to not steal it from caller.
    AVPacket *copy = av_packet_alloc();
    if (!copy)
        return NULL;
    int ret = av_new_packet(copy, bl_dp->len);
    if (ret < 0) {
        av_packet_free(&copy);
        return NULL;
    }
    memcpy(copy->data, bl_dp->buffer, bl_dp->len);
    copy->flags = bl_dp->keyframe ? AV_PKT_FLAG_KEY : 0;

    ret = av_bsf_send_packet(s->bsf, copy);
    av_packet_free(&copy);
    if (ret < 0) {
        MP_VERBOSE(s->demuxer, "dovi_split: BSF send failed: %s\n",
                   mp_strerror(AVUNERROR(ret)));
        return NULL;
    }

    ret = av_bsf_receive_packet(s->bsf, s->staging);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        // No EL NALs in this AU, nothing to emit.
        return NULL;
    }
    if (ret < 0) {
        MP_VERBOSE(s->demuxer, "dovi_split: BSF receive failed: %s; flushing.\n",
                   mp_strerror(AVUNERROR(ret)));
        av_bsf_flush(s->bsf);
        return NULL;
    }

    struct demux_packet *dp =
        new_demux_packet_from_avpacket(s->demuxer->packet_pool, s->staging);
    if (dp) {
        // Mirror the BL packet's timing so the pairing filter can match by PTS.
        dp->pts = bl_dp->pts;
        dp->dts = bl_dp->dts;
        dp->duration = bl_dp->duration;
        dp->keyframe = bl_dp->keyframe;
        dp->stream = s->el->index;
    }
    av_packet_unref(s->staging);
    return dp;
}
