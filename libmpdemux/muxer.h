
#define MUXER_MAX_STREAMS 16

#define MUXER_TYPE_VIDEO 0
#define MUXER_TYPE_AUDIO 1

#define MUXER_TYPE_AVI 0
#define MUXER_TYPE_MPEG 1
#define MUXER_TYPE_RAWVIDEO 2
#define MUXER_TYPE_LAVF 3
#define MUXER_TYPE_RAWAUDIO 4


typedef struct {
  // muxer data:
  int type;  // audio or video
  int id;    // stream no
  uint32_t ckid; // chunk id (00dc 01wb etc)
  double timer;
  off_t size;
  float aspect; // aspect ratio of this stream (set by ve_*.c)
  // buffering:
  unsigned char *buffer;
  unsigned int buffer_size;
  unsigned int buffer_len;
  // mpeg block buffer:
  unsigned char *b_buffer;
  unsigned int b_buffer_size;	//size of b_buffer
  unsigned int b_buffer_ptr;	//index to next data to write
  unsigned int b_buffer_len;	//len of next data to write
  // muxer frame buffer:
  unsigned int muxbuf_seen;
  // source stream:
  void* source; // sh_audio or sh_video
  int codec; // codec used for encoding. 0 means copy
  // avi stream header:
  AVIStreamHeader h;  // Rate/Scale and SampleSize must be filled by caller!
  // stream specific:
  WAVEFORMATEX *wf;
  BITMAPINFOHEADER *bih;   // in format
  int encoder_delay; // in number of frames
  int decoder_delay; // in number of frames
  // mpeg specific:
  size_t ipb[3]; // sizes of I/P/B frames
  // muxer of that stream
  struct muxer_t *muxer;
  void *priv; // private stream specific data stored by the muxer

  int vbv_size;
  int max_rate;
  int avg_rate;
} muxer_stream_t;

typedef struct {
  uint32_t id;
  char *text;
} muxer_info_t;

typedef struct muxer_t{
  // encoding:
  MainAVIHeader avih;
  off_t movi_start;
  off_t movi_end;
  off_t file_end; // for MPEG it's system timestamp in 1/90000 s
  float audio_delay_fix;
  // index:
  AVIINDEXENTRY *idx;
  int idx_pos;
  int idx_size;
  // streams:
  int num_videos;	// for MPEG recalculations
  int num_audios;
  unsigned int sysrate;	// max rate in bytes/s
  //int num_streams;
  muxer_stream_t* def_v;  // default video stream (for general headers)
  muxer_stream_t* streams[MUXER_MAX_STREAMS];
  // muxer frame buffer:
  struct muxbuf_t * muxbuf;
  int muxbuf_num;
  int muxbuf_skip_buffer;
  // functions:
  stream_t *stream;
  void (*fix_stream_parameters)(muxer_stream_t *);
  void (*cont_write_chunk)(muxer_stream_t *,size_t,unsigned int, double dts, double pts);
  void (*cont_write_header)(struct muxer_t *);
  void (*cont_write_index)(struct muxer_t *);
  muxer_stream_t* (*cont_new_stream)(struct muxer_t *,int);
  void *priv;
} muxer_t;

/* muxer frame buffer */
typedef struct muxbuf_t {
  muxer_stream_t *stream; /* pointer back to corresponding stream */
  double dts; /* decode timestamp / time at which this packet should be feeded into the decoder */
  double pts; /* presentation timestamp / time at which the data in this packet will be presented to the user */
  unsigned char *buffer;
  size_t len;
  unsigned int flags;
} muxbuf_t;

extern char *force_fourcc;

muxer_t *muxer_new_muxer(int type,stream_t *stream);
#define muxer_new_stream(muxer,a) muxer->cont_new_stream(muxer,a)
#define muxer_stream_fix_parameters(muxer, a) muxer->fix_stream_parameters(a)
void muxer_write_chunk(muxer_stream_t *s, size_t len, unsigned int flags, double dts, double pts);
#define muxer_write_header(muxer) muxer->cont_write_header(muxer)
#define muxer_write_index(muxer) muxer->cont_write_index(muxer)

int muxer_init_muxer_avi(muxer_t *);
int muxer_init_muxer_mpeg(muxer_t *);
int muxer_init_muxer_rawvideo(muxer_t *);
int muxer_init_muxer_lavf(muxer_t *);
int muxer_init_muxer_rawaudio(muxer_t *);
