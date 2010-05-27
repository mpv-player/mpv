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

#include "demuxer.h"
#include "aviheader.h"
#include "ms_hdr.h"

// Stream headers:

#define SH_COMMON \
  demux_stream_t *ds; \
  struct codecs *codec; \
  unsigned int format; \
  int initialized; \
  float stream_delay; /* number of seconds stream should be delayed (according to dwStart or similar) */ \
  /* things needed for parsing */ \
  int needs_parsing; \
  struct AVCodecContext *avctx; \
  struct AVCodecParserContext *parser; \
  /* audio: last known pts value in output from decoder \
   * video: predicted/interpolated PTS of the current frame */ \
  double pts; \
  /* codec-specific: */ \
  void* context;   /* codec-specific stuff (usually HANDLE or struct pointer) */ \
  char* lang; /* track language */ \
  int default_track; \

typedef struct sh_common {
  SH_COMMON
} sh_common_t;

typedef struct sh_audio {
  SH_COMMON
  int aid;
  // output format:
  int sample_format;
  int samplerate;
  int samplesize;
  int channels;
  int o_bps; // == samplerate*samplesize*channels   (uncompr. bytes/sec)
  int i_bps; // == bitrate  (compressed bytes/sec)
  // in buffers:
  int audio_in_minsize;	// max. compressed packet size (== min. in buffer size)
  char* a_in_buffer;
  int a_in_buffer_len;
  int a_in_buffer_size;
  // decoder buffers:
  int audio_out_minsize; // max. uncompressed packet size (==min. out buffsize)
  char* a_buffer;
  int a_buffer_len;
  int a_buffer_size;
  // output buffers:
  char* a_out_buffer;
  int a_out_buffer_len;
  int a_out_buffer_size;
//  void* audio_out;        // the audio_out handle, used for this audio stream
  struct af_stream *afilter;          // the audio filter stream
  struct ad_functions *ad_driver;
#ifdef CONFIG_DYNAMIC_PLUGINS
  void *dec_handle;
#endif
  // win32-compatible codec parameters:
  AVIStreamHeader audio;
  WAVEFORMATEX* wf;
  // codec-specific:
  unsigned char* codecdata; // extra header data passed from demuxer to codec
  int codecdata_len;
  int pts_bytes; // bytes output by decoder after last known pts
} sh_audio_t;

typedef struct sh_video {
  SH_COMMON
  int vid;
  float timer;		  // absolute time in video stream, since last start/seek
  // frame counters:
  float num_frames;       // number of frames played
  int num_frames_decoded; // number of frames decoded
  // timing (mostly for mpeg):
  double i_pts;   // PTS for the _next_ I/P frame
  float next_frame_time;
  double last_pts;
  double buffered_pts[20];
  int num_buffered_pts;
  // output format: (set by demuxer)
  float fps;              // frames per second (set only if constant fps)
  float frametime;        // 1/fps
  float aspect;           // aspect ratio stored in the file (for prescaling)
  float stream_aspect;  // aspect ratio stored in the media headers (e.g. in DVD IFO files)
  int i_bps;              // == bitrate  (compressed bytes/sec)
  int disp_w,disp_h;      // display size (filled by fileformat parser)
  // output driver/filters: (set by libmpcodecs core)
  unsigned int outfmtidx;
  struct vf_instance *vfilter;          // the video filter chain, used for this video stream
  int vf_initialized;
#ifdef CONFIG_DYNAMIC_PLUGINS
  void *dec_handle;
#endif
  // win32-compatible codec parameters:
  AVIStreamHeader video;
  BITMAPINFOHEADER* bih;
  void* ImageDesc; // for quicktime codecs
} sh_video_t;

typedef struct sh_sub {
  SH_COMMON
  int sid;
  char type;                    // t = text, v = VobSub, a = SSA/ASS
  unsigned char* extradata; // extra header data passed from demuxer
  int extradata_len;
#ifdef CONFIG_ASS
  ass_track_t* ass_track;  // for SSA/ASS streams (type == 'a')
#endif
} sh_sub_t;

// demuxer.c:
#define new_sh_audio(d, i) new_sh_audio_aid(d, i, i)
sh_audio_t* new_sh_audio_aid(demuxer_t *demuxer,int id,int aid);
#define new_sh_video(d, i) new_sh_video_vid(d, i, i)
sh_video_t* new_sh_video_vid(demuxer_t *demuxer,int id,int vid);
#define new_sh_sub(d, i) new_sh_sub_sid(d, i, i)
sh_sub_t *new_sh_sub_sid(demuxer_t *demuxer, int id, int sid);
void free_sh_audio(demuxer_t *demuxer, int id);
void free_sh_video(sh_video_t *sh);

// video.c:
int video_read_properties(sh_video_t *sh_video);
int video_read_frame(sh_video_t* sh_video,float* frame_time_ptr,unsigned char** start,int force_fps);

#endif /* MPLAYER_STHEADER_H */
