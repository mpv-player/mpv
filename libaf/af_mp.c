#include "af.h"

/* Decodes the format from mplayer format to libaf format */
int af_format_decode(int ifmt)
{
  int ofmt = ~0;
  // Check input ifmt
  switch(ifmt){
  case(AFMT_U8):
    ofmt = AF_FORMAT_LE|AF_FORMAT_US; break;
  case(AFMT_S8):
    ofmt = AF_FORMAT_LE|AF_FORMAT_SI; break;
  case(AFMT_S16_LE):
    ofmt = AF_FORMAT_LE|AF_FORMAT_SI; break;
  case(AFMT_S16_BE):
    ofmt = AF_FORMAT_BE|AF_FORMAT_SI; break;
  case(AFMT_U16_LE):	
    ofmt = AF_FORMAT_LE|AF_FORMAT_US; break;
  case(AFMT_U16_BE):	
    ofmt = AF_FORMAT_BE|AF_FORMAT_US; break;
  case(AFMT_S32_LE):
    ofmt = AF_FORMAT_LE|AF_FORMAT_SI; break;
  case(AFMT_S32_BE):	
    ofmt = AF_FORMAT_BE|AF_FORMAT_SI; break;
  case(AFMT_IMA_ADPCM):
    ofmt = AF_FORMAT_IMA_ADPCM; break;
  case(AFMT_MU_LAW):
    ofmt = AF_FORMAT_MU_LAW; break;
  case(AFMT_A_LAW):
    ofmt = AF_FORMAT_A_LAW; break;
  case(AFMT_MPEG):
    ofmt = AF_FORMAT_MPEG2; break;
  case(AFMT_AC3):
    ofmt = AF_FORMAT_AC3; break;
  case(AFMT_FLOAT):
    ofmt = AF_FORMAT_F | AF_FORMAT_NE; break;
  default: 
    //This can not happen .... 
    af_msg(AF_MSG_FATAL,"Unrecognized input audio format %i\n",ifmt);
    break;
  }
  return ofmt;
}

/* Encodes the format from libaf format to mplayer (OSS) format */
int af_format_encode(void* fmtp)
{
  af_data_t* fmt=(af_data_t*) fmtp;
  switch(fmt->format&AF_FORMAT_SPECIAL_MASK){
  case 0: // PCM:
    if((fmt->format&AF_FORMAT_POINT_MASK)==AF_FORMAT_I){
      if((fmt->format&AF_FORMAT_SIGN_MASK)==AF_FORMAT_SI){
        // signed int PCM:
        switch(fmt->bps){
          case 1: return AFMT_S8;
          case 2: return (fmt->format&AF_FORMAT_LE) ? AFMT_S16_LE : AFMT_S16_BE;
          case 4: return (fmt->format&AF_FORMAT_LE) ? AFMT_S32_LE : AFMT_S32_BE;
	}
      } else {
        // unsigned int PCM:
        switch(fmt->bps){
          case 1: return AFMT_U8;
          case 2: return (fmt->format&AF_FORMAT_LE) ? AFMT_U16_LE : AFMT_U16_BE;
//          case 4: return (fmt->format&AF_FORMAT_LE) ? AFMT_U32_LE : AFMT_U32_BE;
	}
      }
    } else {
      // float PCM:
      return AFMT_FLOAT; // FIXME?
    }
    break;
  case AF_FORMAT_MU_LAW: return AFMT_MU_LAW;
  case AF_FORMAT_A_LAW:  return AFMT_A_LAW;
  case AF_FORMAT_MPEG2:  return AFMT_MPEG;
  case AF_FORMAT_AC3:    return AFMT_AC3;
  case AF_FORMAT_IMA_ADPCM: return AFMT_IMA_ADPCM;
  }
  return AFMT_S16_LE; // shouldn't happen
}

