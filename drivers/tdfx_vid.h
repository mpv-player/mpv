

#define TDFX_VID_VERSION 1

#define TDFX_VID_MOVE_2_PACKED  0
#define TDFX_VID_MOVE_2_YUV     1
#define TDFX_VID_MOVE_2_3D      2
#define TDFX_VID_MOVE_2_TEXTURE 3

#define TDFX_VID_FORMAT_BGR1  (('B'<<24)|('G'<<16)|('R'<<8)|1)
#define TDFX_VID_FORMAT_BGR8  (('B'<<24)|('G'<<16)|('R'<<8)|8)
#define TDFX_VID_FORMAT_BGR16 (('B'<<24)|('G'<<16)|('R'<<8)|16)
#define TDFX_VID_FORMAT_BGR24 (('B'<<24)|('G'<<16)|('R'<<8)|24)
#define TDFX_VID_FORMAT_BGR32 (('B'<<24)|('G'<<16)|('R'<<8)|32)

#define TDFX_VID_FORMAT_YUY2 (('2'<<24)|('Y'<<16)|('U'<<8)|'Y')
#define TDFX_VID_FORMAT_UYVY (('U'<<24)|('Y'<<16)|('V'<<8)|'Y')

#define TDFX_VID_FORMAT_YV12 0x32315659
#define TDFX_VID_FORMAT_IYUV (('I'<<24)|('Y'<<16)|('U'<<8)|'V')
#define TDFX_VID_FORMAT_I420 (('I'<<24)|('4'<<16)|('2'<<8)|'0')

#define TDFX_VID_YUV_STRIDE        (1024)
#define TDFX_VID_YUV_PLANE_SIZE    (0x0100000)
                                 

typedef struct tdfx_vid_blit_s {
  uint32_t src;
  uint32_t src_stride;
  uint16_t src_x,src_y;
  uint16_t src_w,src_h;
  uint32_t src_format;

  uint32_t  dst;
  uint32_t dst_stride;
  uint16_t dst_x,dst_y;
  uint16_t dst_w,dst_h;
  uint32_t dst_format;
} tdfx_vid_blit_t;

typedef struct tdfx_vid_config_s {
  uint16_t version;
  uint16_t card_type;
  uint32_t ram_size;
  uint16_t screen_width;
  uint16_t screen_height;
  uint16_t screen_stride;
  uint32_t screen_format;
  uint32_t screen_start;
} tdfx_vid_config_t;

typedef struct tdfx_vid_agp_move_s {
  uint16_t move2;
  uint16_t width,height;

  uint32_t src;
  uint32_t src_stride;

  uint32_t dst;
  uint32_t dst_stride;
} tdfx_vid_agp_move_t;

typedef struct tdfx_vid_yuv_s {
  uint32_t base;
  uint16_t stride;
} tdfx_vid_yuv_t;

#define TDFX_VID_GET_CONFIG _IOR('J', 1, tdfx_vid_config_t)
#define TDFX_VID_AGP_MOVE _IOR('J', 2, tdfx_vid_agp_move_t)
#define TDFX_VID_BLIT _IOR('J', 3, tdfx_vid_blit_t)
#define TDFX_VID_SET_YUV _IOR('J', 4, tdfx_vid_blit_t)
#define TDFX_VID_GET_YUV _IOR('J', 5, tdfx_vid_blit_t)

#define TDFX_VID_BUMP0 _IOR('J', 6, u16)
