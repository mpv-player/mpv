#ifndef DS_OUTPUTPIN_H
#define DS_OUTPUTPIN_H

/* "output pin" - the one that connects to output of filter. */

#include "allocator.h"

typedef struct _COutputMemPin COutputMemPin;
typedef struct _COutputPin COutputPin;

struct _COutputPin
{
    IPin_vt* vt;
    DECLARE_IUNKNOWN();
    COutputMemPin* mempin;
    AM_MEDIA_TYPE type;
    IPin* remote;
    void ( *SetFramePointer )(COutputPin*, char** z);
    void ( *SetPointer2 )(COutputPin*, char* p);
    void ( *SetFrameSizePointer )(COutputPin*, long* z);
    void ( *SetNewFormat )(COutputPin*, const AM_MEDIA_TYPE* a);
};

COutputPin* COutputPinCreate(const AM_MEDIA_TYPE* vhdr);

#endif /* DS_OUTPUTPIN_H */
