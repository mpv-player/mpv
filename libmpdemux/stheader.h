#ifndef __ST_HEADER_H
#define __ST_HEADER_H 1

// for AVIStreamHeader:
#include "wine/avifmt.h"

#ifndef _WAVEFORMATEX_
#define _WAVEFORMATEX_
typedef struct __attribute__((__packed__)) _WAVEFORMATEX {
  WORD   wFormatTag;
  WORD   nChannels;
  DWORD  nSamplesPerSec;
  DWORD  nAvgBytesPerSec;
  WORD   nBlockAlign;
  WORD   wBitsPerSample;
  WORD   cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *NPWAVEFORMATEX, *LPWAVEFORMATEX;
#endif /* _WAVEFORMATEX_ */

#ifndef _BITMAPINFOHEADER_
#define _BITMAPINFOHEADER_
typedef struct __attribute__((__packed__))
{
    int 	biSize;
    int  	biWidth;
    int  	biHeight;
    short 	biPlanes;
    short 	biBitCount;
    int 	biCompression;
    int 	biSizeImage;
    int  	biXPelsPerMeter;
    int  	biYPelsPerMeter;
    int 	biClrUsed;
    int 	biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct {
	BITMAPINFOHEADER bmiHeader;
	int	bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO;
#endif

// Stream headers:

typedef struct {
  demux_stream_t *ds;
  struct codecs_st *codec;
  unsigned int format;
  int inited;
  float timer;		   // value of old a_frame
  // output format:
  int sample_format;
  int samplerate;
  int samplesize;
  int channels;
  int o_bps; // == samplerate*samplesize*channels   (uncompr. bytes/sec)
  int i_bps; // == bitrate  (compressed bytes/sec)
  // in buffers:
  int audio_in_minsize;
  char* a_in_buffer;
  int a_in_buffer_len;
  int a_in_buffer_size;
  // out buffers:
  int audio_out_minsize;
  char* a_buffer;
  int a_buffer_len;
  int a_buffer_size;
  // win32-compatible codec parameters:
  AVIStreamHeader audio;
  WAVEFORMATEX* wf;
  // codec-specific:
  void* context; // codec-specific stuff (usually HANDLE or struct pointer)
  unsigned char* codecdata; // extra header data passed from demuxer to codec
  int codecdata_len;
} sh_audio_t;

typedef struct {
  demux_stream_t *ds;
  struct codecs_st *codec;
  unsigned int format;
  int inited;
  float timer;		  // value of old v_frame
  // frame counters:
  float num_frames;       // number of frames played
  int num_frames_decoded; // number of frames decoded
  // output format: (set by demuxer)
  float fps;              // frames per second (set only if constant fps)
  float frametime;        // 1/fps
  float aspect;           // aspect ratio stored in the file (for prescaling)
  int i_bps;              // == bitrate  (compressed bytes/sec)
  int disp_w,disp_h;      // display size (filled by fileformat parser)
  // output driver/filters: (set by libmpcodecs core)
  unsigned int outfmtidx;
  void* video_out;        // the video_out handle, used for this video stream
  void* vfilter;          // the video filter chain, used for this video stream
  int vf_inited;
  // win32-compatible codec parameters:
  AVIStreamHeader video;
  BITMAPINFOHEADER* bih;
  // codec-specific:
  void* context;   // codec-specific stuff (usually HANDLE or struct pointer)
} sh_video_t;

// demuxer.c:
sh_audio_t* new_sh_audio(demuxer_t *demuxer,int id);
sh_video_t* new_sh_video(demuxer_t *demuxer,int id);
void free_sh_audio(sh_audio_t *sh);
void free_sh_video(sh_video_t *sh);

// video.c:
int video_read_properties(sh_video_t *sh_video);
int video_read_frame(sh_video_t* sh_video,float* frame_time_ptr,unsigned char** start,int force_fps);

#endif
