/*
 * OSS audio output driver
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "core/mp_msg.h"
#include "audio/mixer.h"

#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#else
#ifdef HAVE_SOUNDCARD_H
#include <soundcard.h>
#endif
#endif

#include "audio/format.h"

#include "ao.h"
#include "audio_out_internal.h"

static const ao_info_t info =
{
	"OSS/ioctl audio output",
	"oss",
	"A'rpi",
	""
};

/* Support for >2 output channels added 2001-11-25 - Steve Davies <steve@daviesfam.org> */

LIBAO_EXTERN(oss)

static int format2oss(int format)
{
    switch(format)
    {
    case AF_FORMAT_U8: return AFMT_U8;
    case AF_FORMAT_S8: return AFMT_S8;
    case AF_FORMAT_U16_LE: return AFMT_U16_LE;
    case AF_FORMAT_U16_BE: return AFMT_U16_BE;
    case AF_FORMAT_S16_LE: return AFMT_S16_LE;
    case AF_FORMAT_S16_BE: return AFMT_S16_BE;
#ifdef AFMT_S24_PACKED
    case AF_FORMAT_S24_LE: return AFMT_S24_PACKED;
#endif
#ifdef AFMT_U32_LE
    case AF_FORMAT_U32_LE: return AFMT_U32_LE;
#endif
#ifdef AFMT_U32_BE
    case AF_FORMAT_U32_BE: return AFMT_U32_BE;
#endif
#ifdef AFMT_S32_LE
    case AF_FORMAT_S32_LE: return AFMT_S32_LE;
#endif
#ifdef AFMT_S32_BE
    case AF_FORMAT_S32_BE: return AFMT_S32_BE;
#endif
#ifdef AFMT_FLOAT
    case AF_FORMAT_FLOAT_NE: return AFMT_FLOAT;
#endif
    // SPECIALS
#ifdef AFMT_MPEG
    case AF_FORMAT_MPEG2: return AFMT_MPEG;
#endif
#ifdef AFMT_AC3
    case AF_FORMAT_AC3_NE: return AFMT_AC3;
#endif
    }
    mp_msg(MSGT_AO, MSGL_V, "OSS: Unknown/not supported internal format: %s\n", af_fmt2str_short(format));
    return -1;
}

static int oss2format(int format)
{
    switch(format)
    {
    case AFMT_U8: return AF_FORMAT_U8;
    case AFMT_S8: return AF_FORMAT_S8;
    case AFMT_U16_LE: return AF_FORMAT_U16_LE;
    case AFMT_U16_BE: return AF_FORMAT_U16_BE;
    case AFMT_S16_LE: return AF_FORMAT_S16_LE;
    case AFMT_S16_BE: return AF_FORMAT_S16_BE;
#ifdef AFMT_S24_PACKED
    case AFMT_S24_PACKED: return AF_FORMAT_S24_LE;
#endif
#ifdef AFMT_U32_LE
    case AFMT_U32_LE: return AF_FORMAT_U32_LE;
#endif
#ifdef AFMT_U32_BE
    case AFMT_U32_BE: return AF_FORMAT_U32_BE;
#endif
#ifdef AFMT_S32_LE
    case AFMT_S32_LE: return AF_FORMAT_S32_LE;
#endif
#ifdef AFMT_S32_BE
    case AFMT_S32_BE: return AF_FORMAT_S32_BE;
#endif
#ifdef AFMT_FLOAT
    case AFMT_FLOAT: return AF_FORMAT_FLOAT_NE;
#endif
    // SPECIALS
#ifdef AFMT_MPEG
    case AFMT_MPEG: return AF_FORMAT_MPEG2;
#endif
#ifdef AFMT_AC3
    case AFMT_AC3: return AF_FORMAT_AC3_NE;
#endif
    }
    mp_tmsg(MSGT_GLOBAL,MSGL_ERR,"[AO OSS] Unknown/Unsupported OSS format: %x.\n", format);
    return -1;
}

static char *dsp=PATH_DEV_DSP;
static audio_buf_info zz;
static int audio_fd=-1;
static int prepause_space;

static const char *oss_mixer_device = PATH_DEV_MIXER;
static int oss_mixer_channel = SOUND_MIXER_PCM;

#ifdef SNDCTL_DSP_GETPLAYVOL
static int volume_oss4(ao_control_vol_t *vol, int cmd) {
    int v;

    if (audio_fd < 0)
        return CONTROL_ERROR;

    if (cmd == AOCONTROL_GET_VOLUME) {
        if (ioctl(audio_fd, SNDCTL_DSP_GETPLAYVOL, &v) == -1)
            return CONTROL_ERROR;
        vol->right = (v & 0xff00) >> 8;
        vol->left = v & 0x00ff;
        return CONTROL_OK;
    } else if (cmd == AOCONTROL_SET_VOLUME) {
        v = ((int) vol->right << 8) | (int) vol->left;
        if (ioctl(audio_fd, SNDCTL_DSP_SETPLAYVOL, &v) == -1)
            return CONTROL_ERROR;
        return CONTROL_OK;
    } else
        return CONTROL_UNKNOWN;
}
#endif

// to set/get/query special features/parameters
static int control(int cmd,void *arg){
    switch(cmd){
	case AOCONTROL_GET_VOLUME:
	case AOCONTROL_SET_VOLUME:
	{
	    ao_control_vol_t *vol = (ao_control_vol_t *)arg;
	    int fd, v, devs;

#ifdef SNDCTL_DSP_GETPLAYVOL
        // Try OSS4 first
        if (volume_oss4(vol, cmd) == CONTROL_OK)
            return CONTROL_OK;
#endif

	    if(AF_FORMAT_IS_AC3(ao_data.format))
		return CONTROL_TRUE;

	    if ((fd = open(oss_mixer_device, O_RDONLY)) != -1)
	    {
		ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
		if (devs & (1 << oss_mixer_channel))
		{
		    if (cmd == AOCONTROL_GET_VOLUME)
		    {
		        ioctl(fd, MIXER_READ(oss_mixer_channel), &v);
			vol->right = (v & 0xFF00) >> 8;
			vol->left = v & 0x00FF;
		    }
		    else
		    {
		        v = ((int)vol->right << 8) | (int)vol->left;
			ioctl(fd, MIXER_WRITE(oss_mixer_channel), &v);
		    }
		}
		else
		{
		    close(fd);
		    return CONTROL_ERROR;
		}
		close(fd);
		return CONTROL_OK;
	    }
	}
	return CONTROL_ERROR;
    }
    return CONTROL_UNKNOWN;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,const struct mp_chmap *channels,int format,int flags){
  char *mixer_channels [SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;
  int oss_format;
  char *mdev = mixer_device, *mchan = mixer_channel;

  mp_msg(MSGT_AO,MSGL_V,"ao2: %d Hz  %d chans  %s\n",rate,ao_data.channels.num,
    af_fmt2str_short(format));

  if (ao_subdevice) {
    char *m,*c;
    m = strchr(ao_subdevice,':');
    if(m) {
      c = strchr(m+1,':');
      if(c) {
        mchan = c+1;
        c[0] = '\0';
      }
      mdev = m+1;
      m[0] = '\0';
    }
    dsp = ao_subdevice;
  }

  if(mdev)
    oss_mixer_device=mdev;
  else
    oss_mixer_device=PATH_DEV_MIXER;

  if(mchan){
    int fd, devs, i;

    if ((fd = open(oss_mixer_device, O_RDONLY)) == -1){
      mp_tmsg(MSGT_AO,MSGL_ERR,"[AO OSS] audio_setup: Can't open mixer device %s: %s\n",
        oss_mixer_device, strerror(errno));
    }else{
      ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devs);
      close(fd);

      for (i=0; i<SOUND_MIXER_NRDEVICES; i++){
        if(!strcasecmp(mixer_channels[i], mchan)){
          if(!(devs & (1 << i))){
            mp_tmsg(MSGT_AO,MSGL_ERR,"[AO OSS] audio_setup: Audio card mixer does not have channel '%s', using default.\n",mchan);
            i = SOUND_MIXER_NRDEVICES+1;
            break;
          }
          oss_mixer_channel = i;
          break;
        }
      }
      if(i==SOUND_MIXER_NRDEVICES){
        mp_tmsg(MSGT_AO,MSGL_ERR,"[AO OSS] audio_setup: Audio card mixer does not have channel '%s', using default.\n",mchan);
      }
    }
  } else
    oss_mixer_channel = SOUND_MIXER_PCM;

  mp_msg(MSGT_AO,MSGL_V,"audio_setup: using '%s' dsp device\n", dsp);
  mp_msg(MSGT_AO,MSGL_V,"audio_setup: using '%s' mixer device\n", oss_mixer_device);
  mp_msg(MSGT_AO,MSGL_V,"audio_setup: using '%s' mixer device\n", mixer_channels[oss_mixer_channel]);

#ifdef __linux__
  audio_fd=open(dsp, O_WRONLY | O_NONBLOCK);
#else
  audio_fd=open(dsp, O_WRONLY);
#endif
  if(audio_fd<0){
    mp_tmsg(MSGT_AO,MSGL_ERR,"[AO OSS] audio_setup: Can't open audio device %s: %s\n", dsp, strerror(errno));
    return 0;
  }

#ifdef __linux__
  /* Remove the non-blocking flag */
  if(fcntl(audio_fd, F_SETFL, 0) < 0) {
   mp_tmsg(MSGT_AO,MSGL_ERR,"[AO OSS] audio_setup: Can't make file descriptor blocking: %s\n", strerror(errno));
   return 0;
  }
#endif

#if defined(FD_CLOEXEC) && defined(F_SETFD)
  fcntl(audio_fd, F_SETFD, FD_CLOEXEC);
#endif

  if(AF_FORMAT_IS_AC3(format)) {
    ao_data.samplerate=rate;
    ioctl (audio_fd, SNDCTL_DSP_SPEED, &ao_data.samplerate);
  }

ac3_retry:
  if (AF_FORMAT_IS_AC3(format))
    format = AF_FORMAT_AC3_NE;
  ao_data.format=format;
  oss_format=format2oss(format);
  if (oss_format == -1) {
#if BYTE_ORDER == BIG_ENDIAN
    oss_format=AFMT_S16_BE;
#else
    oss_format=AFMT_S16_LE;
#endif
    format=AF_FORMAT_S16_NE;
  }
  if( ioctl(audio_fd, SNDCTL_DSP_SETFMT, &oss_format)<0 ||
      oss_format != format2oss(format)) {
    mp_tmsg(MSGT_AO,MSGL_WARN, "[AO OSS] Can't set audio device %s to %s output, trying %s...\n", dsp,
            af_fmt2str_short(format), af_fmt2str_short(AF_FORMAT_S16_NE) );
    format=AF_FORMAT_S16_NE;
    goto ac3_retry;
  }
#if 0
  if(oss_format!=format2oss(format))
	mp_msg(MSGT_AO,MSGL_WARN,"WARNING! Your soundcard does NOT support %s sample format! Broken audio or bad playback speed are possible! Try with '-af format'\n",audio_out_format_name(format));
#endif

  ao_data.format = oss2format(oss_format);
  if (ao_data.format == -1) return 0;

  mp_msg(MSGT_AO,MSGL_V,"audio_setup: sample format: %s (requested: %s)\n",
    af_fmt2str_short(ao_data.format), af_fmt2str_short(format));

  if(!AF_FORMAT_IS_AC3(format)) {
    mp_chmap_reorder_to_alsa(&ao_data.channels);
    int reqchannels = ao_data.channels.num;
    // We only use SNDCTL_DSP_CHANNELS for >2 channels, in case some drivers don't have it
    if (reqchannels > 2) {
      int nchannels = reqchannels;
      if ( ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &nchannels) == -1 ||
	   nchannels != reqchannels ) {
	mp_tmsg(MSGT_AO,MSGL_ERR,"[AO OSS] audio_setup: Failed to set audio device to %d channels.\n", reqchannels);
	return 0;
      }
    }
    else {
      int c = reqchannels-1;
      if (ioctl (audio_fd, SNDCTL_DSP_STEREO, &c) == -1) {
	mp_tmsg(MSGT_AO,MSGL_ERR,"[AO OSS] audio_setup: Failed to set audio device to %d channels.\n", reqchannels);
	return 0;
      }
      mp_chmap_from_channels(&ao_data.channels, c + 1);
    }
    mp_msg(MSGT_AO,MSGL_V,"audio_setup: using %d channels (requested: %d)\n", ao_data.channels.num, reqchannels);
    // set rate
    ao_data.samplerate=rate;
    ioctl (audio_fd, SNDCTL_DSP_SPEED, &ao_data.samplerate);
    mp_msg(MSGT_AO,MSGL_V,"audio_setup: using %d Hz samplerate (requested: %d)\n",ao_data.samplerate,rate);
  }

  if(ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &zz)==-1){
      int r=0;
      mp_tmsg(MSGT_AO,MSGL_WARN,"[AO OSS] audio_setup: driver doesn't support SNDCTL_DSP_GETOSPACE :-(\n");
      if(ioctl(audio_fd, SNDCTL_DSP_GETBLKSIZE, &r)==-1){
          mp_msg(MSGT_AO,MSGL_V,"audio_setup: %d bytes/frag (config.h)\n",ao_data.outburst);
      } else {
          ao_data.outburst=r;
          mp_msg(MSGT_AO,MSGL_V,"audio_setup: %d bytes/frag (GETBLKSIZE)\n",ao_data.outburst);
      }
  } else {
      mp_msg(MSGT_AO,MSGL_V,"audio_setup: frags: %3d/%d  (%d bytes/frag)  free: %6d\n",
          zz.fragments, zz.fragstotal, zz.fragsize, zz.bytes);
      if(ao_data.buffersize==-1) ao_data.buffersize=zz.bytes;
      ao_data.outburst=zz.fragsize;
  }

  if(ao_data.buffersize==-1){
    // Measuring buffer size:
    void* data;
    ao_data.buffersize=0;
#ifdef HAVE_AUDIO_SELECT
    data=malloc(ao_data.outburst); memset(data,0,ao_data.outburst);
    while(ao_data.buffersize<0x40000){
      fd_set rfds;
      struct timeval tv;
      FD_ZERO(&rfds); FD_SET(audio_fd,&rfds);
      tv.tv_sec=0; tv.tv_usec = 0;
      if(!select(audio_fd+1, NULL, &rfds, NULL, &tv)) break;
      write(audio_fd,data,ao_data.outburst);
      ao_data.buffersize+=ao_data.outburst;
    }
    free(data);
    if(ao_data.buffersize==0){
        mp_tmsg(MSGT_AO,MSGL_ERR,"[AO OSS]\n   ***  Your audio driver DOES NOT support select()  ***\n Recompile mpv with #undef HAVE_AUDIO_SELECT in config.h !\n\n");
        return 0;
    }
#endif
  }

  ao_data.bps=ao_data.channels.num;
  switch (ao_data.format & AF_FORMAT_BITS_MASK) {
  case AF_FORMAT_8BIT:
    break;
  case AF_FORMAT_16BIT:
    ao_data.bps*=2;
    break;
  case AF_FORMAT_24BIT:
    ao_data.bps*=3;
    break;
  case AF_FORMAT_32BIT:
    ao_data.bps*=4;
    break;
  }

  ao_data.outburst-=ao_data.outburst % ao_data.bps; // round down
  ao_data.bps*=ao_data.samplerate;

    return 1;
}

// close audio device
static void uninit(int immed){
    if(audio_fd == -1) return;
#ifdef SNDCTL_DSP_SYNC
    // to get the buffer played
    if (!immed)
	ioctl(audio_fd, SNDCTL_DSP_SYNC, NULL);
#endif
#ifdef SNDCTL_DSP_RESET
    if (immed)
	ioctl(audio_fd, SNDCTL_DSP_RESET, NULL);
#endif
    close(audio_fd);
    audio_fd = -1;
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void){
  int oss_format;
    uninit(1);
    audio_fd=open(dsp, O_WRONLY);
    if(audio_fd < 0){
	mp_tmsg(MSGT_AO,MSGL_ERR,"[AO OSS]\nFatal error: *** CANNOT RE-OPEN / RESET AUDIO DEVICE *** %s\n", strerror(errno));
	return;
    }

#if defined(FD_CLOEXEC) && defined(F_SETFD)
  fcntl(audio_fd, F_SETFD, FD_CLOEXEC);
#endif

  oss_format = format2oss(ao_data.format);
  if(AF_FORMAT_IS_AC3(ao_data.format))
    ioctl (audio_fd, SNDCTL_DSP_SPEED, &ao_data.samplerate);
  ioctl (audio_fd, SNDCTL_DSP_SETFMT, &oss_format);
  if(!AF_FORMAT_IS_AC3(ao_data.format)) {
    if (ao_data.channels.num > 2)
      ioctl (audio_fd, SNDCTL_DSP_CHANNELS, &ao_data.channels.num);
    else {
      int c = ao_data.channels.num-1;
      ioctl (audio_fd, SNDCTL_DSP_STEREO, &c);
    }
    ioctl (audio_fd, SNDCTL_DSP_SPEED, &ao_data.samplerate);
  }
}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
    prepause_space = get_space();
    uninit(1);
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
    int fillcnt;
    reset();
    fillcnt = get_space() - prepause_space;
    if (fillcnt > 0 && !(ao_data.format & AF_FORMAT_SPECIAL_MASK)) {
      void *silence = calloc(fillcnt, 1);
      play(silence, fillcnt, 0);
      free(silence);
    }
}


// return: how many bytes can be played without blocking
static int get_space(void){
  int playsize=ao_data.outburst;

#ifdef SNDCTL_DSP_GETOSPACE
  if(ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &zz)!=-1){
      // calculate exact buffer space:
      playsize = zz.fragments*zz.fragsize;
      return playsize;
  }
#endif

    // check buffer
#ifdef HAVE_AUDIO_SELECT
    {  fd_set rfds;
       struct timeval tv;
       FD_ZERO(&rfds);
       FD_SET(audio_fd, &rfds);
       tv.tv_sec = 0;
       tv.tv_usec = 0;
       if(!select(audio_fd+1, NULL, &rfds, NULL, &tv)) return 0; // not block!
    }
#endif

  return ao_data.outburst;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){
    if(len==0)
        return len;
    if(len>ao_data.outburst || !(flags & AOPLAY_FINAL_CHUNK)) {
        len/=ao_data.outburst;
        len*=ao_data.outburst;
    }
    len=write(audio_fd,data,len);
    return len;
}

static int audio_delay_method=2;

// return: delay in seconds between first and last sample in buffer
static float get_delay(void){
  /* Calculate how many bytes/second is sent out */
  if(audio_delay_method==2){
#ifdef SNDCTL_DSP_GETODELAY
      int r=0;
      if(ioctl(audio_fd, SNDCTL_DSP_GETODELAY, &r)!=-1)
         return ((float)r)/(float)ao_data.bps;
#endif
      audio_delay_method=1; // fallback if not supported
  }
  if(audio_delay_method==1){
      // SNDCTL_DSP_GETOSPACE
      if(ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &zz)!=-1)
         return ((float)(ao_data.buffersize-zz.bytes))/(float)ao_data.bps;
      audio_delay_method=0; // fallback if not supported
  }
  return ((float)ao_data.buffersize)/(float)ao_data.bps;
}
