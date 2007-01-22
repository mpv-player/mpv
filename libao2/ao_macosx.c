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
 *  You should have received a copy of the GNU General Public License along
 *  with libao; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
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
    "Timothy J. Wood & Dan Christiansen & Chris Roccati",
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
  /* AudioUnit */
  AudioUnit theOutputUnit;
  int packetSize;
  int paused;

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

static ao_macosx_t *ao = NULL;

/**
 * \brief return number of free bytes in the buffer
 *    may only be called by mplayer's thread
 * \return minimum number of free bytes in buffer, value may change between
 *    two immediately following calls, and the real number of free bytes
 *    might actually be larger!
 */
static int buf_free(void) {
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
static int buf_used(void) {
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

OSStatus theRenderProc(void *inRefCon, AudioUnitRenderActionFlags *inActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumFrames, AudioBufferList *ioData)
{
int amt=buf_used();
int req=(inNumFrames)*ao->packetSize;

	if(amt>req)
 		amt=req;

	if(amt)
		read_buffer((unsigned char *)ioData->mBuffers[0].mData, amt);
	else audio_pause();
	ioData->mBuffers[0].mDataByteSize = amt;

 	return noErr;
}

static int control(int cmd,void *arg){
ao_control_vol_t *control_vol;
OSStatus err;
Float32 pan, vol;
	switch (cmd) {
	case AOCONTROL_GET_VOLUME:
		control_vol = (ao_control_vol_t*)arg;
		err = AudioUnitGetParameter(ao->theOutputUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, &vol);

		if(err==0) {
			// printf("GET VOL=%f\n", vol);
			control_vol->left=control_vol->right=vol*100.0/4.0;
			return CONTROL_TRUE;
		}
		else {
			return CONTROL_FALSE;
		}

	case AOCONTROL_SET_VOLUME:
		control_vol = (ao_control_vol_t*)arg;
		
		vol=(control_vol->left+control_vol->right)*4.0/200.0;
		err = AudioUnitSetParameter(ao->theOutputUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, vol, 0);
		if(err==0) {
			// printf("SET VOL=%f\n", vol);
			return CONTROL_TRUE;
		}
		else {
			return CONTROL_FALSE;
		}
	  /* Everything is currently unimplemented */
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
AudioStreamBasicDescription inDesc, outDesc;
ComponentDescription desc; 
Component comp; 
AURenderCallbackStruct renderCallback;
OSStatus err;
UInt32 size, maxFrames;
int aoIsCreated = ao != NULL;

	if (!aoIsCreated)	ao = malloc(sizeof(ao_macosx_t));

	// Build Description for the input format
	inDesc.mSampleRate=rate;
	inDesc.mFormatID=kAudioFormatLinearPCM;
	inDesc.mChannelsPerFrame=channels;
	switch(format&AF_FORMAT_BITS_MASK){
	case AF_FORMAT_8BIT:
		inDesc.mBitsPerChannel=8;
		break;
	case AF_FORMAT_16BIT:
		inDesc.mBitsPerChannel=16;
		break;
	case AF_FORMAT_24BIT:
		inDesc.mBitsPerChannel=24;
		break;
	case AF_FORMAT_32BIT:
		inDesc.mBitsPerChannel=32;
		break;
	default:
		ao_msg(MSGT_AO, MSGL_WARN, "Unsupported format (0x%08x)\n", format);
		return CONTROL_FALSE;
		break;
	}

    if((format&AF_FORMAT_POINT_MASK)==AF_FORMAT_F) {
	// float
		inDesc.mFormatFlags = kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked;
    }
    else if((format&AF_FORMAT_SIGN_MASK)==AF_FORMAT_SI) {
	// signed int
		inDesc.mFormatFlags = kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked;
    }
    else {
	// unsigned int
		inDesc.mFormatFlags = kAudioFormatFlagIsPacked;
    }
  
    if((format&AF_FORMAT_END_MASK)==AF_FORMAT_BE)
		inDesc.mFormatFlags |= kAudioFormatFlagIsBigEndian;

    inDesc.mFramesPerPacket = 1;
    ao->packetSize = inDesc.mBytesPerPacket = inDesc.mBytesPerFrame = inDesc.mFramesPerPacket*channels*(inDesc.mBitsPerChannel/8);
    print_format("source: ",&inDesc);

	if (!aoIsCreated) {
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
				
	comp = FindNextComponent(NULL, &desc);  //Finds an component that meets the desc spec's
	if (comp == NULL) {
		ao_msg(MSGT_AO, MSGL_WARN, "Unable to find Output Unit component\n");
		return CONTROL_FALSE;
	}
		
	err = OpenAComponent(comp, &(ao->theOutputUnit));  //gains access to the services provided by the component
	if (err) {
		ao_msg(MSGT_AO, MSGL_WARN, "Unable to open Output Unit component (err=%d)\n", err);
		return CONTROL_FALSE;
	}

	// Initialize AudioUnit 
	err = AudioUnitInitialize(ao->theOutputUnit);
	if (err) {
		ao_msg(MSGT_AO, MSGL_WARN, "Unable to initialize Output Unit component (err=%d)\n", err);
		return CONTROL_FALSE;
	}
	}

	size =  sizeof(AudioStreamBasicDescription);
	err = AudioUnitSetProperty(ao->theOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &inDesc, size);

	if (err) {
		ao_msg(MSGT_AO, MSGL_WARN, "Unable to set the input format (err=%d)\n", err);
		return CONTROL_FALSE;
	}

	size = sizeof(UInt32);
	err = AudioUnitGetProperty(ao->theOutputUnit, kAudioDevicePropertyBufferSize, kAudioUnitScope_Input, 0, &maxFrames, &size);
	
	if (err)
	{
		ao_msg(MSGT_AO,MSGL_WARN, "AudioUnitGetProperty returned %d when getting kAudioDevicePropertyBufferSize\n", (int)err);
		return CONTROL_FALSE;
	}

	ao->chunk_size = maxFrames;//*inDesc.mBytesPerFrame;
    
	ao_data.samplerate = inDesc.mSampleRate;
	ao_data.channels = inDesc.mChannelsPerFrame;
    ao_data.bps = ao_data.samplerate * inDesc.mBytesPerFrame;
    ao_data.outburst = ao->chunk_size;
	ao_data.buffersize = ao_data.bps;

	ao->num_chunks = (ao_data.bps+ao->chunk_size-1)/ao->chunk_size;
    ao->buffer_len = (ao->num_chunks + 1) * ao->chunk_size;
    ao->buffer = aoIsCreated ? realloc(ao->buffer,(ao->num_chunks + 1)*ao->chunk_size)
							: calloc(ao->num_chunks + 1, ao->chunk_size);
	
	ao_msg(MSGT_AO,MSGL_V, "using %5d chunks of %d bytes (buffer len %d bytes)\n", (int)ao->num_chunks, (int)ao->chunk_size, (int)ao->buffer_len);

    renderCallback.inputProc = theRenderProc;
    renderCallback.inputProcRefCon = 0;
    err = AudioUnitSetProperty(ao->theOutputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &renderCallback, sizeof(AURenderCallbackStruct));
	if (err) {
		ao_msg(MSGT_AO, MSGL_WARN, "Unable to set the render callback (err=%d)\n", err);
		return CONTROL_FALSE;
	}

	reset();
    
    return CONTROL_OK;
}


static int play(void* output_samples,int num_bytes,int flags)
{  
int wrote=write_buffer(output_samples, num_bytes);

	audio_resume();
  return wrote;
}

/* set variables and buffer to initial state */
static void reset(void)
{
  audio_pause();
  /* reset ring-buffer state */
  ao->buf_read_pos=0;
  ao->buf_write_pos=0;
  
  return;
}


/* return available space */
static int get_space(void)
{
  return buf_free();
}


/* return delay until audio is played */
static float get_delay(void)
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

  if (!immed) {
    long long timeleft=(1000000LL*buf_used())/ao_data.bps;
    ao_msg(MSGT_AO,MSGL_DBG2, "%d bytes left @%d bps (%ld usec)\n", buf_used(), ao_data.bps, (int)timeleft);
    usec_sleep((int)timeleft);
  }

  AudioOutputUnitStop(ao->theOutputUnit);
  AudioUnitUninitialize(ao->theOutputUnit);
  CloseComponent(ao->theOutputUnit);

  free(ao->buffer);
  free(ao);
  ao = NULL;
}


/* stop playing, keep buffers (for pause) */
static void audio_pause(void)
{
  OSErr status=noErr;

  /* stop callback */
  status=AudioOutputUnitStop(ao->theOutputUnit);
  if (status)
    ao_msg(MSGT_AO,MSGL_WARN, "AudioOutputUnitStop returned %d\n",
	   (int)status);
  ao->paused=1;
}


/* resume playing, after audio_pause() */
static void audio_resume(void)
{
  if(ao->paused) {
  OSErr status=noErr;
	  /* start callback */
  status=AudioOutputUnitStart(ao->theOutputUnit);
  if (status)
    ao_msg(MSGT_AO,MSGL_WARN, "AudioOutputUnitStart returned %d\n",
	   (int)status);
	  ao->paused=0;
  }
}
