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
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>

#include "config.h"
#include "mp_msg.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"

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
#define NUM_BUFS 32

typedef struct ao_macosx_s
{
  /* CoreAudio */
  AudioDeviceID outputDeviceID;
  AudioStreamBasicDescription outputStreamBasicDescription;

  /* Ring-buffer */
  /* does not need explicit synchronization, but needs to allocate
   * (num_chunks + 1) * chunk_size memory to store num_chunks * chunk_size
   * data */
  unsigned char *buffer;
  unsigned int buffer_len; ///< must always be (num_chunks + 1) * chunk_size
  unsigned int num_chunks;
  unsigned int chunk_size;
  
  unsigned int buf_read_pos;
  unsigned int buf_write_pos;
} ao_macosx_t;

static ao_macosx_t *ao;

/**
 * \brief return number of free bytes in the buffer
 *    may only be called by mplayer's thread
 * \return minimum number of free bytes in buffer, value may change between
 *    two immediately following calls, and the real number of free bytes
 *    might actually be larger!
 */
static int buf_free() {
  int free = ao->buf_read_pos - ao->buf_write_pos - ao->chunk_size;
  if (free < 0) free += ao->buffer_len;
  return free;
}

/**
 * \brief return number of buffered bytes
 *    may only be called by playback thread
 * \return minimum number of buffered bytes, value may change between
 *    two immediately following calls, and the real number of buffered bytes
 *    might actually be larger!
 */
static int buf_used() {
  int used = ao->buf_write_pos - ao->buf_read_pos;
  if (used < 0) used += ao->buffer_len;
  return used;
}

/**
 * \brief add data to ringbuffer
 */
static int write_buffer(unsigned char* data, int len){
  int first_len = ao->buffer_len - ao->buf_write_pos;
  int free = buf_free();
  if (len > free) len = free;
  if (first_len > len) first_len = len;
  // till end of buffer
  memcpy (&ao->buffer[ao->buf_write_pos], data, first_len);
  if (len > first_len) { // we have to wrap around
    // remaining part from beginning of buffer
    memcpy (ao->buffer, &data[first_len], len - first_len);
  }
  ao->buf_write_pos = (ao->buf_write_pos + len) % ao->buffer_len;
  return len;
}

/**
 * \brief remove data from ringbuffer
 */
static int read_buffer(unsigned char* data,int len){
  int first_len = ao->buffer_len - ao->buf_read_pos;
  int buffered = buf_used();
  if (len > buffered) len = buffered;
  if (first_len > len) first_len = len;
  // till end of buffer
  memcpy (data, &ao->buffer[ao->buf_read_pos], first_len);
  if (len > first_len) { // we have to wrap around
    // remaining part from beginning of buffer
    memcpy (&data[first_len], ao->buffer, len - first_len);
  }
  ao->buf_read_pos = (ao->buf_read_pos + len) % ao->buffer_len;
  return len;
}

/* end ring buffer stuff */

/* The function that the CoreAudio thread calls when it wants more data */
static OSStatus audioDeviceIOProc(AudioDeviceID inDevice, const AudioTimeStamp *inNow, const AudioBufferList *inInputData, const AudioTimeStamp *inInputTime, AudioBufferList *outOutputData, const AudioTimeStamp *inOutputTime, void *inClientData)
{
  outOutputData->mBuffers[0].mDataByteSize =
    read_buffer((char *)outOutputData->mBuffers[0].mData, ao->chunk_size);

  return 0;
}


static int control(int cmd,void *arg){
  OSStatus status;
  UInt32 propertySize;
  ao_control_vol_t* vol = (ao_control_vol_t*)arg;
  UInt32 stereoChannels[2];
  static float volume=0.5;
	switch (cmd) {
	case AOCONTROL_SET_DEVICE:
	case AOCONTROL_GET_DEVICE:
	  /* unimplemented/meaningless */
	  return CONTROL_FALSE;
	case AOCONTROL_GET_VOLUME:
	    propertySize=sizeof(stereoChannels);
	    status = AudioDeviceGetProperty(ao->outputDeviceID, NULL, 0,
		kAudioDevicePropertyPreferredChannelsForStereo, &propertySize,
		&stereoChannels);
//	    printf("OSX: stereochannels %d ; %d \n",stereoChannels[0],stereoChannels[1]);
	    propertySize=sizeof(volume);
	    status = AudioDeviceGetProperty(ao->outputDeviceID, stereoChannels[0], false, kAudioDevicePropertyVolumeScalar, &propertySize, &volume);
//	    printf("OSX: get volume=%5.3f   status=%d  \n",volume,status);
	    vol->left=(int)(volume*100.0);
	    status = AudioDeviceGetProperty(ao->outputDeviceID, stereoChannels[1], false, kAudioDevicePropertyVolumeScalar, &propertySize, &volume);
	    vol->right=(int)(volume*100.0);
	  return CONTROL_TRUE;
	case AOCONTROL_SET_VOLUME:
	    propertySize=sizeof(stereoChannels);
	    status = AudioDeviceGetProperty(ao->outputDeviceID, NULL, 0,
		kAudioDevicePropertyPreferredChannelsForStereo, &propertySize,
		&stereoChannels);
//	    printf("OSX: stereochannels %d ; %d \n",stereoChannels[0],stereoChannels[1]);
	    propertySize=sizeof(volume);
	    volume=vol->left/100.0;
	    status = AudioDeviceSetProperty(ao->outputDeviceID, 0, stereoChannels[0], 0, kAudioDevicePropertyVolumeScalar, propertySize, &volume);
//	    printf("OSX: set volume=%5.3f   status=%d\n",volume,status);
	    volume=vol->right/100.0;
	    status = AudioDeviceSetProperty(ao->outputDeviceID, 0, stereoChannels[1], 0, kAudioDevicePropertyVolumeScalar, propertySize, &volume);
	  return CONTROL_TRUE;
	case AOCONTROL_QUERY_FORMAT:
	  /* stick with what CoreAudio requests */
	  return CONTROL_FALSE;
	default:
	  return CONTROL_FALSE;
	}
	
}


static void print_format(const char* str,AudioStreamBasicDescription *f){
    uint32_t flags=(uint32_t) f->mFormatFlags;
    ao_msg(MSGT_AO,MSGL_V, "%s %7.1fHz %dbit [%c%c%c%c] %s %s %s%s%s%s\n",
	    str, f->mSampleRate, f->mBitsPerChannel,
	    (int)(f->mFormatID & 0xff000000) >> 24,
	    (int)(f->mFormatID & 0x00ff0000) >> 16,
	    (int)(f->mFormatID & 0x0000ff00) >>  8,
	    (int)(f->mFormatID & 0x000000ff) >>  0,
	    (flags&kAudioFormatFlagIsFloat) ? "float" : "int",
	    (flags&kAudioFormatFlagIsBigEndian) ? "BE" : "LE",
	    (flags&kAudioFormatFlagIsSignedInteger) ? "S" : "U",
	    (flags&kAudioFormatFlagIsPacked) ? " packed" : "",
	    (flags&kAudioFormatFlagIsAlignedHigh) ? " aligned" : "",
	    (flags&kAudioFormatFlagIsNonInterleaved) ? " ni" : "" );

    ao_msg(MSGT_AO,MSGL_DBG2, "%5d mBytesPerPacket\n",
	    (int)f->mBytesPerPacket);
    ao_msg(MSGT_AO,MSGL_DBG2, "%5d mFramesPerPacket\n",
	    (int)f->mFramesPerPacket);
    ao_msg(MSGT_AO,MSGL_DBG2, "%5d mBytesPerFrame\n",
	    (int)f->mBytesPerFrame);
    ao_msg(MSGT_AO,MSGL_DBG2, "%5d mChannelsPerFrame\n",
	    (int)f->mChannelsPerFrame);

}


static int init(int rate,int channels,int format,int flags)
{
  OSStatus status;
  UInt32 propertySize;
  int rc;
  int i;

  ao = (ao_macosx_t *)malloc(sizeof(ao_macosx_t));

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


    propertySize = sizeof(ao->outputStreamBasicDescription);
    status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioDevicePropertyStreamFormat, &propertySize, &ao->outputStreamBasicDescription);
    if(!status) print_format("default:",&ao->outputStreamBasicDescription);


#if 1
// dump supported format list:
{   AudioStreamBasicDescription* p;
    Boolean ow;
    int i;
    propertySize=0; //sizeof(p);
//    status = AudioDeviceGetPropertyInfo(ao->outputDeviceID, 0, false, kAudioStreamPropertyPhysicalFormats, &propertySize, &ow);
    status = AudioDeviceGetPropertyInfo(ao->outputDeviceID, 0, false, kAudioDevicePropertyStreamFormats, &propertySize, &ow);
    if (status) {
        ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceGetPropertyInfo returned 0x%X when getting kAudioDevicePropertyStreamFormats\n", (int)status);
    }
    p=malloc(propertySize);
//    status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioStreamPropertyPhysicalFormats, &propertySize, p);
    status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioDevicePropertyStreamFormats, &propertySize, p);
    if (status) {
        ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceGetProperty returned 0x%X when getting kAudioDevicePropertyStreamFormats\n", (int)status);
//	return CONTROL_FALSE;
    }
    for(i=0;i<propertySize/sizeof(AudioStreamBasicDescription);i++)
	print_format("support:",&p[i]);
//    printf("FORMATS: (%d) %p %p %p %p\n",propertySize,p[0],p[1],p[2],p[3]);
    free(p);
}
#endif

    // fill in our wanted format, and let's see if the driver accepts it or
    // offers some similar alternative:
    propertySize = sizeof(ao->outputStreamBasicDescription);
    memset(&ao->outputStreamBasicDescription,0,propertySize);
    ao->outputStreamBasicDescription.mSampleRate=rate;
    ao->outputStreamBasicDescription.mFormatID=kAudioFormatLinearPCM;
    ao->outputStreamBasicDescription.mChannelsPerFrame=channels;
    switch(format&AF_FORMAT_BITS_MASK){
    case AF_FORMAT_8BIT:  ao->outputStreamBasicDescription.mBitsPerChannel=8; break;
    case AF_FORMAT_16BIT: ao->outputStreamBasicDescription.mBitsPerChannel=16; break;
    case AF_FORMAT_24BIT: ao->outputStreamBasicDescription.mBitsPerChannel=24; break;
    case AF_FORMAT_32BIT: ao->outputStreamBasicDescription.mBitsPerChannel=32; break;
    }
    if((format&AF_FORMAT_POINT_MASK)==AF_FORMAT_F){
	// float
	ao->outputStreamBasicDescription.mFormatFlags=kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked;
    } else if((format&AF_FORMAT_SIGN_MASK)==AF_FORMAT_SI){
	// signed int
	ao->outputStreamBasicDescription.mFormatFlags=kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked;
    } else {
	// unsigned int
	ao->outputStreamBasicDescription.mFormatFlags=kAudioFormatFlagIsPacked;
    }
    if((format&AF_FORMAT_END_MASK)==AF_FORMAT_BE)
	ao->outputStreamBasicDescription.mFormatFlags|=kAudioFormatFlagIsBigEndian;

    ao->outputStreamBasicDescription.mBytesPerPacket=
    ao->outputStreamBasicDescription.mBytesPerFrame=channels*(ao->outputStreamBasicDescription.mBitsPerChannel/8);
    ao->outputStreamBasicDescription.mFramesPerPacket=1;

    print_format("wanted: ",&ao->outputStreamBasicDescription);

    // try 1: ask if it accepts our specific requirements?
    propertySize = sizeof(ao->outputStreamBasicDescription);
//    status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioStreamPropertyPhysicalFormatMatch, &propertySize, &ao->outputStreamBasicDescription);
    status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioDevicePropertyStreamFormatMatch, &propertySize, &ao->outputStreamBasicDescription);
    if (status || ao->outputStreamBasicDescription.mSampleRate!=rate
	       || ao->outputStreamBasicDescription.mFormatID!=kAudioFormatLinearPCM) {
        ao_msg(MSGT_AO,MSGL_V, "AudioDeviceGetProperty returned 0x%X when getting kAudioDevicePropertyStreamFormatMatch\n", (int)status);
	// failed (error, bad rate or bad type)
	// try 2: set only rate & type, no format details (bits, channels etc)
	propertySize = sizeof(ao->outputStreamBasicDescription);
	memset(&ao->outputStreamBasicDescription,0,propertySize);
	ao->outputStreamBasicDescription.mSampleRate=rate;
	ao->outputStreamBasicDescription.mFormatID=kAudioFormatLinearPCM;
	ao->outputStreamBasicDescription.mChannelsPerFrame=channels;
//	status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioStreamPropertyPhysicalFormatMatch, &propertySize, &ao->outputStreamBasicDescription);
	status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioDevicePropertyStreamFormatMatch, &propertySize, &ao->outputStreamBasicDescription);
	if (status || ao->outputStreamBasicDescription.mFormatID!=kAudioFormatLinearPCM) {
    	    ao_msg(MSGT_AO,MSGL_V, "AudioDeviceGetProperty returned 0x%X when getting kAudioDevicePropertyStreamFormatMatch\n", (int)status);
	    // failed again. (error or bad type)
	    // giving up... just read the default.
	    propertySize = sizeof(ao->outputStreamBasicDescription);
//	    status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioStreamPropertyPhysicalFormat, &propertySize, &ao->outputStreamBasicDescription);
	    status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioDevicePropertyStreamFormat, &propertySize, &ao->outputStreamBasicDescription);
	    if (status) {
		// failed to read the default format - WTF?
    		ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceGetProperty returned 0x%X when getting kAudioDevicePropertyStreamFormat\n", (int)status);
		return CONTROL_FALSE;
	    }
	}
    }

//    propertySize = sizeof(ao->outputStreamBasicDescription);
//    status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioDevicePropertyStreamFormatSupported, &propertySize, &ao->outputStreamBasicDescription);
//    if (status) {
//        ao_msg(MSGT_AO,MSGL_V, "AudioDeviceGetProperty returned 0x%X when getting kAudioDevicePropertyStreamFormatSupported\n", (int)status);
//    }

    // ok, now try to set the new (default or matched) audio format:
    print_format("best:   ",&ao->outputStreamBasicDescription);
    propertySize = sizeof(ao->outputStreamBasicDescription);
    status = AudioDeviceSetProperty(ao->outputDeviceID, 0, 0, false, kAudioDevicePropertyStreamFormat, propertySize, &ao->outputStreamBasicDescription);
    if(status)
	ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceSetProperty returned 0x%X when getting kAudioDevicePropertyStreamFormat\n", (int)status);

    // see what did we get finally... we'll be forced to use this anyway :(
    propertySize = sizeof(ao->outputStreamBasicDescription);
    status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioDevicePropertyStreamFormat, &propertySize, &ao->outputStreamBasicDescription);
    print_format("final:  ",&ao->outputStreamBasicDescription);

    /* get requested buffer length */
    // TODO: set NUM_BUFS dinamically, based on buffer size!
    propertySize = sizeof(ao->chunk_size);
    status = AudioDeviceGetProperty(ao->outputDeviceID, 0, false, kAudioDevicePropertyBufferSize, &propertySize, &ao->chunk_size);
    if (status) {
        ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceGetProperty returned %d when getting kAudioDevicePropertyBufferSize\n", (int)status);
	return CONTROL_FALSE;
    }
    ao_msg(MSGT_AO,MSGL_V, "%5d chunk size\n", (int)ao->chunk_size);

    ao_data.samplerate = ao->outputStreamBasicDescription.mSampleRate;
	ao_data.channels = channels;
    ao_data.outburst = ao_data.buffersize = ao->chunk_size;
    ao_data.bps = 
      ao_data.samplerate * ao->outputStreamBasicDescription.mBytesPerFrame;

    if (ao->outputStreamBasicDescription.mFormatID == kAudioFormatLinearPCM) {
      uint32_t flags = ao->outputStreamBasicDescription.mFormatFlags;
      if (flags & kAudioFormatFlagIsFloat) {
	ao_data.format = (flags&kAudioFormatFlagIsBigEndian) ? AF_FORMAT_FLOAT_BE : AF_FORMAT_FLOAT_LE;
      } else {
	ao_msg(MSGT_AO,MSGL_WARN, "Unsupported audio output "
	       "format 0x%X. Please report this to the developer\n", format);
	return CONTROL_FALSE;
      }
      
    } else {
      /* TODO: handle AFMT_AC3, AFMT_MPEG & friends */
      ao_msg(MSGT_AO,MSGL_WARN, "Default Audio Device doesn't "
	     "support Linear PCM!\n");
      return CONTROL_FALSE;
    }
  
    /* Allocate ring-buffer memory */
    ao->num_chunks = NUM_BUFS;
    ao->buffer_len = (ao->num_chunks + 1) * ao->chunk_size;
    ao->buffer = (unsigned char *)malloc(ao->buffer_len);


    /* Prepare for playback */

    /* Set the IO proc that CoreAudio will call when it needs data */
    status = AudioDeviceAddIOProc(ao->outputDeviceID, audioDeviceIOProc, NULL);
    if (status) {
        ao_msg(MSGT_AO,MSGL_WARN, "AudioDeviceAddIOProc returned %d\n", (int)status);
	return CONTROL_FALSE;
    }
 
    /* Start callback */
    reset();
    
    return CONTROL_OK;
}


static int play(void* output_samples,int num_bytes,int flags)
{  
  return write_buffer(output_samples, num_bytes);
}

/* set variables and buffer to initial state */
static void reset()
{
  audio_pause();
  /* reset ring-buffer state */
  ao->buf_read_pos=0;
  ao->buf_write_pos=0;
  audio_resume();
  
  return;
}


/* return available space */
static int get_space()
{
  return buf_free();
}


/* return delay until audio is played */
static float get_delay()
{
  int buffered = ao->buffer_len - ao->chunk_size - buf_free(); // could be less
  // inaccurate, should also contain the data buffered e.g. by the OS
  return (float)(buffered)/(float)ao_data.bps;
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

  free(ao->buffer);
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
