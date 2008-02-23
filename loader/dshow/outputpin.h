#ifndef MPLAYER_OUTPUTPIN_H
#define MPLAYER_OUTPUTPIN_H

/* "output pin" - the one that connects to output of filter. */

#include "allocator.h"

typedef struct COutputMemPin COutputMemPin;
typedef struct COutputPin COutputPin;

/**
 Callback routine for copying samples from pin into filter
 \param pUserData pointer to user's data
 \param sample IMediaSample
*/
typedef  HRESULT STDCALL (*SAMPLEPROC)(void* pUserData,IMediaSample*sample);

struct COutputPin
{
    IPin_vt* vt;
    DECLARE_IUNKNOWN();
    COutputMemPin* mempin;
    AM_MEDIA_TYPE type;
    IPin* remote;
    SAMPLEPROC SampleProc;
    void* pUserData;
    void ( *SetNewFormat )(COutputPin*, const AM_MEDIA_TYPE* a);
};

COutputPin* COutputPinCreate(const AM_MEDIA_TYPE* amt,SAMPLEPROC SampleProc,void* pUserData);

#endif /* MPLAYER_OUTPUTPIN_H */
