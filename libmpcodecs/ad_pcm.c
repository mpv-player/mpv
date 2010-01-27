#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "talloc.h"
#include "config.h"
#include "ad_internal.h"
#include "libaf/af_format.h"
#include "libaf/reorder_ch.h"

static const ad_info_t info =
{
	"Uncompressed PCM audio decoder",
	"pcm",
	"Nick Kurshev",
	"A'rpi",
	""
};

struct ad_pcm_context {
    unsigned char *packet_ptr;
    int packet_len;
};

LIBAD_EXTERN(pcm)

static int init(sh_audio_t *sh_audio)
{
  WAVEFORMATEX *h=sh_audio->wf;
  if (!h)
    return 0;
  sh_audio->i_bps=h->nAvgBytesPerSec;
  sh_audio->channels=h->nChannels;
  sh_audio->samplerate=h->nSamplesPerSec;
  sh_audio->samplesize=(h->wBitsPerSample+7)/8;
  sh_audio->sample_format=AF_FORMAT_S16_LE; // default
  switch(sh_audio->format){ /* hardware formats: */
    case 0x0:
    case 0x1: // Microsoft PCM
    case 0xfffe: // Extended
       switch (sh_audio->samplesize) {
         case 1: sh_audio->sample_format=AF_FORMAT_U8; break;
         case 2: sh_audio->sample_format=AF_FORMAT_S16_LE; break;
         case 3: sh_audio->sample_format=AF_FORMAT_S24_LE; break;
         case 4: sh_audio->sample_format=AF_FORMAT_S32_LE; break;
       }
       break;
    case 0x3: // IEEE float
       sh_audio->sample_format=AF_FORMAT_FLOAT_LE;
       break;
    case 0x6:  sh_audio->sample_format=AF_FORMAT_A_LAW;break;
    case 0x7:  sh_audio->sample_format=AF_FORMAT_MU_LAW;break;
    case 0x11: sh_audio->sample_format=AF_FORMAT_IMA_ADPCM;break;
    case 0x50: sh_audio->sample_format=AF_FORMAT_MPEG2;break;
/*    case 0x2000: sh_audio->sample_format=AFMT_AC3; */
    case 0x20776172: // 'raw '
       sh_audio->sample_format=AF_FORMAT_S16_BE;
       if(sh_audio->samplesize==1) sh_audio->sample_format=AF_FORMAT_U8;
       break;
    case 0x736F7774: // 'twos'
       sh_audio->sample_format=AF_FORMAT_S16_BE;
       // intended fall-through
    case 0x74776F73: // 'sowt'
       if(sh_audio->samplesize==1) sh_audio->sample_format=AF_FORMAT_S8;
       break;
    case 0x32336c66: // 'fl32', bigendian float32
       sh_audio->sample_format=AF_FORMAT_FLOAT_BE;
       sh_audio->samplesize=4;
       break;
    case 0x666c3332: // '23lf', little endian float32, MPlayer internal fourCC
       sh_audio->sample_format=AF_FORMAT_FLOAT_LE;
       sh_audio->samplesize=4;
       break;
/*    case 0x34366c66: // 'fl64', bigendian float64
       sh_audio->sample_format=AF_FORMAT_FLOAT_BE;
       sh_audio->samplesize=8;
       break;
    case 0x666c3634: // '46lf', little endian float64, MPlayer internal fourCC
       sh_audio->sample_format=AF_FORMAT_FLOAT_LE;
       sh_audio->samplesize=8;
       break;*/
    case 0x34326e69: // 'in24', bigendian int24
       sh_audio->sample_format=AF_FORMAT_S24_BE;
       sh_audio->samplesize=3;
       break;
    case 0x696e3234: // '42ni', little endian int24, MPlayer internal fourCC
       sh_audio->sample_format=AF_FORMAT_S24_LE;
       sh_audio->samplesize=3;
       break;
    case 0x32336e69: // 'in32', bigendian int32
       sh_audio->sample_format=AF_FORMAT_S32_BE;
       sh_audio->samplesize=4;
       break;
    case 0x696e3332: // '23ni', little endian int32, MPlayer internal fourCC
       sh_audio->sample_format=AF_FORMAT_S32_LE;
       sh_audio->samplesize=4;
       break;
    default: if(sh_audio->samplesize!=2) sh_audio->sample_format=AF_FORMAT_U8;
  }
  if (!sh_audio->samplesize) // this would cause MPlayer to hang later
    sh_audio->samplesize = 2;
  sh_audio->context = talloc_zero(NULL, struct ad_pcm_context);
  return 1;
}

static int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=2048;
  return 1;
}

static void uninit(sh_audio_t *sh)
{
    talloc_free(sh->context);
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
  int skip;
    switch(cmd)
    {
      case ADCTRL_SKIP_FRAME:
	skip=sh->i_bps/16;
	skip=skip&(~3);
	demux_read_data(sh->ds,NULL,skip);
	return CONTROL_TRUE;
    }
  return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
  unsigned len = sh_audio->channels*sh_audio->samplesize;
  minlen = (minlen + len - 1) / len * len;
  if (minlen > maxlen)
      // if someone needs hundreds of channels adjust audio_out_minsize
      // based on channels in preinit()
      return -1;

  len = 0;
  struct ad_pcm_context *ctx = sh_audio->context;
  while (len < minlen) {
      if (ctx->packet_len == 0) {
          double pts;
          int plen = ds_get_packet_pts(sh_audio->ds, &ctx->packet_ptr, &pts);
          if (plen < 0)
              break;
          ctx->packet_len = plen;
          if (pts != MP_NOPTS_VALUE) {
              sh_audio->pts = pts;
              sh_audio->pts_bytes = 0;
          }
      }
      int from_stored = ctx->packet_len;
      if (from_stored > minlen - len)
          from_stored = minlen - len;
      memcpy(buf + len, ctx->packet_ptr, from_stored);
      ctx->packet_len -= from_stored;
      ctx->packet_ptr += from_stored;
      sh_audio->pts_bytes += from_stored;
      len += from_stored;
  }
  if (len == 0)
      len = -1;  // The loop above only exits at error/EOF
  if (len > 0 && sh_audio->channels >= 5) {
    reorder_channel_nch(buf, AF_CHANNEL_LAYOUT_WAVEEX_DEFAULT,
                        AF_CHANNEL_LAYOUT_MPLAYER_DEFAULT,
                        sh_audio->channels,
                        len / sh_audio->samplesize, sh_audio->samplesize);
  }
  return len;
}
