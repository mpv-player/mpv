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

#ifndef MPLAYER_DEC_VIDEO_H
#define MPLAYER_DEC_VIDEO_H

#include <stdbool.h>

#include "demux/stheader.h"
#include "video/hwdec.h"
#include "video/mp_image.h"

struct osd_state;
struct mp_decoder_list;

struct dec_video {
    struct MPOpts *opts;
    struct vf_instance *vfilter;  // video filter chain
    const struct vd_functions *vd_driver;
    int vf_initialized;   // -1 failed, 0 not done, 1 done
    long vf_reconfig_count; // incremented each mpcodecs_reconfig_vo() call
    struct mp_image_params vf_input; // video filter input params
    struct mp_hwdec_info hwdec_info; // video output hwdec handles
    struct sh_stream *header;

    char *decoder_desc;

    void *priv; // for free use by vd_driver

    // Last PTS from decoder (set with each vd_driver->decode() call)
    double codec_pts;
    int num_codec_pts_problems;

    // Last packet DTS from decoder (passed through from source packets)
    double codec_dts;
    int num_codec_dts_problems;

    // PTS sorting (obscure, non-default)
    double buffered_pts[32];
    int num_buffered_pts;
    double sorted_pts;
    int num_sorted_pts_problems;
    double unsorted_pts;
    int num_unsorted_pts_problems;
    int pts_assoc_mode;

    // PTS or DTS of packet last read
    double last_packet_pdts;

    // Final PTS of previously decoded image
    double decoded_pts;

    // PTS of the last decoded frame (often overwritten by player)
    double pts;

    float stream_aspect;  // aspect ratio in media headers (DVD IFO files)
    int i_bps;            // == bitrate  (compressed bytes/sec)
    float fps;            // FPS from demuxer or from user override
    float initial_decoder_aspect;

    // State used only by player/video.c
    double last_pts;
};

struct mp_decoder_list *video_decoder_list(void);

bool video_init_best_codec(struct dec_video *d_video, char* video_decoders);
void video_uninit(struct dec_video *d_video);

struct demux_packet;
struct mp_image *video_decode(struct dec_video *d_video,
                              struct demux_packet *packet,
                              int drop_frame);

int video_get_colors(struct dec_video *d_video, const char *item, int *value);
int video_set_colors(struct dec_video *d_video, const char *item, int value);
void video_reset_decoding(struct dec_video *d_video);
void video_reinit_vo(struct dec_video *d_video);
int video_vd_control(struct dec_video *d_video, int cmd, void *arg);

#endif /* MPLAYER_DEC_VIDEO_H */
