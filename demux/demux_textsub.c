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

#include <limits.h>
#include <math.h>

#include <subrandr/subrandr.h>

#include "common/common.h"
#include "demux/packet.h"
#include "misc/bstr.h"
#include "options/m_config.h"
#include "options/m_option.h"
#include "stream/stream.h"
#include "demux.h"

#define OPT_BASE_STRUCT struct demux_textsub_opts
struct demux_textsub_opts {
    int probesize;
};

const struct m_sub_options demux_textsub_conf = {
    .opts = (const m_option_t[]) {
        {"demuxer-textsub-probesize", OPT_INT(probesize), M_RANGE(32, INT_MAX)},
        {0}
    },
    .size = sizeof(struct demux_textsub_opts),
    .defaults = &(const struct demux_textsub_opts){
        .probesize = 128,
    },
    .change_flags = UPDATE_DEMUXER,
};

struct format_codec_info {
    const char *codec;
    const char *codec_desc;
};

static const struct format_codec_info FMT_TO_CODEC[] = {
    [SBR_SUBTITLE_FORMAT_UNKOWN] = {NULL, NULL},
    [SBR_SUBTITLE_FORMAT_SRV3  ] = {"textsub/srv3", "srv3"},
    [SBR_SUBTITLE_FORMAT_WEBVTT] = {"textsub/vtt",  "WebVTT"},
};

struct textsub_ext {
    const char *ext;
    struct format_codec_info codec_info;
};

static const struct textsub_ext TEXT_FORMAT_EXTS[] = {
    {".vtt",  FMT_TO_CODEC[SBR_SUBTITLE_FORMAT_WEBVTT]},
    {".srv3", FMT_TO_CODEC[SBR_SUBTITLE_FORMAT_SRV3]},
    {".ytt",  FMT_TO_CODEC[SBR_SUBTITLE_FORMAT_SRV3]},
    {NULL}
};

struct demux_textsub_priv {
    bstr content;
    bool exhausted;
};

static int demux_open_textsub(struct demuxer *demuxer, enum demux_check check)
{
    bstr filename = bstr0(demuxer->filename);
    struct format_codec_info codec_info = {NULL, NULL};

    struct demux_textsub_opts *opts = mp_get_config_group(demuxer, demuxer->global, &demux_textsub_conf);

    for (const struct textsub_ext *ext = TEXT_FORMAT_EXTS; ext->ext; ++ext) {
        if (bstr_endswith0(filename, ext->ext))
            codec_info = ext->codec_info;
    }

    if (!codec_info.codec) {
        int probe_size = stream_peek(demuxer->stream, opts->probesize);
        uint8_t *probe_buffer = demuxer->stream->buffer;

        sbr_subtitle_format fmt = sbr_probe_text((const char *)probe_buffer, (size_t)probe_size);
        if(fmt < MP_ARRAY_SIZE(FMT_TO_CODEC))
            codec_info = FMT_TO_CODEC[fmt];
    }

    if (check != DEMUX_CHECK_REQUEST && !codec_info.codec)
        return -1;

    struct demux_textsub_priv *priv = talloc_zero(demuxer, struct demux_textsub_priv);

    priv->content = stream_read_complete(demuxer->stream, priv, 64 * 1024 * 1024);
    if (priv->content.start == NULL) {
        talloc_free(priv);
        return -1;
    }
    demuxer->priv = priv;

    struct sh_stream *stream = demux_alloc_sh_stream(STREAM_SUB);
    stream->codec->codec = codec_info.codec;
    stream->codec->codec_desc = codec_info.codec_desc;
    demux_add_sh_stream(demuxer, stream);

    demuxer->seekable = true;
    demuxer->fully_read = true;
    demux_close_stream(demuxer);

    return 0;
}

static bool demux_read_packet_textsub(struct demuxer *demuxer, struct demux_packet **packet)
{
    struct demux_textsub_priv *priv = demuxer->priv;

    if (priv->exhausted)
        return false;

    *packet = new_demux_packet_from(demuxer->packet_pool, priv->content.start, priv->content.len);
    if (!*packet)
        return true;

    (*packet)->stream = 0;
    (*packet)->pts = 0.0;
    (*packet)->sub_duration = INFINITY;
    priv->exhausted = true;

    return true;
}

static void demux_seek_textsub(struct demuxer *demuxer, double seek_pts, int flags)
{
    // We only ever emit one packet, no seeking needed or possible.
}

static void demux_switched_tracks_textsub(struct demuxer *demuxer)
{
    struct demux_textsub_priv *ctx = demuxer->priv;
    ctx->exhausted = false;
}


const struct demuxer_desc demuxer_desc_textsub = {
    .name = "textsub",
    .desc = "text subtitle demuxer",
    .open = demux_open_textsub,
    .read_packet = demux_read_packet_textsub,
    .seek = demux_seek_textsub,
    .switched_tracks = demux_switched_tracks_textsub
};
