#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "afmt.h"

char *audio_out_format_name(int format)
{
    switch (format)
    {
	case AFMT_MU_LAW:
	    return("Mu-Law");
	case AFMT_A_LAW:
	    return("A-Law");
	case AFMT_IMA_ADPCM:
	    return("Ima-ADPCM");
	case AFMT_S8:
	    return("Signed 8-bit");
	case AFMT_U8:
	    return("Unsigned 8-bit");
	case AFMT_U16_LE:
	    return("Unsigned 16-bit (Little-Endian)");
	case AFMT_U16_BE: 
	    return("Unsigned 16-bit (Big-Endian)");
	case AFMT_S16_LE:
	    return("Signed 16-bit (Little-Endian)");
	case AFMT_S16_BE:
	    return("Signed 16-bit (Big-Endian)");
	case AFMT_MPEG:
	    return("MPEG (2) audio");
	case AFMT_AC3:
	    return("AC3");
	case AFMT_U32_LE:
	    return("Unsigned 32-bit (Little-Endian)");
	case AFMT_U32_BE:
	    return("Unsigned 32-bit (Big-Endian)");
	case AFMT_S32_LE:
	    return("Signed 32-bit (Little-Endian)");
	case AFMT_S32_BE:
	    return("Signed 32-bit (Big-Endian)");
	case AFMT_U24_LE:
	    return("Unsigned 24-bit (Little-Endian)");
	case AFMT_U24_BE:
	    return("Unsigned 24-bit (Big-Endian)");
	case AFMT_S24_LE:
	    return("Signed 24-bit (Little-Endian)");
	case AFMT_S24_BE:
	    return("Signed 24-bit (Big-Endian)");
	case AFMT_FLOAT:
	    return("Floating Point");
    }
    return("Unknown");
}

// return number of bits for 1 sample in one channel, or 8 bits for compressed
int audio_out_format_bits(int format){
    switch (format)
    {
	case AFMT_S16_LE:
	case AFMT_S16_BE:
	case AFMT_U16_LE:
	case AFMT_U16_BE: 
	return 16;//16 bits

	case AFMT_S32_LE:
	case AFMT_S32_BE:
	case AFMT_U32_LE:
	case AFMT_U32_BE:
	case AFMT_FLOAT:
	return 32;

	case AFMT_S24_LE:
	case AFMT_S24_BE:
	case AFMT_U24_LE:
	case AFMT_U24_BE:
	return 24;
	
	case AFMT_MU_LAW:
	case AFMT_A_LAW:
	case AFMT_IMA_ADPCM:
	case AFMT_S8:
	case AFMT_U8:
	case AFMT_MPEG:
	case AFMT_AC3:
	default:
	    return 8;//default 1 byte
	
    }
    return 8;
}
