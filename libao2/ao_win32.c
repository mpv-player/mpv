/******************************************************************************
 * ao_win32.c: Windows waveOut interface for MPlayer
 * Copyright (c) 2002 Sascha Sommer <saschasommer@freenet.de>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>

#include "afmt.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "../mp_msg.h"
#include "../libvo/fastmemcpy.h"

#define SAMPLESIZE   1024
#define BUFFER_SIZE  4096
#define BUFFER_COUNT 16 


static WAVEHDR*     waveBlocks;         //pointer to our ringbuffer memory
static HWAVEOUT     hWaveOut;           //handle to the waveout device
static DWORD        restoredvolume;     //saves the volume to restore after playing
static unsigned int buf_write=0;
static unsigned int buf_write_pos=0;
static int          full_buffers=0;
static int          buffered_bytes=0;


static ao_info_t info = 
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
    if(full_buffers==0) return;    //no more data buffered!
    buffered_bytes-=BUFFER_SIZE;
    --full_buffers;
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
	WAVEFORMATEX wformat;      
	DWORD totalBufferSize = (BUFFER_SIZE + sizeof(WAVEHDR)) * BUFFER_COUNT;
	MMRESULT result;
	unsigned char* buffer;
	int i;
   
	//fill global ao_data 
	ao_data.channels=channels;
	ao_data.samplerate=rate;
	ao_data.format=format;
	ao_data.bps=channels*rate;
	if(format != AFMT_U8 && format != AFMT_S8)
	  ao_data.bps*=2;
	if(ao_data.buffersize==-1)
	{
		ao_data.buffersize=audio_out_format_bits(format)/8;
        ao_data.buffersize*= channels;
		ao_data.buffersize*= SAMPLESIZE;
	}
	mp_msg(MSGT_AO, MSGL_V,"ao_win32: Samplerate:%iHz Channels:%i Format:%s\n",rate, channels, audio_out_format_name(format));
    mp_msg(MSGT_AO, MSGL_V,"ao_win32: Buffersize:%d\n",ao_data.buffersize);
	
	//fill waveformatex
    ZeroMemory( &wformat, sizeof(WAVEFORMATEX));
    wformat.cbSize          = 0; /* size of _extra_ info */
	wformat.wFormatTag      = WAVE_FORMAT_PCM;  
    wformat.nChannels       = channels;                
    wformat.nSamplesPerSec  = rate;            
    wformat.wBitsPerSample  = audio_out_format_bits(format); 
    wformat.nBlockAlign     = wformat.nChannels * (wformat.wBitsPerSample >> 3);
    wformat.nAvgBytesPerSec = wformat.nSamplesPerSec * wformat.nBlockAlign;
 	
    //open sound device
    //WAVE_MAPPER always points to the default wave device on the system
    result = waveOutOpen(&hWaveOut,WAVE_MAPPER,&wformat,(DWORD_PTR)waveOutProc,0,CALLBACK_FUNCTION);
	if(result == WAVERR_BADFORMAT)
	{
		mp_msg(MSGT_AO, MSGL_ERR,"ao_win32: format not supported switching to default\n");
        ao_data.channels = wformat.nChannels = 2;
	    ao_data.samplerate = wformat.nSamplesPerSec = 44100;
	    ao_data.format = AFMT_S16_LE;
	    ao_data.bps=ao_data.channels * ao_data.samplerate;
	    ao_data.buffersize=wformat.wBitsPerSample=16;
        wformat.nBlockAlign     = wformat.nChannels * (wformat.wBitsPerSample >> 3);
        wformat.nAvgBytesPerSec = wformat.nSamplesPerSec * wformat.nBlockAlign;
		ao_data.buffersize/=8;
		ao_data.buffersize*= ao_data.channels;
		ao_data.buffersize*= SAMPLESIZE;
        result = waveOutOpen(&hWaveOut,WAVE_MAPPER,&wformat,(DWORD_PTR)waveOutProc,0,CALLBACK_FUNCTION);
	}
	if(result != MMSYSERR_NOERROR)
	{
		mp_msg(MSGT_AO, MSGL_ERR,"ao_win32: unable to open wave mapper device\n");
		return 0;
    }
    //save volume
	waveOutGetVolume(hWaveOut,&restoredvolume); 
	//allocate buffer memory as one big block
	buffer = malloc(totalBufferSize);
	memset(buffer,0x0,totalBufferSize);
    //and setup pointers to each buffer 
    waveBlocks = (WAVEHDR*)buffer;
    buffer += sizeof(WAVEHDR) * BUFFER_COUNT;
    for(i = 0; i < BUFFER_COUNT; i++) {
        waveBlocks[i].dwBufferLength = BUFFER_SIZE;
        waveBlocks[i].lpData = buffer;
        buffer += BUFFER_SIZE;
    }

    return 1;
}

// close audio device
static void uninit()
{
    waveOutSetVolume(hWaveOut,restoredvolume);  //restore volume
	waveOutReset(hWaveOut);
	waveOutClose(hWaveOut);
	mp_msg(MSGT_AO, MSGL_V,"waveOut device closed\n");
	full_buffers=0;
    free(waveBlocks);
	mp_msg(MSGT_AO, MSGL_V,"buffer memory freed\n");
}

// stop playing and empty buffers (for seeking/pause)
static void reset()
{
   	waveOutReset(hWaveOut);
	buf_write=0;
    buf_write_pos=0;
	full_buffers=0;
	buffered_bytes=0;
}

// stop playing, keep buffers (for pause)
static void audio_pause()
{
    waveOutPause(hWaveOut);
}

// resume playing, after audio_pause()
static void audio_resume()
{
	waveOutRestart(hWaveOut);
}

// return: how many bytes can be played without blocking
static int get_space()
{
    return (BUFFER_COUNT-full_buffers)*BUFFER_SIZE - buf_write_pos;
}

//writes data into buffer, based on ringbuffer code in ao_sdl.c
static int write_waveOutBuffer(unsigned char* data,int len){
  WAVEHDR* current;
  int len2=0;
  int x;
  while(len>0){                       
    current = &waveBlocks[buf_write];
	if(full_buffers==BUFFER_COUNT) break;  
    //unprepare the header if it is prepared
	if(current->dwFlags & WHDR_PREPARED) 
           waveOutUnprepareHeader(hWaveOut, current, sizeof(WAVEHDR));
	x=BUFFER_SIZE-buf_write_pos;          
    if(x>len) x=len;                   
    memcpy(current->lpData+buf_write_pos,data+len2,x); 
    len2+=x; len-=x;                 
	buffered_bytes+=x; buf_write_pos+=x; 
	//prepare header and write data to device
	waveOutPrepareHeader(hWaveOut, current, sizeof(WAVEHDR));
	waveOutWrite(hWaveOut, current, sizeof(WAVEHDR));
    
	if(buf_write_pos>=BUFFER_SIZE){        //buffer is full find next
       // block is full, find next!
       buf_write=(buf_write+1)%BUFFER_COUNT;  
       ++full_buffers;                
	   buf_write_pos=0;                 
    }                                 
  }
  return len2;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags)
{
	return write_waveOutBuffer(data,len);
}
int previous = 0;
// return: delay in seconds between first and last sample in buffer
static float get_delay()
{
	return (float)(buffered_bytes + ao_data.buffersize)/(float)ao_data.bps;
}
