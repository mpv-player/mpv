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
  default: 
    //This can not happen .... 
    af_msg(AF_MSG_FATAL,"Unrecognized input audio format %i\n",ifmt);
    break;
  }
  return ofmt;
}
