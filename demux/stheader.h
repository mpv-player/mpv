/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPLAYER_STHEADER_H
#define MPLAYER_STHEADER_H

#include <stdbool.h>

#include "audio/chmap.h"
#include "ms_hdr.h"
struct MPOpts;
struct demuxer;

enum stream_type {
    STREAM_VIDEO,
    STREAM_AUDIO,
    STREAM_SUB,
    STREAM_TYPE_COUNT,
};

// Stream headers:

struct sh_stream {
    enum stream_type type;
    struct demuxer *demuxer;
    // Index into demuxer->streams.
    int index;
    // Demuxer/format specific ID. Corresponds to the stream IDs as encoded in
    // some file formats (e.g. MPEG), or an index chosen by demux.c.
    int demuxer_id;
    // One of these is non-NULL, the others are NULL, depending on the stream
    // type.
    struct sh_audio *audio;
    struct sh_video *video;
    struct sh_sub *sub;

    // E.g. "h264" (usually corresponds to AVCodecDescriptor.name)
    const char *codec;

    // Codec specific header data (set by demux_lavf.c)
    // Other demuxers use sh_audio->wf and sh_video->bih instead.
    struct AVCodecContext *lav_headers;

    char *title;
    char *lang;                 // language code
    bool default_track;         // container default track flag

    // stream is a picture (such as album art)
    struct demux_packet *attached_picture;

    // Human readable description of the running decoder, or NULL
    char *decoder_desc;

    // shouldn't exist type of stuff
    struct MPOpts *opts;

    // Internal to demux.c
    struct demux_stream *ds;
};

#define SH_COMMON                                                       \
    struct sh_stream *gsh;                                              \
    struct MPOpts *opts;                                                \
    /* usually a FourCC, exact meaning depends on gsh->format */        \
    unsigned int format;                                                \
    int initialized;                                                    \
    /* audio: last known pts value in output from decoder               \
     * video: predicted/interpolated PTS of the current frame */        \
    double pts;                                                         \
    /* decoder context */                                               \
    void *context;                                                      \

typedef struct sh_audio {
    SH_COMMON
    // output format:
    int sample_format;
    int samplerate;
    int container_out_samplerate;
    int samplesize;
    struct mp_chmap channels;
    int i_bps; // == bitrate  (compressed bytes/sec)
    // decoder buffers:
    int audio_out_minsize;  // minimal output from decoder may be this much
    char *a_buffer;         // buffer for decoder output
    int a_buffer_len;
    int a_buffer_size;
    struct af_stream *afilter;          // the audio filter stream
    const struct ad_functions *ad_driver;
    // win32-compatible codec parameters:
    WAVEFORMATEX *wf;
    // note codec extradata may be either under "wf" or "codecdata"
    unsigned char *codecdata;
    int codecdata_len;
    int pts_bytes;   // bytes output by decoder after last known pts
} sh_audio_t;

typedef struct sh_video {
    SH_COMMON
    float next_frame_time;
    double last_pts;
    double buffered_pts[32];
    int num_buffered_pts;
    double codec_reordered_pts;
    double prev_codec_reordered_pts;
    int num_reordered_pts_problems;
    double sorted_pts;
    double prev_sorted_pts;
    int num_sorted_pts_problems;
    int pts_assoc_mode;
    // output format: (set by demuxer)
    float fps;            // frames per second (set only if constant fps)
    float aspect;         // aspect ratio stored in the file (for prescaling)
    float stream_aspect;  // aspect ratio in media headers (DVD IFO files)
    int i_bps;            // == bitrate  (compressed bytes/sec)
    int disp_w, disp_h;   // display size (filled by demuxer or decoder)
    struct mp_image_params *vf_input; // video filter input params
    // output driver/filters: (set by libmpcodecs core)
    struct vf_instance *vfilter;  // video filter chain
    const struct vd_functions *vd_driver;
    int vf_initialized;   // -1 failed, 0 not done, 1 done
    // win32-compatible codec parameters:
    BITMAPINFOHEADER *bih;
} sh_video_t;

typedef struct sh_sub {
    SH_COMMON
    unsigned char *extradata;   // extra header data passed from demuxer
    int extradata_len;
    int frame_based;            // timestamps are frame-based
    bool is_utf8;               // if false, subtitle packet charset is unknown
    struct ass_track *track;    // loaded by libass
    struct dec_sub *dec_sub;    // decoder context
} sh_sub_t;

#endif /* MPLAYER_STHEADER_H */
