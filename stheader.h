// Stream headers:

/*
typedef struct {
  int driver;
    // codec descriptor from codec.conf
} codecinfo_t;
*/

typedef struct {
  demux_stream_t *ds;
  unsigned int format;
  codecs_t *codec;
  // output format:
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
} sh_audio_t;

typedef struct {
  demux_stream_t *ds;
  unsigned int format;
  codecs_t *codec;
  // output format:
  float fps;
  float frametime;  // 1/fps
  int disp_w,disp_h;   // display size (filled by fileformat parser)
//  int coded_w,coded_h; // coded size (filled by video codec)
  unsigned int outfmtidx;
//  unsigned int bitrate;
  // buffers:
  char *our_out_buffer;
  // win32 codec stuff:
  AVIStreamHeader video;
  BITMAPINFOHEADER *bih;   // in format
  BITMAPINFOHEADER o_bih; // out format
  HIC hic;  // handle
} sh_video_t;

sh_audio_t* new_sh_audio(demuxer_t *demuxer,int id);
sh_video_t* new_sh_video(demuxer_t *demuxer,int id);

