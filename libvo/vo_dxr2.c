
#include "fastmemcpy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "mp_msg.h"
#include "cfgparser.h"

#ifdef X11_FULLSCREEN
#include "x11_common.h"
#endif

#include <dxr2ioctl.h>

LIBVO_EXTERN (dxr2)

extern char *get_path(char *filename);

extern float monitor_aspect;
extern float movie_aspect;

int dxr2_fd = -1;

static int movie_w,movie_h;
static int playing = 0;
static int last_freq_id = -1;

// vo device used to blank the screen for the overlay init
static  vo_functions_t* sub_vo = NULL;

static uint8_t* sub_img = NULL;
static int sub_x,sub_y,sub_w,sub_h;
static int sub_x_off,sub_y_off;
static int sub_config_count;
static int aspect;

static int use_ol = 1;
static int ol_ratio = 1000;
static char *norm = NULL;
static char *ucode = NULL;
static int ar_mode = DXR2_ASPECTRATIOMODE_LETTERBOX;
static int mv_mode = DXR2_MACROVISION_OFF;
static int _75ire_mode = DXR2_75IRE_OFF;
static int bw_mode = DXR2_BLACKWHITE_OFF;
static int interlaced_mode = DXR2_INTERLACED_ON;
static int pixel_mode = DXR2_PIXEL_CCIR601;
static int iec958_mode = DXR2_IEC958_DECODED;
static int mute_mode = DXR2_AUDIO_MUTE_OFF;
static int ignore_cache = 0;
static int update_cache = 0;

static config_t dxr2_opts[] = {
  { "overlay", &use_ol, CONF_TYPE_FLAG, 0, 0, 1, NULL},
  { "nooverlay", &use_ol, CONF_TYPE_FLAG, 0, 1, 0, NULL},
  { "overlay-ratio", &ol_ratio, CONF_TYPE_INT, CONF_RANGE, 1, 2500, NULL },
  { "ucode", &ucode, CONF_TYPE_STRING,0, 0, 0, NULL},

  { "norm", &norm, CONF_TYPE_STRING,0, 0, 0, NULL},

  { "ar-mode",&ar_mode,  CONF_TYPE_INT, CONF_RANGE,0,2,NULL },

  { "macrovision",&mv_mode,CONF_TYPE_INT,CONF_RANGE,0,3, NULL },

  { "75ire",&_75ire_mode,CONF_TYPE_FLAG, 0, 0, 1, NULL},
  { "no75ire",&_75ire_mode,CONF_TYPE_FLAG, 0, 1, 0, NULL},

  { "bw",&bw_mode,CONF_TYPE_FLAG, 0, 0, 1, NULL},
  { "color",&bw_mode,CONF_TYPE_FLAG, 0, 1, 0, NULL},

  { "interlaced",&interlaced_mode,CONF_TYPE_FLAG, 0, 0, 1, NULL},
  { "nointerlaced",&interlaced_mode,CONF_TYPE_FLAG, 0, 1, 0, NULL},

  { "square-pixel",&pixel_mode,CONF_TYPE_FLAG, 0, 0, 1, NULL},
  { "ccir601-pixel",&pixel_mode,CONF_TYPE_FLAG, 0, 1, 0, NULL},

  { "iec958-encoded",&iec958_mode,CONF_TYPE_FLAG, 0, 0, 1, NULL},
  { "iec958-decoded",&iec958_mode,CONF_TYPE_FLAG, 0, 1, 0, NULL},

  { "mute", &mute_mode,CONF_TYPE_FLAG, 0, 1, 0, NULL},
  { "nomute",&mute_mode,CONF_TYPE_FLAG, 0, 0, 1, NULL},

  { "ignore-cache",&ignore_cache,CONF_TYPE_FLAG, 0, 0, 1, NULL},
  { "update-cache",&update_cache,CONF_TYPE_FLAG, 0, 0, 1, NULL},
  { NULL,NULL, 0, 0, 0, 0, NULL}
};

static config_t dxr2_opt[] = {
  { "dxr2", &dxr2_opts, CONF_TYPE_SUBCONFIG, 0, 0, 0, NULL},
  { NULL,NULL, 0, 0, 0, 0, NULL}
};

void vo_dxr2_register_options(m_config_t* cfg) {
  m_config_register_options(cfg,dxr2_opt);
}

static vo_info_t vo_info = {
  "DXR2 video out",
  "dxr2",
  "Alban Bedel <albeu@free.fr> and Tobias Diedrich <ranma@gmx.at>",
  ""
};

static char *ucodesearchpath[] = {
  "/usr/local/lib/dxr2/dvd12.ux",
  "/usr/lib/dxr2/dvd12.ux",
  "/usr/src/dvd12.ux",
  NULL,
};

#define BUF_SIZE	2048

static unsigned char dxr2buf[BUF_SIZE];
static unsigned int  dxr2bufpos = 0;

static void write_dxr2(void *data, int len)
{
  int w = 0;
  while (len>0) if ((dxr2bufpos+len) <= BUF_SIZE) {
    memcpy(dxr2buf+dxr2bufpos, data, len);
    dxr2bufpos+=len;
    len=0;
  } else {
    int copylen=BUF_SIZE-dxr2bufpos;
    if(copylen > 0) {
      memcpy(dxr2buf+dxr2bufpos, data, copylen);
      dxr2bufpos += copylen;
      data+=copylen;
      len-=copylen;
    }
    w = write(dxr2_fd, dxr2buf, BUF_SIZE);
    if(w < 0) {
      mp_msg(MSGT_VO,MSGL_WARN,"DXR2 : write failed : %s \n",strerror(errno));
      dxr2bufpos = 0;
      break;
    }
    dxr2bufpos -= w;
    if(dxr2bufpos)
      memmove(dxr2buf,dxr2buf + w,dxr2bufpos);
  }
}

static void flush_dxr2()
{
  int w;
  while (dxr2bufpos) {
    w = write(dxr2_fd, dxr2buf, dxr2bufpos);
    if(w < 0) {
      mp_msg(MSGT_VO,MSGL_WARN,"DXR2 : write failed %s \n",strerror(errno));
      dxr2bufpos = 0;
      break;
    }
    dxr2bufpos -= w;
  }
}

#define PACK_MAX_SIZE 2048

static unsigned char pack[PACK_MAX_SIZE];

static unsigned char mpg_header[]={
  0x00, 0x00, 0x01, 0xba, 0x44, 0x00, 0x04, 0x00,
  0x04, 0x01, 0x01, 0x86, 0xa3, 0xf8
};

static unsigned char mpg_eof[]={
  0x00, 0x00, 0x01, 0xb9
};

static void dxr2_send_header(void)
{
  write_dxr2(&mpg_header, sizeof(mpg_header));
}

static void dxr2_send_eof(void)
{
  write_dxr2(&mpg_eof, sizeof(mpg_eof));
}

void dxr2_send_packet(unsigned char* data,int len,int id,int timestamp)
{
  int ptslen=5;

  if(dxr2_fd < 0) {
    mp_msg(MSGT_VO,MSGL_ERR,"DXR2 fd is not valid\n");
    return;
  }

  mp_msg(MSGT_VO,MSGL_DBG2,"DXR2 packet : 0x%x => %d   \n",id,timestamp);
  dxr2_send_header();

  // startcode:
  pack[0]=pack[1]=0;pack[2]=0x01;
  // stream id
  pack[3]=id;

  while(len>0){
    int payload_size=len;  // data + PTS
    if(9+ptslen+payload_size>PACK_MAX_SIZE) payload_size=PACK_MAX_SIZE-(6+ptslen);

    // construct PES header:  (code from ffmpeg's libav)
    // packetsize:
    pack[4]=(3+ptslen+payload_size)>>8;
    pack[5]=(3+ptslen+payload_size)&255;

    pack[6]=0x81;
    if(ptslen){
      int x;
      pack[7]=0x80;
      pack[8]=ptslen;
      // presentation time stamp:
      x=(0x02 << 4) | (((timestamp >> 30) & 0x07) << 1) | 1;
      pack[9]=x;
      x=((((timestamp >> 15) & 0x7fff) << 1) | 1);
      pack[10]=x>>8; pack[11]=x&255;
      x=((((timestamp) & 0x7fff) << 1) | 1);
      pack[12]=x>>8; pack[13]=x&255;
    } else {
      pack[7]=0x00;
      pack[8]=0x00;
    }

    write_dxr2(pack, 9+ptslen);
    write_dxr2(data, payload_size);

    len-=payload_size; data+=payload_size;
    ptslen=0; // store PTS only once, at first packet!
  }
}

void dxr2_send_lpcm_packet(unsigned char* data,int len,int id,unsigned int timestamp,int freq_id)
{
  int arg;
  int ptslen=5;

  if(dxr2_fd < 0) {
    mp_msg(MSGT_VO,MSGL_ERR,"DXR2 fd is not valid\n");
    return;
  }    

  if(last_freq_id != freq_id) {
    ioctl(dxr2_fd, DXR2_IOC_SET_AUDIO_SAMPLE_FREQUENCY, &freq_id);
    last_freq_id = freq_id;
  }

  if (((int) timestamp)<0)
    timestamp=0;

  mp_msg(MSGT_VO,MSGL_DBG2,"dxr2_send_lpcm_packet(timestamp=%d)\n", timestamp);
  // startcode:
  pack[0]=pack[1]=0;pack[2]=0x01;

  // stream id
  pack[3]=0xBD;

  while(len>=4){
    int payload_size;

    payload_size=PACK_MAX_SIZE-6-3-ptslen-7; // max possible data len
    if(payload_size>len) payload_size=len;
    payload_size&=(~3); // align!

    // packetsize:
    pack[4]=(payload_size+3+ptslen+7)>>8;
    pack[5]=(payload_size+3+ptslen+7)&255;

    // stuffing:
    pack[6]=0x81;
    //		pack[7]=0x00; //0x80

    // hdrlen:
    pack[8]=ptslen;

    if(ptslen){
      int x;
      pack[7]=0x80;
      // presentation time stamp:
      x=(0x02 << 4) | (((timestamp >> 30) & 0x07) << 1) | 1;
      pack[9]=x;
      x=((((timestamp >> 15) & 0x7fff) << 1) | 1);
      pack[10]=x>>8; pack[11]=x&255;
      x=((((timestamp) & 0x7fff) << 1) | 1);
      pack[12]=x>>8; pack[13]=x&255;
    } else {
      pack[7]=0x00;
    }

    // ============ LPCM header: (7 bytes) =================
    // Info by mocm@convergence.de

    //	   ID:
    pack[ptslen+9]=id;

    //	   number of frames:
    pack[ptslen+10]=0x07;

    //	   first acces unit pointer, i.e. start of audio frame:
    pack[ptslen+11]=0x00;
    pack[ptslen+12]=0x04;

    //	   audio emphasis on-off                                  1 bit
    //	   audio mute on-off                                      1 bit
    //	   reserved                                               1 bit
    //	   audio frame number                                     5 bit
    pack[ptslen+13]=0x0C;

    //	   quantization word length                               2 bit
    //	   audio sampling frequency (48khz = 0, 96khz = 1)        2 bit
    //	   reserved                                               1 bit
    //	   number of audio channels - 1 (e.g. stereo = 1)         3 bit
    pack[ptslen+14]=1;

    //	   dynamic range control (0x80 if off)
    pack[ptslen+15]=0x80;

    write_dxr2(pack, 6+3+ptslen+7);
    write_dxr2(data, payload_size);

    len-=payload_size; data+=payload_size;
    timestamp+=90000/4*payload_size/48000;
  }
}

void dxr2_send_sub_packet(unsigned char* data,int len,int id,unsigned int timestamp) {
  int ptslen=5;

  if(dxr2_fd < 0) {
    mp_msg(MSGT_VO,MSGL_ERR,"DXR2 fd is not valid\n");
    return;
  }

  if (((int) timestamp)<0)
    timestamp=0;

  mp_msg(MSGT_VO,MSGL_DBG2,"dxr2_send_sub_packet(timestamp=%d)\n", timestamp);
  // startcode:
  pack[0]=pack[1]=0;pack[2]=0x01;

  // stream id
  pack[3]=0xBD;

  while(len>=4){
    int payload_size= PACK_MAX_SIZE-(7+ptslen+3);
    if(payload_size>len) payload_size= len;
    
    pack[4]=(3+ptslen+1+payload_size)>>8;
    pack[5]=(3+ptslen+1+payload_size)&255;

    pack[6]=0x81;
    if(ptslen){
      int x;
      pack[7]=0x80;
      pack[8]=ptslen;
      // presentation time stamp:
      x=(0x02 << 4) | (((timestamp >> 30) & 0x07) << 1) | 1;
      pack[9]=x;
      x=((((timestamp >> 15) & 0x7fff) << 1) | 1);
      pack[10]=x>>8; pack[11]=x&255;
      x=((((timestamp) & 0x7fff) << 1) | 1);
      pack[12]=x>>8; pack[13]=x&255;
    } else {
      pack[7]=0x00;
      pack[8]=0x00;
    }
    pack[ptslen+9] = id;
    
    write_dxr2(pack,7+ptslen+3);
    write_dxr2(data,payload_size);
    len -= payload_size;
    data += payload_size;   
    ptslen = 0;
  }
}

static int dxr2_set_vga_params(dxr2_vgaParams_t* vga,int detect) {
  // Init the overlay, don't ask me how it work ;-)
  dxr2_sixArg_t oc;
  dxr2_oneArg_t om;
  dxr2_twoArg_t win;
  dxr2_fourArg_t crop;

  crop.arg1=0;
  crop.arg2=0;
  crop.arg3=55;
  crop.arg4=300;
  ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_CROPPING, &crop);
  
  oc.arg1 = 0x40;
  oc.arg2 = 0xff;
  oc.arg3 = 0x40;
  oc.arg4 = 0xff;
  oc.arg5 = 0x40;
  oc.arg6 = 0xff;
  ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_COLOUR, &oc);

  om.arg = ol_ratio;
  ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_RATIO,&om);

  win.arg1 = 100;
  win.arg2 = 3;
  ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_POSITION,&win);

  win.arg1 = vo_screenwidth;
  win.arg2 = vo_screenheight;
  ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_DIMENSION,&win);

  om.arg = 3;
  ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_IN_DELAY,&om);

  if(detect) {
    // First we need a white screen
    uint8_t* img = malloc(vo_screenwidth*vo_screenheight*3);
    uint8_t* src[] = { img, NULL, NULL };
    int cc = vo_config_count;
   
    memset(img,255,vo_screenwidth*vo_screenheight*3);
    vo_config_count = sub_config_count;
    if(sub_vo->config(vo_screenwidth,vo_screenheight,vo_screenwidth,vo_screenheight,
		    VOFLAG_FULLSCREEN ,"DXR2 sub vo",IMGFMT_BGR24,NULL) != 0) {
      mp_msg(MSGT_VO,MSGL_WARN,"VO: [dxr2] sub vo config failed => No overlay\n");
      sub_vo->uninit();
      sub_vo = NULL;
      use_ol = 0;
      vo_config_count = cc;
      return 0;
    }
    sub_vo->draw_frame(src);
    sub_vo->flip_page();
    free(img);
    sub_config_count++;
    vo_config_count = cc;
    
    om.arg = DXR2_OVERLAY_WINDOW_COLOUR_KEY;
    ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_MODE,&om);
    
    vga->xScreen = vo_screenwidth;
    vga->yScreen = vo_screenheight;
    vga->hOffWinKey = 100;
    vga->vOffWinKey = 3;
    ioctl(dxr2_fd, DXR2_IOC_CALCULATE_VGA_PARAMETERS, vga);
  }
  ioctl(dxr2_fd, DXR2_IOC_SET_VGA_PARAMETERS,vga);

  return 1;
}

static int dxr2_save_vga_params(dxr2_vgaParams_t* vga,char* name) {
  struct stat s;
  char* p = get_path("dxr2_cache");
  int p_len = strlen(p), name_len = strlen(name);
  char cache_path[p_len + name_len + 2];
  int ret;
  FILE* fd;

  if(stat(p,&s) !=0) {
    mp_msg(MSGT_VO,MSGL_WARN,"VO: [dxr2] No vga cache dir found (%s)\n",strerror(errno));
    if(errno == EACCES) {
      free(p);
      return 0;
    }
    // Try to create the dir
    if(mkdir(p,S_IRWXU) != 0) {
      mp_msg(MSGT_VO,MSGL_ERR,"VO: [dxr2] Unable to create vga cache dir %s (%s)\n",p,strerror(errno));
      free(p);
      return 0;
    }
  }
  sprintf(cache_path,"%s/%s",p,name);
  free(p);
  fd = fopen(cache_path,"w");
  if(fd == NULL) {
    mp_msg(MSGT_VO,MSGL_ERR,"VO: [dxr2] Unable to open cache file %s for writing (%s)\n",cache_path,strerror(errno));
    return 0;
  }

  ret = fprintf(fd,"%d %d %d %d %d %d %d %d %d %d %d\n",
		vga->hOffWinKey,
		vga->vOffWinKey,
		vga->xScreen,
		vga->yScreen,
		vga->hsyncPol,
		vga->vsyncPol,
		vga->blankStart,
		vga->blankWidth,
		vga->hOffset,
		vga->vOffset,
		vga->ratio);

  fclose(fd);
  return ret == 11 ? 1 : 0;
}

static int dxr2_load_vga_params(dxr2_vgaParams_t* vga,char* name) {
  char* p = get_path("dxr2_cache");
  int p_len = strlen(p), name_len = strlen(name);
  char cache_path[p_len + name_len + 2];
  int ret;
  FILE* fd;

  sprintf(cache_path,"%s/%s",p,name);
  free(p);

  fd = fopen(cache_path,"r");
  if(fd == NULL) {
    mp_msg(MSGT_VO,MSGL_ERR,"VO: [dxr2] Unable to open cache file %s for reading (%s)\n",cache_path,strerror(errno));
    return 0;
  }
  ret = fscanf(fd, "%d %d %d %d %d %d %d %d %d %d %d\n",
	       &vga->hOffWinKey,
	       &vga->vOffWinKey,
	       &vga->xScreen,
	       &vga->yScreen,
	       &vga->hsyncPol,
	       &vga->vsyncPol,
	       &vga->blankStart,
	       &vga->blankWidth,
	       &vga->hOffset,
	       &vga->vOffset,
	       &vga->ratio);

  fclose(fd);
  return ret == 11 ? 1 : 0;
}

static int dxr2_setup_vga_params(void) {
  const vo_info_t* vi = sub_vo->get_info();
  dxr2_vgaParams_t vga;

  int loaded = dxr2_load_vga_params(&vga,(char*)vi->short_name);
  if(!dxr2_set_vga_params(&vga,(update_cache || ignore_cache) ? 1 : !loaded ))
    return 0;
  if(!loaded || update_cache)
    dxr2_save_vga_params(&vga,(char*)vi->short_name);
  return 1;
}

static void dxr2_set_overlay_window(void) {
  uint8_t* src[] = { sub_img, NULL, NULL };
  dxr2_twoArg_t win;
  int redisp = 0;
  int cc = vo_config_count;
  vo_config_count = sub_config_count;
  sub_vo->draw_frame(src);
  sub_vo->flip_page();
  vo_config_count = cc;

  if(sub_w != vo_dwidth || sub_h != vo_dheight) {
    int new_aspect = ((1<<16)*vo_dwidth + vo_dheight/2)/vo_dheight;
    sub_w = vo_dwidth;
    sub_h = vo_dheight;
    if(new_aspect > aspect)
      sub_w = (sub_h*aspect + (1<<15))>>16;
    else
      sub_h = ((sub_w<<16) + (aspect>>1)) /aspect;
    sub_x_off = (vo_dwidth-sub_w) / 2;
    sub_y_off = (vo_dheight-sub_h) / 2;
    sub_x = -vo_dx; // Be sure to also replace the overlay
    win.arg1 = sub_w;
    win.arg2 = sub_h;
    mp_msg(MSGT_VO,MSGL_V,"VO: [dxr2] set win size w=%d h=%d and offset x=%d y=%d \n",win.arg1,win.arg2,sub_x_off,sub_y_off);
    ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_DIMENSION, &win);
  }
  
  if(vo_dx != sub_x || vo_dy != sub_y) {
    sub_x = vo_dx;
    sub_y = vo_dy;
    win.arg1 = (sub_x > 0 ? sub_x : 0) + sub_x_off;
    win.arg2 = (sub_y > 0 ? sub_y : 0) + sub_y_off;
    mp_msg(MSGT_VO,MSGL_V,"VO: [dxr2] set pos x=%d y=%d \n",win.arg1,win.arg2);
    ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_POSITION,&win);
  }

}

static uint32_t config(uint32_t s_width, uint32_t s_height, uint32_t width, uint32_t height, uint32_t flags, char *title, uint32_t format, const vo_tune_info_t *info)
{
  int arg;
  dxr2_threeArg_t arg3;

  if(dxr2_fd < 0) {
    mp_msg(MSGT_VO,MSGL_ERR,"DXR2 fd is not valid\n");
    return VO_ERROR;
  }

  if(playing) {
    dxr2_send_eof();
    flush_dxr2();
    ioctl(dxr2_fd, DXR2_IOC_STOP, NULL);
    playing = 0;
  }

  last_freq_id = -1;

  // Video stream setup
  arg3.arg1 = DXR2_STREAM_VIDEO;
  arg3.arg2 = 0;
  ioctl(dxr2_fd, DXR2_IOC_SELECT_STREAM, &arg3);	
  if (vo_fps > 28)
    arg3.arg1 = DXR2_SRC_VIDEO_FREQ_30;
  else arg3.arg1 = DXR2_SRC_VIDEO_FREQ_25;
  arg3.arg2 = s_width;
  arg3.arg3 = s_height;
  ioctl(dxr2_fd, DXR2_IOC_SET_SOURCE_VIDEO_FORMAT, &arg3);
  arg = DXR2_BITSTREAM_TYPE_MPEG_VOB;
  ioctl(dxr2_fd, DXR2_IOC_SET_BITSTREAM_TYPE, &arg);

  // Aspect ratio
  if (1.76 <= movie_aspect && movie_aspect <= 1.80) {
    arg = DXR2_ASPECTRATIO_16_9;
    mp_msg(MSGT_VO,MSGL_V,"VO: [dxr2] source aspect ratio 16:9\n");
  } else {
    arg = DXR2_ASPECTRATIO_4_3;
    mp_msg(MSGT_VO,MSGL_V,"VO: [dxr2] source aspect ratio 4:3\n");
  }
  ioctl(dxr2_fd, DXR2_IOC_SET_SOURCE_ASPECT_RATIO, &arg);
  if (1.76 <= monitor_aspect && monitor_aspect <=1.80) {
    arg = DXR2_ASPECTRATIO_16_9;
    mp_msg(MSGT_VO,MSGL_V,"VO: [dxr2] monitor aspect ratio 16:9\n");
  } else {
    arg = DXR2_ASPECTRATIO_4_3;
    mp_msg(MSGT_VO,MSGL_V,"VO: [dxr2] monitor aspect ratio 4:3\n");
  }
  ioctl(dxr2_fd, DXR2_IOC_SET_OUTPUT_ASPECT_RATIO, &arg);

  arg = ar_mode;
  ioctl(dxr2_fd, DXR2_IOC_SET_ASPECT_RATIO_MODE, &arg);

  // TV setup
  arg = mv_mode;
  ioctl(dxr2_fd, DXR2_IOC_SET_TV_MACROVISION_MODE, &arg);
  arg = _75ire_mode;
  ioctl(dxr2_fd, DXR2_IOC_SET_TV_75IRE_MODE, &arg);
  arg = bw_mode;
  ioctl(dxr2_fd, DXR2_IOC_SET_TV_BLACKWHITE_MODE, &arg);
  arg = interlaced_mode;
  ioctl(dxr2_fd, DXR2_IOC_SET_TV_INTERLACED_MODE, &arg);
  arg = pixel_mode;
  ioctl(dxr2_fd, DXR2_IOC_SET_TV_PIXEL_MODE, &arg);
  
  if (norm) {
    if (strcmp(norm, "ntsc")==0)
      arg = DXR2_OUTPUTFORMAT_NTSC;
    else if (strcmp(norm, "pal")==0) {
      if (vo_fps > 28) {
	mp_msg(MSGT_VO,MSGL_WARN,"VO: [dxr2] you want pal, but we play at 30 fps, selecting pal60 instead\n");
	arg = DXR2_OUTPUTFORMAT_PAL_60;
	norm="pal60";
      } else arg = DXR2_OUTPUTFORMAT_PAL_BDGHI;
    } else if (strcmp(norm, "pal60")==0) {
      if (vo_fps > 28)
	arg = DXR2_OUTPUTFORMAT_PAL_60;
      else {
	mp_msg(MSGT_VO,MSGL_WARN,"VO: [dxr2] you want pal60, but we play at 25 fps, selecting pal instead\n");
	arg = DXR2_OUTPUTFORMAT_PAL_BDGHI;
	norm="pal";
      }
    } else if (strcmp(norm, "palm")==0)
      arg = DXR2_OUTPUTFORMAT_PAL_M;
    else if (strcmp(norm, "paln")==0)
      arg = DXR2_OUTPUTFORMAT_PAL_N;
    else if (strcmp(norm, "palnc")==0)
      arg = DXR2_OUTPUTFORMAT_PAL_Nc;
    else {
      mp_msg(MSGT_VO,MSGL_WARN,"[dxr2] invalid norm %s\n", norm);
      mp_msg(MSGT_VO,MSGL_WARN,"Valid values are ntsc,pal,pal60,palm,paln,palnc\n");
      mp_msg(MSGT_VO,MSGL_WARN,"Using ntsc\n");
      norm="ntsc";
    }
  } else {
    if (vo_fps > 28) {
      arg = DXR2_OUTPUTFORMAT_NTSC;
      norm="ntsc";
    } else {
      arg = DXR2_OUTPUTFORMAT_PAL_BDGHI;
      norm="pal";
    }
  }
  mp_msg(MSGT_VO,MSGL_V,"VO: [dxr2] output norm set to %s\n", norm);
  ioctl(dxr2_fd, DXR2_IOC_SET_TV_OUTPUT_FORMAT, &arg);

  // Subtitles

  arg = DXR2_SUBPICTURE_ON;
  ioctl(dxr2_fd,DXR2_IOC_ENABLE_SUBPICTURE,&arg);
  arg3.arg1 = DXR2_STREAM_SUBPICTURE;
  arg3.arg2 = 0;
  ioctl(dxr2_fd, DXR2_IOC_SELECT_STREAM, &arg3);

  // Audio
  arg = iec958_mode;
  ioctl(dxr2_fd, DXR2_IOC_IEC958_OUTPUT_MODE, &arg);
  arg = DXR2_AUDIO_WIDTH_16;
  ioctl(dxr2_fd, DXR2_IOC_SET_AUDIO_DATA_WIDTH, &arg);
  arg = DXR2_AUDIO_FREQ_48;
  ioctl(dxr2_fd, DXR2_IOC_SET_AUDIO_SAMPLE_FREQUENCY, &arg);
  arg3.arg1 = DXR2_STREAM_AUDIO_LPCM;
  arg3.arg2 = 0;
  ioctl(dxr2_fd, DXR2_IOC_SELECT_STREAM, &arg3);
  arg = 19;
  ioctl(dxr2_fd, DXR2_IOC_SET_AUDIO_VOLUME, &arg);
  arg = mute_mode;
  ioctl(dxr2_fd, DXR2_IOC_AUDIO_MUTE, &arg);

  // Overlay
  while(use_ol) {
    dxr2_twoArg_t win;
    dxr2_oneArg_t om;
    int cc = vo_config_count;
    vo_config_count = sub_config_count;
    if(!dxr2_setup_vga_params()) {
      sub_vo->uninit();
      sub_vo = NULL;
      vo_config_count = cc;
      break;
    }
    if(strcmp(sub_vo->get_info()->short_name,"x11") != 0) {
      sub_vo->uninit();
      vo_config_count = cc;
      sub_vo = NULL;
    } 
 
    while(sub_vo) {
      dxr2_sixArg_t oc;
      int i,sub_flags = VOFLAG_SWSCALE | (flags & VOFLAG_FULLSCREEN);
      if(sub_vo->config(width,height,width,height,sub_flags,
			"MPlayer DXR2 render",IMGFMT_BGR24,NULL) != 0) {
	mp_msg(MSGT_VO,MSGL_WARN,"VO: [dxr2] sub vo config failed => No X11 window\n");
	sub_vo->uninit();
	sub_vo = NULL;
	break;
      }
      sub_config_count++;

      // Feel free to try some other other color and report your results
      oc.arg1 = 0x40;
      oc.arg2 = 0xff;
      oc.arg3 = 0x00;
      oc.arg4 = 0x20;
      oc.arg5 = 0x40;
      oc.arg6 = 0xff;
      ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_COLOUR, &oc);
      
      om.arg = DXR2_OVERLAY_WINDOW_COLOUR_KEY;
      ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_MODE,&om);
      sub_img = malloc(width*height*3);
      for(i = 0 ; i < width*height*3 ; i += 3) {
	sub_img[i] = 255;
	sub_img[i+1] = 0;
	sub_img[i+2] = 255;
      }
      aspect = ((1<<16)*width + height/2)/height;
      sub_w = sub_h = 0;
      dxr2_set_overlay_window();
      break;
    }
    vo_config_count = cc;
    if(!sub_vo) { // Fallback on non windowed overlay
      om.arg = DXR2_OVERLAY_WINDOW_KEY;
      ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_MODE,&om);
      win.arg1 = flags & VOFLAG_FULLSCREEN ? vo_screenwidth : width;
      win.arg2 = flags & VOFLAG_FULLSCREEN ? vo_screenheight : height;
      ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_DIMENSION, &win);
      win.arg1 = (vo_screenwidth - win.arg1) / 2;
      win.arg2 = (vo_screenheight - win.arg2) / 2;
      ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_POSITION,&win);
    }
    break;
  }
  vo_fs = flags & VOFLAG_FULLSCREEN ? 1 : 0;
  movie_w = width;
  movie_h = height;

  // start playing
  if(ioctl(dxr2_fd, DXR2_IOC_PLAY, NULL) == 0) {
    playing = 1;
    return 0;
  } else
    return VO_ERROR;
}

static const vo_info_t* get_info(void)
{
  return &vo_info;
}

static void draw_osd(void)
{
}

static uint32_t draw_frame(uint8_t * src[])
{
  vo_mpegpes_t *p=(vo_mpegpes_t *)src[0];
  if(p->id == 0x1E0) {// Video
    dxr2_send_packet(p->data, p->size, p->id, p->timestamp);
  } else if(p->id == 0x20) // Subtitles
    dxr2_send_sub_packet(p->data, p->size, p->id, p->timestamp);
  return 0;
}

static void flip_page (void)
{
}

static uint32_t draw_slice( uint8_t *srcimg[], int stride[], int w, int h, int x0, int y0 )
{
  return 0;
}


static uint32_t query_format(uint32_t format)
{
  if (format==IMGFMT_MPEGPES)
    return VFCAP_CSP_SUPPORTED|VFCAP_CSP_SUPPORTED_BY_HW|VFCAP_TIMER|VFCAP_SPU;
  return 0;
}


static void query_vaa(vo_vaa_t *vaa) {
  memset(vaa,0,sizeof(vo_vaa_t));
}

static void uninit(void)
{
  mp_msg(MSGT_VO,MSGL_DBG2, "VO: [dxr2] Uninitializing\n" );

  if (dxr2_fd > 0) {
    if(playing) {
      dxr2_send_eof();
      flush_dxr2();
      playing = 0;
    }
    close(dxr2_fd);
    dxr2_fd = -1;
  }
  if(sub_img) {
    free(sub_img);
    sub_img = NULL;
  }
  if(sub_vo) {
    int cc = vo_config_count;
    vo_config_count = sub_config_count;
    sub_vo->uninit();
    sub_vo = NULL;
    vo_config_count = cc;
  }
}


static void check_events(void)
{
  // I'd like to have this done in an x11 independent way
  // It's because of this that we are limited to vo_x11 for windowed overlay :-(
#ifdef X11_FULLSCREEN
  if(sub_vo) {
    int e=vo_x11_check_events(mDisplay);
    if ( !(e&VO_EVENT_RESIZE) && !(e&VO_EVENT_EXPOSE) ) return;
    XSetBackground(mDisplay, vo_gc, 0);
    XClearWindow(mDisplay, vo_window);
    dxr2_set_overlay_window();
  }
#endif
}

static uint32_t preinit(const char *arg) {
  int uCodeFD = -1;
  int uCodeSize;
  dxr2_uCode_t* uCode;
  dxr2_fourArg_t crop;
  int n=0;

  sub_vo = NULL;
  sub_config_count = 0;
  if(use_ol) {
    if (arg) {
      for(n = 0 ; video_out_drivers[n] != NULL ; n++) {
	const vo_info_t* vi = video_out_drivers[n]->get_info();
	if(!vi)
	  continue;
	if(strcasecmp(arg,vi->short_name) == 0)
	  break;
      }
      sub_vo = video_out_drivers[n];
    } else {
      mp_msg(MSGT_VO,MSGL_WARN,"VO: [dxr2] We need a sub driver to initialize the overlay\n");
      use_ol = 0;
    }
  }
  
  if(!sub_vo) {
    if(use_ol)
      mp_msg(MSGT_VO,MSGL_WARN,"VO: [dxr2] Sub driver '%s' not found => no overlay\n",arg);
    use_ol = 0;
  } else {
    if(sub_vo->preinit(NULL) != 0) {
      mp_msg(MSGT_VO,MSGL_WARN,"VO: [dxr2] Sub vo %s preinit failed => no overlay\n",arg);
      sub_vo = NULL;
      use_ol = 0;
    } else
      mp_msg(MSGT_VO,MSGL_V,"VO: [dxr2] Sub vo %s inited\n",arg);
  }

  dxr2_fd = open( "/dev/dxr2", O_WRONLY);
  if( dxr2_fd < 0 ) {
      mp_msg(MSGT_VO,MSGL_V, "VO: [dxr2] Error opening /dev/dxr2 for writing!\n" );
      return VO_ERROR;
  }

  if(ucode)
    uCodeFD = open(ucode, O_RDONLY);
  else for (n=0; ucodesearchpath[n] != NULL; n++) {
    mp_msg(MSGT_VO,MSGL_V,"VO: [dxr2] Looking for microcode in %s... ",
	   ucodesearchpath[n]);
    if ((uCodeFD = open(ucodesearchpath[n], O_RDONLY))>0) {
      mp_msg(MSGT_VO,MSGL_V,"ok\n");
      break;
    } else {
      mp_msg(MSGT_VO,MSGL_V,"failed (%s)\n", strerror(errno));
    }
  }
  if (uCodeFD < 0) {
    mp_msg(MSGT_VO,MSGL_ERR,"VO: [dxr2] Could not open microcode\n");
    return VO_ERROR;
  }

  uCodeSize = lseek(uCodeFD, 0, SEEK_END);
  if ((uCode = malloc(uCodeSize + 4)) == NULL) {

    mp_msg(MSGT_VO,MSGL_FATAL,"VO: [dxr2] Could not allocate memory for uCode: %s\n", strerror(errno));
    return VO_ERROR;
  }
  lseek(uCodeFD, 0, SEEK_SET);
  if (read(uCodeFD, uCode+4, uCodeSize) != uCodeSize) {

    mp_msg(MSGT_VO,MSGL_ERR,"VO: [dxr2] Could not read uCode uCode: %s\n", strerror(errno));
    return VO_ERROR;
  }
  close(uCodeFD);
  uCode->uCodeLength = uCodeSize;

  // upload ucode
  ioctl(dxr2_fd, DXR2_IOC_INIT_ZIVADS, uCode);

  // reset card
  ioctl(dxr2_fd, DXR2_IOC_RESET, NULL);
  playing = 0;

  if(!use_ol) {
    crop.arg1=0;
    crop.arg2=0;
    crop.arg3=0;
    crop.arg4=0;
    ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_CROPPING, &crop);
  }
  return 0;
}

static uint32_t control(uint32_t request, void *data, ...)
{
  switch (request) {
  case VOCTRL_QUERY_FORMAT:
    return query_format(*((uint32_t*)data));
  case VOCTRL_PAUSE:
    ioctl(dxr2_fd,DXR2_IOC_PAUSE, NULL);
    return VO_TRUE;
  case VOCTRL_RESUME:
    ioctl(dxr2_fd, DXR2_IOC_PLAY, NULL);
    return VO_TRUE;
  case VOCTRL_RESET:
    flush_dxr2();
    ioctl(dxr2_fd, DXR2_IOC_PLAY, NULL);
    return VO_TRUE;
  case VOCTRL_FULLSCREEN:
    if(!use_ol)
      return VO_NOTIMPL;
    else if(sub_vo) 
      return sub_vo->control(VOCTRL_FULLSCREEN,0);
    else {
      dxr2_twoArg_t win;
      vo_fs = !vo_fs;
      win.arg1 = vo_fs ? vo_screenwidth : movie_w;
      win.arg2 = vo_fs ? vo_screenheight : movie_h;
      ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_DIMENSION, &win);
      win.arg1 = (vo_screenwidth - win.arg1) / 2;
      win.arg2 = (vo_screenheight - win.arg2) / 2;
      ioctl(dxr2_fd, DXR2_IOC_SET_OVERLAY_POSITION,&win);
      return VO_TRUE;
    }
  case VOCTRL_SET_SPU_PALETTE: { 
    if(ioctl(dxr2_fd,DXR2_IOC_SET_SUBPICTURE_PALETTE,data) < 0) {
      mp_msg(MSGT_VO,MSGL_WARN,"VO: [dxr2] SPU palette loading failed\n");
      return VO_ERROR;
    }
    return VO_TRUE; 
  } 
  case VOCTRL_QUERY_VAA:
    query_vaa((vo_vaa_t*)data);
    return VO_TRUE;
  }
  return VO_NOTIMPL;
}
