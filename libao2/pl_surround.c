/* 
   This is an ao2 plugin to do simple decoding of matrixed surround
   sound.  This will provide a (basic) surround-sound effect from
   audio encoded for Dolby Surround, Pro Logic etc.

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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Original author: Steve Davies <steve@daviesfam.org>
*/

/* The principle:  Make rear channels by extracting anti-phase data
   from the front channels, delay by 15msec and feed to rear in anti-phase
   www.dolby.com has the background
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "audio_out.h"
#include "audio_plugin.h"
#include "audio_plugin_internal.h"
#include "afmt.h"

static ao_info_t info =
{
        "Surround decoder plugin",
        "surround",
        "Steve Davies <steve@daviesfam.org>",
        ""
};

LIBAO_PLUGIN_EXTERN(surround)

// local data
typedef struct pl_surround_s
{
  int passthrough;      // Just be a "NO-OP"
  int msecs;            // Rear channel delay in milliseconds
  int16_t* databuf;     // Output audio buffer
  int16_t* delaybuf;    // circular buffer to be used for delaying audio signal
  int delaybuf_len;     // local buffer length in samples
  int delaybuf_ptr;     // offset in buffer where we are reading/writing
  int rate;             // input data rate
  int format;           // input format
  int input_channels;   // input channels

} pl_surround_t;

static pl_surround_t pl_surround={0,15,NULL,NULL,0,0,0,0,0};

// to set/get/query special features/parameters
static int control(int cmd,int arg){
  switch(cmd){
  case AOCONTROL_PLUGIN_SET_LEN:
    if (pl_surround.passthrough) return CONTROL_OK;
    //fprintf(stderr, "pl_surround: AOCONTROL_PLUGIN_SET_LEN with arg=%d\n", arg);
    //fprintf(stderr, "pl_surround: ao_plugin_data.len=%d\n", ao_plugin_data.len);
    // Allocate an output buffer
    if (pl_surround.databuf != NULL) {
      free(pl_surround.databuf);  pl_surround.databuf = NULL;
    }
    pl_surround.databuf = calloc(ao_plugin_data.len, 1);
    // Return back smaller len so we don't get overflowed...  (??seems the right thing to do?)
    ao_plugin_data.len /= 2;
    return CONTROL_OK;
  }
  return -1;
}

// open & setup audio device
// return: 1=success 0=fail
static int init(){

  fprintf(stderr, "pl_surround: init input rate=%d, channels=%d\n", ao_plugin_data.rate, ao_plugin_data.channels);
  if (ao_plugin_data.channels != 2) {
    fprintf(stderr, "pl_surround: source audio must have 2 channels, using passthrough mode\n");
    pl_surround.passthrough = 1;
    return 1;
  }
  if (ao_plugin_data.format != AFMT_S16_LE) {
    fprintf(stderr, "pl_surround: I'm dumb and can only handle AFMT_S16_LE audio format, using passthrough mode\n");
    pl_surround.passthrough = 1;
    return 1;
  }

  pl_surround.passthrough = 0;

  /* Store info on input format to expect */
  pl_surround.rate=ao_plugin_data.rate;
  pl_surround.format=ao_plugin_data.format;
  pl_surround.input_channels=ao_plugin_data.channels;

  // Input 2 channels, output will be 4 - tell ao_plugin
  ao_plugin_data.channels    = 4;
  ao_plugin_data.sz_mult    /= 2;

  // Figure out buffer space needed for the 15msec delay
  pl_surround.delaybuf_len = pl_surround.rate * pl_surround.msecs / 1000;
  // Allocate delay buffer
  pl_surround.delaybuf=(void*)calloc(pl_surround.delaybuf_len,sizeof(int16_t));
  fprintf(stderr, "pl_surround: %dmsec surround delay, rate %d - buffer is %d samples\n",
	  pl_surround.msecs,pl_surround.rate,  pl_surround.delaybuf_len);
  pl_surround.delaybuf_ptr = 0;

  return 1;
}

// close plugin
static void uninit(){
  //  fprintf(stderr, "pl_surround: uninit called!\n");
  if (pl_surround.passthrough) return;
  if(pl_surround.delaybuf) 
    free(pl_surround.delaybuf);
  if(pl_surround.databuf) 
    free(pl_surround.databuf);
  pl_surround.delaybuf_len=0;
}

// empty buffers
static void reset()
{
  if (pl_surround.passthrough) return;
  //fprintf(stderr, "pl_surround: reset called\n");
  pl_surround.delaybuf_ptr = 0;
  memset(pl_surround.delaybuf, 0, sizeof(int16_t)*pl_surround.delaybuf_len);
}


// processes 'ao_plugin_data.len' bytes of 'data'
// called for every block of data
static int play(){
  int16_t *in, *out;
  int i, samples;
  int surround;

  if (pl_surround.passthrough) return 1;

  //  fprintf(stderr, "pl_surround: play %d bytes, %d samples\n", ao_plugin_data.len, samples);

  samples  = ao_plugin_data.len / sizeof(int16_t) / pl_surround.input_channels;

  out = pl_surround.databuf;  in = (int16_t *)ao_plugin_data.data;
  for (i=0; i<samples; i++) {
    // front left and right
    out[0] = in[0];
    out[1] = in[1];
    // surround - from 15msec ago
    out[2] = pl_surround.delaybuf[pl_surround.delaybuf_ptr];
    out[3] = -out[2];
    // calculate and save surround for 15msecs time
    pl_surround.delaybuf[pl_surround.delaybuf_ptr++] = (in[0]/2 - in[1]/2);
    pl_surround.delaybuf_ptr %= pl_surround.delaybuf_len;
    // next samples...
    in = &in[pl_surround.input_channels];  out = &out[4];
  }
  
  // Set output block/len
  ao_plugin_data.data=pl_surround.databuf;
  ao_plugin_data.len=samples*sizeof(int16_t)*4;
  return 1;
}



