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
#include "../bswap.h"
#include "../libvo/fastmemcpy.h"

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

static af_data_t* play(struct af_instance_s* af, af_data_t* data);
static af_data_t* play_swapendian(struct af_instance_s* af, af_data_t* data);
static af_data_t* play_float_s16(struct af_instance_s* af, af_data_t* data);
static af_data_t* play_s16_float(struct af_instance_s* af, af_data_t* data);

// Convert from string to format
int af_str2fmt(char* str)
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

inline int af_fmt2bits(int format)
{
    return (format & AF_FORMAT_BITS_MASK)+8;
//    return (((format & AF_FORMAT_BITS_MASK)>>3)+1) * 8;
#if 0
    switch(format & AF_FORMAT_BITS_MASK)
    {
	case AF_FORMAT_8BIT: return 8;
	case AF_FORMAT_16BIT: return 16;
	case AF_FORMAT_24BIT: return 24;
	case AF_FORMAT_32BIT: return 32;
	case AF_FORMAT_48BIT: return 48;
    }
#endif
    return -1;
}

inline int af_bits2fmt(int bits)
{
    return (bits/8 - 1) << 3;
}

/* Convert format to str input str is a buffer for the 
   converted string, size is the size of the buffer */
char* af_fmt2str(int format, char* str, int size)
{
  int i=0;

  if (size < 1)
    return NULL;
  size--; // reserve one for terminating 0

  // Endianess
  if(AF_FORMAT_LE == (format & AF_FORMAT_END_MASK))
    i+=snprintf(str,size-i,"little-endian ");
  else
    i+=snprintf(str,size-i,"big-endian ");
  
  if(format & AF_FORMAT_SPECIAL_MASK){
    switch(format & AF_FORMAT_SPECIAL_MASK){
    case(AF_FORMAT_MU_LAW): 
      i+=snprintf(&str[i],size-i,"mu-law "); break;
    case(AF_FORMAT_A_LAW): 
      i+=snprintf(&str[i],size-i,"A-law "); break;
    case(AF_FORMAT_MPEG2): 
      i+=snprintf(&str[i],size-i,"MPEG-2 "); break;
    case(AF_FORMAT_AC3): 
      i+=snprintf(&str[i],size-i,"AC3 "); break;
    case(AF_FORMAT_IMA_ADPCM): 
      i+=snprintf(&str[i],size-i,"IMA-ADPCM "); break;
    default:
      printf("Unknown special\n");
    }
  }
  else{
    // Bits
    i+=snprintf(&str[i],size-i,"%d-bit ", af_fmt2bits(format));

    // Point
    if(AF_FORMAT_F == (format & AF_FORMAT_POINT_MASK))
      i+=snprintf(&str[i],size-i,"float ");
    else{
      // Sign
      if(AF_FORMAT_US == (format & AF_FORMAT_SIGN_MASK))
	i+=snprintf(&str[i],size-i,"unsigned ");
      else
	i+=snprintf(&str[i],size-i,"signed ");

      i+=snprintf(&str[i],size-i,"int ");
    }
  }
  // remove trailing space
  if (i > 0 && str[i - 1] == ' ')
    i--;
  str[i] = 0; // make sure it is 0 terminated.
  return str;
}

char *af_fmt2str_short(int format)
{
    switch(format)
    {
	// special
	case AF_FORMAT_MU_LAW: return "mulaw";
	case AF_FORMAT_A_LAW: return "alaw";
	case AF_FORMAT_MPEG2: return "mpeg2";
	case AF_FORMAT_AC3: return "ac3";
	case AF_FORMAT_IMA_ADPCM: return "imaadpcm";
	// ordinary
	case AF_FORMAT_U8: return "u8";
	case AF_FORMAT_S8: return "s8";
	case AF_FORMAT_U16_LE: return "u16le";
	case AF_FORMAT_U16_BE: return "u16be";
	case AF_FORMAT_S16_LE: return "s16le";
	case AF_FORMAT_S16_BE: return "s16be";
	case AF_FORMAT_U24_LE: return "u24le";
	case AF_FORMAT_U24_BE: return "u24be";
	case AF_FORMAT_S24_LE: return "s24le";
	case AF_FORMAT_S24_BE: return "s24be";
	case AF_FORMAT_U32_LE: return "u32le";
	case AF_FORMAT_U32_BE: return "u32be";
	case AF_FORMAT_S32_LE: return "s32le";
	case AF_FORMAT_S32_BE: return "s32be";
	case AF_FORMAT_FLOAT_LE: return "floatle";
	case AF_FORMAT_FLOAT_BE: return "floatbe";
    }
    return "??";
}

int af_str2fmt_short(char* str)
{
    int i;
    static struct {
	const char *name;
	const int format;
    } table[] = {
	{ "mulaw", AF_FORMAT_MU_LAW },
	{ "alaw", AF_FORMAT_A_LAW },
	{ "mpeg2", AF_FORMAT_MPEG2 },
	{ "ac3", AF_FORMAT_AC3 },
	{ "imaadpcm", AF_FORMAT_IMA_ADPCM },

	{ "u8", AF_FORMAT_U8 },
	{ "s8", AF_FORMAT_S8 },
	{ "u16le", AF_FORMAT_U16_LE },
	{ "u16be", AF_FORMAT_U16_BE },
	{ "u16ne", AF_FORMAT_U16_NE },
	{ "s16le", AF_FORMAT_S16_LE },
	{ "s16be", AF_FORMAT_S16_BE },
	{ "s16ne", AF_FORMAT_S16_NE },
	{ "u24le", AF_FORMAT_U24_LE },
	{ "u24be", AF_FORMAT_U24_BE },
	{ "u24ne", AF_FORMAT_U24_NE },
	{ "s24le", AF_FORMAT_S24_LE },
	{ "s24be", AF_FORMAT_S24_BE },
	{ "s24ne", AF_FORMAT_S24_NE },
	{ "u32le", AF_FORMAT_U32_LE },
	{ "u32be", AF_FORMAT_U32_BE },
	{ "u32ne", AF_FORMAT_U32_NE },
	{ "s32le", AF_FORMAT_S32_LE },
	{ "s32be", AF_FORMAT_S32_BE },
	{ "s32ne", AF_FORMAT_S32_NE },
	{ "floatle", AF_FORMAT_FLOAT_LE },
	{ "floatbe", AF_FORMAT_FLOAT_BE },
	{ "floatne", AF_FORMAT_FLOAT_NE },
	
	{ NULL, 0 }
    };
    
    for (i = 0; table[i].name; i++)
	if (!strcasecmp(str, table[i].name))
	    return table[i].format;

    return -1;
}

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
    if(af->data->format == ((af_data_t*)arg)->format && 
       af->data->bps == ((af_data_t*)arg)->bps)
      return AF_DETACH;

    // Check for errors in configuraton
    if((AF_OK != check_bps(((af_data_t*)arg)->bps)) ||
       (AF_OK != check_format(((af_data_t*)arg)->format)) ||
       (AF_OK != check_bps(af->data->bps)) ||
       (AF_OK != check_format(af->data->format)))
      return AF_ERROR;

    af_msg(AF_MSG_VERBOSE,"[format] Changing sample format from %s to %s\n",
	   af_fmt2str(((af_data_t*)arg)->format,buf1,256),
	   af_fmt2str(af->data->format,buf2,256));

    af->data->rate = ((af_data_t*)arg)->rate;
    af->data->nch  = ((af_data_t*)arg)->nch;
    af->mul.n      = af->data->bps;
    af->mul.d      = ((af_data_t*)arg)->bps;
    af_frac_cancel(&af->mul);
    
    af->play = play; // set default
    
    // look whether only endianess differences are there
    if ((af->data->format & ~AF_FORMAT_END_MASK) ==
	(data->format & ~AF_FORMAT_END_MASK))
    {
	af_msg(AF_MSG_VERBOSE,"[format] Accelerated endianess conversion only\n");
	af->play = play_swapendian;
    }
    if ((data->format == AF_FORMAT_FLOAT_NE) &&
	(af->data->format == AF_FORMAT_S16_NE))
    {
	af_msg(AF_MSG_VERBOSE,"[format] Accelerated %s to %s conversion\n",
	   af_fmt2str(((af_data_t*)arg)->format,buf1,256),
	   af_fmt2str(af->data->format,buf2,256));
	af->play = play_float_s16;
    }
    if ((data->format == AF_FORMAT_S16_NE) &&
	(af->data->format == AF_FORMAT_FLOAT_NE))
    {
	af_msg(AF_MSG_VERBOSE,"[format] Accelerated %s to %s conversion\n",
	   af_fmt2str(((af_data_t*)arg)->format,buf1,256),
	   af_fmt2str(af->data->format,buf2,256));
	af->play = play_s16_float;
    }
    return AF_OK;
  }
  case AF_CONTROL_COMMAND_LINE:{
    int format = af_str2fmt_short(arg);
    if(AF_OK != af->control(af,AF_CONTROL_FORMAT_FMT | AF_CONTROL_SET,&format))
      return AF_ERROR;
    return AF_OK;
  }
  case AF_CONTROL_FORMAT_FMT | AF_CONTROL_SET:{
    // Check for errors in configuraton
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
  if(af->data)
    free(af->data);
  af->setup = 0;  
}

static af_data_t* play_swapendian(struct af_instance_s* af, af_data_t* data)
{
  af_data_t*   l   = af->data;	// Local data
  af_data_t*   c   = data;	// Current working data
  int 	       len = c->len/c->bps; // Lenght in samples of current audio block

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
  int 	       len = c->len/4; // Lenght in samples of current audio block

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
  int 	       len = c->len/2; // Lenght in samples of current audio block

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
  int 	       len = c->len/c->bps; // Lenght in samples of current audio block

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
      si2us(l->audio,l->audio,len,l->bps);
  } else if((c->format & AF_FORMAT_SPECIAL_MASK) == AF_FORMAT_A_LAW) {
    from_alaw(c->audio, l->audio, len, l->bps, l->format&AF_FORMAT_POINT_MASK);
    if(AF_FORMAT_A_LAW == (l->format&AF_FORMAT_SPECIAL_MASK))
      to_alaw(l->audio, l->audio, len, 1, AF_FORMAT_SI);
    if((l->format&AF_FORMAT_SIGN_MASK) == AF_FORMAT_US)
      si2us(l->audio,l->audio,len,l->bps);
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
	si2us(l->audio,l->audio,len,l->bps);
      break;
    }
  } else {
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

static inline uint32_t load24bit(void* data, int pos) {
#if WORDS_BIGENDIAN
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
#if WORDS_BIGENDIAN
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
  case(3):
    for(i=0;i<len;i++)
      store24bit(out, i, (uint32_t)(INT_MAX+(int32_t)load24bit(in, i)));
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
  case(3):
    for(i=0;i<len;i++)
      store24bit(out, i, (int32_t)(INT_MIN+(uint32_t)load24bit(in, i)));
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
  case(3):
    for(i=0;i<len;i++)
      store24bit(out, i, (int32_t)lrintf(INT_MAX*((float*)in)[i]));
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
  case(3):
    for(i=0;i<len;i++)
      ((float*)out)[i]=(1.0/INT_MAX)*((float)((int32_t)load24bit(in, i)));
    break;
  case(4):
    for(i=0;i<len;i++)
      ((float*)out)[i]=(1.0/INT_MAX)*((float)((int32_t*)in)[i]);
    break;
  }	
}
