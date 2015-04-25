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

#ifndef MPLAYER_DEC_VIDEO_H
#define MPLAYER_DEC_VIDEO_H

#include <stdbool.h>

#include "demux/stheader.h"
#include "video/hwdec.h"
#include "video/mp_image.h"

struct mp_decoder_list;
struct vo;

struct dec_video {
    struct mp_log *log;
    struct mpv_global *global;
    struct MPOpts *opts;
    struct vf_chain *vfilter;  // video filter chain
    struct vo *vo;  // (still) needed by video_set/get_colors
    const struct vd_functions *vd_driver;
    struct mp_hwdec_info *hwdec_info; // video output hwdec handles
    struct sh_stream *header;

    char *decoder_desc;

    // Used temporarily during decoding (important for format changes)
    struct mp_image *waiting_decoded_mpi;
    struct mp_image_params decoder_output; // last output of the decoder

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

    // There was at least one packet with non-sense timestamps.
    int has_broken_packet_pts; // <0: uninitialized, 0: no problems, 1: broken

    // Final PTS of previously decoded image
    double decoded_pts;

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
int video_vd_control(struct dec_video *d_video, int cmd, void *arg);

int video_reconfig_filters(struct dec_video *d_video,
                           const struct mp_image_params *params);

int video_vf_vo_control(struct dec_video *d_video, int vf_cmd, void *data);

#endif /* MPLAYER_DEC_VIDEO_H */
