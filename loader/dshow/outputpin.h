#ifndef DS_OUTPUTPIN_H
#define DS_OUTPUTPIN_H

/* "output pin" - the one that connects to output of filter. */

#include "interfaces.h"
#include "guids.h"
#include "allocator.h"

struct COutputPin;

struct COutputMemPin : public IMemInputPin
{
    char** frame_pointer;
    long* frame_size_pointer;
    MemAllocator* pAllocator;
    COutputPin* parent;
};

struct COutputPin : public IPin
{
    COutputMemPin* mempin;
    int refcount;
    AM_MEDIA_TYPE type;
    IPin* remote;
    COutputPin(const AM_MEDIA_TYPE& vhdr);
    ~COutputPin();
    void SetFramePointer(char** z) { mempin->frame_pointer = z; }
    void SetPointer2(char* p) {
	if (mempin->pAllocator)
	    mempin->pAllocator->SetPointer(p);
    }
    void SetFrameSizePointer(long* z) { mempin->frame_size_pointer = z; }
    void SetNewFormat(const AM_MEDIA_TYPE& a) { type = a; }
};

#endif /* DS_OUTPUTPIN_H */
