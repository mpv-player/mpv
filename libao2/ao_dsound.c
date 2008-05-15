/*
 * Windows DirectSound interface
 *
 * Copyright (c) 2004 Gabor Szecsi <deje@miki.hu>
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

/**
\todo verify/extend multichannel support
*/


#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#define DIRECTSOUND_VERSION 0x0600
#include <dsound.h>
#include <math.h>

#include "config.h"
#include "libaf/af_format.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "mp_msg.h"
#include "libvo/fastmemcpy.h"
#include "osdep/timer.h"
#include "subopt-helper.h"


static ao_info_t info =
{
	"Windows DirectSound audio output",
	"dsound",
	"Gabor Szecsi <deje@miki.hu>",
	""
};

LIBAO_EXTERN(dsound)

/**
\todo use the definitions from the win32 api headers when they define these
*/
#if 1
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

static const GUID KSDATAFORMAT_SUBTYPE_PCM = {0x1,0x0000,0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};

#define SPEAKER_FRONT_LEFT             0x1
#define SPEAKER_FRONT_RIGHT            0x2
#define SPEAKER_FRONT_CENTER           0x4
#define SPEAKER_LOW_FREQUENCY          0x8
#define SPEAKER_BACK_LEFT              0x10
#define SPEAKER_BACK_RIGHT             0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#define SPEAKER_BACK_CENTER            0x100
#define SPEAKER_SIDE_LEFT              0x200
#define SPEAKER_SIDE_RIGHT             0x400
#define SPEAKER_TOP_CENTER             0x800
#define SPEAKER_TOP_FRONT_LEFT         0x1000
#define SPEAKER_TOP_FRONT_CENTER       0x2000
#define SPEAKER_TOP_FRONT_RIGHT        0x4000
#define SPEAKER_TOP_BACK_LEFT          0x8000
#define SPEAKER_TOP_BACK_CENTER        0x10000
#define SPEAKER_TOP_BACK_RIGHT         0x20000
#define SPEAKER_RESERVED               0x80000000

#define DSSPEAKER_HEADPHONE         0x00000001
#define DSSPEAKER_MONO              0x00000002
#define DSSPEAKER_QUAD              0x00000003
#define DSSPEAKER_STEREO            0x00000004
#define DSSPEAKER_SURROUND          0x00000005
#define DSSPEAKER_5POINT1           0x00000006

#ifndef _WAVEFORMATEXTENSIBLE_
typedef struct {
    WAVEFORMATEX    Format;
    union {
        WORD wValidBitsPerSample;       /* bits of precision  */
        WORD wSamplesPerBlock;          /* valid if wBitsPerSample==0 */
        WORD wReserved;                 /* If neither applies, set to zero. */
    } Samples;
    DWORD           dwChannelMask;      /* which channels are */
                                        /* present in stream  */
    GUID            SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif

#endif

static const int channel_mask[] = {
  SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_RIGHT  | SPEAKER_LOW_FREQUENCY,
  SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT    | SPEAKER_BACK_RIGHT,
  SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT    | SPEAKER_BACK_RIGHT   | SPEAKER_LOW_FREQUENCY,
  SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT    | SPEAKER_BACK_RIGHT     | SPEAKER_LOW_FREQUENCY
};

static HINSTANCE hdsound_dll = NULL;      ///handle to the dll
static LPDIRECTSOUND hds = NULL;          ///direct sound object 
static LPDIRECTSOUNDBUFFER hdspribuf = NULL; ///primary direct sound buffer
static LPDIRECTSOUNDBUFFER hdsbuf = NULL; ///secondary direct sound buffer (stream buffer)
static int buffer_size = 0;               ///size in bytes of the direct sound buffer   
static int write_offset = 0;              ///offset of the write cursor in the direct sound buffer
static int min_free_space = 0;            ///if the free space is below this value get_space() will return 0
                                          ///there will always be at least this amout of free space to prevent
                                          ///get_space() from returning wrong values when buffer is 100% full.
                                          ///will be replaced with nBlockAlign in init()
static int device_num = 0;                ///wanted device number
static GUID device;                       ///guid of the device 

/***************************************************************************************/

/**
\brief output error message
\param err error code
\return string with the error message
*/
static char * dserr2str(int err)
{
	switch (err) {
		case DS_OK: return "DS_OK";
		case DS_NO_VIRTUALIZATION: return "DS_NO_VIRTUALIZATION";
		case DSERR_ALLOCATED: return "DS_NO_VIRTUALIZATION";
		case DSERR_CONTROLUNAVAIL: return "DSERR_CONTROLUNAVAIL";
		case DSERR_INVALIDPARAM: return "DSERR_INVALIDPARAM";
		case DSERR_INVALIDCALL: return "DSERR_INVALIDCALL";
		case DSERR_GENERIC: return "DSERR_GENERIC";
		case DSERR_PRIOLEVELNEEDED: return "DSERR_PRIOLEVELNEEDED";
		case DSERR_OUTOFMEMORY: return "DSERR_OUTOFMEMORY";
		case DSERR_BADFORMAT: return "DSERR_BADFORMAT";
		case DSERR_UNSUPPORTED: return "DSERR_UNSUPPORTED";
		case DSERR_NODRIVER: return "DSERR_NODRIVER";
		case DSERR_ALREADYINITIALIZED: return "DSERR_ALREADYINITIALIZED";
		case DSERR_NOAGGREGATION: return "DSERR_NOAGGREGATION";
		case DSERR_BUFFERLOST: return "DSERR_BUFFERLOST";
		case DSERR_OTHERAPPHASPRIO: return "DSERR_OTHERAPPHASPRIO";
		case DSERR_UNINITIALIZED: return "DSERR_UNINITIALIZED";
		case DSERR_NOINTERFACE: return "DSERR_NOINTERFACE";
		case DSERR_ACCESSDENIED: return "DSERR_ACCESSDENIED";
		default: return "unknown";
	}
}

/**
\brief uninitialize direct sound
*/
static void UninitDirectSound(void)
{
    // finally release the DirectSound object
    if (hds) {
    	IDirectSound_Release(hds);
    	hds = NULL;
    }
    // free DSOUND.DLL
    if (hdsound_dll) {
    	FreeLibrary(hdsound_dll);
    	hdsound_dll = NULL;
    }
	mp_msg(MSGT_AO, MSGL_V, "ao_dsound: DirectSound uninitialized\n");
}

/**
\brief print the commandline help
*/
static void print_help(void)
{
  mp_msg(MSGT_AO, MSGL_FATAL,
           "\n-ao dsound commandline help:\n"
           "Example: mplayer -ao dsound:device=1\n"
           "  sets 1st device\n"
           "\nOptions:\n"
           "  device=<device-number>\n"
           "    Sets device number, use -v to get a list\n");
}


/**
\brief enumerate direct sound devices
\return TRUE to continue with the enumeration
*/
static BOOL CALLBACK DirectSoundEnum(LPGUID guid,LPCSTR desc,LPCSTR module,LPVOID context)
{
    int* device_index=context;
    mp_msg(MSGT_AO, MSGL_V,"%i %s ",*device_index,desc);
    if(device_num==*device_index){
        mp_msg(MSGT_AO, MSGL_V,"<--");
        if(guid){
            memcpy(&device,guid,sizeof(GUID));
        }
    }
    mp_msg(MSGT_AO, MSGL_V,"\n");
    (*device_index)++;
    return TRUE;
}


/**
\brief initilize direct sound
\return 0 if error, 1 if ok
*/
static int InitDirectSound(void)
{
	DSCAPS dscaps;

	// initialize directsound
    HRESULT (WINAPI *OurDirectSoundCreate)(LPGUID, LPDIRECTSOUND *, LPUNKNOWN);
	HRESULT (WINAPI *OurDirectSoundEnumerate)(LPDSENUMCALLBACKA, LPVOID);   
	int device_index=0;
	opt_t subopts[] = {
	  {"device", OPT_ARG_INT, &device_num,NULL},
	  {NULL}
	}; 
	if (subopt_parse(ao_subdevice, subopts) != 0) {
		print_help();
		return 0;
	}
    
	hdsound_dll = LoadLibrary("DSOUND.DLL");
	if (hdsound_dll == NULL) {
		mp_msg(MSGT_AO, MSGL_ERR, "ao_dsound: cannot load DSOUND.DLL\n");
		return 0;
	}
	OurDirectSoundCreate = (void*)GetProcAddress(hdsound_dll, "DirectSoundCreate");
	OurDirectSoundEnumerate = (void*)GetProcAddress(hdsound_dll, "DirectSoundEnumerateA");

	if (OurDirectSoundCreate == NULL || OurDirectSoundEnumerate == NULL) {
		mp_msg(MSGT_AO, MSGL_ERR, "ao_dsound: GetProcAddress FAILED\n");
		FreeLibrary(hdsound_dll);
		return 0;
	}
    
	// Enumerate all directsound devices
	mp_msg(MSGT_AO, MSGL_V,"ao_dsound: Output Devices:\n");
	OurDirectSoundEnumerate(DirectSoundEnum,&device_index);

	// Create the direct sound object
	if FAILED(OurDirectSoundCreate((device_num)?&device:NULL, &hds, NULL )) {
		mp_msg(MSGT_AO, MSGL_ERR, "ao_dsound: cannot create a DirectSound device\n");
		FreeLibrary(hdsound_dll);
		return 0;
	}

	/* Set DirectSound Cooperative level, ie what control we want over Windows
	 * sound device. In our case, DSSCL_EXCLUSIVE means that we can modify the
	 * settings of the primary buffer, but also that only the sound of our
	 * application will be hearable when it will have the focus.
	 * !!! (this is not really working as intended yet because to set the
	 * cooperative level you need the window handle of your application, and
	 * I don't know of any easy way to get it. Especially since we might play
	 * sound without any video, and so what window handle should we use ???
	 * The hack for now is to use the Desktop window handle - it seems to be
	 * working */
	if (IDirectSound_SetCooperativeLevel(hds, GetDesktopWindow(), DSSCL_EXCLUSIVE)) {
		mp_msg(MSGT_AO, MSGL_ERR, "ao_dsound: cannot set direct sound cooperative level\n");
		IDirectSound_Release(hds);
		FreeLibrary(hdsound_dll);
		return 0;
	}
	mp_msg(MSGT_AO, MSGL_V, "ao_dsound: DirectSound initialized\n");

	memset(&dscaps, 0, sizeof(DSCAPS));
	dscaps.dwSize = sizeof(DSCAPS);
	if (DS_OK == IDirectSound_GetCaps(hds, &dscaps)) {
		if (dscaps.dwFlags & DSCAPS_EMULDRIVER) mp_msg(MSGT_AO, MSGL_V, "ao_dsound: DirectSound is emulated, waveOut may give better performance\n");
	} else {
		mp_msg(MSGT_AO, MSGL_V, "ao_dsound: cannot get device capabilities\n");
	}

	return 1;
}

/**
\brief destroy the direct sound buffer
*/
static void DestroyBuffer(void)
{
	if (hdsbuf) {
		IDirectSoundBuffer_Release(hdsbuf);
		hdsbuf = NULL;
	}
	if (hdspribuf) {
		IDirectSoundBuffer_Release(hdspribuf);
		hdspribuf = NULL;
	}
}

/**
\brief fill sound buffer
\param data pointer to the sound data to copy
\param len length of the data to copy in bytes
\return number of copyed bytes
*/
static int write_buffer(unsigned char *data, int len)
{
  HRESULT res;
  LPVOID lpvPtr1; 
  DWORD dwBytes1; 
  LPVOID lpvPtr2; 
  DWORD dwBytes2; 
	
  // Lock the buffer
  res = IDirectSoundBuffer_Lock(hdsbuf,write_offset, len, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0); 
  // If the buffer was lost, restore and retry lock. 
  if (DSERR_BUFFERLOST == res) 
  { 
    IDirectSoundBuffer_Restore(hdsbuf);
	res = IDirectSoundBuffer_Lock(hdsbuf,write_offset, len, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);
  }
 
  
  if (SUCCEEDED(res)) 
  {
  	if( (ao_data.channels == 6) && (ao_data.format!=AF_FORMAT_AC3) ) {
  	    // reorder channels while writing to pointers.
  	    // it's this easy because buffer size and len are always
  	    // aligned to multiples of channels*bytespersample
  	    // there's probably some room for speed improvements here
  	    const int chantable[6] = {0, 1, 4, 5, 2, 3}; // reorder "matrix"
  	    int i, j;
  	    int numsamp,sampsize;

  	    sampsize = af_fmt2bits(ao_data.format)>>3; // bytes per sample
  	    numsamp = dwBytes1 / (ao_data.channels * sampsize);  // number of samples for each channel in this buffer

  	    for( i = 0; i < numsamp; i++ ) for( j = 0; j < ao_data.channels; j++ ) {
  	        memcpy(lpvPtr1+(i*ao_data.channels*sampsize)+(chantable[j]*sampsize),data+(i*ao_data.channels*sampsize)+(j*sampsize),sampsize);
  	    }

  	    if (NULL != lpvPtr2 )
  	    {
  	        numsamp = dwBytes2 / (ao_data.channels * sampsize);
  	        for( i = 0; i < numsamp; i++ ) for( j = 0; j < ao_data.channels; j++ ) {
  	            memcpy(lpvPtr2+(i*ao_data.channels*sampsize)+(chantable[j]*sampsize),data+dwBytes1+(i*ao_data.channels*sampsize)+(j*sampsize),sampsize);
  	        }
  	    }

  	    write_offset+=dwBytes1+dwBytes2;
  	    if(write_offset>=buffer_size)write_offset=dwBytes2;
  	} else {
  	    // Write to pointers without reordering. 
	fast_memcpy(lpvPtr1,data,dwBytes1);
    if (NULL != lpvPtr2 )fast_memcpy(lpvPtr2,data+dwBytes1,dwBytes2);
	write_offset+=dwBytes1+dwBytes2;
    if(write_offset>=buffer_size)write_offset=dwBytes2;
  	}
	
   // Release the data back to DirectSound. 
    res = IDirectSoundBuffer_Unlock(hdsbuf,lpvPtr1,dwBytes1,lpvPtr2,dwBytes2);
    if (SUCCEEDED(res)) 
    { 
	  // Success. 
	  DWORD status;
	  IDirectSoundBuffer_GetStatus(hdsbuf, &status);
      if (!(status & DSBSTATUS_PLAYING)){
	    res = IDirectSoundBuffer_Play(hdsbuf, 0, 0, DSBPLAY_LOOPING);
	  }
	  return dwBytes1+dwBytes2; 
    } 
  } 
  // Lock, Unlock, or Restore failed. 
  return 0;
}

/***************************************************************************************/

/**
\brief handle control commands
\param cmd command
\param arg argument
\return CONTROL_OK or -1 in case the command can't be handled
*/
static int control(int cmd, void *arg)
{
	DWORD volume;
	switch (cmd) {
		case AOCONTROL_GET_VOLUME: {
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			IDirectSoundBuffer_GetVolume(hdsbuf, &volume);
			vol->left = vol->right = pow(10.0, (float)(volume+10000) / 5000.0);
			//printf("ao_dsound: volume: %f\n",vol->left);
			return CONTROL_OK;
		}
		case AOCONTROL_SET_VOLUME: {
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			volume = (DWORD)(log10(vol->right) * 5000.0) - 10000;
			IDirectSoundBuffer_SetVolume(hdsbuf, volume);
			//printf("ao_dsound: volume: %f\n",vol->left);
			return CONTROL_OK;
		}
	}
	return -1;
}

/** 
\brief setup sound device
\param rate samplerate
\param channels number of channels
\param format format
\param flags unused
\return 1=success 0=fail
*/
static int init(int rate, int channels, int format, int flags)
{
    int res;
	if (!InitDirectSound()) return 0;

	// ok, now create the buffers
	WAVEFORMATEXTENSIBLE wformat;
	DSBUFFERDESC dsbpridesc;
	DSBUFFERDESC dsbdesc;

	//check if the format is supported in general
	switch(format){
		case AF_FORMAT_AC3:
		case AF_FORMAT_S24_LE:
		case AF_FORMAT_S16_LE:
		case AF_FORMAT_S8:
			break;
		default:
			mp_msg(MSGT_AO, MSGL_V,"ao_dsound: format %s not supported defaulting to Signed 16-bit Little-Endian\n",af_fmt2str_short(format));
			format=AF_FORMAT_S16_LE;
	}   	
	//fill global ao_data
	ao_data.channels = channels;
	ao_data.samplerate = rate;
	ao_data.format = format;
	ao_data.bps = channels * rate * (af_fmt2bits(format)>>3);
	if(ao_data.buffersize==-1) ao_data.buffersize = ao_data.bps; // space for 1 sec
	mp_msg(MSGT_AO, MSGL_V,"ao_dsound: Samplerate:%iHz Channels:%i Format:%s\n", rate, channels, af_fmt2str_short(format));
	mp_msg(MSGT_AO, MSGL_V,"ao_dsound: Buffersize:%d bytes (%d msec)\n", ao_data.buffersize, ao_data.buffersize / ao_data.bps * 1000);

	//fill waveformatex
	ZeroMemory(&wformat, sizeof(WAVEFORMATEXTENSIBLE));
	wformat.Format.cbSize          = (channels > 2) ? sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX) : 0;
	wformat.Format.nChannels       = channels;
	wformat.Format.nSamplesPerSec  = rate;
	if (format == AF_FORMAT_AC3) {
		wformat.Format.wFormatTag      = WAVE_FORMAT_DOLBY_AC3_SPDIF;
		wformat.Format.wBitsPerSample  = 16;
		wformat.Format.nBlockAlign     = 4;
	} else {
		wformat.Format.wFormatTag      = (channels > 2) ? WAVE_FORMAT_EXTENSIBLE : WAVE_FORMAT_PCM;
		wformat.Format.wBitsPerSample  = af_fmt2bits(format);
		wformat.Format.nBlockAlign     = wformat.Format.nChannels * (wformat.Format.wBitsPerSample >> 3);
	}

	// fill in primary sound buffer descriptor
	memset(&dsbpridesc, 0, sizeof(DSBUFFERDESC));
	dsbpridesc.dwSize = sizeof(DSBUFFERDESC);
	dsbpridesc.dwFlags       = DSBCAPS_PRIMARYBUFFER;
	dsbpridesc.dwBufferBytes = 0;
	dsbpridesc.lpwfxFormat   = NULL;


	// fill in the secondary sound buffer (=stream buffer) descriptor
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 /** Better position accuracy */
	                | DSBCAPS_GLOBALFOCUS         /** Allows background playing */
	                | DSBCAPS_CTRLVOLUME;         /** volume control enabled */

	if (channels > 2) {
		wformat.dwChannelMask = channel_mask[channels - 3];
		wformat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
		wformat.Samples.wValidBitsPerSample = wformat.Format.wBitsPerSample;
		// Needed for 5.1 on emu101k - shit soundblaster
		dsbdesc.dwFlags |= DSBCAPS_LOCHARDWARE;
	}
	wformat.Format.nAvgBytesPerSec = wformat.Format.nSamplesPerSec * wformat.Format.nBlockAlign;

	dsbdesc.dwBufferBytes = ao_data.buffersize;
	dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&wformat;
	buffer_size = dsbdesc.dwBufferBytes;
	write_offset = 0;
	min_free_space = wformat.Format.nBlockAlign;
	ao_data.outburst = wformat.Format.nBlockAlign * 512;

	// create primary buffer and set its format
    
	res = IDirectSound_CreateSoundBuffer( hds, &dsbpridesc, &hdspribuf, NULL );
	if ( res != DS_OK ) {
		UninitDirectSound();
		mp_msg(MSGT_AO, MSGL_ERR,"ao_dsound: cannot create primary buffer (%s)\n", dserr2str(res));
		return 0;
	}
	res = IDirectSoundBuffer_SetFormat( hdspribuf, (WAVEFORMATEX *)&wformat );
	if ( res != DS_OK ) mp_msg(MSGT_AO, MSGL_WARN,"ao_dsound: cannot set primary buffer format (%s), using standard setting (bad quality)", dserr2str(res));

	mp_msg(MSGT_AO, MSGL_V, "ao_dsound: primary buffer created\n");

	// now create the stream buffer

	res = IDirectSound_CreateSoundBuffer(hds, &dsbdesc, &hdsbuf, NULL);
	if (res != DS_OK) {
		if (dsbdesc.dwFlags & DSBCAPS_LOCHARDWARE) {
			// Try without DSBCAPS_LOCHARDWARE
			dsbdesc.dwFlags &= ~DSBCAPS_LOCHARDWARE;
			res = IDirectSound_CreateSoundBuffer(hds, &dsbdesc, &hdsbuf, NULL);
		}
		if (res != DS_OK) {
			UninitDirectSound();
			mp_msg(MSGT_AO, MSGL_ERR, "ao_dsound: cannot create secondary (stream)buffer (%s)\n", dserr2str(res));
			return 0;
		}
	}
	mp_msg(MSGT_AO, MSGL_V, "ao_dsound: secondary (stream)buffer created\n");
	return 1;
}



/**
\brief stop playing and empty buffers (for seeking/pause)
*/
static void reset(void)
{
	IDirectSoundBuffer_Stop(hdsbuf);
	// reset directsound buffer
	IDirectSoundBuffer_SetCurrentPosition(hdsbuf, 0);
	write_offset=0;
}

/**
\brief stop playing, keep buffers (for pause)
*/
static void audio_pause(void)
{
	IDirectSoundBuffer_Stop(hdsbuf);
}

/**
\brief resume playing, after audio_pause()
*/
static void audio_resume(void)
{
	IDirectSoundBuffer_Play(hdsbuf, 0, 0, DSBPLAY_LOOPING);
}

/** 
\brief close audio device
\param immed stop playback immediately
*/
static void uninit(int immed)
{
	if(immed)reset();
	else{
		DWORD status;
		IDirectSoundBuffer_Play(hdsbuf, 0, 0, 0);
		while(!IDirectSoundBuffer_GetStatus(hdsbuf,&status) && (status&DSBSTATUS_PLAYING))
			usec_sleep(20000);
	}
	DestroyBuffer();
	UninitDirectSound();
}

/**
\brief find out how many bytes can be written into the audio buffer without
\return free space in bytes, has to return 0 if the buffer is almost full
*/
static int get_space(void)
{
	int space;
	DWORD play_offset;
	IDirectSoundBuffer_GetCurrentPosition(hdsbuf,&play_offset,NULL);
	space=buffer_size-(write_offset-play_offset);                                             
	//                |                                                      | <-- const --> |                |                 |
	//                buffer start                                           play_cursor     write_cursor     write_offset      buffer end
	// play_cursor is the actual postion of the play cursor
	// write_cursor is the position after which it is assumed to be save to write data
	// write_offset is the postion where we actually write the data to
	if(space > buffer_size)space -= buffer_size; // write_offset < play_offset
	if(space < min_free_space)return 0;
	return space-min_free_space;
}

/**
\brief play 'len' bytes of 'data'
\param data pointer to the data to play
\param len size in bytes of the data buffer, gets rounded down to outburst*n
\param flags currently unused
\return number of played bytes
*/
static int play(void* data, int len, int flags)
{
	DWORD play_offset;
	int space;
  
	// make sure we have enough space to write data
	IDirectSoundBuffer_GetCurrentPosition(hdsbuf,&play_offset,NULL);
	space=buffer_size-(write_offset-play_offset);                                             
	if(space > buffer_size)space -= buffer_size; // write_offset < play_offset
	if(space < len) len = space;

	if (!(flags & AOPLAY_FINAL_CHUNK))
	len = (len / ao_data.outburst) * ao_data.outburst;
	return write_buffer(data, len);
}

/**
\brief get the delay between the first and last sample in the buffer
\return delay in seconds
*/
static float get_delay(void)
{
	DWORD play_offset;
	int space;
	IDirectSoundBuffer_GetCurrentPosition(hdsbuf,&play_offset,NULL);
	space=play_offset-write_offset;                                             
	if(space <= 0)space += buffer_size;
	return (float)(buffer_size - space) / (float)ao_data.bps;
}
