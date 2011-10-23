/*
 * CoreAudio audio output driver for Mac OS X
 *
 * original copyright (C) Timothy J. Wood - Aug 2000
 * ported to MPlayer libao2 by Dan Christiansen
 *
 * The S/PDIF part of the code is based on the auhal audio output
 * module from VideoLAN:
 * Copyright (c) 2006 Derk-Jan Hartman <hartman at videolan dot org>
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
 * along with MPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * The MacOS X CoreAudio framework doesn't mesh as simply as some
 * simpler frameworks do.  This is due to the fact that CoreAudio pulls
 * audio samples rather than having them pushed at it (which is nice
 * when you are wanting to do good buffering of audio).
 *
 * AC-3 and MPEG audio passthrough is possible, but has never been tested
 * due to lack of a soundcard that supports it.
 */

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"

#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"
#include "osdep/timer.h"
#include "libavutil/fifo.h"
#include "subopt-helper.h"

static const ao_info_t info =
  {
    "Darwin/Mac OS X native audio output",
    "coreaudio",
    "Timothy J. Wood & Dan Christiansen & Chris Roccati",
    ""
  };

LIBAO_EXTERN(coreaudio)

/* Prefix for all mp_msg() calls */
#define ao_msg(a, b, c...) mp_msg(a, b, "AO: [coreaudio] " c)

#if MAC_OS_X_VERSION_MAX_ALLOWED <= 1040
/* AudioDeviceIOProcID does not exist in Mac OS X 10.4. We can emulate
 * this by using AudioDeviceAddIOProc() and AudioDeviceRemoveIOProc(). */
#define AudioDeviceIOProcID AudioDeviceIOProc
#define AudioDeviceDestroyIOProcID AudioDeviceRemoveIOProc
static OSStatus AudioDeviceCreateIOProcID(AudioDeviceID dev,
                                          AudioDeviceIOProc proc,
                                          void *data,
                                          AudioDeviceIOProcID *procid)
{
  *procid = proc;
  return AudioDeviceAddIOProc(dev, proc, data);
}
#endif

typedef struct ao_coreaudio_s
{
  AudioDeviceID i_selected_dev;             /* Keeps DeviceID of the selected device. */
  int b_supports_digital;                   /* Does the currently selected device support digital mode? */
  int b_digital;                            /* Are we running in digital mode? */
  int b_muted;                              /* Are we muted in digital mode? */

  AudioDeviceIOProcID renderCallback;       /* Render callback used for SPDIF */

  /* AudioUnit */
  AudioUnit theOutputUnit;

  /* CoreAudio SPDIF mode specific */
  pid_t i_hog_pid;                          /* Keeps the pid of our hog status. */
  AudioStreamID i_stream_id;                /* The StreamID that has a cac3 streamformat */
  int i_stream_index;                       /* The index of i_stream_id in an AudioBufferList */
  AudioStreamBasicDescription stream_format;/* The format we changed the stream to */
  AudioStreamBasicDescription sfmt_revert;  /* The original format of the stream */
  int b_revert;                             /* Whether we need to revert the stream format */
  int b_changed_mixing;                     /* Whether we need to set the mixing mode back */
  int b_stream_format_changed;              /* Flag for main thread to reset stream's format to digital and reset buffer */

  /* Original common part */
  int packetSize;
  int paused;

  /* Ring-buffer */
  AVFifoBuffer *buffer;
  unsigned int buffer_len; ///< must always be num_chunks * chunk_size
  unsigned int num_chunks;
  unsigned int chunk_size;
} ao_coreaudio_t;

static ao_coreaudio_t *ao = NULL;

/**
 * \brief add data to ringbuffer
 */
static int write_buffer(unsigned char* data, int len){
  int free = ao->buffer_len - av_fifo_size(ao->buffer);
  if (len > free) len = free;
  return av_fifo_generic_write(ao->buffer, data, len, NULL);
}

/**
 * \brief remove data from ringbuffer
 */
static int read_buffer(unsigned char* data,int len){
  int buffered = av_fifo_size(ao->buffer);
  if (len > buffered) len = buffered;
  if (data)
    av_fifo_generic_read(ao->buffer, data, len, NULL);
  else
    av_fifo_drain(ao->buffer, len);
  return len;
}

static OSStatus theRenderProc(void *inRefCon,
                              AudioUnitRenderActionFlags *inActionFlags,
                              const AudioTimeStamp *inTimeStamp,
                              UInt32 inBusNumber, UInt32 inNumFrames,
                              AudioBufferList *ioData)
{
int amt=av_fifo_size(ao->buffer);
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
Float32 vol;
	switch (cmd) {
	case AOCONTROL_GET_VOLUME:
		control_vol = (ao_control_vol_t*)arg;
		if (ao->b_digital) {
			// Digital output has no volume adjust.
			return CONTROL_FALSE;
		}
		err = AudioUnitGetParameter(ao->theOutputUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, &vol);

		if(err==0) {
			// printf("GET VOL=%f\n", vol);
			control_vol->left=control_vol->right=vol*100.0/4.0;
			return CONTROL_TRUE;
		}
		else {
			ao_msg(MSGT_AO, MSGL_WARN, "could not get HAL output volume: [%4.4s]\n", (char *)&err);
			return CONTROL_FALSE;
		}

	case AOCONTROL_SET_VOLUME:
		control_vol = (ao_control_vol_t*)arg;

		if (ao->b_digital) {
			// Digital output can not set volume. Here we have to return true
			// to make mixer forget it. Else mixer will add a soft filter,
			// that's not we expected and the filter not support ac3 stream
			// will cause mplayer die.

			// Although not support set volume, but at least we support mute.
			// MPlayer set mute by set volume to zero, we handle it.
			if (control_vol->left == 0 && control_vol->right == 0)
				ao->b_muted = 1;
			else
				ao->b_muted = 0;
			return CONTROL_TRUE;
		}

		vol=(control_vol->left+control_vol->right)*4.0/200.0;
		err = AudioUnitSetParameter(ao->theOutputUnit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, vol, 0);
		if(err==0) {
			// printf("SET VOL=%f\n", vol);
			return CONTROL_TRUE;
		}
		else {
			ao_msg(MSGT_AO, MSGL_WARN, "could not set HAL output volume: [%4.4s]\n", (char *)&err);
			return CONTROL_FALSE;
		}
	  /* Everything is currently unimplemented */
	default:
	  return CONTROL_FALSE;
	}

}


static void print_format(int lev, const char* str, const AudioStreamBasicDescription *f){
    uint32_t flags=(uint32_t) f->mFormatFlags;
    ao_msg(MSGT_AO,lev, "%s %7.1fHz %"PRIu32"bit [%c%c%c%c][%"PRIu32"][%"PRIu32"][%"PRIu32"][%"PRIu32"][%"PRIu32"] %s %s %s%s%s%s\n",
	    str, f->mSampleRate, f->mBitsPerChannel,
	    (int)(f->mFormatID & 0xff000000) >> 24,
	    (int)(f->mFormatID & 0x00ff0000) >> 16,
	    (int)(f->mFormatID & 0x0000ff00) >>  8,
	    (int)(f->mFormatID & 0x000000ff) >>  0,
	    f->mFormatFlags, f->mBytesPerPacket,
	    f->mFramesPerPacket, f->mBytesPerFrame,
	    f->mChannelsPerFrame,
	    (flags&kAudioFormatFlagIsFloat) ? "float" : "int",
	    (flags&kAudioFormatFlagIsBigEndian) ? "BE" : "LE",
	    (flags&kAudioFormatFlagIsSignedInteger) ? "S" : "U",
	    (flags&kAudioFormatFlagIsPacked) ? " packed" : "",
	    (flags&kAudioFormatFlagIsAlignedHigh) ? " aligned" : "",
	    (flags&kAudioFormatFlagIsNonInterleaved) ? " ni" : "" );
}

static OSStatus GetAudioProperty(AudioObjectID id,
                                 AudioObjectPropertySelector selector,
                                 UInt32 outSize, void *outData)
{
    AudioObjectPropertyAddress property_address;

    property_address.mSelector = selector;
    property_address.mScope    = kAudioObjectPropertyScopeGlobal;
    property_address.mElement  = kAudioObjectPropertyElementMaster;

    return AudioObjectGetPropertyData(id, &property_address, 0, NULL, &outSize, outData);
}

static UInt32 GetAudioPropertyArray(AudioObjectID id,
                                    AudioObjectPropertySelector selector,
                                    AudioObjectPropertyScope scope,
                                    void **outData)
{
    OSStatus err;
    AudioObjectPropertyAddress property_address;
    UInt32 i_param_size;

    property_address.mSelector = selector;
    property_address.mScope    = scope;
    property_address.mElement  = kAudioObjectPropertyElementMaster;

    err = AudioObjectGetPropertyDataSize(id, &property_address, 0, NULL, &i_param_size);

    if (err != noErr)
        return 0;

    *outData = malloc(i_param_size);


    err = AudioObjectGetPropertyData(id, &property_address, 0, NULL, &i_param_size, *outData);

    if (err != noErr) {
        free(*outData);
        return 0;
    }

    return i_param_size;
}

static UInt32 GetGlobalAudioPropertyArray(AudioObjectID id,
                                          AudioObjectPropertySelector selector,
                                          void **outData)
{
    return GetAudioPropertyArray(id, selector, kAudioObjectPropertyScopeGlobal, outData);
}

static OSStatus GetAudioPropertyString(AudioObjectID id,
                                       AudioObjectPropertySelector selector,
                                       char **outData)
{
    OSStatus err;
    AudioObjectPropertyAddress property_address;
    UInt32 i_param_size;
    CFStringRef string;
    CFIndex string_length;

    property_address.mSelector = selector;
    property_address.mScope    = kAudioObjectPropertyScopeGlobal;
    property_address.mElement  = kAudioObjectPropertyElementMaster;

    i_param_size = sizeof(CFStringRef);
    err = AudioObjectGetPropertyData(id, &property_address, 0, NULL, &i_param_size, &string);
    if (err != noErr)
        return err;

    string_length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(string),
                                                      kCFStringEncodingASCII);
    *outData = malloc(string_length + 1);
    CFStringGetCString(string, *outData, string_length + 1, kCFStringEncodingASCII);

    CFRelease(string);

    return err;
}

static OSStatus SetAudioProperty(AudioObjectID id,
                                 AudioObjectPropertySelector selector,
                                 UInt32 inDataSize, void *inData)
{
    AudioObjectPropertyAddress property_address;

    property_address.mSelector = selector;
    property_address.mScope    = kAudioObjectPropertyScopeGlobal;
    property_address.mElement  = kAudioObjectPropertyElementMaster;

    return AudioObjectSetPropertyData(id, &property_address, 0, NULL, inDataSize, inData);
}

static Boolean IsAudioPropertySettable(AudioObjectID id,
                                       AudioObjectPropertySelector selector,
                                       Boolean *outData)
{
    AudioObjectPropertyAddress property_address;

    property_address.mSelector = selector;
    property_address.mScope    = kAudioObjectPropertyScopeGlobal;
    property_address.mElement  = kAudioObjectPropertyElementMaster;

    return AudioObjectIsPropertySettable(id, &property_address, outData);
}

static int AudioDeviceSupportsDigital( AudioDeviceID i_dev_id );
static int AudioStreamSupportsDigital( AudioStreamID i_stream_id );
static int OpenSPDIF(void);
static int AudioStreamChangeFormat( AudioStreamID i_stream_id, AudioStreamBasicDescription change_format );
static OSStatus RenderCallbackSPDIF( AudioDeviceID inDevice,
                                    const AudioTimeStamp * inNow,
                                    const void * inInputData,
                                    const AudioTimeStamp * inInputTime,
                                    AudioBufferList * outOutputData,
                                    const AudioTimeStamp * inOutputTime,
                                    void * threadGlobals );
static OSStatus StreamListener( AudioObjectID inObjectID,
                                UInt32 inNumberAddresses,
                                const AudioObjectPropertyAddress inAddresses[],
                                void *inClientData );
static OSStatus DeviceListener( AudioObjectID inObjectID,
                                UInt32 inNumberAddresses,
                                const AudioObjectPropertyAddress inAddresses[],
                                void *inClientData );

static void print_help(void)
{
    OSStatus err;
    UInt32 i_param_size;
    int num_devices;
    AudioDeviceID *devids;
    char *device_name;

    mp_msg(MSGT_AO, MSGL_FATAL,
           "\n-ao coreaudio commandline help:\n"
           "Example: mplayer -ao coreaudio:device_id=266\n"
           "    open Core Audio with output device ID 266.\n"
           "\nOptions:\n"
           "    device_id\n"
           "        ID of output device to use (0 = default device)\n"
           "    help\n"
           "        This help including list of available devices.\n"
           "\n"
           "Available output devices:\n");

    i_param_size = GetGlobalAudioPropertyArray(kAudioObjectSystemObject, kAudioHardwarePropertyDevices, (void **)&devids);

    if (!i_param_size) {
        mp_msg(MSGT_AO, MSGL_FATAL, "Failed to get list of output devices.\n");
        return;
    }

    num_devices = i_param_size / sizeof(AudioDeviceID);

    for (int i = 0; i < num_devices; ++i) {
        err = GetAudioPropertyString(devids[i], kAudioObjectPropertyName, &device_name);

        if (err == noErr) {
            mp_msg(MSGT_AO, MSGL_FATAL, "%s (id: %"PRIu32")\n", device_name, devids[i]);
            free(device_name);
        } else
            mp_msg(MSGT_AO, MSGL_FATAL, "Unknown (id: %"PRIu32")\n", devids[i]);
    }

    mp_msg(MSGT_AO, MSGL_FATAL, "\n");

    free(devids);
}

static int init(int rate,int channels,int format,int flags)
{
AudioStreamBasicDescription inDesc;
ComponentDescription desc;
Component comp;
AURenderCallbackStruct renderCallback;
OSStatus err;
UInt32 size, maxFrames, b_alive;
char *psz_name;
AudioDeviceID devid_def = 0;
int device_id, display_help = 0;

    const opt_t subopts[] = {
        {"device_id", OPT_ARG_INT,  &device_id,    NULL},
        {"help",      OPT_ARG_BOOL, &display_help, NULL},
        {NULL}
    };

    // set defaults
    device_id = 0;

    if (subopt_parse(ao_subdevice, subopts) != 0 || display_help) {
        print_help();
        if (!display_help)
            return 0;
    }

    ao_msg(MSGT_AO,MSGL_V, "init([%dHz][%dch][%s][%d])\n", rate, channels, af_fmt2str_short(format), flags);

    ao = calloc(1, sizeof(ao_coreaudio_t));

    ao->i_selected_dev = 0;
    ao->b_supports_digital = 0;
    ao->b_digital = 0;
    ao->b_muted = 0;
    ao->b_stream_format_changed = 0;
    ao->i_hog_pid = -1;
    ao->i_stream_id = 0;
    ao->i_stream_index = -1;
    ao->b_revert = 0;
    ao->b_changed_mixing = 0;

    if (device_id == 0) {
        /* Find the ID of the default Device. */
        err = GetAudioProperty(kAudioObjectSystemObject,
                               kAudioHardwarePropertyDefaultOutputDevice,
                               sizeof(UInt32), &devid_def);
        if (err != noErr)
        {
            ao_msg(MSGT_AO, MSGL_WARN, "could not get default audio device: [%4.4s]\n", (char *)&err);
            goto err_out;
        }
    } else {
        devid_def = device_id;
    }

    /* Retrieve the name of the device. */
    err = GetAudioPropertyString(devid_def,
                                 kAudioObjectPropertyName,
                                 &psz_name);
    if (err != noErr)
    {
        ao_msg(MSGT_AO, MSGL_WARN, "could not get default audio device name: [%4.4s]\n", (char *)&err);
        goto err_out;
    }

    ao_msg(MSGT_AO,MSGL_V, "got audio output device ID: %"PRIu32" Name: %s\n", devid_def, psz_name );

    /* Probe whether device support S/PDIF stream output if input is AC3 stream. */
    if (AF_FORMAT_IS_AC3(format)) {
        if (AudioDeviceSupportsDigital(devid_def))
        {
            ao->b_supports_digital = 1;
        }
        ao_msg(MSGT_AO, MSGL_V,
               "probe default audio output device about support for digital s/pdif output: %d\n",
               ao->b_supports_digital );
    }

    free(psz_name);

    // Save selected device id
    ao->i_selected_dev = devid_def;

	// Build Description for the input format
	inDesc.mSampleRate=rate;
	inDesc.mFormatID=ao->b_supports_digital ? kAudioFormat60958AC3 : kAudioFormatLinearPCM;
	inDesc.mChannelsPerFrame=channels;
	inDesc.mBitsPerChannel=af_fmt2bits(format);

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
    if ((format & AF_FORMAT_END_MASK) == AF_FORMAT_BE)
        inDesc.mFormatFlags |= kAudioFormatFlagIsBigEndian;

    inDesc.mFramesPerPacket = 1;
    ao->packetSize = inDesc.mBytesPerPacket = inDesc.mBytesPerFrame = inDesc.mFramesPerPacket*channels*(inDesc.mBitsPerChannel/8);
    print_format(MSGL_V, "source:",&inDesc);

    if (ao->b_supports_digital)
    {
        b_alive = 1;
        err = GetAudioProperty(ao->i_selected_dev,
                               kAudioDevicePropertyDeviceIsAlive,
                               sizeof(UInt32), &b_alive);
        if (err != noErr)
            ao_msg(MSGT_AO, MSGL_WARN, "could not check whether device is alive: [%4.4s]\n", (char *)&err);
        if (!b_alive)
            ao_msg(MSGT_AO, MSGL_WARN, "device is not alive\n" );

        /* S/PDIF output need device in HogMode. */
        err = GetAudioProperty(ao->i_selected_dev,
                               kAudioDevicePropertyHogMode,
                               sizeof(pid_t), &ao->i_hog_pid);
        if (err != noErr)
        {
            /* This is not a fatal error. Some drivers simply don't support this property. */
            ao_msg(MSGT_AO, MSGL_WARN, "could not check whether device is hogged: [%4.4s]\n",
                     (char *)&err);
            ao->i_hog_pid = -1;
        }

        if (ao->i_hog_pid != -1 && ao->i_hog_pid != getpid())
        {
            ao_msg(MSGT_AO, MSGL_WARN, "Selected audio device is exclusively in use by another program.\n" );
            goto err_out;
        }
        ao->stream_format = inDesc;
        return OpenSPDIF();
    }

	/* original analog output code */
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = (device_id == 0) ? kAudioUnitSubType_DefaultOutput : kAudioUnitSubType_HALOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	comp = FindNextComponent(NULL, &desc);  //Finds an component that meets the desc spec's
	if (comp == NULL) {
		ao_msg(MSGT_AO, MSGL_WARN, "Unable to find Output Unit component\n");
		goto err_out;
	}

	err = OpenAComponent(comp, &(ao->theOutputUnit));  //gains access to the services provided by the component
	if (err) {
		ao_msg(MSGT_AO, MSGL_WARN, "Unable to open Output Unit component: [%4.4s]\n", (char *)&err);
		goto err_out;
	}

	// Initialize AudioUnit
	err = AudioUnitInitialize(ao->theOutputUnit);
	if (err) {
		ao_msg(MSGT_AO, MSGL_WARN, "Unable to initialize Output Unit component: [%4.4s]\n", (char *)&err);
		goto err_out1;
	}

	size =  sizeof(AudioStreamBasicDescription);
	err = AudioUnitSetProperty(ao->theOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &inDesc, size);

	if (err) {
		ao_msg(MSGT_AO, MSGL_WARN, "Unable to set the input format: [%4.4s]\n", (char *)&err);
		goto err_out2;
	}

	size = sizeof(UInt32);
	err = AudioUnitGetProperty(ao->theOutputUnit, kAudioDevicePropertyBufferSize, kAudioUnitScope_Input, 0, &maxFrames, &size);

	if (err)
	{
		ao_msg(MSGT_AO,MSGL_WARN, "AudioUnitGetProperty returned [%4.4s] when getting kAudioDevicePropertyBufferSize\n", (char *)&err);
		goto err_out2;
	}

	//Set the Current Device to the Default Output Unit.
    err = AudioUnitSetProperty(ao->theOutputUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &ao->i_selected_dev, sizeof(ao->i_selected_dev));

	ao->chunk_size = maxFrames;//*inDesc.mBytesPerFrame;

	ao_data.samplerate = inDesc.mSampleRate;
	ao_data.channels = inDesc.mChannelsPerFrame;
    ao_data.bps = ao_data.samplerate * inDesc.mBytesPerFrame;
    ao_data.outburst = ao->chunk_size;
	ao_data.buffersize = ao_data.bps;

	ao->num_chunks = (ao_data.bps+ao->chunk_size-1)/ao->chunk_size;
    ao->buffer_len = ao->num_chunks * ao->chunk_size;
    ao->buffer = av_fifo_alloc(ao->buffer_len);

	ao_msg(MSGT_AO,MSGL_V, "using %5d chunks of %d bytes (buffer len %d bytes)\n", (int)ao->num_chunks, (int)ao->chunk_size, (int)ao->buffer_len);

    renderCallback.inputProc = theRenderProc;
    renderCallback.inputProcRefCon = 0;
    err = AudioUnitSetProperty(ao->theOutputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &renderCallback, sizeof(AURenderCallbackStruct));
	if (err) {
		ao_msg(MSGT_AO, MSGL_WARN, "Unable to set the render callback: [%4.4s]\n", (char *)&err);
		goto err_out2;
	}

	reset();

    return CONTROL_OK;

err_out2:
    AudioUnitUninitialize(ao->theOutputUnit);
err_out1:
    CloseComponent(ao->theOutputUnit);
err_out:
    av_fifo_free(ao->buffer);
    free(ao);
    ao = NULL;
    return CONTROL_FALSE;
}

/*****************************************************************************
 * Setup a encoded digital stream (SPDIF)
 *****************************************************************************/
static int OpenSPDIF(void)
{
    OSStatus                    err = noErr;
    UInt32                      i_param_size, b_mix = 0;
    Boolean                     b_writeable = 0;
    AudioStreamID               *p_streams = NULL;
    int                         i, i_streams = 0;
    AudioObjectPropertyAddress  property_address;

    /* Start doing the SPDIF setup process. */
    ao->b_digital = 1;

    /* Hog the device. */
    ao->i_hog_pid = getpid() ;

    err = SetAudioProperty(ao->i_selected_dev,
                           kAudioDevicePropertyHogMode,
                           sizeof(ao->i_hog_pid), &ao->i_hog_pid);
    if (err != noErr)
    {
        ao_msg(MSGT_AO, MSGL_WARN, "failed to set hogmode: [%4.4s]\n", (char *)&err);
        ao->i_hog_pid = -1;
        goto err_out;
    }

    property_address.mSelector = kAudioDevicePropertySupportsMixing;
    property_address.mScope    = kAudioObjectPropertyScopeGlobal;
    property_address.mElement  = kAudioObjectPropertyElementMaster;

    /* Set mixable to false if we are allowed to. */
    if (AudioObjectHasProperty(ao->i_selected_dev, &property_address)) {
        /* Set mixable to false if we are allowed to. */
        err = IsAudioPropertySettable(ao->i_selected_dev,
                                      kAudioDevicePropertySupportsMixing,
                                      &b_writeable);
        err = GetAudioProperty(ao->i_selected_dev,
                               kAudioDevicePropertySupportsMixing,
                               sizeof(UInt32), &b_mix);
        if (err == noErr && b_writeable)
        {
            b_mix = 0;
            err = SetAudioProperty(ao->i_selected_dev,
                                   kAudioDevicePropertySupportsMixing,
                                   sizeof(UInt32), &b_mix);
            ao->b_changed_mixing = 1;
        }
        if (err != noErr)
        {
            ao_msg(MSGT_AO, MSGL_WARN, "failed to set mixmode: [%4.4s]\n", (char *)&err);
            goto err_out;
        }
    }

    /* Get a list of all the streams on this device. */
    i_param_size = GetAudioPropertyArray(ao->i_selected_dev,
                                         kAudioDevicePropertyStreams,
                                         kAudioDevicePropertyScopeOutput,
                                         (void **)&p_streams);

    if (!i_param_size) {
        ao_msg(MSGT_AO, MSGL_WARN, "could not get number of streams.\n");
        goto err_out;
    }

    i_streams = i_param_size / sizeof(AudioStreamID);

    ao_msg(MSGT_AO, MSGL_V, "current device stream number: %d\n", i_streams);

    for (i = 0; i < i_streams && ao->i_stream_index < 0; ++i)
    {
        /* Find a stream with a cac3 stream. */
        AudioStreamRangedDescription *p_format_list = NULL;
        int i_formats = 0, j = 0, b_digital = 0;

        i_param_size = GetGlobalAudioPropertyArray(p_streams[i],
                                                   kAudioStreamPropertyAvailablePhysicalFormats,
                                                   (void **)&p_format_list);

        if (!i_param_size) {
            ao_msg(MSGT_AO, MSGL_WARN,
                   "Could not get number of stream formats.\n");
            continue;
        }

        i_formats = i_param_size / sizeof(AudioStreamRangedDescription);

        /* Check if one of the supported formats is a digital format. */
        for (j = 0; j < i_formats; ++j)
        {
            if (p_format_list[j].mFormat.mFormatID == 'IAC3'               ||
                p_format_list[j].mFormat.mFormatID == 'iac3'               ||
                p_format_list[j].mFormat.mFormatID == kAudioFormat60958AC3 ||
                p_format_list[j].mFormat.mFormatID == kAudioFormatAC3)
            {
                b_digital = 1;
                break;
            }
        }

        if (b_digital)
        {
            /* If this stream supports a digital (cac3) format, then set it. */
            int i_requested_rate_format = -1;
            int i_current_rate_format = -1;
            int i_backup_rate_format = -1;

            ao->i_stream_id = p_streams[i];
            ao->i_stream_index = i;

            if (ao->b_revert == 0)
            {
                /* Retrieve the original format of this stream first if not done so already. */
                err = GetAudioProperty(ao->i_stream_id,
                                       kAudioStreamPropertyPhysicalFormat,
                                       sizeof(ao->sfmt_revert), &ao->sfmt_revert);
                if (err != noErr)
                {
                    ao_msg(MSGT_AO, MSGL_WARN,
                           "Could not retrieve the original stream format: [%4.4s]\n",
                           (char *)&err);
                    free(p_format_list);
                    continue;
                }
                ao->b_revert = 1;
            }

            for (j = 0; j < i_formats; ++j)
                if (p_format_list[j].mFormat.mFormatID == 'IAC3'               ||
                    p_format_list[j].mFormat.mFormatID == 'iac3'               ||
                    p_format_list[j].mFormat.mFormatID == kAudioFormat60958AC3 ||
                    p_format_list[j].mFormat.mFormatID == kAudioFormatAC3)
                {
                   if (p_format_list[j].mFormat.mSampleRate == ao->stream_format.mSampleRate)
                    {
                        i_requested_rate_format = j;
                        break;
                    }
                    if (p_format_list[j].mFormat.mSampleRate == ao->sfmt_revert.mSampleRate)
                        i_current_rate_format = j;
                    else if (i_backup_rate_format < 0 || p_format_list[j].mFormat.mSampleRate > p_format_list[i_backup_rate_format].mFormat.mSampleRate)
                        i_backup_rate_format = j;
                }

            if (i_requested_rate_format >= 0) /* We prefer to output at the samplerate of the original audio. */
                ao->stream_format = p_format_list[i_requested_rate_format].mFormat;
            else if (i_current_rate_format >= 0) /* If not possible, we will try to use the current samplerate of the device. */
                ao->stream_format = p_format_list[i_current_rate_format].mFormat;
            else ao->stream_format = p_format_list[i_backup_rate_format].mFormat; /* And if we have to, any digital format will be just fine (highest rate possible). */
        }
        free(p_format_list);
    }
    free(p_streams);

    if (ao->i_stream_index < 0)
    {
        ao_msg(MSGT_AO, MSGL_WARN,
               "Cannot find any digital output stream format when OpenSPDIF().\n");
        goto err_out;
    }

    print_format(MSGL_V, "original stream format:", &ao->sfmt_revert);

    if (!AudioStreamChangeFormat(ao->i_stream_id, ao->stream_format))
        goto err_out;

    property_address.mSelector = kAudioDevicePropertyDeviceHasChanged;
    property_address.mScope    = kAudioObjectPropertyScopeGlobal;
    property_address.mElement  = kAudioObjectPropertyElementMaster;

    err = AudioObjectAddPropertyListener(ao->i_selected_dev,
                                         &property_address,
                                         DeviceListener,
                                         NULL);
    if (err != noErr)
        ao_msg(MSGT_AO, MSGL_WARN, "AudioDeviceAddPropertyListener for kAudioDevicePropertyDeviceHasChanged failed: [%4.4s]\n", (char *)&err);


    /* FIXME: If output stream is not native byte-order, we need change endian somewhere. */
    /*        Although there's no such case reported.                                     */
#if HAVE_BIGENDIAN
    if (!(ao->stream_format.mFormatFlags & kAudioFormatFlagIsBigEndian))
#else
    /* tell mplayer that we need a byteswap on AC3 streams, */
    if (ao->stream_format.mFormatID & kAudioFormat60958AC3)
        ao_data.format = AF_FORMAT_AC3_LE;

    if (ao->stream_format.mFormatFlags & kAudioFormatFlagIsBigEndian)
#endif
        ao_msg(MSGT_AO, MSGL_WARN,
               "Output stream has non-native byte order, digital output may fail.\n");

    /* For ac3/dts, just use packet size 6144 bytes as chunk size. */
    ao->chunk_size = ao->stream_format.mBytesPerPacket;

    ao_data.samplerate = ao->stream_format.mSampleRate;
    ao_data.channels = ao->stream_format.mChannelsPerFrame;
    ao_data.bps = ao_data.samplerate * (ao->stream_format.mBytesPerPacket/ao->stream_format.mFramesPerPacket);
    ao_data.outburst = ao->chunk_size;
    ao_data.buffersize = ao_data.bps;

    ao->num_chunks = (ao_data.bps+ao->chunk_size-1)/ao->chunk_size;
    ao->buffer_len = ao->num_chunks * ao->chunk_size;
    ao->buffer = av_fifo_alloc(ao->buffer_len);

    ao_msg(MSGT_AO,MSGL_V, "using %5d chunks of %d bytes (buffer len %d bytes)\n", (int)ao->num_chunks, (int)ao->chunk_size, (int)ao->buffer_len);


    /* Create IOProc callback. */
    err = AudioDeviceCreateIOProcID(ao->i_selected_dev,
                                    (AudioDeviceIOProc)RenderCallbackSPDIF,
                                    (void *)ao,
                                    &ao->renderCallback);

    if (err != noErr || ao->renderCallback == NULL)
    {
        ao_msg(MSGT_AO, MSGL_WARN, "AudioDeviceAddIOProc failed: [%4.4s]\n", (char *)&err);
        goto err_out1;
    }

    reset();

    return CONTROL_TRUE;

err_out1:
    if (ao->b_revert)
        AudioStreamChangeFormat(ao->i_stream_id, ao->sfmt_revert);
err_out:
    if (ao->b_changed_mixing && ao->sfmt_revert.mFormatID != kAudioFormat60958AC3)
    {
        int b_mix = 1;
        err = SetAudioProperty(ao->i_selected_dev,
                               kAudioDevicePropertySupportsMixing,
                               sizeof(int), &b_mix);
        if (err != noErr)
            ao_msg(MSGT_AO, MSGL_WARN, "failed to set mixmode: [%4.4s]\n",
                   (char *)&err);
    }
    if (ao->i_hog_pid == getpid())
    {
        ao->i_hog_pid = -1;
        err = SetAudioProperty(ao->i_selected_dev,
                               kAudioDevicePropertyHogMode,
                               sizeof(ao->i_hog_pid), &ao->i_hog_pid);
        if (err != noErr)
            ao_msg(MSGT_AO, MSGL_WARN, "Could not release hogmode: [%4.4s]\n",
                   (char *)&err);
    }
    av_fifo_free(ao->buffer);
    free(ao);
    ao = NULL;
    return CONTROL_FALSE;
}

/*****************************************************************************
 * AudioDeviceSupportsDigital: Check i_dev_id for digital stream support.
 *****************************************************************************/
static int AudioDeviceSupportsDigital( AudioDeviceID i_dev_id )
{
    UInt32                      i_param_size = 0;
    AudioStreamID               *p_streams = NULL;
    int                         i = 0, i_streams = 0;
    int                         b_return = CONTROL_FALSE;

    /* Retrieve all the output streams. */
    i_param_size = GetAudioPropertyArray(i_dev_id,
                                         kAudioDevicePropertyStreams,
                                         kAudioDevicePropertyScopeOutput,
                                         (void **)&p_streams);

    if (!i_param_size) {
        ao_msg(MSGT_AO, MSGL_WARN, "could not get number of streams.\n");
        return CONTROL_FALSE;
    }

    i_streams = i_param_size / sizeof(AudioStreamID);

    for (i = 0; i < i_streams; ++i)
    {
        if (AudioStreamSupportsDigital(p_streams[i]))
            b_return = CONTROL_OK;
    }

    free(p_streams);
    return b_return;
}

/*****************************************************************************
 * AudioStreamSupportsDigital: Check i_stream_id for digital stream support.
 *****************************************************************************/
static int AudioStreamSupportsDigital( AudioStreamID i_stream_id )
{
    UInt32 i_param_size;
    AudioStreamRangedDescription *p_format_list = NULL;
    int i, i_formats, b_return = CONTROL_FALSE;

    /* Retrieve all the stream formats supported by each output stream. */
    i_param_size = GetGlobalAudioPropertyArray(i_stream_id,
                                               kAudioStreamPropertyAvailablePhysicalFormats,
                                               (void **)&p_format_list);

    if (!i_param_size) {
        ao_msg(MSGT_AO, MSGL_WARN, "Could not get number of stream formats.\n");
        return CONTROL_FALSE;
    }

    i_formats = i_param_size / sizeof(AudioStreamRangedDescription);

    for (i = 0; i < i_formats; ++i)
    {
        print_format(MSGL_V, "supported format:", &(p_format_list[i].mFormat));

        if (p_format_list[i].mFormat.mFormatID == 'IAC3'               ||
            p_format_list[i].mFormat.mFormatID == 'iac3'               ||
            p_format_list[i].mFormat.mFormatID == kAudioFormat60958AC3 ||
            p_format_list[i].mFormat.mFormatID == kAudioFormatAC3)
            b_return = CONTROL_OK;
    }

    free(p_format_list);
    return b_return;
}

/*****************************************************************************
 * AudioStreamChangeFormat: Change i_stream_id to change_format
 *****************************************************************************/
static int AudioStreamChangeFormat( AudioStreamID i_stream_id, AudioStreamBasicDescription change_format )
{
    OSStatus err = noErr;
    int i;
    AudioObjectPropertyAddress property_address;

    static volatile int stream_format_changed;
    stream_format_changed = 0;

    print_format(MSGL_V, "setting stream format:", &change_format);

    /* Install the callback. */
    property_address.mSelector = kAudioStreamPropertyPhysicalFormat;
    property_address.mScope    = kAudioObjectPropertyScopeGlobal;
    property_address.mElement  = kAudioObjectPropertyElementMaster;

    err = AudioObjectAddPropertyListener(i_stream_id,
                                         &property_address,
                                         StreamListener,
                                         (void *)&stream_format_changed);
    if (err != noErr)
    {
        ao_msg(MSGT_AO, MSGL_WARN, "AudioStreamAddPropertyListener failed: [%4.4s]\n", (char *)&err);
        return CONTROL_FALSE;
    }

    /* Change the format. */
    err = SetAudioProperty(i_stream_id,
                           kAudioStreamPropertyPhysicalFormat,
                           sizeof(AudioStreamBasicDescription), &change_format);
    if (err != noErr)
    {
        ao_msg(MSGT_AO, MSGL_WARN, "could not set the stream format: [%4.4s]\n", (char *)&err);
        return CONTROL_FALSE;
    }

    /* The AudioStreamSetProperty is not only asynchronious,
     * it is also not Atomic, in its behaviour.
     * Therefore we check 5 times before we really give up.
     * FIXME: failing isn't actually implemented yet. */
    for (i = 0; i < 5; ++i)
    {
        AudioStreamBasicDescription actual_format;
        int j;
        for (j = 0; !stream_format_changed && j < 50; ++j)
            usec_sleep(10000);
        if (stream_format_changed)
            stream_format_changed = 0;
        else
            ao_msg(MSGT_AO, MSGL_V, "reached timeout\n" );

        err = GetAudioProperty(i_stream_id,
                               kAudioStreamPropertyPhysicalFormat,
                               sizeof(AudioStreamBasicDescription), &actual_format);

        print_format(MSGL_V, "actual format in use:", &actual_format);
        if (actual_format.mSampleRate == change_format.mSampleRate &&
            actual_format.mFormatID == change_format.mFormatID &&
            actual_format.mFramesPerPacket == change_format.mFramesPerPacket)
        {
            /* The right format is now active. */
            break;
        }
        /* We need to check again. */
    }

    /* Removing the property listener. */
    err = AudioObjectRemovePropertyListener(i_stream_id,
                                            &property_address,
                                            StreamListener,
                                            (void *)&stream_format_changed);
    if (err != noErr)
    {
        ao_msg(MSGT_AO, MSGL_WARN, "AudioStreamRemovePropertyListener failed: [%4.4s]\n", (char *)&err);
        return CONTROL_FALSE;
    }

    return CONTROL_TRUE;
}

/*****************************************************************************
 * RenderCallbackSPDIF: callback for SPDIF audio output
 *****************************************************************************/
static OSStatus RenderCallbackSPDIF( AudioDeviceID inDevice,
                                    const AudioTimeStamp * inNow,
                                    const void * inInputData,
                                    const AudioTimeStamp * inInputTime,
                                    AudioBufferList * outOutputData,
                                    const AudioTimeStamp * inOutputTime,
                                    void * threadGlobals )
{
    int amt = av_fifo_size(ao->buffer);
    int req = outOutputData->mBuffers[ao->i_stream_index].mDataByteSize;

    if (amt > req)
        amt = req;
    if (amt)
        read_buffer(ao->b_muted ? NULL : (unsigned char *)outOutputData->mBuffers[ao->i_stream_index].mData, amt);

    return noErr;
}


static int play(void* output_samples,int num_bytes,int flags)
{
    int wrote, b_digital;
    SInt32 exit_reason;

    // Check whether we need to reset the digital output stream.
    if (ao->b_digital && ao->b_stream_format_changed)
    {
        ao->b_stream_format_changed = 0;
        b_digital = AudioStreamSupportsDigital(ao->i_stream_id);
        if (b_digital)
        {
            /* Current stream supports digital format output, let's set it. */
            ao_msg(MSGT_AO, MSGL_V,
                   "Detected current stream supports digital, try to restore digital output...\n");

            if (!AudioStreamChangeFormat(ao->i_stream_id, ao->stream_format))
            {
                ao_msg(MSGT_AO, MSGL_WARN, "Restoring digital output failed.\n");
            }
            else
            {
                ao_msg(MSGT_AO, MSGL_WARN, "Restoring digital output succeeded.\n");
                reset();
            }
        }
        else
            ao_msg(MSGT_AO, MSGL_V, "Detected current stream does not support digital.\n");
    }

    wrote=write_buffer(output_samples, num_bytes);
    audio_resume();

    do {
        exit_reason = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
    } while (exit_reason == kCFRunLoopRunHandledSource);

    return wrote;
}

/* set variables and buffer to initial state */
static void reset(void)
{
  audio_pause();
  av_fifo_reset(ao->buffer);
}


/* return available space */
static int get_space(void)
{
  return ao->buffer_len - av_fifo_size(ao->buffer);
}


/* return delay until audio is played */
static float get_delay(void)
{
  // inaccurate, should also contain the data buffered e.g. by the OS
  return (float)av_fifo_size(ao->buffer)/(float)ao_data.bps;
}


/* unload plugin and deregister from coreaudio */
static void uninit(int immed)
{
  OSStatus            err = noErr;

  if (!immed) {
    long long timeleft=(1000000LL*av_fifo_size(ao->buffer))/ao_data.bps;
    ao_msg(MSGT_AO,MSGL_DBG2, "%d bytes left @%d bps (%d usec)\n", av_fifo_size(ao->buffer), ao_data.bps, (int)timeleft);
    usec_sleep((int)timeleft);
  }

  if (!ao->b_digital) {
      AudioOutputUnitStop(ao->theOutputUnit);
      AudioUnitUninitialize(ao->theOutputUnit);
      CloseComponent(ao->theOutputUnit);
  }
  else {
      /* Stop device. */
      err = AudioDeviceStop(ao->i_selected_dev, ao->renderCallback);
      if (err != noErr)
          ao_msg(MSGT_AO, MSGL_WARN, "AudioDeviceStop failed: [%4.4s]\n", (char *)&err);

      /* Remove IOProc callback. */
      err = AudioDeviceDestroyIOProcID(ao->i_selected_dev, ao->renderCallback);
      if (err != noErr)
          ao_msg(MSGT_AO, MSGL_WARN, "AudioDeviceRemoveIOProc failed: [%4.4s]\n", (char *)&err);

      if (ao->b_revert)
          AudioStreamChangeFormat(ao->i_stream_id, ao->sfmt_revert);

      if (ao->b_changed_mixing && ao->sfmt_revert.mFormatID != kAudioFormat60958AC3)
      {
          UInt32 b_mix;
          Boolean b_writeable = 0;
          /* Revert mixable to true if we are allowed to. */
          err = IsAudioPropertySettable(ao->i_selected_dev,
                                        kAudioDevicePropertySupportsMixing,
                                        &b_writeable);
          err = GetAudioProperty(ao->i_selected_dev,
                                 kAudioDevicePropertySupportsMixing,
                                 sizeof(UInt32), &b_mix);
          if (err == noErr && b_writeable)
          {
              b_mix = 1;
              err = SetAudioProperty(ao->i_selected_dev,
                                     kAudioDevicePropertySupportsMixing,
                                     sizeof(UInt32), &b_mix);
          }
          if (err != noErr)
              ao_msg(MSGT_AO, MSGL_WARN, "failed to set mixmode: [%4.4s]\n", (char *)&err);
      }
      if (ao->i_hog_pid == getpid())
      {
          ao->i_hog_pid = -1;
          err = SetAudioProperty(ao->i_selected_dev,
                                 kAudioDevicePropertyHogMode,
                                 sizeof(ao->i_hog_pid), &ao->i_hog_pid);
          if (err != noErr) ao_msg(MSGT_AO, MSGL_WARN, "Could not release hogmode: [%4.4s]\n", (char *)&err);
      }
  }

  av_fifo_free(ao->buffer);
  free(ao);
  ao = NULL;
}


/* stop playing, keep buffers (for pause) */
static void audio_pause(void)
{
    OSErr err=noErr;

    /* Stop callback. */
    if (!ao->b_digital)
    {
        err=AudioOutputUnitStop(ao->theOutputUnit);
        if (err != noErr)
            ao_msg(MSGT_AO,MSGL_WARN, "AudioOutputUnitStop returned [%4.4s]\n", (char *)&err);
    }
    else
    {
        err = AudioDeviceStop(ao->i_selected_dev, ao->renderCallback);
        if (err != noErr)
            ao_msg(MSGT_AO, MSGL_WARN, "AudioDeviceStop failed: [%4.4s]\n", (char *)&err);
    }
    ao->paused = 1;
}


/* resume playing, after audio_pause() */
static void audio_resume(void)
{
    OSErr err=noErr;

    if (!ao->paused)
        return;

    /* Start callback. */
    if (!ao->b_digital)
    {
        err = AudioOutputUnitStart(ao->theOutputUnit);
        if (err != noErr)
            ao_msg(MSGT_AO,MSGL_WARN, "AudioOutputUnitStart returned [%4.4s]\n", (char *)&err);
    }
    else
    {
        err = AudioDeviceStart(ao->i_selected_dev, ao->renderCallback);
        if (err != noErr)
            ao_msg(MSGT_AO, MSGL_WARN, "AudioDeviceStart failed: [%4.4s]\n", (char *)&err);
    }
    ao->paused = 0;
}

/*****************************************************************************
 * StreamListener
 *****************************************************************************/
static OSStatus StreamListener( AudioObjectID inObjectID,
                                UInt32 inNumberAddresses,
                                const AudioObjectPropertyAddress inAddresses[],
                                void *inClientData )
{
    for (int i=0; i < inNumberAddresses; ++i)
    {
        if (inAddresses[i].mSelector == kAudioStreamPropertyPhysicalFormat) {
            ao_msg(MSGT_AO, MSGL_WARN, "got notify kAudioStreamPropertyPhysicalFormat changed.\n");
            if (inClientData)
                *(volatile int *)inClientData = 1;
            break;
        }
    }
    return noErr;
}

static OSStatus DeviceListener( AudioObjectID inObjectID,
                                UInt32 inNumberAddresses,
                                const AudioObjectPropertyAddress inAddresses[],
                                void *inClientData )
{
    for (int i=0; i < inNumberAddresses; ++i)
    {
        if (inAddresses[i].mSelector == kAudioDevicePropertyDeviceHasChanged) {
            ao_msg(MSGT_AO, MSGL_WARN, "got notify kAudioDevicePropertyDeviceHasChanged.\n");
            ao->b_stream_format_changed = 1;
            break;
        }
    }
    return noErr;
}
