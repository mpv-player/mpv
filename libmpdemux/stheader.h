#ifndef __ST_HEADER_H
#define __ST_HEADER_H 1

// Stream headers:

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

#include "../mp_image.h"

typedef struct {
  demux_stream_t *ds;
  unsigned int format;
  struct codecs_st *codec;
  int inited;
  // output format:
  float timer;		   // value of old a_frame
  int samplerate;
  int samplesize;
  int channels;
  int o_bps; // == samplerate*samplesize*channels   (uncompr. bytes/sec)
  int i_bps; // == bitrate  (compressed bytes/sec)
  // in buffers:
  char* a_in_buffer;
  int a_in_buffer_len;
  int a_in_buffer_size;
  // out buffers:
  char* a_buffer;
  int a_buffer_len;
  int a_buffer_size;
  int sample_format;
  // win32 codec stuff:
  AVIStreamHeader audio;
  WAVEFORMATEX *wf;
//  char wf_ext[64];     // in format
  WAVEFORMATEX o_wf;   // out format
  HACMSTREAM srcstream;  // handle
  int audio_in_minsize;
  int audio_out_minsize;
  // other codecs:
  void* context; // codec-specific stuff (usually HANDLE or struct pointer)
//  ac3_frame_t *ac3_frame;
  void* ac3_frame;  // TODO: use *context
  int pcm_bswap;
#ifdef HAVE_OGGVORBIS
  struct ov_struct_st *ov; // should be assigned on init  TODO: use *context
#endif
  unsigned char *codecdata;
  int codecdata_len;
} sh_audio_t;

typedef struct {
  demux_stream_t *ds;
  unsigned int format;
  struct codecs_st *codec;
  int inited;
  // output format:
  float timer;		   // value of old v_frame
  float fps;
  float frametime;  // 1/fps
  int i_bps; // == bitrate  (compressed bytes/sec)
  int disp_w,disp_h;   // display size (filled by fileformat parser)
//  int coded_w,coded_h; // coded size (filled by video codec)
  float aspect;
  unsigned int outfmtidx;
//  unsigned int bitrate;
  // buffers:
  float num_frames;       // number of frames played
  int num_frames_decoded;       // number of frames decoded
  unsigned our_out_buffer_size;
  char *our_out_buffer;
  mp_image_t *image;
  // win32 codec stuff:
  AVIStreamHeader video;
  BITMAPINFOHEADER *bih;   // in format
  BITMAPINFOHEADER o_bih; // out format
  void* context; // codec-specific stuff (usually HANDLE or struct pointer)
  HIC hic;  // handle
  void* video_out;
  void* vfilter;
} sh_video_t;

sh_audio_t* get_sh_audio(demuxer_t *demuxer,int id);
sh_video_t* get_sh_video(demuxer_t *demuxer,int id);
sh_audio_t* new_sh_audio(demuxer_t *demuxer,int id);
sh_video_t* new_sh_video(demuxer_t *demuxer,int id);
void free_sh_audio(sh_audio_t *sh);
void free_sh_video(sh_video_t *sh);

int video_read_properties(sh_video_t *sh_video);
int video_read_frame(sh_video_t* sh_video,float* frame_time_ptr,unsigned char** start,int force_fps);

#endif
