#ifndef MPLAYER_CMEDIASAMPLE_H
#define MPLAYER_CMEDIASAMPLE_H

#include "interfaces.h"
#include "guids.h"

typedef struct CMediaSample CMediaSample;
struct CMediaSample
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

CMediaSample* CMediaSampleCreate(IMemAllocator* allocator, int size);
// called from allocator
void CMediaSample_Destroy(CMediaSample* This);

#endif /* MPLAYER_CMEDIASAMPLE_H */
