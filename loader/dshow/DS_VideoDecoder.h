#ifndef AVIFILE_DS_VIDEODECODER_H
#define AVIFILE_DS_VIDEODECODER_H

#ifndef NOAVIFILE_HEADERS
#include "videodecoder.h"
#else
#include "libwin32.h"
#endif
#include "DS_Filter.h"

typedef struct _DS_VideoDecoder
{
    IVideoDecoder iv;
    
    DS_Filter* m_pDS_Filter;
    AM_MEDIA_TYPE m_sOurType, m_sDestType;
    VIDEOINFOHEADER* m_sVhdr;
    VIDEOINFOHEADER* m_sVhdr2;
    int m_Caps;//CAPS m_Caps;                // capabilities of DirectShow decoder
    int m_iLastQuality;         // remember last quality as integer
    int m_iMinBuffers;
    int m_iMaxAuto;
    int m_bIsDivX;             // for speed
    int m_bIsDivX4;            // for speed
}DS_VideoDecoder;



int DS_VideoDecoder_GetCapabilities(DS_VideoDecoder *this);

DS_VideoDecoder * DS_VideoDecoder_Create(CodecInfo * info,  BITMAPINFOHEADER * format, int flip, int maxauto);

void DS_VideoDecoder_Destroy(DS_VideoDecoder *this);

void DS_VideoDecoder_StartInternal(DS_VideoDecoder *this);

void DS_VideoDecoder_StopInternal(DS_VideoDecoder *this);

int DS_VideoDecoder_DecodeInternal(DS_VideoDecoder *this, const void* src, int size, int is_keyframe, char* pImage);

/*
 * bits == 0   - leave unchanged
 */
//int SetDestFmt(DS_VideoDecoder * this, int bits = 24, fourcc_t csp = 0);
int DS_VideoDecoder_SetDestFmt(DS_VideoDecoder *this, int bits, fourcc_t csp);


int DS_VideoDecoder_SetDirection(DS_VideoDecoder *this, int d);

HRESULT DS_VideoDecoder_GetValue(DS_VideoDecoder *this, const char* name, int* value);

HRESULT DS_VideoDecoder_SetValue(DS_VideoDecoder *this, const char* name, int value);


#endif /* AVIFILE_DS_VIDEODECODER_H */
