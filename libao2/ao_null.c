/*
 * null audio output driver
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
#include <sys/time.h>

#include "config.h"
#include "libaf/af_format.h"
#include "audio_out.h"
#include "audio_out_internal.h"

static const ao_info_t info =
{
	"Null audio output",
	"null",
	"Tobias Diedrich <ranma+mplayer@tdiedrich.de>",
	""
};

LIBAO_EXTERN(null)

struct	timeval last_tv;
int	buffer;

static void drain(void){

    struct timeval now_tv;
    int temp, temp2;

    gettimeofday(&now_tv, 0);
    temp = now_tv.tv_sec - last_tv.tv_sec;
    temp *= ao_data.bps;

    temp2 = now_tv.tv_usec - last_tv.tv_usec;
    temp2 /= 1000;
    temp2 *= ao_data.bps;
    temp2 /= 1000;
    temp += temp2;

    buffer-=temp;
    if (buffer<0) buffer=0;

    if(temp>0) last_tv = now_tv;//mplayer is fast
}

// to set/get/query special features/parameters
static int control(int cmd,void *arg){
    return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags){

    int samplesize = af_fmt2bits(format) / 8;
    ao_data.outburst = 256 * channels * samplesize;
    // A "buffer" for about 0.2 seconds of audio
    ao_data.buffersize = (int)(rate * 0.2 / 256 + 1) * ao_data.outburst;
    ao_data.channels=channels;
    ao_data.samplerate=rate;
    ao_data.format=format;
    ao_data.bps=channels*rate*samplesize;
    buffer=0;
    gettimeofday(&last_tv, 0);

    return 1;
}

// close audio device
static void uninit(int immed){

}

// stop playing and empty buffers (for seeking/pause)
static void reset(void){
    buffer=0;
}

// stop playing, keep buffers (for pause)
static void audio_pause(void)
{
    // for now, just call reset();
    reset();
}

// resume playing, after audio_pause()
static void audio_resume(void)
{
}

// return: how many bytes can be played without blocking
static int get_space(void){

    drain();
    return ao_data.buffersize - buffer;
}

// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags){

    int maxbursts = (ao_data.buffersize - buffer) / ao_data.outburst;
    int playbursts = len / ao_data.outburst;
    int bursts = playbursts > maxbursts ? maxbursts : playbursts;
    buffer += bursts * ao_data.outburst;
    return bursts * ao_data.outburst;
}

// return: delay in seconds between first and last sample in buffer
static float get_delay(void){

    drain();
    return (float) buffer / (float) ao_data.bps;
}
