#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/stat.h>

#include "config.h"

#include "mp_msg.h"
#include "help_mp.h"

#include "codec-cfg.h"
//#include "stream/stream.h"
//#include "libmpdemux/demuxer.h"
//#include "libmpdemux/stheader.h"

#include "loader/loader.h"
//#include "loader/wine/mmreg.h"
#include "loader/wine/vfw.h"
#include "loader/wine/avifmt.h"

#include "img_format.h"
#include "mp_image.h"
#include "vf.h"

#include "stream/stream.h"
#include "libmpdemux/muxer.h"

//===========================================================================//

static char *vfw_param_codec = NULL;
static char *vfw_param_compdata = NULL;

#include "m_option.h"

m_option_t vfwopts_conf[]={
    {"codec", &vfw_param_codec, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {"compdata", &vfw_param_compdata, CONF_TYPE_STRING, 0, 0, 0, NULL},
    {NULL, NULL, 0, 0, 0, 0, NULL}
};

struct vf_priv_s {
    muxer_stream_t* mux;
    BITMAPINFOHEADER* bih;
};

static HIC encoder_hic;
static void* encoder_buf=NULL;
static int encoder_buf_size=0;
static int encoder_frameno=0;

//int init_vfw_encoder(char *dll_name, BITMAPINFOHEADER *input_bih, BITMAPINFOHEADER *output_bih)
static BITMAPINFOHEADER* vfw_open_encoder(char *dll_name, char *compdatafile, BITMAPINFOHEADER *input_bih,unsigned int out_fourcc)
{
  HRESULT ret;
  BITMAPINFOHEADER* output_bih=NULL;
  int temp_len;
  FILE *fd=NULL;
  char *drvdata=NULL;
  struct stat st;

//sh_video = malloc(sizeof(sh_video_t));

  mp_msg(MSGT_WIN32,MSGL_V,"======= Win32 (VFW) VIDEO Encoder init =======\n");

//  memset(&sh_video->o_bih, 0, sizeof(BITMAPINFOHEADER));
//  output_bih->biSize = sizeof(BITMAPINFOHEADER);

//  encoder_hic = ICOpen( 0x63646976, out_fourcc, ICMODE_COMPRESS);
    encoder_hic = ICOpen( (long) dll_name, out_fourcc, ICMODE_COMPRESS);
  if(!encoder_hic){
    mp_msg(MSGT_WIN32,MSGL_ERR,"ICOpen failed! unknown codec / wrong parameters?\n");
    return NULL;
  }
  mp_msg(MSGT_WIN32,MSGL_INFO,"HIC: %x\n", encoder_hic);

#if 1
{
  ICINFO icinfo;

  ret = ICGetInfo(encoder_hic, &icinfo, sizeof(ICINFO));
  mp_msg(MSGT_WIN32,MSGL_INFO,"%ld - %ld - %d\n", ret, icinfo.dwSize, sizeof(ICINFO));
  mp_msg(MSGT_WIN32,MSGL_INFO,MSGTR_MPCODECS_CompressorType, icinfo.fccType);
  mp_msg(MSGT_WIN32,MSGL_INFO,MSGTR_MPCODECS_CompressorSubtype, icinfo.fccHandler);
  mp_msg(MSGT_WIN32,MSGL_INFO,MSGTR_MPCODECS_CompressorFlags,
    icinfo.dwFlags, icinfo.dwVersion, icinfo.dwVersionICM);
//printf("Compressor name: %s\n", icinfo.szName);
//printf("Compressor description: %s\n", icinfo.szDescription);

mp_msg(MSGT_WIN32,MSGL_INFO,MSGTR_MPCODECS_Flags);
if (icinfo.dwFlags & VIDCF_QUALITY)
    mp_msg(MSGT_WIN32,MSGL_INFO,MSGTR_MPCODECS_Quality);
if (icinfo.dwFlags & VIDCF_FASTTEMPORALD)
    mp_msg(MSGT_WIN32,MSGL_INFO," fast-decompr");
if (icinfo.dwFlags & VIDCF_QUALITYTIME)
    mp_msg(MSGT_WIN32,MSGL_INFO," temp-quality");
mp_msg(MSGT_WIN32,MSGL_INFO,"\n");
}
#endif

  if(compdatafile){
    if (!strncmp(compdatafile, "dialog", 6)){
      if (ICSendMessage(encoder_hic, ICM_CONFIGURE, -1, 0) != ICERR_OK){
        mp_msg(MSGT_WIN32,MSGL_ERR,"Compressor doesn't have a configure dialog!\n");
        return NULL;
      }
      if (ICSendMessage(encoder_hic, ICM_CONFIGURE, 0, 0) != ICERR_OK){
        mp_msg(MSGT_WIN32,MSGL_ERR,"Compressor configure dialog failed!\n");
        return NULL;
      }
    }
    else {
      if (stat(compdatafile, &st) < 0){
        mp_msg(MSGT_WIN32,MSGL_ERR,"Compressor data file not found!\n");
        return NULL;
      }
      fd = fopen(compdatafile, "rb");
      if (!fd){
        mp_msg(MSGT_WIN32,MSGL_ERR,"Cannot open Compressor data file!\n");
        return NULL;
      }
      drvdata = (char *) malloc(st.st_size);
      if (fread(drvdata, st.st_size, 1, fd) != 1) {
        mp_msg(MSGT_WIN32,MSGL_ERR,"Cannot read Compressor data file!\n");
        fclose(fd);
        free(drvdata);
        return NULL;
      }
      fclose(fd);
      mp_msg(MSGT_WIN32,MSGL_ERR,"Compressor data %d bytes\n", st.st_size);
      if (!(temp_len = (unsigned int) ICSendMessage(encoder_hic, ICM_SETSTATE, (LPARAM) drvdata, (int) st.st_size))){
        mp_msg(MSGT_WIN32,MSGL_ERR,"ICSetState failed!\n");
        free(drvdata);
        return NULL;
      }
      free(drvdata);
      mp_msg(MSGT_WIN32,MSGL_INFO,"ICSetState ret: %d\n", temp_len);
    }
  }

  temp_len = ICCompressGetFormatSize(encoder_hic, input_bih);
  mp_msg(MSGT_WIN32,MSGL_INFO,"ICCompressGetFormatSize ret: %d\n", temp_len);

  if (temp_len < sizeof(BITMAPINFOHEADER)) temp_len=sizeof(BITMAPINFOHEADER);

  output_bih = malloc(temp_len+4);
  memset(output_bih,0,temp_len);
  output_bih->biSize = temp_len; //sizeof(BITMAPINFOHEADER);

  return output_bih;
}

static int vfw_start_encoder(BITMAPINFOHEADER *input_bih, BITMAPINFOHEADER *output_bih){
  HRESULT ret;
  int temp_len=output_bih->biSize;
  int i;

  ret = ICCompressGetFormat(encoder_hic, input_bih, output_bih);
  if(ret < 0){
    unsigned char* temp=(unsigned char*)output_bih;
    mp_msg(MSGT_WIN32,MSGL_ERR,"ICCompressGetFormat failed: Error %d  (0x%X)\n", (int)ret, (int)ret);
    for (i=0; i < temp_len; i++) mp_msg(MSGT_WIN32, MSGL_DBG2, "%02x ", temp[i]);
    return 0;
  }
  mp_msg(MSGT_WIN32,MSGL_V,"ICCompressGetFormat OK\n");
  
  if (temp_len > sizeof(BITMAPINFOHEADER))
  {
    unsigned char* temp=(unsigned char*)output_bih;
    mp_msg(MSGT_WIN32, MSGL_V, "Extra info in o_bih (%d bytes)!\n",
	temp_len-sizeof(BITMAPINFOHEADER));
    for(i=sizeof(output_bih);i<temp_len;i++) mp_msg(MSGT_WIN32, MSGL_DBG2, "%02X ",temp[i]);
  }

//  if( mp_msg_test(MSGT_WIN32,MSGL_V) ) {
    printf("Starting compression:\n");
    printf(" Input format:\n");
	printf("  biSize %d\n", input_bih->biSize);
	printf("  biWidth %d\n", input_bih->biWidth);
	printf("  biHeight %d\n", input_bih->biHeight);
	printf("  biPlanes %d\n", input_bih->biPlanes);
	printf("  biBitCount %d\n", input_bih->biBitCount);
	printf("  biCompression 0x%x ('%.4s')\n", input_bih->biCompression, (char *)&input_bih->biCompression);
	printf("  biSizeImage %d\n", input_bih->biSizeImage);
    printf(" Output format:\n");
	printf("  biSize %d\n", output_bih->biSize);
	printf("  biWidth %d\n", output_bih->biWidth);
	printf("  biHeight %d\n", output_bih->biHeight);
	printf("  biPlanes %d\n", output_bih->biPlanes);
	printf("  biBitCount %d\n", output_bih->biBitCount);
	printf("  biCompression 0x%x ('%.4s')\n", output_bih->biCompression, (char *)&output_bih->biCompression);
	printf("  biSizeImage %d\n", output_bih->biSizeImage);
//  }

  output_bih->biWidth=input_bih->biWidth;
  output_bih->biHeight=input_bih->biHeight;

  ret = ICCompressQuery(encoder_hic, input_bih, output_bih);
  if(ret){
    mp_msg(MSGT_WIN32,MSGL_ERR,"ICCompressQuery failed: Error %d\n", (int)ret);
    return 0;
  } else
  mp_msg(MSGT_WIN32,MSGL_V,"ICCompressQuery OK\n");

  ret = ICCompressBegin(encoder_hic, input_bih, output_bih);
  if(ret){
    mp_msg(MSGT_WIN32,MSGL_ERR,"ICCompressBegin failed: Error %d\n", (int)ret);
//    return 0;
  } else
    mp_msg(MSGT_WIN32,MSGL_V,"ICCompressBegin OK\n");
    mp_msg(MSGT_WIN32,MSGL_INFO," Output format after query/begin:\n");
    mp_msg(MSGT_WIN32,MSGL_INFO,"  biSize %d\n", output_bih->biSize);
    mp_msg(MSGT_WIN32,MSGL_INFO,"  biWidth %d\n", output_bih->biWidth);
    mp_msg(MSGT_WIN32,MSGL_INFO,"  biHeight %d\n", output_bih->biHeight);
    mp_msg(MSGT_WIN32,MSGL_INFO,"  biPlanes %d\n", output_bih->biPlanes);
    mp_msg(MSGT_WIN32,MSGL_INFO,"  biBitCount %d\n", output_bih->biBitCount);
    mp_msg(MSGT_WIN32,MSGL_INFO,"  biCompression 0x%x ('%.4s')\n", output_bih->biCompression, (char *)&output_bih->biCompression);
    mp_msg(MSGT_WIN32,MSGL_INFO,"  biSizeImage %d\n", output_bih->biSizeImage);
  
  encoder_buf_size=input_bih->biSizeImage;
  encoder_buf=malloc(encoder_buf_size);
  encoder_frameno=0;

  mp_msg(MSGT_WIN32,MSGL_V,"VIDEO CODEC Init OK!!! ;-)\n");
  return 1;
}

static int vfw_encode_frame(BITMAPINFOHEADER* biOutput,void* OutBuf,
		     BITMAPINFOHEADER* biInput,void* Image,
		     long* keyframe, int quality){
    HRESULT ret;

//long VFWAPIV ICCompress(
//	HIC hic,long dwFlags,LPBITMAPINFOHEADER lpbiOutput,void* lpOutputBuf,
//	LPBITMAPINFOHEADER lpbiInput,void* lpImage,long* lpckid,
//	long* lpdwFlags,long lFrameNum,long dwFrameSize,long dwQuality,
//	LPBITMAPINFOHEADER lpbiInputPrev,void* lpImagePrev
//);

//    printf("vfw_encode_frame(%p,%p, %p,%p, %p,%d)\n",biOutput,OutBuf,biInput,Image,keyframe,quality);

    ret=ICCompress(encoder_hic, 0,
	biOutput, OutBuf,
	biInput, Image,
	NULL, keyframe, encoder_frameno, 0, quality,
	biInput, encoder_buf);

//    printf("ok. size=%d\n",biOutput->biSizeImage);

    memcpy(encoder_buf,Image,encoder_buf_size);
    ++encoder_frameno;

    return (int)ret;
}
#define mux_v (vf->priv->mux)
#define vfw_bih (vf->priv->bih)

static int config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt){

    vfw_bih->biWidth=width;
    vfw_bih->biHeight=height;
    vfw_bih->biSizeImage=width*height*((vfw_bih->biBitCount+7)/8);
    mux_v->aspect = (float)d_width/d_height;

    if(!vfw_start_encoder(vfw_bih, mux_v->bih)) return 0;

//    mux_v->bih->biWidth=width;
//    mux_v->bih->biHeight=height;
//    mux_v->bih->biSizeImage=width*height*((mux_v->bih->biBitCount+7)/8);

    return 1;
}

static int control(struct vf_instance_s* vf, int request, void* data){

    return CONTROL_UNKNOWN;
}

static int query_format(struct vf_instance_s* vf, unsigned int fmt){
    if(fmt==IMGFMT_BGR24) return VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW | VFCAP_FLIPPED;
    return 0;
}

static int put_image(struct vf_instance_s* vf, mp_image_t *mpi, double pts){
    long flags=0;
    int ret;
//    flip_upside_down(vo_image_ptr,vo_image_ptr,3*vo_w,vo_h); // dirty hack
    ret=vfw_encode_frame(mux_v->bih, mux_v->buffer, vfw_bih, mpi->planes[0], &flags, 10000);
//    if (ret != ICERR_OK)
//	return 0;
    muxer_write_chunk(mux_v,mux_v->bih->biSizeImage,flags, pts, pts);
    return 1;
}

//===========================================================================//

static int vf_open(vf_instance_t *vf, char* args){
    vf->config=config;
    vf->default_caps=VFCAP_CONSTANT;
    vf->control=control;
    vf->query_format=query_format;
    vf->put_image=put_image;
    vf->priv=malloc(sizeof(struct vf_priv_s));
    memset(vf->priv,0,sizeof(struct vf_priv_s));
    vf->priv->mux=(muxer_stream_t*)args;

    vfw_bih=calloc(1, sizeof(BITMAPINFOHEADER));
    vfw_bih->biSize=sizeof(BITMAPINFOHEADER);
    vfw_bih->biWidth=0; // FIXME ?
    vfw_bih->biHeight=0;
    vfw_bih->biPlanes=1;
    vfw_bih->biBitCount=24;
    vfw_bih->biCompression=0;
//    vfw_bih->biSizeImage=vo_w*vo_h*((vfw_bih->biBitCount+7)/8);

    if (!vfw_param_codec)
    {
	mp_msg(MSGT_WIN32,MSGL_WARN, MSGTR_MPCODECS_NoVfwCodecSpecified);
	return 0;
    }
//    mux_v->bih=vfw_open_encoder("divxc32.dll",vfw_bih,mmioFOURCC('D', 'I', 'V', '3'));
//    mux_v->bih=vfw_open_encoder("AvidAVICodec.dll",vfw_bih, 0);
    mux_v->bih = vfw_open_encoder(vfw_param_codec, vfw_param_compdata, vfw_bih, 0);
    if(!mux_v->bih) return 0;

    return 1;
}

vf_info_t ve_info_vfw = {
    "Win32/VfW encoders",
    "vfw",
    "A'rpi",
    "for internal use by mencoder",
    vf_open
};

//===========================================================================//
