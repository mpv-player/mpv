/*
 *
 * LCL (LossLess Codec Library) Decoder for Mplayer
 * (c) 2002 Roberto Togni
 *
 * Fourcc: MSZH, ZLIB
 *
 * Win32 dll:
 * Ver2.23 By Kenji Oshima 2000.09.20
 * avimszh.dll, avizlib.dll
 *
 * A description of the decoding algorithm can be found here:
 *   http://www.pcisys.net/~melanson/codecs
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include "mp_msg.h"

#include "vd_internal.h"


static vd_info_t info = {
  "LCL Video decoder",
  "lcl",
  "Roberto Togni",
  "Roberto Togni",
  "native codec"
};

LIBVD_EXTERN(lcl)


#define BMPTYPE_YUV 1
#define BMPTYPE_RGB 2

#define IMGTYPE_YUV111 0
#define IMGTYPE_YUV422 1
#define IMGTYPE_RGB24 2
#define IMGTYPE_YUV411 3
#define IMGTYPE_YUV211 4
#define IMGTYPE_YUV420 5

#define COMP_MSZH 0
#define COMP_MSZH_NOCOMP 1
#define COMP_ZLIB_HISPEED 1
#define COMP_ZLIB_HICOMP 9
#define COMP_ZLIB_NORMAL -1

#define FLAG_MULTITHREAD 1
#define FLAG_NULLFRAME 2
#define FLAG_PNGFILTER 4
#define FLAGMASK_UNUSED 0xf8

#define CODEC_MSZH 1
#define CODEC_ZLIB 3

#define FOURCC_MSZH mmioFOURCC('M','S','Z','H')
#define FOURCC_ZLIB mmioFOURCC('Z','L','I','B')

/*
 * Decoder context
 */
typedef struct {
  // Image type
  int imgtype;
  // Compression type
  int compression;
  // Flags
  int flags;
  // Codec type
  int codec;
  // Decompressed data size
  unsigned int decomp_size;
  // Decompression buffer
  unsigned char* decomp_buf;
#ifdef HAVE_ZLIB
  z_stream zstream;
#endif
} lcl_context_t;


/*
 * Internal function prototypes
 */


// to set/get/query special features/parameters
static int control(sh_video_t *sh,int cmd,void* arg,...)
{
  return CONTROL_UNKNOWN;
}


/*
 *
 * Init LCL decoder
 *
 */
static int init(sh_video_t *sh)
{
  int vo_ret; // Video output init ret value
  int zret; // Zlib return code
  lcl_context_t *hc; // Decoder context
  BITMAPINFOHEADER *bih = sh->bih;
  int basesize = bih->biWidth * bih->biHeight;
  
  if ((hc = malloc(sizeof(lcl_context_t))) == NULL) {
    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Can't allocate memory for LCL decoder context.\n");
    return 0;
  }

  sh->context = (void *)hc;

#ifdef HAVE_ZLIB
  // Needed if zlib unused or init aborted before inflateInit
  memset(&(hc->zstream), 0, sizeof(z_stream)); 
#endif

  if ((bih->biCompression != FOURCC_MSZH) && (bih->biCompression != FOURCC_ZLIB)) {
    mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[LCL] Unknown BITMAPHEADER fourcc.\n");
    return 0;
  }

  if (bih->biSize < sizeof(BITMAPINFOHEADER) + 8) {
    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] BITMAPHEADER size too small.\n");
    return 0;
  }

  /* Detect codec type */ 
  switch (hc->codec = *((char *)bih + sizeof(BITMAPINFOHEADER) + 7)) {
    case CODEC_MSZH:
      mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] Codec is MSZH.\n");
      break;
    case CODEC_ZLIB:
      mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] Codec is ZLIB.\n");
      break;
    default:
      mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[LCL] Unknown codec id %d. Trusting fourcc.\n", hc->codec);
      switch (bih->biCompression) {
        case FOURCC_MSZH:
          hc->codec = CODEC_MSZH;
          break;
        case FOURCC_ZLIB:
          hc->codec = CODEC_ZLIB;
          break;
        default:
          mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[LCL] BUG! Unknown coded id and fourcc. Why am I here?.\n");
          return 0;
      }
  }


  /* Detect image type */
  switch (hc->imgtype = *((char *)bih + sizeof(BITMAPINFOHEADER) + 4)) {
    case IMGTYPE_YUV111:
      hc->decomp_size = basesize * 3;
      mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] Image type is YUV 1:1:1.\n");
      break;
    case IMGTYPE_YUV422:
      hc->decomp_size = basesize * 2;
      mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] Image type is YUV 4:2:2.\n");
      break;
    case IMGTYPE_RGB24:
      hc->decomp_size = basesize * 3;
      mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] Image type is RGB 24.\n");
      break;
    case IMGTYPE_YUV411:
      hc->decomp_size = basesize / 2 * 3;
      mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] Image type is YUV 4:1:1.\n");
      break;
    case IMGTYPE_YUV211:
      hc->decomp_size = basesize * 2;
      mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] Image type is YUV 2:1:1.\n");
      break;
    case IMGTYPE_YUV420:
      hc->decomp_size = basesize / 2 * 3;
      mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] Image type is YUV 4:2:0.\n");
      break;
    default:
      mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] Unsupported image format %d.\n", hc->imgtype);
      return 0;
  }

  /* Detect compression method */
  hc->compression = *((char *)bih + sizeof(BITMAPINFOHEADER) + 5);
  switch (hc->codec) {
    case CODEC_MSZH:
      switch (hc->compression) {
        case COMP_MSZH:
          mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] Compression enabled.\n");
          break;
        case COMP_MSZH_NOCOMP:
          hc->decomp_size = 0;
          mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] No compression.\n");
          break;
        default:
          mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] Unsupported compression format for MSZH (%d).\n", hc->compression);
          return 0;
      }
      break;
    case CODEC_ZLIB:
      switch (hc->compression) {
        case COMP_ZLIB_HISPEED:
          mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] High speed compression.\n");
          break;
        case COMP_ZLIB_HICOMP:
          mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] High compression.\n");
          break;
        case COMP_ZLIB_NORMAL:
          mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] Normal compression.\n");
          break;
        default:
          mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] Unsupported compression format for ZLIB (%d).\n", hc->compression);
          return 0;
      }
      break;
    default:
      mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] BUG! Unknown codec in compression switch.\n");
      return 0;
  }

  /* Allocate decompression buffer */
  /* 4*8 max oveflow space for mszh decomp algorithm */
  if (hc->decomp_size) {
    if ((hc->decomp_buf = malloc(hc->decomp_size+4*8)) == NULL) {
      mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] Can't allocate decompression buffer.\n");
      return 0;
    }
  }
  
  /* Detect flags */ 
  hc->flags = *((char *)bih + sizeof(BITMAPINFOHEADER) + 6);
  if (hc->flags & FLAG_MULTITHREAD)
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] Multithread encoder flag set.\n");
  if (hc->flags & FLAG_NULLFRAME)
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] Nullframe insertion flag set.\n");
  if ((hc->codec == CODEC_ZLIB) && (hc->flags & FLAG_PNGFILTER))
    mp_msg(MSGT_DECVIDEO, MSGL_INFO, "[LCL] PNG filter flag set.\n");
  if (hc->flags & FLAGMASK_UNUSED)
    mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[LCL] Unknown flag set (%d).\n", hc->flags);

  /* If needed init zlib */
  if (hc->codec == CODEC_ZLIB) {
#ifdef HAVE_ZLIB
    hc->zstream.zalloc = Z_NULL;
    hc->zstream.zfree = Z_NULL;
    hc->zstream.opaque = Z_NULL;
    zret = inflateInit(&(hc->zstream));
    if (zret != Z_OK) {
      mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] Inflate init error: %d\n", zret);
      return 0;
    }
#else
    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] Zlib support not compiled.\n");
    return 0;
#endif
  }

  /*
   * Initialize video output device
   */
  vo_ret = mpcodecs_config_vo(sh,sh->disp_w,sh->disp_h,IMGFMT_BGR24);

  return vo_ret;
}




/*
 *
 * Uninit LCL decoder
 *
 */
static void uninit(sh_video_t *sh)
{
  lcl_context_t *hc = (lcl_context_t *) sh->context; // Decoder context

  if (sh->context) {
#ifdef HAVE_ZLIB
    inflateEnd(&hc->zstream);
#endif
    free(sh->context);
  }
}



inline unsigned char fix (int pix14)
{
  int tmp;
  
  tmp = (pix14 + 0x80000) >> 20;
  if (tmp < 0)
    return 0;
  if (tmp > 255)
    return 255;
  return tmp;
}



inline unsigned char get_b (unsigned char yq, signed char bq)
{
  return fix((yq << 20) + bq * 1858076);
}



inline unsigned char get_g (unsigned char yq, signed char bq, signed char rq)
{
  return fix((yq << 20) - bq * 360857 - rq * 748830);
}



inline unsigned char get_r (unsigned char yq, signed char rq)
{
  return fix((yq << 20) + rq * 1470103);
}


int mszh_decomp(unsigned char * srcptr, int srclen, unsigned char * destptr);

/*
 *
 * Decode a frame
 *
 */
static mp_image_t* decode(sh_video_t *sh,void* data,int len,int flags)
{
  mp_image_t* mpi;
  int pixel_ptr;
  int row, col;
  unsigned char *encoded = (unsigned char *)data;
  lcl_context_t *hc = (lcl_context_t *) sh->context; // Decoder context
  unsigned char *outptr;
  int width = sh->disp_w; // Real image width
  int height = sh->disp_h; // Real image height
#ifdef HAVE_ZLIB
  int zret; // Zlib return code
#endif
  unsigned int mszh_dlen;
  unsigned char yq, y1q, uq, vq;
  int uqvq;
  unsigned int mthread_inlen, mthread_outlen;

  // Skipped frame
  if(len <= 0)
    return NULL;

  /* Get output image buffer */
  mpi=mpcodecs_get_image(sh, MP_IMGTYPE_TEMP, 0, sh->disp_w, sh->disp_h);
  if (!mpi) {
    mp_msg(MSGT_DECVIDEO, MSGL_ERR, "Can't allocate mpi image for lcl codec.\n");
    return NULL;
  }

  outptr = mpi->planes[0]; // Output image pointer


  /* Decompress frame */
  switch (hc->codec) {
    case CODEC_MSZH:
      switch (hc->compression) {
        case COMP_MSZH:
          if (hc->flags & FLAG_MULTITHREAD) {
            mthread_inlen = *((unsigned int*)encoded);
            mthread_outlen = *((unsigned int*)(encoded+4));
            mszh_dlen = mszh_decomp(encoded + 8, mthread_inlen, hc->decomp_buf);
            if (mthread_outlen != mszh_dlen) {
              mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[LCL] MSZH: mthread1 decoded size differs (%d != %d)\n",
                     mthread_outlen, mszh_dlen);
            }
            mszh_dlen = mszh_decomp(encoded + 8 + mthread_inlen, len - mthread_inlen,
                                    hc->decomp_buf + mthread_outlen);
            if ((hc->decomp_size - mthread_outlen) != mszh_dlen) {
              mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[LCL] MSZH: mthread2 decoded size differs (%d != %d)\n",
                     hc->decomp_size - mthread_outlen, mszh_dlen);
            }
            encoded = hc->decomp_buf;
            len = hc->decomp_size;            
          } else {
            mszh_dlen = mszh_decomp(encoded, len, hc->decomp_buf);
            if (hc->decomp_size != mszh_dlen) {
              mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[LCL] MSZH: decoded size differs (%d != %d)\n",
                     hc->decomp_size, mszh_dlen);
            }
            encoded = hc->decomp_buf;
            len = mszh_dlen;
          }
          break;
        case COMP_MSZH_NOCOMP:
          break;
        default:
          mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] BUG! Unknown MSZH compression in frame decoder.\n");
          return 0;
      }
      break;
    case CODEC_ZLIB:
      switch (hc->compression) {
        case COMP_ZLIB_HISPEED:
        case COMP_ZLIB_HICOMP:
        case COMP_ZLIB_NORMAL:
#ifdef HAVE_ZLIB
          zret = inflateReset(&(hc->zstream));
          if (zret != Z_OK) {
            mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] ZLIB: inflate reset error: %d\n", zret);
            return 0;
          }
          if (hc->flags & FLAG_MULTITHREAD) {
            mthread_inlen = *((unsigned int*)encoded);
            mthread_outlen = *((unsigned int*)(encoded+4));
            hc->zstream.next_in = encoded + 8;
            hc->zstream.avail_in = mthread_inlen;
            hc->zstream.next_out = hc->decomp_buf;
            hc->zstream.avail_out = mthread_outlen;    
            zret = inflate(&(hc->zstream), Z_FINISH);
            if ((zret != Z_OK) && (zret != Z_STREAM_END)) {
              mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] ZLIB: mthread1 inflate error: %d\n", zret);
              return 0;
            }
            if (mthread_outlen != (unsigned int)(hc->zstream.total_out)) {
              mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[LCL] ZLIB: mthread1 decoded size differs (%d != %d)\n",
                     mthread_outlen, hc->zstream.total_out);
            }
            zret = inflateReset(&(hc->zstream));
            if (zret != Z_OK) {
              mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] ZLIB: mthread2 inflate reset error: %d\n", zret);
              return 0;
            }
            hc->zstream.next_in = encoded + 8 + mthread_inlen;
            hc->zstream.avail_in = len - mthread_inlen;
            hc->zstream.next_out = hc->decomp_buf + mthread_outlen;
            hc->zstream.avail_out = mthread_outlen;    
            zret = inflate(&(hc->zstream), Z_FINISH);
            if ((zret != Z_OK) && (zret != Z_STREAM_END)) {
              mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] ZLIB: mthread2 inflate error: %d\n", zret);
              return 0;
            }
            if ((hc->decomp_size - mthread_outlen) != (unsigned int)(hc->zstream.total_out)) {
              mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[LCL] ZLIB: mthread2 decoded size differs (%d != %d)\n",
                     hc->decomp_size - mthread_outlen, hc->zstream.total_out);
            }
          } else {
            hc->zstream.next_in = data;
            hc->zstream.avail_in = len;
            hc->zstream.next_out = hc->decomp_buf;
            hc->zstream.avail_out = hc->decomp_size;    
            zret = inflate(&(hc->zstream), Z_FINISH);
            if ((zret != Z_OK) && (zret != Z_STREAM_END)) {
              mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] ZLIB: inflate error: %d\n", zret);
              return 0;
            }
            if (hc->decomp_size != (unsigned int)(hc->zstream.total_out)) {
              mp_msg(MSGT_DECVIDEO, MSGL_WARN, "[LCL] ZLIB: decoded size differs (%d != %d)\n",
                     hc->decomp_size, hc->zstream.total_out);
            }
          }
          encoded = hc->decomp_buf;
          len = hc->decomp_size;;
#else
          mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] BUG! Zlib support not compiled in frame decoder.\n");
          return 0;
#endif
          break;
        default:
          mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] BUG! Unknown ZLIB compression in frame decoder.\n");
          return 0;
      }
      break;
    default:
      mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] BUG! Unknown codec in frame decoder compression switch.\n");
      return 0;
  }


  /* Apply PNG filter */
  if ((hc->codec == CODEC_ZLIB) && (hc->flags & FLAG_PNGFILTER)) {
    switch (hc->imgtype) {
      case IMGTYPE_YUV111:
        for (row = 0; row < height; row++) {
          pixel_ptr = row * width * 3;
          yq = encoded[pixel_ptr++];
          uqvq = encoded[pixel_ptr++] + (encoded[pixel_ptr++] << 8);
          for (col = 1; col < width; col++) {
            encoded[pixel_ptr] = yq -= encoded[pixel_ptr];
            uqvq -= (encoded[pixel_ptr+1] | (encoded[pixel_ptr+2]<<8));
            encoded[pixel_ptr+1] = (uqvq) & 0xff;
            encoded[pixel_ptr+2] = ((uqvq)>>8) & 0xff;
            pixel_ptr += 3;
          }
        }
        break;
      case IMGTYPE_RGB24: // No
        for (row = 0; row < height; row++) {
          pixel_ptr = row * width * 3;
          yq = encoded[pixel_ptr++];
          uqvq = encoded[pixel_ptr++] + (encoded[pixel_ptr++] << 8);
          for (col = 1; col < width; col++) {
            encoded[pixel_ptr] = yq -= encoded[pixel_ptr];
            uqvq -= (encoded[pixel_ptr+1] | (encoded[pixel_ptr+2]<<8));
            encoded[pixel_ptr+1] = (uqvq) & 0xff;
            encoded[pixel_ptr+2] = ((uqvq)>>8) & 0xff;
            pixel_ptr += 3;
          }
        }
        break;
      case IMGTYPE_YUV422:
        for (row = 0; row < height; row++) {
          pixel_ptr = row * width * 2;
          yq = uq = vq =0;
          for (col = 0; col < width/4; col++) {
            encoded[pixel_ptr] = yq -= encoded[pixel_ptr];
            encoded[pixel_ptr+1] = yq -= encoded[pixel_ptr+1];
            encoded[pixel_ptr+2] = yq -= encoded[pixel_ptr+2];
            encoded[pixel_ptr+3] = yq -= encoded[pixel_ptr+3];
            encoded[pixel_ptr+4] = uq -= encoded[pixel_ptr+4];
            encoded[pixel_ptr+5] = uq -= encoded[pixel_ptr+5];
            encoded[pixel_ptr+6] = vq -= encoded[pixel_ptr+6];
            encoded[pixel_ptr+7] = vq -= encoded[pixel_ptr+7];
            pixel_ptr += 8;
          }
        }
        break;
      case IMGTYPE_YUV411:
        for (row = 0; row < height; row++) {
          pixel_ptr = row * width / 2 * 3;
          yq = uq = vq =0;
          for (col = 0; col < width/4; col++) {
            encoded[pixel_ptr] = yq -= encoded[pixel_ptr];
            encoded[pixel_ptr+1] = yq -= encoded[pixel_ptr+1];
            encoded[pixel_ptr+2] = yq -= encoded[pixel_ptr+2];
            encoded[pixel_ptr+3] = yq -= encoded[pixel_ptr+3];
            encoded[pixel_ptr+4] = uq -= encoded[pixel_ptr+4];
            encoded[pixel_ptr+5] = vq -= encoded[pixel_ptr+5];
            pixel_ptr += 6;
          }
        }
        break;
      case IMGTYPE_YUV211:
        for (row = 0; row < height; row++) {
          pixel_ptr = row * width * 2;
          yq = uq = vq =0;
          for (col = 0; col < width/2; col++) {
            encoded[pixel_ptr] = yq -= encoded[pixel_ptr];
            encoded[pixel_ptr+1] = yq -= encoded[pixel_ptr+1];
            encoded[pixel_ptr+2] = uq -= encoded[pixel_ptr+2];
            encoded[pixel_ptr+3] = vq -= encoded[pixel_ptr+3];
            pixel_ptr += 4;
          }
        }
        break;
      case IMGTYPE_YUV420:
        for (row = 0; row < height/2; row++) {
          pixel_ptr = row * width * 3;
          yq = y1q = uq = vq =0;
          for (col = 0; col < width/2; col++) {
            encoded[pixel_ptr] = yq -= encoded[pixel_ptr];
            encoded[pixel_ptr+1] = yq -= encoded[pixel_ptr+1];
            encoded[pixel_ptr+2] = y1q -= encoded[pixel_ptr+2];
            encoded[pixel_ptr+3] = y1q -= encoded[pixel_ptr+3];
            encoded[pixel_ptr+4] = uq -= encoded[pixel_ptr+4];
            encoded[pixel_ptr+5] = vq -= encoded[pixel_ptr+5];
            pixel_ptr += 6;
          }
        }
        break;
      break;
      default:
        mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] BUG! Unknown imagetype in pngfilter switch.\n");
        return 0;
    }
  }

  /* Convert colorspace */
  switch (hc->imgtype) {
    case IMGTYPE_YUV111:
      for (row = height - 1; row >= 0; row--) {
        pixel_ptr = row * mpi->stride[0];
        for (col = 0; col < width; col++) {
          outptr[pixel_ptr++] = get_b(encoded[0], encoded[1]);
          outptr[pixel_ptr++] = get_g(encoded[0], encoded[1], encoded[2]);
          outptr[pixel_ptr++] = get_r(encoded[0], encoded[2]);
          encoded += 3;
        }
      }
      break;
    case IMGTYPE_YUV422:
      for (row = height - 1; row >= 0; row--) {
        pixel_ptr = row * mpi->stride[0];
        for (col = 0; col < width/4; col++) {
          outptr[pixel_ptr++] = get_b(encoded[0], encoded[4]);
          outptr[pixel_ptr++] = get_g(encoded[0], encoded[4], encoded[6]);
          outptr[pixel_ptr++] = get_r(encoded[0], encoded[6]);
          outptr[pixel_ptr++] = get_b(encoded[1], encoded[4]);
          outptr[pixel_ptr++] = get_g(encoded[1], encoded[4], encoded[6]);
          outptr[pixel_ptr++] = get_r(encoded[1], encoded[6]);
          outptr[pixel_ptr++] = get_b(encoded[2], encoded[5]);
          outptr[pixel_ptr++] = get_g(encoded[2], encoded[5], encoded[7]);
          outptr[pixel_ptr++] = get_r(encoded[2], encoded[7]);
          outptr[pixel_ptr++] = get_b(encoded[3], encoded[5]);
          outptr[pixel_ptr++] = get_g(encoded[3], encoded[5], encoded[7]);
          outptr[pixel_ptr++] = get_r(encoded[3], encoded[7]);
          encoded += 8;
        }
      }
      break;
    case IMGTYPE_RGB24:
      for (row = height - 1; row >= 0; row--) {
        pixel_ptr = row * mpi->stride[0];
        for (col = 0; col < width; col++) {
          outptr[pixel_ptr++] = encoded[0];
          outptr[pixel_ptr++] = encoded[1];
          outptr[pixel_ptr++] = encoded[2];
          encoded += 3;
        }
      }
      break;
    case IMGTYPE_YUV411:
      for (row = height - 1; row >= 0; row--) {
        pixel_ptr = row * mpi->stride[0];
        for (col = 0; col < width/4; col++) {
          outptr[pixel_ptr++] = get_b(encoded[0], encoded[4]);
          outptr[pixel_ptr++] = get_g(encoded[0], encoded[4], encoded[5]);
          outptr[pixel_ptr++] = get_r(encoded[0], encoded[5]);
          outptr[pixel_ptr++] = get_b(encoded[1], encoded[4]);
          outptr[pixel_ptr++] = get_g(encoded[1], encoded[4], encoded[5]);
          outptr[pixel_ptr++] = get_r(encoded[1], encoded[5]);
          outptr[pixel_ptr++] = get_b(encoded[2], encoded[4]);
          outptr[pixel_ptr++] = get_g(encoded[2], encoded[4], encoded[5]);
          outptr[pixel_ptr++] = get_r(encoded[2], encoded[5]);
          outptr[pixel_ptr++] = get_b(encoded[3], encoded[4]);
          outptr[pixel_ptr++] = get_g(encoded[3], encoded[4], encoded[5]);
          outptr[pixel_ptr++] = get_r(encoded[3], encoded[5]);
          encoded += 6;
        }
      }
      break;
    case IMGTYPE_YUV211:
      for (row = height - 1; row >= 0; row--) {
        pixel_ptr = row * mpi->stride[0];
        for (col = 0; col < width/2; col++) {
          outptr[pixel_ptr++] = get_b(encoded[0], encoded[2]);
          outptr[pixel_ptr++] = get_g(encoded[0], encoded[2], encoded[3]);
          outptr[pixel_ptr++] = get_r(encoded[0], encoded[3]);
          outptr[pixel_ptr++] = get_b(encoded[1], encoded[2]);
          outptr[pixel_ptr++] = get_g(encoded[1], encoded[2], encoded[3]);
          outptr[pixel_ptr++] = get_r(encoded[1], encoded[3]);
          encoded += 4;
        }
      }
      break;
    case IMGTYPE_YUV420:
      for (row = height / 2 - 1; row >= 0; row--) {
        pixel_ptr = 2 * row * mpi->stride[0];
        for (col = 0; col < width/2; col++) {
          outptr[pixel_ptr] = get_b(encoded[0], encoded[4]);
          outptr[pixel_ptr+1] = get_g(encoded[0], encoded[4], encoded[5]);
          outptr[pixel_ptr+2] = get_r(encoded[0], encoded[5]);
          outptr[pixel_ptr+3] = get_b(encoded[1], encoded[4]);
          outptr[pixel_ptr+4] = get_g(encoded[1], encoded[4], encoded[5]);
          outptr[pixel_ptr+5] = get_r(encoded[1], encoded[5]);
          outptr[pixel_ptr-mpi->stride[0]] = get_b(encoded[2], encoded[4]);
          outptr[pixel_ptr-mpi->stride[0]+1] = get_g(encoded[2], encoded[4], encoded[5]);
          outptr[pixel_ptr-mpi->stride[0]+2] = get_r(encoded[2], encoded[5]);
          outptr[pixel_ptr-mpi->stride[0]+3] = get_b(encoded[3], encoded[4]);
          outptr[pixel_ptr-mpi->stride[0]+4] = get_g(encoded[3], encoded[4], encoded[5]);
          outptr[pixel_ptr-mpi->stride[0]+5] = get_r(encoded[3], encoded[5]);
          pixel_ptr += 6;
          encoded += 6;
        }
      }
      break;
    default:
      mp_msg(MSGT_DECVIDEO, MSGL_ERR, "[LCL] BUG! Unknown imagetype in image decoder.\n");
      return 0;
  }

  return mpi;
}



int mszh_decomp(unsigned char * srcptr, int srclen, unsigned char * destptr)
{
  unsigned char *destptr_bak = destptr;
  unsigned char mask = 0;
  unsigned char maskbit = 0;
  unsigned int ofs, cnt;
  
  while (srclen > 0) {
    if (maskbit == 0) {
      mask = *(srcptr++);
      maskbit = 8;
      srclen--;
      continue;
    }
    if ((mask & (1 << (--maskbit))) == 0) {
      *(destptr++) = *(srcptr++);
      *(destptr++) = *(srcptr++);
      *(destptr++) = *(srcptr++);
      *(destptr++) = *(srcptr++);
      srclen -= 4;
    } else {
      ofs = *(srcptr++);
      cnt = *(srcptr++);
      ofs += cnt * 256;;
      cnt = ((cnt >> 3) & 0x1f) + 1;
      ofs &= 0x7ff;
      srclen -= 2;
      cnt *= 4;
      for (; cnt > 0; cnt--) {
        *(destptr) = *(destptr - ofs);
        destptr++;
      }
    }
  }

  return (destptr - destptr_bak);
}

