
#define AVIWRITE_MAX_STREAMS 16

#define AVIWRITE_TYPE_VIDEO 0
#define AVIWRITE_TYPE_AUDIO 1

typedef struct {
  // muxer data:
  int type;  // audio or video
  int id;    // stream no
  unsigned int ckid; // chunk id (00dc 01wb etc)
  double timer;
  off_t size;
  // buffering:
  unsigned char *buffer;
  unsigned int buffer_size;
  unsigned int buffer_len;
  // source stream:
  void* source; // sh_audio or sh_video
  int codec; // codec used for encoding. 0 means copy
  // avi stream header:
  AVIStreamHeader h;  // Rate/Scale and SampleSize must be filled by caller!
  // stream specific:
  WAVEFORMATEX *wf;
  BITMAPINFOHEADER *bih;   // in format
} aviwrite_stream_t;

typedef struct {
  // encoding:
  MainAVIHeader avih;
  unsigned int movi_start;
  unsigned int movi_end;
  unsigned int file_end;
  // index:
  AVIINDEXENTRY *idx;
  int idx_pos;
  int idx_size;
  // streams:
  //int num_streams;
  aviwrite_stream_t* def_v;  // default video stream (for general headers)
  aviwrite_stream_t* streams[AVIWRITE_MAX_STREAMS];
} aviwrite_t;

aviwrite_stream_t* aviwrite_new_stream(aviwrite_t *muxer,int type);
aviwrite_t* aviwrite_new_muxer();
void aviwrite_write_chunk(aviwrite_t *muxer,aviwrite_stream_t *s, FILE *f,int len,unsigned int flags);
void aviwrite_write_header(aviwrite_t *muxer,FILE *f);
void aviwrite_write_index(aviwrite_t *muxer,FILE *f);



