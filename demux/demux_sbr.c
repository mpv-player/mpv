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

// This demuxer is specific to the `sd_sbr` subtitle driver and exists
// so that we can take subtitle files like .srv3 or .vtt in their
// full text form and pass them to subrandr in the subtitle driver.
//
// There are two reasons for this:
// - subrandr doesn't currently support parsing packetized streams like
//   what is output by ffmpeg for WebVTT.
// - Demuxing .srv3 is not supported by ffmpeg, it also doesn't really have
//   a standard packetized representation that ffmpeg could demux it to
//   (that I know of).
//
// Note that for now this demuxer only recognizes srv3, this is to avoid
// regressing the behavior of other formats that were previously rendered
// with libass via ffmpeg conversion.

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

#define OPT_BASE_STRUCT struct demux_sbr_opts
struct demux_sbr_opts {
    int probesize;
};

const struct m_sub_options demux_sbr_conf = {
    .opts = (const m_option_t[]) {
        {"probesize", OPT_INT(probesize), M_RANGE(32, INT_MAX)},
        {0}
    },
    .size = sizeof(struct demux_sbr_opts),
    .defaults = &(const struct demux_sbr_opts){
        .probesize = 128,
    },
    .change_flags = UPDATE_DEMUXER,
};

struct format_codec_info {
    const char *codec;
    const char *codec_desc;
};

static const struct format_codec_info fmt_to_codec[] = {
    [SBR_SUBTITLE_FORMAT_UNKNOWN] = {NULL, NULL},
    [SBR_SUBTITLE_FORMAT_SRV3   ] = {"subrandr/srv3", "srv3"},
};

struct sub_codec_ext {
    const char *ext;
    struct format_codec_info codec_info;
};

static const struct sub_codec_ext codec_exts[] = {
    {".srv3", fmt_to_codec[SBR_SUBTITLE_FORMAT_SRV3]},
    {".ytt",  fmt_to_codec[SBR_SUBTITLE_FORMAT_SRV3]},
    {NULL}
};

struct demux_sbr_priv {
    bstr content;
    bool exhausted;
};

static int demux_open_sbr(struct demuxer *demuxer, enum demux_check check)
{
    bstr filename = bstr0(demuxer->filename);
    struct format_codec_info codec_info = {0};
    struct demux_sbr_opts *opts = mp_get_config_group(demuxer, demuxer->global, &demux_sbr_conf);

    for (const struct sub_codec_ext *ext = codec_exts; ext->ext; ++ext) {
        if (bstr_endswith0(filename, ext->ext))
            codec_info = ext->codec_info;
    }

    if (!codec_info.codec) {
        int probe_size = stream_peek(demuxer->stream, opts->probesize);
        uint8_t *probe_buffer = demuxer->stream->buffer;

        sbr_subtitle_format fmt = sbr_probe_text((const char *)probe_buffer, (size_t)probe_size);
        if (fmt < MP_ARRAY_SIZE(fmt_to_codec))
            codec_info = fmt_to_codec[fmt];
    }

    if (check != DEMUX_CHECK_REQUEST && !codec_info.codec)
        return -1;

    struct demux_sbr_priv *priv = talloc_zero(demuxer, struct demux_sbr_priv);

    priv->content = stream_read_complete(demuxer->stream, priv, 64 * 1024 * 1024);
    if (priv->content.start == NULL)
        return -1;
    demuxer->priv = priv;

    struct sh_stream *stream = demux_alloc_sh_stream(STREAM_SUB);
    stream->codec->codec = codec_info.codec;
    stream->codec->codec_desc = codec_info.codec_desc;
    demux_add_sh_stream(demuxer, stream);

    // Note that while in practice seeking on this stream is not possible,
    // if `demuxer->seekable` is `false` then a warning is emitted when one
    // tries to seek in the player which is undesirable. Therefore we mark
    // it as seekable and make seeking a no-op instead.
    demuxer->seekable = true;
    demuxer->fully_read = true;
    demux_close_stream(demuxer);

    return 0;
}

static bool demux_read_packet_sbr(struct demuxer *demuxer, struct demux_packet **packet)
{
    struct demux_sbr_priv *priv = demuxer->priv;

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

static void demux_seek_sbr(struct demuxer *demuxer, double seek_pts, int flags)
{
    // We only ever emit one packet, no seeking needed or possible.
}

static void demux_switched_tracks_sbr(struct demuxer *demuxer)
{
    struct demux_sbr_priv *ctx = demuxer->priv;
    ctx->exhausted = false;
}


const struct demuxer_desc demuxer_desc_sbr = {
    .name = "subrandr",
    .desc = "subrandr text subtitle demuxer",
    .open = demux_open_sbr,
    .read_packet = demux_read_packet_sbr,
    .seek = demux_seek_sbr,
    .switched_tracks = demux_switched_tracks_sbr
};
