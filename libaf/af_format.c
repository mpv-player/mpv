/* This audio output filter changes the format of a data block. Valid
   formats are: AFMT_U8, AFMT_S8, AFMT_S16_LE, AFMT_S16_BE
   AFMT_U16_LE, AFMT_U16_BE, AFMT_S32_LE and AFMT_S32_BE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <limits.h>

#include "af.h"

// Integer to float conversion through lrintf()
#ifdef HAVE_LRINTF
#define __USE_ISOC99 1
#include <math.h>
#else
#define lrintf(x) ((int)(x))
#endif

/* Functions used by play to convert the input audio to the correct
   format */

/* The below includes retrives functions for converting to and from
   ulaw and alaw */ 
#include "af_format_ulaw.c"
#include "af_format_alaw.c"

// Switch endianess
static void endian(void* in, void* out, int len, int bps);
// From singed to unsigned
static void si2us(void* in, void* out, int len, int bps);
// From unsinged to signed
static void us2si(void* in, void* out, int len, int bps);
// Change the number of bits per sample
static void change_bps(void* in, void* out, int len, int inbps, int outbps);
// From float to int signed
static void float2int(void* in, void* out, int len, int bps);
// From signed int to float
static void int2float(void* in, void* out, int len, int bps);

// Convert from string to format
static int str2fmt(char* str)
{
  int format=0;
  // Scan for endianess
  if(strstr(str,"be") || strstr(str,"BE"))
    format |= AF_FORMAT_BE;
  else if(strstr(str,"le") || strstr(str,"LE"))
    format |= AF_FORMAT_LE;
  else
    format |= AF_FORMAT_NE;    

  // Scan for special formats
  if(strstr(str,"mulaw") || strstr(str,"MULAW")){
    format |= AF_FORMAT_MU_LAW; return format;
  }
  if(strstr(str,"alaw") || strstr(str,"ALAW")){
    format |= AF_FORMAT_A_LAW; return format;
  }
  if(strstr(str,"ac3") || strstr(str,"AC3")){
    format |= AF_FORMAT_AC3; return format;
  }
  if(strstr(str,"mpeg2") || strstr(str,"MPEG2")){
    format |= AF_FORMAT_MPEG2; return format;
  }
  if(strstr(str,"imaadpcm") || strstr(str,"IMAADPCM")){
    format |= AF_FORMAT_IMA_ADPCM; return format;
  }
  
  // Scan for int/float
  if(strstr(str,"float") || strstr(str,"FLOAT")){
    format |= AF_FORMAT_F; return format;
  }
  else
    format |= AF_FORMAT_I;

  // Scan for signed/unsigned
  if(strstr(str,"unsigned") || strstr(str,"UNSIGNED"))
    format |= AF_FORMAT_US; 
  else
    format |= AF_FORMAT_SI;
  
  return format;
}

/* Convert format to str input str is a buffer for the 
   converted string, size is the size of the buffer */
char* fmt2str(int format, char* str, size_t size)
{
  int i=0;
  // Print endinaness
  if(AF_FORMAT_LE == (format & AF_FORMAT_END_MASK))
    i+=snprintf(str,size,"little endian ");
  else
    i+=snprintf(str,size,"big endian ");
  
  if(format & AF_FORMAT_SPECIAL_MASK){
    switch(format & AF_FORMAT_SPECIAL_MASK){
    case(AF_FORMAT_MU_LAW): 
      i+=snprintf(&str[i],size-i,"mu law "); break;
    case(AF_FORMAT_A_LAW): 
      i+=snprintf(&str[i],size-i,"A law "); break;
    case(AF_FORMAT_MPEG2): 
      i+=snprintf(&str[i],size-i,"MPEG 2 "); break;
    case(AF_FORMAT_AC3): 
      i+=snprintf(&str[i],size-i,"AC3 "); break;
    }
  }
  else{
    // Point
    if(AF_FORMAT_F == (format & AF_FORMAT_POINT_MASK))
      i+=snprintf(&str[i],size,"float ");
    else{
      // Sign
      if(AF_FORMAT_US == (format & AF_FORMAT_SIGN_MASK))
	i+=snprintf(&str[i],size-i,"unsigned ");
      else
	i+=snprintf(&str[i],size-i,"signed ");

      i+=snprintf(&str[i],size,"int ");
    }
  }
  return str;
}

// Helper functions to check sanity for input arguments

// Sanity check for bytes per sample
int check_bps(int bps)
{
  if(bps != 4 && bps != 2 && bps != 1){
    af_msg(AF_MSG_ERROR,"[format] The number of bytes per sample" 
	   " must be 1, 2 or 4. Current value is %i \n",bps);
    return AF_ERROR;
  }
  return AF_OK;
}

// Check for unsupported formats
int check_format(int format)
{
  char buf[256];
  switch(format & AF_FORMAT_SPECIAL_MASK){
  case(AF_FORMAT_MPEG2): 
  case(AF_FORMAT_AC3):
    af_msg(AF_MSG_ERROR,"[format] Sample format %s not yet supported \n",
	 fmt2str(format,buf,255)); 
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
    // Make sure this filter isn't redundant 
    if(af->data->format == ((af_data_t*)arg)->format && 
       af->data->bps == ((af_data_t*)arg)->bps)
      return AF_DETACH;

    // Check for errors in configuraton
    if((AF_OK != check_bps(((af_data_t*)arg)->bps)) ||
       (AF_OK != check_format(((af_data_t*)arg)->format)) ||
       (AF_OK != check_bps(af->data->bps)) ||
       (AF_OK != check_format(af->data->format)))
      return AF_ERROR;

    af_msg(AF_MSG_VERBOSE,"[format] Changing sample format from %ibit %sto %ibit %s \n",
	   ((af_data_t*)arg)->bps*8,fmt2str(((af_data_t*)arg)->format,buf1,255),
	   af->data->bps*8,fmt2str(af->data->format,buf2,255));

    af->data->rate = ((af_data_t*)arg)->rate;
    af->data->nch  = ((af_data_t*)arg)->nch;
    af->mul.n      = af->data->bps;
    af->mul.d      = ((af_data_t*)arg)->bps;
    return AF_OK;
  }
  case AF_CONTROL_COMMAND_LINE:{
    int bps = 2;
    int format = AF_FORMAT_NE;
    char str[256];
    str[0] = '\0';
    sscanf((char*)arg,"%i:%s",&bps,str);
    // Convert string to format
    format = str2fmt(str);
    
    // Automatic correction of errors
    switch(format & AF_FORMAT_SPECIAL_MASK){
    case(AF_FORMAT_A_LAW):
    case(AF_FORMAT_MU_LAW): 
      bps=1; break;
    case(AF_FORMAT_AC3):
      bps=4; break; // I think
    }
    if(AF_FORMAT_F == (format & AF_FORMAT_POINT_MASK))
      bps=4;
    
    if((AF_OK != af->control(af,AF_CONTROL_FORMAT_BPS | AF_CONTROL_SET,&bps)) ||
       (AF_OK != af->control(af,AF_CONTROL_FORMAT_FMT | AF_CONTROL_SET,&format)))
      return AF_ERROR;
    return AF_OK;
  }
  case AF_CONTROL_FORMAT_BPS | AF_CONTROL_SET:
    // Reinit must be called after this function has been called
    
    // Check for errors in configuraton
    if(AF_OK != check_bps(*(int*)arg))
      return AF_ERROR;

    af->data->bps = *(int*)arg;
    return AF_OK;
  case AF_CONTROL_FORMAT_FMT | AF_CONTROL_SET:
    // Reinit must be called after this function has been called

    // Check for errors in configuraton
    if(AF_OK != check_format(*(int*)arg))
      return AF_ERROR;

    af->data->format = *(int*)arg;
    return AF_OK;
  }
  return AF_UNKNOWN;
}

// Deallocate memory 
static void uninit(struct af_instance_s* af)
{
  if(af->data)
    free(af->data);
  (int)af->setup = 0;  
}

// Filter data through filter
static af_data_t* play(struct af_instance_s* af, af_data_t* data)
{
  af_data_t*   l   = af->data;	// Local data
  af_data_t*   c   = data;	// Current working data
  int 	       len = c->len/c->bps; // Lenght in samples of current audio block

  if(AF_OK != RESIZE_LOCAL_BUFFER(af,data))
    return NULL;

  // Change to cpu native endian format
  if((c->format&AF_FORMAT_END_MASK)!=AF_FORMAT_NE)
    endian(c->audio,c->audio,len,c->bps);

  // Conversion table
  switch(c->format & ~AF_FORMAT_END_MASK){
  case(AF_FORMAT_MU_LAW):
    from_ulaw(c->audio, l->audio, len, l->bps, l->format&AF_FORMAT_POINT_MASK);
    if(AF_FORMAT_A_LAW == (l->format&AF_FORMAT_SPECIAL_MASK))
      to_ulaw(l->audio, l->audio, len, 1, AF_FORMAT_SI);
    if((l->format&AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
      si2us(l->audio,l->audio,len,l->bps);
    break;
  case(AF_FORMAT_A_LAW):
    from_alaw(c->audio, l->audio, len, l->bps, l->format&AF_FORMAT_POINT_MASK);
    if(AF_FORMAT_A_LAW == (l->format&AF_FORMAT_SPECIAL_MASK))
      to_alaw(l->audio, l->audio, len, 1, AF_FORMAT_SI);
    if((l->format&AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
      si2us(l->audio,l->audio,len,l->bps);
    break;
  case(AF_FORMAT_F):
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
	si2us(l->audio,l->audio,len,l->bps);
      break;
    }
    break;
  default:
    // Input must be int
    
    // Change signed/unsigned
    if((c->format&AF_FORMAT_SIGN_MASK) != (l->format&AF_FORMAT_SIGN_MASK)){
      if((c->format&AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
	us2si(c->audio,c->audio,len,c->bps);
      else
	si2us(c->audio,c->audio,len,c->bps); 
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
	memcpy(l->audio,c->audio,len*c->bps);
      break;
    }
    break;
  }

  // Switch from cpu native endian to the correct endianess 
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
static int open(af_instance_t* af){
  af->control=control;
  af->uninit=uninit;
  af->play=play;
  af->mul.n=1;
  af->mul.d=1;
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
  open
};

// Function implementations used by play
static void endian(void* in, void* out, int len, int bps)
{
  register int i;
  switch(bps){
    case(2):{
      register uint16_t s;
      for(i=0;i<len;i++){
	s=((uint16_t*)in)[i];
	((uint16_t*)out)[i]=(uint16_t)(((s&0x00FF)<<8) | (s&0xFF00)>>8);
      }
      break;
    }
    case(4):{
      register uint32_t s;
      for(i=0;i<len;i++){
	s=((uint32_t*)in)[i];
	((uint32_t*)out)[i]=(uint32_t)(((s&0x000000FF)<<24) | 
				       ((s&0x0000FF00)<<8)  |
				       ((s&0x00FF0000)>>8)  |
				       ((s&0xFF000000)>>24));
      }
      break;
    }
  }
}

static void si2us(void* in, void* out, int len, int bps)
{
  register int i;
  switch(bps){
  case(1):
    for(i=0;i<len;i++)
      ((uint8_t*)out)[i]=(uint8_t)(SCHAR_MAX+((int)((int8_t*)in)[i]));
    break;
  case(2):
    for(i=0;i<len;i++)
      ((uint16_t*)out)[i]=(uint16_t)(SHRT_MAX+((int)((int16_t*)in)[i]));
    break;
  case(4):
    for(i=0;i<len;i++)
      ((uint32_t*)out)[i]=(uint32_t)(INT_MAX+((int32_t*)in)[i]);
    break;
  }
}

static void us2si(void* in, void* out, int len, int bps)
{
  register int i;
  switch(bps){
  case(1):
    for(i=0;i<len;i++)
      ((int8_t*)out)[i]=(int8_t)(SCHAR_MIN+((int)((uint8_t*)in)[i]));
    break;
  case(2):
    for(i=0;i<len;i++)
      ((int16_t*)out)[i]=(int16_t)(SHRT_MIN+((int)((uint16_t*)in)[i]));
    break;
  case(4):
    for(i=0;i<len;i++)
      ((int32_t*)out)[i]=(int32_t)(INT_MIN+((uint32_t*)in)[i]);
    break;
  }	
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
    case(4):
      for(i=0;i<len;i++)
	((uint32_t*)out)[i]=((uint32_t)((uint16_t*)in)[i])<<16;
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
    }
    break;      
  }
}

static void float2int(void* in, void* out, int len, int bps)
{
  register int i;
  switch(bps){
  case(1):
    for(i=0;i<len;i++)
      ((int8_t*)out)[i]=(int8_t)lrintf(SCHAR_MAX*((float*)in)[i]);
    break;
  case(2): 
    for(i=0;i<len;i++)
      ((int16_t*)out)[i]=(int16_t)lrintf(SHRT_MAX*((float*)in)[i]);
    break;
  case(4):
    for(i=0;i<len;i++)
      ((int32_t*)out)[i]=(int32_t)lrintf(INT_MAX*((float*)in)[i]);
    break;
  }	
}

static void int2float(void* in, void* out, int len, int bps)
{
  register int i;
  switch(bps){
  case(1):
    for(i=0;i<len;i++)
      ((float*)out)[i]=(1.0/SCHAR_MAX)*((float)((int8_t*)in)[i]);
    break;
  case(2):
    for(i=0;i<len;i++)
      ((float*)out)[i]=(1.0/SHRT_MAX)*((float)((int16_t*)in)[i]);
    break;
  case(4):
    for(i=0;i<len;i++)
      ((float*)out)[i]=(1.0/INT_MAX)*((float)((int32_t*)in)[i]);
    break;
  }	
}
