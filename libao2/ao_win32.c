/*
 * Windows waveOut interface
 *
 * Copyright (c) 2002 - 2004 Sascha Sommer <saschasommer@freenet.de>
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
#include <windows.h>
#include <mmsystem.h>

#include "config.h"
#include "libaf/af_format.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "mp_msg.h"
#include "libvo/fastmemcpy.h"
#include "osdep/timer.h"

#define WAVE_FORMAT_DOLBY_AC3_SPDIF 0x0092
#define WAVE_FORMAT_EXTENSIBLE      0xFFFE

static const  GUID KSDATAFORMAT_SUBTYPE_PCM = {
	0x1,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}
};

typedef struct {
  WAVEFORMATEX  Format;
  union {
    WORD  wValidBitsPerSample;
    WORD  wSamplesPerBlock;
    WORD  wReserved;
  } Samples;
  DWORD  dwChannelMask;
  GUID  SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;

#define SPEAKER_FRONT_LEFT              0x1
#define SPEAKER_FRONT_RIGHT             0x2
#define SPEAKER_FRONT_CENTER            0x4
#define SPEAKER_LOW_FREQUENCY           0x8
#define SPEAKER_BACK_LEFT               0x10
#define SPEAKER_BACK_RIGHT              0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER    0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER   0x80
#define SPEAKER_BACK_CENTER             0x100
#define SPEAKER_SIDE_LEFT               0x200
#define SPEAKER_SIDE_RIGHT              0x400
#define SPEAKER_TOP_CENTER              0x800
#define SPEAKER_TOP_FRONT_LEFT          0x1000
#define SPEAKER_TOP_FRONT_CENTER        0x2000
#define SPEAKER_TOP_FRONT_RIGHT         0x4000
#define SPEAKER_TOP_BACK_LEFT           0x8000
#define SPEAKER_TOP_BACK_CENTER         0x10000
#define SPEAKER_TOP_BACK_RIGHT          0x20000

static const int channel_mask[] = {
  SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_RIGHT  | SPEAKER_LOW_FREQUENCY,
  SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT  | SPEAKER_LOW_FREQUENCY,
  SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_CENTER  | SPEAKER_LOW_FREQUENCY,
  SPEAKER_FRONT_LEFT   | SPEAKER_FRONT_CENTER | SPEAKER_FRONT_RIGHT  | SPEAKER_BACK_LEFT    | SPEAKER_BACK_RIGHT     | SPEAKER_LOW_FREQUENCY
};



#define SAMPLESIZE   1024
#define BUFFER_SIZE  4096
#define BUFFER_COUNT 16


static WAVEHDR*     waveBlocks;         //pointer to our ringbuffer memory
static HWAVEOUT     hWaveOut;           //handle to the waveout device
static unsigned int buf_write=0;
static volatile int buf_read=0;


static const ao_info_t info =
{
	"Windows waveOut audio output",
	"win32",
	"Sascha Sommer <saschasommer@freenet.de>",
	""
};

LIBAO_EXTERN(win32)

static void CALLBACK waveOutProc(HWAVEOUT hWaveOut,UINT uMsg,DWORD dwInstance,
    DWORD dwParam1,DWORD dwParam2)
{
	if(uMsg != WOM_DONE)
        return;
	buf_read = (buf_read + 1) % BUFFER_COUNT;
}

// to set/get/query special features/parameters
static int control(int cmd,void *arg)
{
	DWORD volume;
	switch (cmd)
	{
		case AOCONTROL_GET_VOLUME:
		{
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			waveOutGetVolume(hWaveOut,&volume);
			vol->left = (float)(LOWORD(volume)/655.35);
			vol->right = (float)(HIWORD(volume)/655.35);
			mp_msg(MSGT_AO, MSGL_DBG2,"ao_win32: volume left:%f volume right:%f\n",vol->left,vol->right);
			return CONTROL_OK;
		}
		case AOCONTROL_SET_VOLUME:
		{
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			volume = MAKELONG(vol->left*655.35,vol->right*655.35);
			waveOutSetVolume(hWaveOut,volume);
			return CONTROL_OK;
		}
	}
    return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags)
{
	WAVEFORMATEXTENSIBLE wformat;
	MMRESULT result;
	unsigned char* buffer;
	int i;

	if (AF_FORMAT_IS_AC3(format))
		format = AF_FORMAT_AC3_NE;
	switch(format){
		case AF_FORMAT_AC3_NE:
		case AF_FORMAT_S24_LE:
		case AF_FORMAT_S16_LE:
		case AF_FORMAT_U8:
			break;
		default:
			mp_msg(MSGT_AO, MSGL_V,"ao_win32: format %s not supported defaulting to Signed 16-bit Little-Endian\n",af_fmt2str_short(format));
			format=AF_FORMAT_S16_LE;
	}

	// FIXME multichannel mode is buggy
	if(channels > 2)
		channels = 2;

	//fill global ao_data
	ao_data.channels=channels;
	ao_data.samplerate=rate;
	ao_data.format=format;
	ao_data.bps=channels*rate;
	if(format != AF_FORMAT_U8 && format != AF_FORMAT_S8)
	  ao_data.bps*=2;
	ao_data.outburst = BUFFER_SIZE;
	if(ao_data.buffersize==-1)
	{
		ao_data.buffersize=af_fmt2bits(format)/8;
        ao_data.buffersize*= channels;
		ao_data.buffersize*= SAMPLESIZE;
	}
	mp_msg(MSGT_AO, MSGL_V,"ao_win32: Samplerate:%iHz Channels:%i Format:%s\n",rate, channels, af_fmt2str_short(format));
    mp_msg(MSGT_AO, MSGL_V,"ao_win32: Buffersize:%d\n",ao_data.buffersize);

	//fill waveformatex
    ZeroMemory( &wformat, sizeof(WAVEFORMATEXTENSIBLE));
    wformat.Format.cbSize          = (channels>2)?sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX):0;
    wformat.Format.nChannels       = channels;
    wformat.Format.nSamplesPerSec  = rate;
    if(AF_FORMAT_IS_AC3(format))
    {
        wformat.Format.wFormatTag      = WAVE_FORMAT_DOLBY_AC3_SPDIF;
        wformat.Format.wBitsPerSample  = 16;
        wformat.Format.nBlockAlign     = 4;
    }
    else
    {
        wformat.Format.wFormatTag      = (channels>2)?WAVE_FORMAT_EXTENSIBLE:WAVE_FORMAT_PCM;
        wformat.Format.wBitsPerSample  = af_fmt2bits(format);
        wformat.Format.nBlockAlign     = wformat.Format.nChannels * (wformat.Format.wBitsPerSample >> 3);
    }
	if(channels>2)
	{
        wformat.dwChannelMask = channel_mask[channels-3];
        wformat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
	    wformat.Samples.wValidBitsPerSample=af_fmt2bits(format);
    }

    wformat.Format.nAvgBytesPerSec = wformat.Format.nSamplesPerSec * wformat.Format.nBlockAlign;

    //open sound device
    //WAVE_MAPPER always points to the default wave device on the system
    result = waveOutOpen(&hWaveOut,WAVE_MAPPER,(WAVEFORMATEX*)&wformat,(DWORD_PTR)waveOutProc,0,CALLBACK_FUNCTION);
	if(result == WAVERR_BADFORMAT)
	{
		mp_msg(MSGT_AO, MSGL_ERR,"ao_win32: format not supported switching to default\n");
        ao_data.channels = wformat.Format.nChannels = 2;
	    ao_data.samplerate = wformat.Format.nSamplesPerSec = 44100;
	    ao_data.format = AF_FORMAT_S16_LE;
		ao_data.bps=ao_data.channels * ao_data.samplerate*2;
	    wformat.Format.wBitsPerSample=16;
        wformat.Format.wFormatTag=WAVE_FORMAT_PCM;
		wformat.Format.nBlockAlign     = wformat.Format.nChannels * (wformat.Format.wBitsPerSample >> 3);
        wformat.Format.nAvgBytesPerSec = wformat.Format.nSamplesPerSec * wformat.Format.nBlockAlign;
		ao_data.buffersize=(wformat.Format.wBitsPerSample>>3)*wformat.Format.nChannels*SAMPLESIZE;
        result = waveOutOpen(&hWaveOut,WAVE_MAPPER,(WAVEFORMATEX*)&wformat,(DWORD_PTR)waveOutProc,0,CALLBACK_FUNCTION);
	}
	if(result != MMSYSERR_NOERROR)
	{
		mp_msg(MSGT_AO, MSGL_ERR,"ao_win32: unable to open wave mapper device (result=%i)\n",result);
		return 0;
    }
	//allocate buffer memory as one big block
	buffer = calloc(BUFFER_COUNT, BUFFER_SIZE + sizeof(WAVEHDR));
    //and setup pointers to each buffer
    waveBlocks = (WAVEHDR*)buffer;
    buffer += sizeof(WAVEHDR) * BUFFER_COUNT;
    for(i = 0; i < BUFFER_COUNT; i++) {
        waveBlocks[i].lpData = buffer;
        buffer += BUFFER_SIZE;
    }
    buf_write=0;
    buf_read=0;

    return 1;
}

// close audio device
static void uninit(int immed)
{
    if(!immed)
	usec_sleep(get_delay() * 1000 * 1000);
    else
	waveOutReset(hWaveOut);
    while (waveOutClose(hWaveOut) == WAVERR_STILLPLAYING) usec_sleep(0);
	mp_msg(MSGT_AO, MSGL_V,"waveOut device closed\n");
    free(waveBlocks);
	mp_msg(MSGT_AO, MSGL_V,"buffer memory freed\n");
}

// stop playing and empty buffers (for seeking/pause)
static void reset(void)
{
   	waveOutReset(hWaveOut);
	buf_write=0;
	buf_read=0;
}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
    waveOutPause(hWaveOut);
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
	waveOutRestart(hWaveOut);
}

// return: how many bytes can be played without blocking
static int get_space(void)
{
    int free = buf_read - buf_write - 1;
    if (free < 0) free += BUFFER_COUNT;
    return free * BUFFER_SIZE;
}

//writes data into buffer, based on ringbuffer code in ao_sdl.c
static int write_waveOutBuffer(unsigned char* data,int len){
  WAVEHDR* current;
  int len2=0;
  int x;
  while(len>0){
    int buf_next = (buf_write + 1) % BUFFER_COUNT;
    current = &waveBlocks[buf_write];
    if(buf_next == buf_read) break;
    //unprepare the header if it is prepared
	if(current->dwFlags & WHDR_PREPARED)
           waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));
	x=BUFFER_SIZE;
    if(x>len) x=len;
    fast_memcpy(current->lpData,data+len2,x);
    len2+=x; len-=x;
	//prepare header and write data to device
	current->dwBufferLength = x;
	waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
	waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));

       buf_write = buf_next;
  }
  return len2;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags)
{
	if (!(flags & AOPLAY_FINAL_CHUNK))
	len = (len/ao_data.outburst)*ao_data.outburst;
	return write_waveOutBuffer(data,len);
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(void)
{
	int used = buf_write - buf_read;
	if (used < 0) used += BUFFER_COUNT;
	return (float)(used * BUFFER_SIZE + ao_data.buffersize)/(float)ao_data.bps;
}
