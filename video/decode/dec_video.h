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
    const struct vd_functions *vd_driver;
    struct mp_hwdec_info *hwdec_info; // video output hwdec handles
    struct sh_stream *header;

    char *decoder_desc;

    float fps;            // FPS from demuxer or from user override

    int dropped_frames;

    // Internal (shared with vd_lavc.c).

    void *priv; // for free use by vd_driver

    // Strictly internal (dec_video.c).

    // Last PTS from decoder (set with each vd_driver->decode() call)
    double codec_pts;
    int num_codec_pts_problems;

    // Last packet DTS from decoder (passed through from source packets)
    double codec_dts;
    int num_codec_dts_problems;

    // PTS sorting (needed for AVI-style timestamps)
    double buffered_pts[128];
    int num_buffered_pts;

    // PTS or DTS of packet first read
    double first_packet_pdts;

    // There was at least one packet with non-sense timestamps.
    int has_broken_packet_pts; // <0: uninitialized, 0: no problems, 1: broken

    // Final PTS of previously decoded image
    double decoded_pts;

    struct mp_image_params last_format, fixed_format;
    float initial_decoder_aspect;

    double start_pts;
    bool framedrop_enabled;
    struct mp_image *cover_art_mpi;
    struct mp_image *current_mpi;
    int current_state;
};

struct mp_decoder_list *video_decoder_list(void);

bool video_init_best_codec(struct dec_video *d_video, char* video_decoders);
void video_uninit(struct dec_video *d_video);

void video_work(struct dec_video *d_video);
int video_get_frame(struct dec_video *d_video, struct mp_image **out_mpi);

void video_set_framedrop(struct dec_video *d_video, bool enabled);
void video_set_start(struct dec_video *d_video, double start_pts);

int video_vd_control(struct dec_video *d_video, int cmd, void *arg);
void video_reset(struct dec_video *d_video);
void video_reset_aspect(struct dec_video *d_video);

#endif /* MPLAYER_DEC_VIDEO_H */
