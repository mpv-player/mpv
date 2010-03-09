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

#ifndef MPLAYER_MPEG_HDR_H
#define MPLAYER_MPEG_HDR_H

typedef struct {
    // video info:
    int mpeg1; // 0=mpeg2  1=mpeg1
    int display_picture_width;
    int display_picture_height;
    int aspect_ratio_information;
    int frame_rate_code;
    float fps;
    int frame_rate_extension_n;
    int frame_rate_extension_d;
    int bitrate; // 0x3FFFF==VBR
    // timing:
    int picture_structure;
    int progressive_sequence;
    int repeat_first_field;
    int progressive_frame;
    int top_field_first;
    int display_time; // secs*100
    //the following are for mpeg4
    unsigned int timeinc_resolution, timeinc_bits, timeinc_unit;
    int picture_type;
} mp_mpeg_header_t;

int mp_header_process_sequence_header (mp_mpeg_header_t * picture, const unsigned char * buffer);
int mp_header_process_extension (mp_mpeg_header_t * picture, unsigned char * buffer);
float mpeg12_aspect_info(mp_mpeg_header_t *picture);
int mp4_header_process_vol(mp_mpeg_header_t * picture, unsigned char * buffer);
void mp4_header_process_vop(mp_mpeg_header_t * picture, unsigned char * buffer);
int h264_parse_sps(mp_mpeg_header_t * picture, unsigned char * buf, int len);
int mp_vc1_decode_sequence_header(mp_mpeg_header_t * picture, unsigned char * buf, int len);

unsigned char mp_getbits(unsigned char *buffer, unsigned int from, unsigned char len);

#endif /* MPLAYER_MPEG_HDR_H */
