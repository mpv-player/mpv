
#define MUXER_MAX_STREAMS 16

#define MUXER_TYPE_VIDEO 0
#define MUXER_TYPE_AUDIO 1

#define MUXER_TYPE_AVI 0
#define MUXER_TYPE_MPEG 1

#define MUXER_MPEG_BLOCKSIZE 2048	// 2048 or 2324 - ?

typedef struct {
  // muxer data:
  int type;  // audio or video
  int id;    // stream no
  uint32_t ckid; // chunk id (00dc 01wb etc)
  double timer;
  off_t size;
  // buffering:
  unsigned char *buffer;
  unsigned int buffer_size;
  unsigned int buffer_len;
  // mpeg block buffer:
  unsigned char *b_buffer;
  unsigned int b_buffer_ptr;
  // source stream:
  void* source; // sh_audio or sh_video
  int codec; // codec used for encoding. 0 means copy
  // avi stream header:
  AVIStreamHeader h;  // Rate/Scale and SampleSize must be filled by caller!
  // stream specific:
  WAVEFORMATEX *wf;
  BITMAPINFOHEADER *bih;   // in format
  // mpeg specific:
  unsigned int gop_start; // frame number of this GOP start
  size_t ipb[3]; // sizes of I/P/B frames
  // muxer of that stream
  struct muxer_t *muxer;
} muxer_stream_t;

typedef struct {
  uint32_t id;
  char *text;
} muxer_info_t;

typedef struct muxer_t{
  // encoding:
  MainAVIHeader avih;
  unsigned int movi_start;
  unsigned int movi_end;
  unsigned int file_end; // for MPEG it's system timestamp in 1/90000 s
  // index:
  AVIINDEXENTRY *idx;
  int idx_pos;
  int idx_size;
  // streams:
  int num_videos;	// for MPEG recalculations
  unsigned int sysrate;	// max rate in bytes/s
  //int num_streams;
  muxer_stream_t* def_v;  // default video stream (for general headers)
  muxer_stream_t* streams[MUXER_MAX_STREAMS];
  void (*cont_write_chunk)(muxer_stream_t *,size_t,unsigned int);
  void (*cont_write_header)(struct muxer_t *);
  void (*cont_write_index)(struct muxer_t *);
  muxer_stream_t* (*cont_new_stream)(struct muxer_t *,int);
  FILE* file;
} muxer_t;

muxer_t *muxer_new_muxer(int type,FILE *);
#define muxer_new_stream(muxer,a) muxer->cont_new_stream(muxer,a)
#define muxer_write_chunk(a,b,c) a->muxer->cont_write_chunk(a,b,c)
#define muxer_write_header(muxer) muxer->cont_write_header(muxer)
#define muxer_write_index(muxer) muxer->cont_write_index(muxer)

void muxer_init_muxer_avi(muxer_t *);
void muxer_init_muxer_mpeg(muxer_t *);

