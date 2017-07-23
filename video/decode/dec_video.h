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
 *
 * Almost LGPL.
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
    struct mp_hwdec_devices *hwdec_devs; // video output hwdec handles
    struct sh_stream *header;
    struct mp_codec_params *codec;
    struct vo *vo; // required for direct rendering into video memory

    char *decoder_desc;

    float fps;            // FPS from demuxer or from user override

    int dropped_frames;

    struct mp_recorder_sink *recorder_sink;

    // Internal (shared with vd_lavc.c).

    void *priv; // for free use by vd_driver

    // Strictly internal (dec_video.c).

    // Last PTS from decoder (set with each vd_driver->decode() call)
    double codec_pts;
    int num_codec_pts_problems;

    // Last packet DTS from decoder (passed through from source packets)
    double codec_dts;
    int num_codec_dts_problems;

    // PTS or DTS of packet first read
    double first_packet_pdts;

    // There was at least one packet with non-sense timestamps.
    int has_broken_packet_pts; // <0: uninitialized, 0: no problems, 1: broken

    int has_broken_decoded_pts;

    // Final PTS of previously decoded image
    double decoded_pts;

    struct mp_image_params dec_format, last_format, fixed_format;

    double start_pts;
    double start, end;
    struct demux_packet *new_segment;
    struct demux_packet *packet;
    bool framedrop_enabled;
    struct mp_image *current_mpi;
    int current_state;
};

struct mp_decoder_list *video_decoder_list(void);

bool video_init_best_codec(struct dec_video *d_video);
void video_uninit(struct dec_video *d_video);

void video_work(struct dec_video *d_video);
int video_get_frame(struct dec_video *d_video, struct mp_image **out_mpi);

void video_set_framedrop(struct dec_video *d_video, bool enabled);
void video_set_start(struct dec_video *d_video, double start_pts);

int video_vd_control(struct dec_video *d_video, int cmd, void *arg);
void video_reset(struct dec_video *d_video);
void video_reset_params(struct dec_video *d_video);
void video_get_dec_params(struct dec_video *d_video, struct mp_image_params *p);

#endif /* MPLAYER_DEC_VIDEO_H */
