/*
 *
 *  ao_macosx.c
 *
 *      Original Copyright (C) Timothy J. Wood - Aug 2000
 *
 *  This file is part of libao, a cross-platform library.  See
 *  README for a history of this source code.
 *
 *  libao is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  libao is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * The MacOS X CoreAudio framework doesn't mesh as simply as some
 * simpler frameworks do.  This is due to the fact that CoreAudio pulls
 * audio samples rather than having them pushed at it (which is nice
 * when you are wanting to do good buffering of audio). 
 */

/* Change log:
 * 
 * 14/5-2003: Ported to MPlayer libao2 by Dan Christiansen
 *
 *            AC-3 and MPEG audio passthrough is possible, but I don't have
 *            access to a sound card that supports it.
 */

#include <CoreAudio/AudioHardware.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>

#include "mp_msg.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "afmt.h"

static ao_info_t info =
  {
    "Darwin/Mac OS X native audio output",
    "macosx",
    "Timothy J. Wood & Dan Christiansen",
    ""
  };

LIBAO_EXTERN(macosx)

/* Prefix for all mp_msg() calls */
#define ao_msg(a, b, c...) mp_msg(a, b, "AO: [macosx] " c)

/* This is large, but best (maybe it should be even larger).
 * CoreAudio supposedly has an internal latency in the order of 2ms */
#define NUM_BUFS 16

typedef struct ao_macosx_s
{
  /* CoreAudio */
  AudioDeviceID outputDeviceID;
  AudioStreamBasicDescription outputStreamBasicDescription;

  /* Ring-buffer */
  pthread_mutex_t buffer_mutex; /* mutex covering buffer variables */

  unsigned char *buffer[NUM_BUFS];
  unsigned int buffer_len;
  
  unsigned int buf_read;
  unsigned int buf_write;
  unsigned int buf_read_pos;
  unsigned int buf_write_pos;
  int full_buffers;
  int buffered_bytes;
} ao_macosx_t;

static ao_macosx_t *ao;

/* General purpose Ring-buffering routines */
static int write_buffer(unsigned char* data,int len){
  int len2=0;
  int x;

  while(len>0){
    if(ao->full_buffers==NUM_BUFS) {
      ao_msg(MSGT_AO,MSGL_V, "Buffer overrun\n");
      break;
    }

    x=ao->buffer_len-ao->buf_write_pos;
    if(x>len) x=len;
    memcpy(ao->buffer[ao->buf_write]+ao->buf_write_pos,data+len2,x);

    /* accessing common variables, locking mutex */
    pthread_mutex_lock(&ao->buffer_mutex);
    len2+=x; len-=x;
    ao->buffered_bytes+=x; ao->buf_write_pos+=x;
    if(ao->buf_write_pos>=ao->buffer_len) {
      /* block is full, find next! */
      ao->buf_write=(ao->buf_write+1)%NUM_BUFS;
      ++ao->full_buffers;
      ao->buf_write_pos=0;
    }
    pthread_mutex_unlock(&ao->buffer_mutex);
  }

  return len2;
}

static int read_buffer(unsigned char* data,int len){
  int len2=0;
  int x;

  while(len>0){
    if(ao->full_buffers==0) {
      ao_msg(MSGT_AO,MSGL_V, "Buffer underrun\n");
      break;
    }

    x=ao->buffer_len-ao->buf_read_pos;
    if(x>len) x=len;
    memcpy(data+len2,ao->buffer[ao->buf_read]+ao->buf_read_pos,x);
    len2+=x; len-=x;

    /* accessing common variables, locking mutex */
    pthread_mutex_lock(&ao->buffer_mutex);
    ao->buffered_bytes-=x; ao->buf_read_pos+=x;
    if(ao->buf_read_pos>=ao->buffer_len){
      /* block is empty, find next! */
       ao->buf_read=(ao->buf_read+1)%NUM_BUFS;
       --ao->full_buffers;
       ao->buf_read_pos=0;
    }
    pthread_mutex_unlock(&ao->buffer_mutex);
  }
  
  
  return len2;
}

/* end ring buffer stuff */

/* The function that the CoreAudio thread calls when it wants more data */
static OSStatus audioDeviceIOProc(AudioDeviceID inDevice, const AudioTimeStamp *inNow, const AudioBufferList *inInputData, const AudioTimeStamp *inInputTime, AudioBufferList *outOutputData, const AudioTimeStamp *inOutputTime, void *inClientData)
{
  outOutputData->mBuffers[0].mDataByteSize =
    read_buffer((char *)outOutputData->mBuffers[0].mData, ao->buffer_len);

  return 0;
}


static int control(int cmd,void *arg){
	switch (cmd) {
	case AOCONTROL_SET_DEVICE:
	case AOCONTROL_GET_DEVICE:
	case AOCONTROL_GET_VOLUME:
	case AOCONTROL_SET_VOLUME:
	  /* unimplemented/meaningless */
	  return CONTROL_FALSE;
	case AOCONTROL_QUERY_FORMAT:
	  /* stick with what CoreAudio requests */
	  return CONTROL_FALSE;
	default:
	  return CONTROL_FALSE;
	}
	
}


static int init(int rate,int channels,int format,int flags)
{
  OSStatus status;
  UInt32 propertySize;
  int rc;
  int i;

  ao = (ao_macosx_t *)malloc(sizeof(ao_macosx_t));

  /* initialise mutex */
  pthread_mutex_init(&ao->buffer_mutex, NULL);
  pthread_mutex_unlock(&ao->buffer_mutex);

  /* get default output device */ 
  propertySize = sizeof(ao->outputDeviceID);
  status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &propertySize, &(ao->outputDeviceID));
  if (status) {
        ao_msg(MSGT_AO,MSGL_WARN, 
		"AudioHardwareGetProperty returned %d\n",
		(int)status);
	return CONTROL_FALSE;
    }
    
    if (ao->outputDeviceID == kAudioDeviceUnknown) {
        ao_msg(MSGT_AO,MSGL_WARN, "AudioHardwareGetProperty: ao->outputDeviceID is kAudioDeviceUnknown\n");
	return CONTROL_FALSE;
    }
    
    /* get default output format
     * TODO: get all support formats and iterate through them
     */
    propertySize = sizeof(ao->outputStreamBasicDescription);
    status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioDevicePropertyStreamFormat, &propertySize, &ao->outputStreamBasicDescription);
    if (status) {
        ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceGetProperty returned %d when getting kAudioDevicePropertyStreamFormat\n", (int)status);
	return CONTROL_FALSE;
    }

    ao_msg(MSGT_AO,MSGL_V, "hardware format...\n");
    ao_msg(MSGT_AO,MSGL_V, "%f mSampleRate\n", ao->outputStreamBasicDescription.mSampleRate);
    ao_msg(MSGT_AO,MSGL_V, " %c%c%c%c mFormatID\n", 
	    (int)(ao->outputStreamBasicDescription.mFormatID & 0xff000000) >> 24,
	    (int)(ao->outputStreamBasicDescription.mFormatID & 0x00ff0000) >> 16,
	    (int)(ao->outputStreamBasicDescription.mFormatID & 0x0000ff00) >>  8,
	    (int)(ao->outputStreamBasicDescription.mFormatID & 0x000000ff) >>  0);
    ao_msg(MSGT_AO,MSGL_V, "%5d mBytesPerPacket\n",
	    (int)ao->outputStreamBasicDescription.mBytesPerPacket);
    ao_msg(MSGT_AO,MSGL_V, "%5d mFramesPerPacket\n",
	    (int)ao->outputStreamBasicDescription.mFramesPerPacket);
    ao_msg(MSGT_AO,MSGL_V, "%5d mBytesPerFrame\n",
	    (int)ao->outputStreamBasicDescription.mBytesPerFrame);
    ao_msg(MSGT_AO,MSGL_V, "%5d mChannelsPerFrame\n",
	    (int)ao->outputStreamBasicDescription.mChannelsPerFrame);

    /* get requested buffer length */
    propertySize = sizeof(ao->buffer_len);
    status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioDevicePropertyBufferSize, &propertySize, &ao->buffer_len);
    if (status) {
        ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceGetProperty returned %d when getting kAudioDevicePropertyBufferSize\n", (int)status);
	return CONTROL_FALSE;
    }
    ao_msg(MSGT_AO,MSGL_V, "%5d ao->buffer_len\n", (int)ao->buffer_len);

    ao_data.samplerate = ao->outputStreamBasicDescription.mSampleRate;
	ao_data.channels = channels;
    ao_data.outburst = ao_data.buffersize = ao->buffer_len;
    ao_data.bps = 
      ao_data.samplerate * ao->outputStreamBasicDescription.mBytesPerFrame;

    if (ao->outputStreamBasicDescription.mFormatID == kAudioFormatLinearPCM) {
      uint32_t flags = ao->outputStreamBasicDescription.mFormatFlags;
      if (flags & kAudioFormatFlagIsFloat) {
	ao_data.format = AFMT_FLOAT;
      } else {
	ao_msg(MSGT_AO,MSGL_WARN, "Unsupported audio output "
	       "format %d. Please report this to the developer\n",
	       (int)status);
	return CONTROL_FALSE;
      }
      
    } else {
      /* TODO: handle AFMT_AC3, AFMT_MPEG & friends */
      ao_msg(MSGT_AO,MSGL_WARN, "Default Audio Device doesn't "
	     "support Linear PCM!\n");
      return CONTROL_FALSE;
    }
  
    /* Allocate ring-buffer memory */
    for(i=0;i<NUM_BUFS;i++) 
      ao->buffer[i]=(unsigned char *) malloc(ao->buffer_len);


    /* Prepare for playback */

    reset();
    
    /* Set the IO proc that CoreAudio will call when it needs data */
    status = AudioDeviceAddIOProc(ao->outputDeviceID, audioDeviceIOProc, NULL);
    if (status) {
        ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceAddIOProc returned %d\n", (int)status);
	return CONTROL_FALSE;
    }
 
    /* Start callback */
    status = AudioDeviceStart(ao->outputDeviceID, audioDeviceIOProc);
    if (status) {
      ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceStart returned %d\n",
	     (int)status);
      return CONTROL_FALSE;
    }
    
    return CONTROL_OK;
}


static int play(void* output_samples,int num_bytes,int flags)
{  
  return write_buffer(output_samples, num_bytes);
}

/* set variables and buffer to initial state */
static void reset()
{
  int i;
  
  pthread_mutex_lock(&ao->buffer_mutex);
  
  /* reset ring-buffer state */
  ao->buf_read=0;
  ao->buf_write=0;
  ao->buf_read_pos=0;
  ao->buf_write_pos=0;
  
  ao->full_buffers=0;
  ao->buffered_bytes=0;
  
  /* zero output buffer */
  for (i = 0; i < NUM_BUFS; i++)
    bzero(ao->buffer[i], ao->buffer_len);

  pthread_mutex_unlock(&ao->buffer_mutex);
       
  return;
}


/* return available space */
static int get_space()
{
  return (NUM_BUFS-ao->full_buffers)*ao_data.buffersize - ao->buf_write_pos;
}


/* return delay until audio is played */
static float get_delay()
{
  return (float)(ao->buffered_bytes)/(float)ao_data.bps;
}


/* unload plugin and deregister from coreaudio */
static void uninit(int immed)
{
  int i;
  OSErr status;

  reset();

  status = AudioDeviceRemoveIOProc(ao->outputDeviceID, audioDeviceIOProc);
  if (status)
    ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceRemoveIOProc "
	   "returned %d\n", (int)status);

  for(i=0;i<NUM_BUFS;i++) free(ao->buffer[i]);
  free(ao);
}


/* stop playing, keep buffers (for pause) */
static void audio_pause()
{
  OSErr status;

  /* stop callback */
  status = AudioDeviceStop(ao->outputDeviceID, audioDeviceIOProc);
  if (status)
    ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceStop returned %d\n",
	   (int)status);
}


/* resume playing, after audio_pause() */
static void audio_resume()
{
  OSErr status = AudioDeviceStart(ao->outputDeviceID, audioDeviceIOProc);
  if (status)
    ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceStart returned %d\n",
	   (int)status);
}
