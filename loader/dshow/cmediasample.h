#ifndef DS_CMEDIASAMPLE_H
#define DS_CMEDIASAMPLE_H

#include "interfaces.h"
#include "guids.h"

struct CMediaSample: public IMediaSample
{
    IMemAllocator* all;
    int size;
    int actual_size;
    char* block;
    char* own_block;
    int refcount;
    int isPreroll;
    int isSyncPoint;
    AM_MEDIA_TYPE media_type;
    int type_valid;
    CMediaSample(IMemAllocator* allocator, long _size);
    ~CMediaSample();
    void SetPointer(char* pointer) { block = pointer; }
    void ResetPointer() { block = own_block; }
};

#endif /* DS_CMEDIASAMPLE_H */
