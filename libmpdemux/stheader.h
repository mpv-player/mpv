// Stream headers:

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

#ifdef HAVE_OGGVORBIS
#include <math.h>
#include <vorbis/codec.h>
typedef struct {
  ogg_sync_state   oy; /* sync and verify incoming physical bitstream */
  ogg_stream_state os; /* take physical pages, weld into a logical
			  stream of packets */
  ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
  ogg_packet       op; /* one raw packet of data for decode */
  
  vorbis_info      vi; /* struct that stores all the static vorbis bitstream
			  settings */
  vorbis_comment   vc; /* struct that stores all the bitstream user comments */
  vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
  vorbis_block     vb; /* local working space for packet->PCM decode */
} ov_struct_t;
#endif

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
//  ac3_frame_t *ac3_frame;
  void* ac3_frame;
  int pcm_bswap;
#ifdef HAVE_OGGVORBIS
  ov_struct_t *ov; // should be assigned on init
#endif
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
  char *our_out_buffer;
  // win32 codec stuff:
  AVIStreamHeader video;
  BITMAPINFOHEADER *bih;   // in format
  BITMAPINFOHEADER o_bih; // out format
  HIC hic;  // handle
} sh_video_t;

sh_audio_t* new_sh_audio(demuxer_t *demuxer,int id);
sh_video_t* new_sh_video(demuxer_t *demuxer,int id);

