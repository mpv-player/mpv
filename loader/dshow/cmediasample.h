#ifndef DS_CMEDIASAMPLE_H
#define DS_CMEDIASAMPLE_H

#include "interfaces.h"
#include "guids.h"

typedef struct _CMediaSample CMediaSample;
struct _CMediaSample
{
    IMediaSample_vt* vt;
    DECLARE_IUNKNOWN();
    IMemAllocator* all;
    int size;
    int actual_size;
    char* block;
    char* own_block;
    int isPreroll;
    int isSyncPoint;
    int isDiscontinuity;
    LONGLONG time_start;
    LONGLONG time_end;
    AM_MEDIA_TYPE media_type;
    int type_valid;
    void ( *SetPointer) (CMediaSample* This, char* pointer);
    void ( *ResetPointer) (CMediaSample* This); // FIXME replace with Set & 0
};

CMediaSample* CMediaSampleCreate(IMemAllocator* allocator, int _size);
// called from allocator
void CMediaSample_Destroy(CMediaSample* This);

#endif /* DS_CMEDIASAMPLE_H */
