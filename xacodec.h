typedef struct
{
  unsigned int		what;
  unsigned int		id;
  int			(*iq_func)();	/* init/query function */
  unsigned int		(*dec_func)();  /* opt decode function */
} XAVID_FUNC_HDR;

#define XAVID_WHAT_NO_MORE	0x0000
#define XAVID_AVI_QUERY		0x0001
#define XAVID_QT_QUERY		0x0002
#define XAVID_DEC_FUNC		0x0100

#define XAVID_API_REV		0x0003

typedef struct
{
  unsigned int		api_rev;
  char			*desc;
  char			*rev;
  char			*copyright;
  char			*mod_author;
  char			*authors;
  unsigned int		num_funcs;
  XAVID_FUNC_HDR	*funcs;
} XAVID_MOD_HDR;

/* XA CODEC .. */
typedef struct
{
  void			*anim_hdr;
  unsigned int		compression;
  unsigned int		x, y;
  unsigned int		depth;
  void			*extra;
  unsigned int		xapi_rev;
  unsigned int		(*decoder)();
  char			*description;
  unsigned int		avi_ctab_flag;
  unsigned int		(*avi_read_ext)();
} XA_CODEC_HDR;

#define CODEC_SUPPORTED 1
#define CODEC_UNKNOWN 0
#define CODEC_UNSUPPORTED -1

/* fuckin colormap structures for xanim */
typedef struct
{
  unsigned short	red;
  unsigned short	green;
  unsigned short	blue;
  unsigned short	gray;
} ColorReg;

typedef struct XA_ACTION_STRUCT
{
  int			type;
  int			cmap_rev;
  unsigned char		*data;
  struct XA_ACTION_STRUCT *next;
  struct XA_CHDR_STRUCT	*chdr;
  ColorReg		*h_cmap;
  unsigned int		*map;
  struct XA_ACTION_STRUCT *next_same_chdr;
} XA_ACTION;

typedef struct XA_CHDR_STRUCT
{
  unsigned int		rev;
  ColorReg		*cmap;
  unsigned int		csize, coff;
  unsigned int		*map;
  unsigned int		msize, moff;
  struct XA_CHDR_STRUCT	*next;
  XA_ACTION		*acts;
  struct XA_CHDR_STRUCT	*new_chdr;
} XA_CHDR;

typedef struct
{
  unsigned int		cmd;
  unsigned int		skip_flag;
  unsigned int		imagex, imagey;	/* image buffer size */
  unsigned int		imaged;		/* image depth */
  XA_CHDR		*chdr;		/* color map header */
  unsigned int		map_flag;
  unsigned int		*map;
  unsigned int		xs, ys;
  unsigned int		xe, ye;
  unsigned int		special;
  void			*extra;
} XA_DEC_INFO;

typedef struct
{
    unsigned int	file_num;
    unsigned int	anim_type;
    unsigned int	imagex;
    unsigned int	imagey;
    unsigned int	imagec;
    unsigned int	imaged;
} XA_ANIM_HDR;

// Added by A'rpi
typedef struct {
    unsigned int out_fmt;
    int bpp;
    int width,height;
    unsigned char* planes[3];
    int stride[3];
    unsigned char *mem;
} xacodec_image_t;

int xacodec_init_video(sh_video_t *vidinfo, int out_format);
xacodec_image_t* xacodec_decode_frame(uint8_t *frame, int frame_size, int skip_flag);
int xacodec_exit();

