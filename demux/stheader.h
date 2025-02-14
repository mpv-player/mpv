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

#ifndef MPLAYER_STHEADER_H
#define MPLAYER_STHEADER_H

#include <stdatomic.h>
#include <stdbool.h>

#include "common/common.h"
#include "audio/chmap.h"
#include "video/mp_image.h"

struct MPOpts;
struct demuxer;

// Stream headers:

struct sh_stream {
    enum stream_type type;
    // Index into demuxer->streams.
    int index;
    // Demuxer/format specific ID. Corresponds to the stream IDs as encoded in
    // some file formats (e.g. MPEG), or an index chosen by demux.c.
    int demuxer_id;
    // FFmpeg stream index (AVFormatContext.streams[index]), or equivalent.
    int ff_index;

    struct mp_codec_params *codec;

    char *title;
    char *lang;                 // language code
    bool default_track;         // container default track flag
    bool forced_track;          // container forced track flag
    bool dependent_track;       // container dependent track flag
    bool visual_impaired_track; // container flag
    bool hearing_impaired_track;// container flag
    bool image;                 // video stream is an image
    bool still_image;           // video consists of multiple sparse still images
    int hls_bitrate;
    int program_id;

    struct mp_tags *tags;

    bool missing_timestamps;

    double seek_preroll;

    // stream is a picture (such as album art)
    struct demux_packet *attached_picture;

    // Internal to demux.c
    struct demux_stream *ds;
};

struct mp_codec_params {
    enum stream_type type;

    // E.g. "h264" (usually corresponds to AVCodecDescriptor.name)
    const char *_Atomic codec;

    // Usually corresponds to AVCodecDescriptor.long_name
    const char *_Atomic codec_desc;

    // Corresponding codec profile
    const char *_Atomic codec_profile;

    // E.g. "h264" (usually corresponds to AVCodec.name)
    const char *_Atomic decoder;

    // Usually corresponds to AVCodec.long_name
    const char *_Atomic decoder_desc;

    // Usually a FourCC, exact meaning depends on codec.
    unsigned int codec_tag;

    unsigned char *extradata;   // codec specific per-stream header
    int extradata_size;

    // Codec specific header data (set by demux_{lavf,mkv,raw}, useful to pass
    // through stream global side data)
    struct AVCodecParameters *lav_codecpar;

    // Timestamp granularity for converting double<->rational timestamps.
    int native_tb_num, native_tb_den;

    // Used by an obscure bug workaround mechanism. As an exception to the usual
    // rules, demuxers are allowed to set this after adding the sh_stream, but
    // only before the demuxer open call returns.
    struct demux_packet *first_packet;

    // STREAM_AUDIO
    int samplerate;
    struct mp_chmap channels;
    bool force_channels;
    int bitrate; // compressed bits/sec
    int block_align;
    struct replaygain_data *replaygain_data;

    // STREAM_VIDEO
    bool avi_dts;         // use DTS timing; first frame and DTS is 0
    double fps;           // frames per second (set only if constant fps)
    bool reliable_fps;    // the fps field is definitely not broken
    int par_w, par_h;     // pixel aspect ratio (0 if unknown/square)
    int disp_w, disp_h;   // display size
    int rotate;           // intended display rotation, in degrees, [0, 359]
    int stereo_mode;      // mp_stereo3d_mode (0 if none/unknown)
    struct pl_color_space color; // colorspace info where available
    struct pl_color_repr repr;   // color representation info where available
    struct mp_rect crop;         // crop to be applied

    bool dovi;
    uint8_t dv_profile;
    uint8_t dv_level;

    // STREAM_VIDEO + STREAM_AUDIO
    int bits_per_coded_sample;
    char *format_name;    // pixel format (video) or sample format (audio)

    // STREAM_SUB
    double frame_based;   // timestamps are frame-based (and this is the
                          // fallback framerate used for timestamps)
};

#endif /* MPLAYER_STHEADER_H */
