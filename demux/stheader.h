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

#ifndef MPLAYER_STHEADER_H
#define MPLAYER_STHEADER_H

#include <stdbool.h>

#include "common/common.h"
#include "audio/chmap.h"

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
    // One of these is non-NULL, the others are NULL, depending on the stream
    // type.
    struct sh_audio *audio;
    struct sh_video *video;
    struct sh_sub *sub;

    // E.g. "h264" (usually corresponds to AVCodecDescriptor.name)
    const char *codec;

    // Usually a FourCC, exact meaning depends on codec.
    unsigned int format;

    // Codec specific header data (set by demux_lavf.c only)
    struct AVCodecContext *lav_headers;

    char *title;
    char *lang;                 // language code
    bool default_track;         // container default track flag
    int hls_bitrate;

    bool missing_timestamps;

    // stream is a picture (such as album art)
    struct demux_packet *attached_picture;

    // Internal to demux.c
    struct demux_stream *ds;
};

typedef struct sh_audio {
    int samplerate;
    struct mp_chmap channels;
    bool force_channels;
    int bitrate; // compressed bits/sec
    int block_align;
    int bits_per_coded_sample;
    unsigned char *codecdata;
    int codecdata_len;
    struct replaygain_data *replaygain_data;
} sh_audio_t;

typedef struct sh_video {
    bool avi_dts;         // use DTS timing; first frame and DTS is 0
    float fps;            // frames per second (set only if constant fps)
    float aspect;         // aspect ratio stored in the file (for prescaling)
    int bits_per_coded_sample;
    unsigned char *extradata;
    int extradata_len;
    int disp_w, disp_h;   // display size
    int rotate;           // intended display rotation, in degrees, [0, 359]
    int stereo_mode;      // mp_stereo3d_mode (0 if none/unknown)
} sh_video_t;

typedef struct sh_sub {
    unsigned char *extradata;   // extra header data passed from demuxer
    int extradata_len;
    double frame_based;         // timestamps are frame-based (and this is the
                                // fallback framerate used for timestamps)
    bool is_utf8;               // if false, subtitle packet charset is unknown
    struct dec_sub *dec_sub;    // decoder context
} sh_sub_t;

#endif /* MPLAYER_STHEADER_H */
