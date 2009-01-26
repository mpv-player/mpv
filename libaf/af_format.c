/*
 * This audio filter changes the format of a data block. Valid
 * formats are: AFMT_U8, AFMT_S8, AFMT_S16_LE, AFMT_S16_BE
 * AFMT_U16_LE, AFMT_U16_BE, AFMT_S32_LE and AFMT_S32_BE.
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
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>

#include "config.h"
#include "af.h"
#include "mpbswap.h"
#include "libvo/fastmemcpy.h"

/* Functions used by play to convert the input audio to the correct
   format */

/* The below includes retrieves functions for converting to and from
   ulaw and alaw */ 
#include "af_format_ulaw.h"
#include "af_format_alaw.h"

// Switch endianness
static void endian(void* in, void* out, int len, int bps);
// From signed to unsigned and the other way
static void si2us(void* data, int len, int bps);
// Change the number of bits per sample
static void change_bps(void* in, void* out, int len, int inbps, int outbps);
// From float to int signed
static void float2int(float* in, void* out, int len, int bps);
// From signed int to float
static void int2float(void* in, float* out, int len, int bps);

static af_data_t* play(struct af_instance_s* af, af_data_t* data);
static af_data_t* play_swapendian(struct af_instance_s* af, af_data_t* data);
static af_data_t* play_float_s16(struct af_instance_s* af, af_data_t* data);
static af_data_t* play_s16_float(struct af_instance_s* af, af_data_t* data);

// Helper functions to check sanity for input arguments

// Sanity check for bytes per sample
static int check_bps(int bps)
{
  if(bps != 4 && bps != 3 && bps != 2 && bps != 1){
    af_msg(AF_MSG_ERROR,"[format] The number of bytes per sample" 
	   " must be 1, 2, 3 or 4. Current value is %i \n",bps);
    return AF_ERROR;
  }
  return AF_OK;
}

// Check for unsupported formats
static int check_format(int format)
{
  char buf[256];
  switch(format & AF_FORMAT_SPECIAL_MASK){
  case(AF_FORMAT_IMA_ADPCM): 
  case(AF_FORMAT_MPEG2): 
  case(AF_FORMAT_AC3):
    af_msg(AF_MSG_ERROR,"[format] Sample format %s not yet supported \n",
	 af_fmt2str(format,buf,256)); 
    return AF_ERROR;
  }
  return AF_OK;
}

// Initialization and runtime control
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  switch(cmd){
  case AF_CONTROL_REINIT:{
    char buf1[256];
    char buf2[256];
    af_data_t *data = arg;
    
    // Make sure this filter isn't redundant 
    if(af->data->format == data->format && 
       af->data->bps == data->bps)
      return AF_DETACH;

    // Check for errors in configuration
    if((AF_OK != check_bps(data->bps)) ||
       (AF_OK != check_format(data->format)) ||
       (AF_OK != check_bps(af->data->bps)) ||
       (AF_OK != check_format(af->data->format)))
      return AF_ERROR;

    af_msg(AF_MSG_VERBOSE,"[format] Changing sample format from %s to %s\n",
	   af_fmt2str(data->format,buf1,256),
	   af_fmt2str(af->data->format,buf2,256));

    af->data->rate = data->rate;
    af->data->nch  = data->nch;
    af->mul        = (double)af->data->bps / data->bps;
    
    af->play = play; // set default
    
    // look whether only endianness differences are there
    if ((af->data->format & ~AF_FORMAT_END_MASK) ==
	(data->format & ~AF_FORMAT_END_MASK))
    {
	af_msg(AF_MSG_VERBOSE,"[format] Accelerated endianness conversion only\n");
	af->play = play_swapendian;
    }
    if ((data->format == AF_FORMAT_FLOAT_NE) &&
	(af->data->format == AF_FORMAT_S16_NE))
    {
	af_msg(AF_MSG_VERBOSE,"[format] Accelerated %s to %s conversion\n",
	   af_fmt2str(data->format,buf1,256),
	   af_fmt2str(af->data->format,buf2,256));
	af->play = play_float_s16;
    }
    if ((data->format == AF_FORMAT_S16_NE) &&
	(af->data->format == AF_FORMAT_FLOAT_NE))
    {
	af_msg(AF_MSG_VERBOSE,"[format] Accelerated %s to %s conversion\n",
	   af_fmt2str(data->format,buf1,256),
	   af_fmt2str(af->data->format,buf2,256));
	af->play = play_s16_float;
    }
    return AF_OK;
  }
  case AF_CONTROL_COMMAND_LINE:{
    int format = af_str2fmt_short(arg);
    if (format == -1) {
      af_msg(AF_MSG_ERROR, "[format] %s is not a valid format\n", (char *)arg);
      return AF_ERROR;
    }
    if(AF_OK != af->control(af,AF_CONTROL_FORMAT_FMT | AF_CONTROL_SET,&format))
      return AF_ERROR;
    return AF_OK;
  }
  case AF_CONTROL_FORMAT_FMT | AF_CONTROL_SET:{
    // Check for errors in configuration
    if(AF_OK != check_format(*(int*)arg))
      return AF_ERROR;

    af->data->format = *(int*)arg;
    af->data->bps = af_fmt2bits(af->data->format)/8;

    return AF_OK;
  }
  }
  return AF_UNKNOWN;
}

// Deallocate memory 
static void uninit(struct af_instance_s* af)
{
  if (af->data)
      free(af->data->audio);
  free(af->data);
  af->setup = 0;  
}

static af_data_t* play_swapendian(struct af_instance_s* af, af_data_t* data)
{
  af_data_t*   l   = af->data;	// Local data
  af_data_t*   c   = data;	// Current working data
  int 	       len = c->len/c->bps; // Length in samples of current audio block

  if(AF_OK != RESIZE_LOCAL_BUFFER(af,data))
    return NULL;

  endian(c->audio,l->audio,len,c->bps);

  c->audio = l->audio;
  c->format = l->format;

  return c;
}

static af_data_t* play_float_s16(struct af_instance_s* af, af_data_t* data)
{
  af_data_t*   l   = af->data;	// Local data
  af_data_t*   c   = data;	// Current working data
  int 	       len = c->len/4; // Length in samples of current audio block

  if(AF_OK != RESIZE_LOCAL_BUFFER(af,data))
    return NULL;

  float2int(c->audio, l->audio, len, 2);

  c->audio = l->audio;
  c->len = len*2;
  c->bps = 2;
  c->format = l->format;

  return c;
}

static af_data_t* play_s16_float(struct af_instance_s* af, af_data_t* data)
{
  af_data_t*   l   = af->data;	// Local data
  af_data_t*   c   = data;	// Current working data
  int 	       len = c->len/2; // Length in samples of current audio block

  if(AF_OK != RESIZE_LOCAL_BUFFER(af,data))
    return NULL;

  int2float(c->audio, l->audio, len, 2);

  c->audio = l->audio;
  c->len = len*4;
  c->bps = 4;
  c->format = l->format;

  return c;
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{
  af_data_t*   l   = af->data;	// Local data
  af_data_t*   c   = data;	// Current working data
  int 	       len = c->len/c->bps; // Length in samples of current audio block

  if(AF_OK != RESIZE_LOCAL_BUFFER(af,data))
    return NULL;

  // Change to cpu native endian format
  if((c->format&AF_FORMAT_END_MASK)!=AF_FORMAT_NE)
    endian(c->audio,c->audio,len,c->bps);

  // Conversion table
  if((c->format & AF_FORMAT_SPECIAL_MASK) == AF_FORMAT_MU_LAW) {
    from_ulaw(c->audio, l->audio, len, l->bps, l->format&AF_FORMAT_POINT_MASK);
    if(AF_FORMAT_A_LAW == (l->format&AF_FORMAT_SPECIAL_MASK))
      to_ulaw(l->audio, l->audio, len, 1, AF_FORMAT_SI);
    if((l->format&AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
      si2us(l->audio,len,l->bps);
  } else if((c->format & AF_FORMAT_SPECIAL_MASK) == AF_FORMAT_A_LAW) {
    from_alaw(c->audio, l->audio, len, l->bps, l->format&AF_FORMAT_POINT_MASK);
    if(AF_FORMAT_A_LAW == (l->format&AF_FORMAT_SPECIAL_MASK))
      to_alaw(l->audio, l->audio, len, 1, AF_FORMAT_SI);
    if((l->format&AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
      si2us(l->audio,len,l->bps);
  } else if((c->format & AF_FORMAT_POINT_MASK) == AF_FORMAT_F) {
    switch(l->format&AF_FORMAT_SPECIAL_MASK){
    case(AF_FORMAT_MU_LAW):
      to_ulaw(c->audio, l->audio, len, c->bps, c->format&AF_FORMAT_POINT_MASK);
      break;
    case(AF_FORMAT_A_LAW):
      to_alaw(c->audio, l->audio, len, c->bps, c->format&AF_FORMAT_POINT_MASK);
      break;
    default:
      float2int(c->audio, l->audio, len, l->bps);
      if((l->format&AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
	si2us(l->audio,len,l->bps);
      break;
    }
  } else {
    // Input must be int
    
    // Change signed/unsigned
    if((c->format&AF_FORMAT_SIGN_MASK) != (l->format&AF_FORMAT_SIGN_MASK)){
      si2us(c->audio,len,c->bps); 
    }
    // Convert to special formats
    switch(l->format&(AF_FORMAT_SPECIAL_MASK|AF_FORMAT_POINT_MASK)){
    case(AF_FORMAT_MU_LAW):
      to_ulaw(c->audio, l->audio, len, c->bps, c->format&AF_FORMAT_POINT_MASK);
      break;
    case(AF_FORMAT_A_LAW):
      to_alaw(c->audio, l->audio, len, c->bps, c->format&AF_FORMAT_POINT_MASK);
      break;
    case(AF_FORMAT_F):
      int2float(c->audio, l->audio, len, c->bps);
      break;
    default:
      // Change the number of bits
      if(c->bps != l->bps)
	change_bps(c->audio,l->audio,len,c->bps,l->bps);
      else
	fast_memcpy(l->audio,c->audio,len*c->bps);
      break;
    }
  }

  // Switch from cpu native endian to the correct endianness 
  if((l->format&AF_FORMAT_END_MASK)!=AF_FORMAT_NE)
    endian(l->audio,l->audio,len,l->bps);

  // Set output data
  c->audio  = l->audio;
  c->len    = len*l->bps;
  c->bps    = l->bps;
  c->format = l->format;
  return c;
}

// Allocate memory and set function pointers
static int af_open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul=1;
  af->data=calloc(1,sizeof(af_data_t));
  if(af->data == NULL)
    return AF_ERROR;
  return AF_OK;
}

// Description of this filter
af_info_t af_info_format = {
  "Sample format conversion",
  "format",
  "Anders",
  "",
  AF_FLAGS_REENTRANT,
  af_open
};

static inline uint32_t load24bit(void* data, int pos) {
#ifdef WORDS_BIGENDIAN
  return (((uint32_t)((uint8_t*)data)[3*pos])<<24) |
	 (((uint32_t)((uint8_t*)data)[3*pos+1])<<16) |
	 (((uint32_t)((uint8_t*)data)[3*pos+2])<<8);
#else
  return (((uint32_t)((uint8_t*)data)[3*pos])<<8) |
	 (((uint32_t)((uint8_t*)data)[3*pos+1])<<16) |
	 (((uint32_t)((uint8_t*)data)[3*pos+2])<<24);
#endif
}

static inline void store24bit(void* data, int pos, uint32_t expanded_value) {
#ifdef WORDS_BIGENDIAN
      ((uint8_t*)data)[3*pos]=expanded_value>>24;
      ((uint8_t*)data)[3*pos+1]=expanded_value>>16;
      ((uint8_t*)data)[3*pos+2]=expanded_value>>8;
#else
      ((uint8_t*)data)[3*pos]=expanded_value>>8;
      ((uint8_t*)data)[3*pos+1]=expanded_value>>16;
      ((uint8_t*)data)[3*pos+2]=expanded_value>>24;
#endif
}

// Function implementations used by play
static void endian(void* in, void* out, int len, int bps)
{
  register int i;
  switch(bps){
    case(2):{
      for(i=0;i<len;i++){
	((uint16_t*)out)[i]=bswap_16(((uint16_t*)in)[i]);
      }
      break;
    }
    case(3):{
      register uint8_t s;
      for(i=0;i<len;i++){
	s=((uint8_t*)in)[3*i];
	((uint8_t*)out)[3*i]=((uint8_t*)in)[3*i+2];
	if (in != out)
	  ((uint8_t*)out)[3*i+1]=((uint8_t*)in)[3*i+1];
	((uint8_t*)out)[3*i+2]=s;
      }
      break;
    }
    case(4):{
      for(i=0;i<len;i++){
	((uint32_t*)out)[i]=bswap_32(((uint32_t*)in)[i]);
      }
      break;
    }
  }
}

static void si2us(void* data, int len, int bps)
{
  register long i = -(len * bps);
  register uint8_t *p = &((uint8_t *)data)[len * bps];
#if AF_FORMAT_NE == AF_FORMAT_LE
  p += bps - 1;
#endif
  if (len <= 0) return;
  do {
    p[i] ^= 0x80;
  } while (i += bps);
}

static void change_bps(void* in, void* out, int len, int inbps, int outbps)
{
  register int i;
  switch(inbps){
  case(1):
    switch(outbps){
    case(2):
      for(i=0;i<len;i++)
	((uint16_t*)out)[i]=((uint16_t)((uint8_t*)in)[i])<<8;
      break;
    case(3):
      for(i=0;i<len;i++)
	store24bit(out, i, ((uint32_t)((uint8_t*)in)[i])<<24);
      break;
    case(4):
      for(i=0;i<len;i++)
	((uint32_t*)out)[i]=((uint32_t)((uint8_t*)in)[i])<<24;
      break;
    }
    break;
  case(2):
    switch(outbps){
    case(1):
      for(i=0;i<len;i++)
	((uint8_t*)out)[i]=(uint8_t)((((uint16_t*)in)[i])>>8);
      break;
    case(3):
      for(i=0;i<len;i++)
	store24bit(out, i, ((uint32_t)((uint16_t*)in)[i])<<16);
      break;
    case(4):
      for(i=0;i<len;i++)
	((uint32_t*)out)[i]=((uint32_t)((uint16_t*)in)[i])<<16;
      break;
    }
    break;
  case(3):
    switch(outbps){
    case(1):
      for(i=0;i<len;i++)
	((uint8_t*)out)[i]=(uint8_t)(load24bit(in, i)>>24);
      break;
    case(2):
      for(i=0;i<len;i++)
	((uint16_t*)out)[i]=(uint16_t)(load24bit(in, i)>>16);
      break;
    case(4):
      for(i=0;i<len;i++)
	((uint32_t*)out)[i]=(uint32_t)load24bit(in, i);
      break;
    }
    break;      
  case(4):
    switch(outbps){
    case(1):
      for(i=0;i<len;i++)
	((uint8_t*)out)[i]=(uint8_t)((((uint32_t*)in)[i])>>24);
      break;
    case(2):
      for(i=0;i<len;i++)
	((uint16_t*)out)[i]=(uint16_t)((((uint32_t*)in)[i])>>16);
      break;
    case(3):
      for(i=0;i<len;i++)
        store24bit(out, i, ((uint32_t*)in)[i]);
      break;
    }
    break;      
  }
}

static void float2int(float* in, void* out, int len, int bps)
{
  register int i;
  switch(bps){
  case(1):
    for(i=0;i<len;i++)
      ((int8_t*)out)[i] = lrintf(127.0 * in[i]);
    break;
  case(2): 
    for(i=0;i<len;i++)
      ((int16_t*)out)[i] = lrintf(32767.0 * in[i]);
    break;
  case(3):
    for(i=0;i<len;i++)
      store24bit(out, i, lrintf(2147483647.0 * in[i]));
    break;
  case(4):
    for(i=0;i<len;i++)
      ((int32_t*)out)[i] = lrintf(2147483647.0 * in[i]);
    break;
  }	
}

static void int2float(void* in, float* out, int len, int bps)
{
  register int i;
  switch(bps){
  case(1):
    for(i=0;i<len;i++)
      out[i]=(1.0/128.0)*((int8_t*)in)[i];
    break;
  case(2):
    for(i=0;i<len;i++)
      out[i]=(1.0/32768.0)*((int16_t*)in)[i];
    break;
  case(3):
    for(i=0;i<len;i++)
      out[i]=(1.0/2147483648.0)*((int32_t)load24bit(in, i));
    break;
  case(4):
    for(i=0;i<len;i++)
      out[i]=(1.0/2147483648.0)*((int32_t*)in)[i];
    break;
  }	
}
