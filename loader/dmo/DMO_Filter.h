#ifndef MPLAYER_DMO_FILTER_H
#define MPLAYER_DMO_FILTER_H

#include "dmo_guids.h"
#include "dmo_interfaces.h"

typedef struct DMO_Filter
{
    int m_iHandle;
    IDMOVideoOutputOptimizations* m_pOptim;
    IMediaObject* m_pMedia;
    IMediaObjectInPlace* m_pInPlace;
    AM_MEDIA_TYPE *m_pOurType, *m_pDestType;
} DMO_Filter;

typedef struct CMediaBuffer CMediaBuffer;

/**
 * Create DMO_Filter object - similar syntax as for DS_Filter
 */
DMO_Filter* DMO_FilterCreate(const char* dllname, const GUID* id,
			     AM_MEDIA_TYPE* in_fmt, AM_MEDIA_TYPE* out_fmt);
/**
 * Destroy DMO_Filter object - release all allocated resources
 */
void DMO_Filter_Destroy(DMO_Filter* This);


/**
 * Create IMediaBuffer object - to pass/receive data from DMO_Filter
 *
 * maxlen - maximum size for this buffer
 * mem - initial memory  0 - creates memory
 * len - initial size of used portion of the buffer
 * copy - make a local copy of data
 */
CMediaBuffer* CMediaBufferCreate(unsigned long maxlen, void* mem, unsigned long len, int copy);

#endif /* MPLAYER_DMO_FILTER_H */
