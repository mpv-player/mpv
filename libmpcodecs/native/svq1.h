
#define SVQ1_BLOCK_SKIP		0
#define SVQ1_BLOCK_INTER	1
#define SVQ1_BLOCK_INTER_4V	2
#define SVQ1_BLOCK_INTRA	3

#define SVQ1_FRAME_INTRA	0
#define SVQ1_FRAME_INTER	1
#define SVQ1_FRAME_DROPPABLE	2

/* motion vector (prediction) */
typedef struct svq1_pmv_s {
  int		 x;
  int		 y;
} svq1_pmv_t;

typedef struct svq1_s {
  int		 frame_code;
  int		 frame_type;
  int		 frame_width;
  int		 frame_height;
  int		 luma_width;
  int		 luma_height;
  int		 chroma_width;
  int		 chroma_height;
  svq1_pmv_t	*motion;
  uint8_t	*current;
  uint8_t	*previous;
  int		 offsets[3];
  int		 reference_frame;
 
  uint8_t	*base[3];
  int		 width;
  int		 height;
} svq1_t;

int svq1_decode_frame (svq1_t *svq1, uint8_t *buffer);
void svq1_free (svq1_t *svq1);
