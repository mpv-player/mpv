/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#include "cmediasample.h"
#include "mediatype.h"
#include "loader/wine/winerror.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * currently hack to make some extra room for DS Acel codec which
 * seems to overwrite allocated memory - FIXME better later
 * check the buffer allocation
 */
static const int SAFETY_ACEL = 1024;

/**
 * \brief IPin::QueryInternalConnections (retries pin's internal connections)
 *
 * \param[in]     This  pointer to IPin interface
 * \param[out]    apPin Array that receives pins, internally connected to this
 * \param[in,out] nPint Size of an array
 *
 * \return S_OK - success
 * \return S_FALSE - pin rejects media type
 * \return E_NOTIMPL - not implemented
 *
 */
static long STDCALL CMediaSample_QueryInterface(IUnknown* This,
						/* [in] */ const GUID* iid,
						/* [iid_is][out] */ void **ppv)
{
    Debug printf("CMediaSample_QueryInterface(%p) called\n", This);
    if (!ppv)
	return E_INVALIDARG;
    if (memcmp(iid, &IID_IUnknown, sizeof(*iid)) == 0)
    {
	*ppv = (void*)This;
	((IMediaSample*) This)->vt->AddRef(This);
	return 0;
    }
    if (memcmp(iid, &IID_IMediaSample, sizeof(*iid)) == 0)
    {
	*ppv = (void*)This;
	((IMediaSample*) This)->vt->AddRef(This);
	return 0;
    }
    return E_NOINTERFACE;
}

/**
 * \brief IUnknown::AddRef (increases reference counter for interface)
 *
 * \param[in]  This pointer to IUnknown class
 *
 * \return new value of reference counter
 *
 * \remarks
 * Return value should be used only for debug purposes
 *
 */
static long STDCALL CMediaSample_AddRef(IUnknown* This)
{
    Debug printf("CMediaSample_AddRef(%p) called\n", This);
    ((CMediaSample*)This)->refcount++;
    return 0;
}

/**
 * \brief CMediaSample destructor
 *
 * \param[in] This pointer to CMediaSample object
 *
 */
void CMediaSample_Destroy(CMediaSample* This)
{

    Debug printf("CMediaSample_Destroy(%p) called (ref:%d)\n", This, This->refcount);
    free(This->vt);
    free(This->own_block);
    if(((CMediaSample*)This)->type_valid)
	FreeMediaType(&(This->media_type));
    free(This);
}

/**
 * \brief IUnknown::Release (desreases reference counter for interface)
 *
 * \param[in]  This pointer to IUnknown class
 *
 * \return new value of reference counter
 *
 * \remarks
 * When reference counter reaches zero calls destructor
 * Return value should be used only for debug purposes
 *
 */
static long STDCALL CMediaSample_Release(IUnknown* This)
{
    CMediaSample* parent = (CMediaSample*)This;
    Debug printf("CMediaSample_Release(%p) called  (new ref:%d)\n",
		 This, ((CMediaSample*)This)->refcount-1);

    if (--((CMediaSample*) This)->refcount == 0)
    {
	parent->all->vt->ReleaseBuffer((IMemAllocator*)(parent->all),
				       (IMediaSample*)This);
    }
    return 0;
}

/**
 * \brief IMediaSample::GetPointer (retrieves a read/write pointer to the media sample's buffer)
 *
 * \param[in] This pointer to CMediaSample object
 * \param[out] address of variable that receives pointer to sample's buffer
 *
 * \return S_OK success
 * \return apropriate error otherwise
 *
 * \note The calles should not free or reallocate buffer
 *
 */
static HRESULT STDCALL CMediaSample_GetPointer(IMediaSample* This,
					       /* [out] */ BYTE** ppBuffer)
{
    Debug printf("CMediaSample_GetPointer(%p) called -> %p, size: %d  %d\n", This, ((CMediaSample*) This)->block, ((CMediaSample*)This)->actual_size, ((CMediaSample*)This)->size);
    if (!ppBuffer)
	return E_INVALIDARG;
    *ppBuffer = (BYTE*) ((CMediaSample*) This)->block;
    return 0;
}

/**
 * \brief IMediaSample::GetSize (retrieves a size of buffer in bytes)
 *
 * \param[in] This pointer to CMediaSample object
 *
 * \return size of buffer in bytes
 *
 */
static long STDCALL CMediaSample_GetSize(IMediaSample * This)
{
    Debug printf("CMediaSample_GetSize(%p) called -> %d\n", This, ((CMediaSample*) This)->size);
    return ((CMediaSample*) This)->size;
}

/**
 * \brief IMediaSample::GetTime (retrieves a stream time at wich sample sould start and finish)
 *
 * \param[in] This pointer to CMediaSample object
 * \param[out] pTimeStart pointer to variable that receives start time
 * \param[out] pTimeEnd pointer to variable that receives end time
 *
 * \return S_OK success
 * \return VFW_E_NO_STOP_TIME The sample has valid start time, but no stop time
 * \return VFW_E_SAMPLE_TIME_NOT_SET The sample is not time-stamped
 *
 * \remarks
 * Both values are relative to stream time
 *
 */
static HRESULT STDCALL CMediaSample_GetTime(IMediaSample * This,
					    /* [out] */ REFERENCE_TIME *pTimeStart,
					    /* [out] */ REFERENCE_TIME *pTimeEnd)
{
    Debug printf("CMediaSample_GetTime(%p) called (UNIMPLEMENTED)\n", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediaSample::SetTime (sets a stream time at wich sample sould start and finish)
 *
 * \param[in] This pointer to CMediaSample object
 * \param[out] pTimeStart pointer to variable that contains start time
 * \param[out] pTimeEnd pointer to variable that contains end time
 *
 * \return S_OK success
 * \return apropriate error otherwise
 *
 * \remarks
 * Both values are relative to stream time
 * To invalidate the stream times set pTimeStart and pTimeEnd to NULL. this will cause
 * IMEdiaSample::GetTime to return VFW_E_SAMPLE_TIME_NOT_SET
 *
 */
static HRESULT STDCALL CMediaSample_SetTime(IMediaSample * This,
					    /* [in] */ REFERENCE_TIME *pTimeStart,
					    /* [in] */ REFERENCE_TIME *pTimeEnd)
{
    Debug printf("CMediaSample_SetTime(%p) called (UNIMPLEMENTED)\n", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediaSample::IsSyncPoint (determines if start of this sample is sync point)
 *
 * \param[in] This pointer to CMediaSample object
 *
 * \return S_OK start of this sample is sync point
 * \return S_FALSE start of this sample is not sync point
 *
 * \remarks
 * If bTemporalCompression of AM_MEDIA_TYPE is FALSE, all samples are sync points.
 *
 */
static HRESULT STDCALL CMediaSample_IsSyncPoint(IMediaSample * This)
{
    Debug printf("CMediaSample_IsSyncPoint(%p) called\n", This);
    if (((CMediaSample*)This)->isSyncPoint)
	return 0;
    return 1;
}

/**
 * \brief IMediaSample::SetSyncPoint (specifies if start of this sample is sync point)
 *
 * \param[in] This pointer to CMediaSample object
 * \param[in] bIsSyncPoint specifies whether this is sync point or not
 *
 * \return S_OK success
 * \return apropriate error code otherwise
 *
 */
static HRESULT STDCALL CMediaSample_SetSyncPoint(IMediaSample * This,
						 long bIsSyncPoint)
{
    Debug printf("CMediaSample_SetSyncPoint(%p) called\n", This);
    ((CMediaSample*)This)->isSyncPoint = bIsSyncPoint;
    return 0;
}

/**
 * \brief IMediaSample::IsPreroll (determines if this sample is preroll sample)
 *
 * \param[in] This pointer to CMediaSample object
 *
 * \return S_OK if this sample is preroll sample
 * \return S_FALSE if this sample is not preroll sample
 *
 * \remarks
 * Preroll samples are processed but  not displayed. They are lokated in media stream
 * before displayable samples.
 *
 */
static HRESULT STDCALL CMediaSample_IsPreroll(IMediaSample * This)
{
    Debug printf("CMediaSample_IsPreroll(%p) called\n", This);

    if (((CMediaSample*)This)->isPreroll)
	return 0;//S_OK

    return 1;//S_FALSE
}

/**
 * \brief IMediaSample::SetPreroll (specifies if this sample is preroll sample)
 *
 * \param[in] This pointer to CMediaSample object
 * \param[in] bIsPreroll specifies whether this sample is preroll sample or not
 *
 * \return S_OK success
 * \return apropriate error code otherwise
 *
 * \remarks
 * Preroll samples are processed but  not displayed. They are lokated in media stream
 * before displayable samples.
 *
 */
static HRESULT STDCALL CMediaSample_SetPreroll(IMediaSample * This,
					       long bIsPreroll)
{
    Debug printf("CMediaSample_SetPreroll(%p) called\n", This);
    ((CMediaSample*)This)->isPreroll=bIsPreroll;
    return 0;
}

/**
 * \brief IMediaSample::GetActualDataLength (retrieves the length of valid data in the buffer)
 *
 * \param[in] This pointer to CMediaSample object
 *
 * \return length of valid data in buffer in bytes
 *
 */
static long STDCALL CMediaSample_GetActualDataLength(IMediaSample* This)
{
    Debug printf("CMediaSample_GetActualDataLength(%p) called -> %d\n", This, ((CMediaSample*)This)->actual_size);
    return ((CMediaSample*)This)->actual_size;
}

/**
 * \brief IMediaSample::SetActualDataLength (specifies the length of valid data in the buffer)
 *
 * \param[in] This pointer to CMediaSample object
 * \param[in]  __MIDL_0010 length of data in sample in bytes
 *
 * \return S_OK success
 * \return VFW_E_BUFFER_OVERFLOW length specified by parameter is larger than buffer size
 *
 */
static HRESULT STDCALL CMediaSample_SetActualDataLength(IMediaSample* This,
							long __MIDL_0010)
{
    CMediaSample* cms = (CMediaSample*)This;
    Debug printf("CMediaSample_SetActualDataLength(%p, %ld) called\n", This, __MIDL_0010);

    if (__MIDL_0010 > cms->size)
    {
        char* c = cms->own_block;
	Debug printf("CMediaSample - buffer overflow   %ld %d   %p %p\n",
		     __MIDL_0010, ((CMediaSample*)This)->size, cms->own_block, cms->block);
	cms->own_block = realloc(cms->own_block, (size_t) __MIDL_0010 + SAFETY_ACEL);
	if (c == cms->block)
	    cms->block = cms->own_block;
        cms->size = __MIDL_0010;
    }
    cms->actual_size = __MIDL_0010;
    return 0;
}

/**
 * \brief IMediaSample::GetMediaType (retrieves media type, if it changed from previous sample)
 *
 * \param[in] This pointer to CMediaSample object
 * \param[out] ppMediaType address of variable that receives pointer to AM_MEDIA_TYPE.
 *
 * \return S_OK success
 * \return S_FALSE Media type was not changed from previous sample
 * \return E_OUTOFMEMORY Insufficient memory
 *
 * \remarks
 * If media type is not changed from previous sample, ppMediaType is null
 * If method returns S_OK caller should free memory allocated for structure
 * including pbFormat block
 */
static HRESULT STDCALL CMediaSample_GetMediaType(IMediaSample* This,
						 AM_MEDIA_TYPE** ppMediaType)
{
    AM_MEDIA_TYPE* t;
    Debug printf("CMediaSample_GetMediaType(%p) called\n", This);
    if(!ppMediaType)
	return E_INVALIDARG;
    if(!((CMediaSample*)This)->type_valid)
    {
	*ppMediaType=0;
	return 1;
    }

    t = &((CMediaSample*)This)->media_type;
    //    if(t.pbFormat)free(t.pbFormat);
    *ppMediaType=CreateMediaType(t);
    //    *ppMediaType=0; //media type was not changed
    return 0;
}

/**
 * \brief IMediaType::SetMediaType (specifies media type for sample)
 *
 * \param[in] This pointer to CMediaSample object
 * \param[in] pMediaType pointer to AM_MEDIA_TYPE specifies new media type
 *
 * \return S_OK success
 * \return E_OUTOFMEMORY insufficient memory
 *
 */
static HRESULT STDCALL CMediaSample_SetMediaType(IMediaSample * This,
						 AM_MEDIA_TYPE *pMediaType)
{
    AM_MEDIA_TYPE* t;
    Debug printf("CMediaSample_SetMediaType(%p) called\n", This);
    if (!pMediaType)
	return E_INVALIDARG;
    t = &((CMediaSample*)This)->media_type;
    if(((CMediaSample*)This)->type_valid)
	FreeMediaType(t);
    CopyMediaType(t,pMediaType);
    ((CMediaSample*) This)->type_valid=1;

    return 0;
}

/**
 * \brief IMediaSample::IsDiscontinuity (determines if this sample represents data break
 *        in stream)
 *
 * \param[in] This pointer to CMediaSample object
 *
 * \return S_OK if this sample is break in data stream
 * \return S_FALSE otherwise
 *
 * \remarks
 * Discontinuity occures when filter seeks to different place in the stream or when drops
 * samples.
 *
 */
static HRESULT STDCALL CMediaSample_IsDiscontinuity(IMediaSample * This)
{
    Debug printf("CMediaSample_IsDiscontinuity(%p) called\n", This);
    return ((CMediaSample*) This)->isDiscontinuity;
}

/**
 * \brief IMediaSample::IsDiscontinuity (specifies whether this sample represents data break
 *        in stream)
 *
 * \param[in] This pointer to CMediaSample object
 * \param[in] bDiscontinuity if TRUE this sample represents discontinuity with previous sample
 *
 * \return S_OK success
 * \return apropriate error code otherwise
 *
 */
static HRESULT STDCALL CMediaSample_SetDiscontinuity(IMediaSample * This,
						     long bDiscontinuity)
{
    Debug printf("CMediaSample_SetDiscontinuity(%p) called (%ld)\n", This, bDiscontinuity);
    ((CMediaSample*) This)->isDiscontinuity = bDiscontinuity;
    return 0;
}

/**
 * \brief IMediaSample::GetMediaTime (retrieves the media times of this sample)
 *
 * \param[in] This pointer to CMediaSample object
 * \param[out] pTimeStart pointer to variable that receives start time
 * \param[out] pTimeEnd pointer to variable that receives end time
 *
 * \return S_OK success
 * \return VFW_E_MEDIA_TIME_NOT_SET The sample is not time-stamped
 *
 */
static HRESULT STDCALL CMediaSample_GetMediaTime(IMediaSample * This,
						 /* [out] */ LONGLONG *pTimeStart,
						 /* [out] */ LONGLONG *pTimeEnd)
{
    Debug printf("CMediaSample_GetMediaTime(%p) called\n", This);
    if (pTimeStart)
	*pTimeStart = ((CMediaSample*) This)->time_start;
    if (pTimeEnd)
	*pTimeEnd = ((CMediaSample*) This)->time_end;
    return 0;
}

/**
 * \brief IMediaSample::GetMediaTime (retrieves the media times of this sample)
 *
 * \param[in] This pointer to CMediaSample object
 * \param[out] pTimeStart pointer to variable that specifies start time
 * \param[out] pTimeEnd pointer to variable that specifies end time
 *
 * \return S_OK success
 * \return apropriate error code otherwise
 *
 * \remarks
 * To invalidate the media times set pTimeStart and pTimeEnd to NULL. this will cause
 * IMEdiaSample::GetTime to return VFW_E_MEDIA_TIME_NOT_SET
 */
static HRESULT STDCALL CMediaSample_SetMediaTime(IMediaSample * This,
						 /* [in] */ LONGLONG *pTimeStart,
						 /* [in] */ LONGLONG *pTimeEnd)
{
    Debug printf("CMediaSample_SetMediaTime(%p) called\n", This);
    if (pTimeStart)
	((CMediaSample*) This)->time_start = *pTimeStart;
    if (pTimeEnd)
        ((CMediaSample*) This)->time_end = *pTimeEnd;
    return 0;
}

/**
 * \brief CMediaSample::SetPointer (extension for direct memory write of decompressed data)
 *
 * \param[in] This pointer to CMediaSample object
 * \param[in] pointer pointer to an external buffer to store data to
 *
 */
static void CMediaSample_SetPointer(CMediaSample* This, char* pointer)
{
    Debug printf("CMediaSample_SetPointer(%p) called  -> %p\n", This, pointer);
    if (pointer)
	This->block = pointer;
    else
	This->block = This->own_block;
}

/**
 * \brief CMediaSample::SetPointer (resets pointer to external buffer with internal one)
 *
 * \param[in] This pointer to CMediaSample object
 *
 */
static void CMediaSample_ResetPointer(CMediaSample* This)
{
    Debug printf("CMediaSample_ResetPointer(%p) called\n", This);
    This->block = This->own_block;
}

/**
 * \brief CMediaSample constructor
 *
 * \param[in] allocator IMemallocator interface of allocator to use
 * \param[in] size size of internal buffer
 *
 * \return pointer to CMediaSample object of NULL if error occured
 *
 */
CMediaSample* CMediaSampleCreate(IMemAllocator* allocator, int size)
{
    CMediaSample* This = malloc(sizeof(CMediaSample));
    if (!This)
	return NULL;

    // some hack here!
    // it looks like Acelp decoder is actually accessing
    // the allocated memory before it sets the new size for it ???
    // -- maybe it's being initialized with wrong parameters
    // anyway this is fixes the problem somehow with some reserves
    //
    // using different trick for now - in DS_Audio modify sample size
    //if (size < 0x1000)
    //    size = (size + 0xfff) & ~0xfff;

    This->vt        = malloc(sizeof(IMediaSample_vt));
    This->own_block = malloc((size_t)size + SAFETY_ACEL);
    This->media_type.pbFormat = 0;
    This->media_type.pUnk = 0;

    if (!This->vt || !This->own_block)
    {
	CMediaSample_Destroy(This);
	return NULL;
    }

    This->vt->QueryInterface = CMediaSample_QueryInterface;
    This->vt->AddRef = CMediaSample_AddRef;
    This->vt->Release = CMediaSample_Release;
    This->vt->GetPointer = CMediaSample_GetPointer;
    This->vt->GetSize = CMediaSample_GetSize;
    This->vt->GetTime = CMediaSample_GetTime;
    This->vt->SetTime = CMediaSample_SetTime;
    This->vt->IsSyncPoint = CMediaSample_IsSyncPoint;
    This->vt->SetSyncPoint = CMediaSample_SetSyncPoint;
    This->vt->IsPreroll = CMediaSample_IsPreroll;
    This->vt->SetPreroll = CMediaSample_SetPreroll;
    This->vt->GetActualDataLength = CMediaSample_GetActualDataLength;
    This->vt->SetActualDataLength = CMediaSample_SetActualDataLength;
    This->vt->GetMediaType = CMediaSample_GetMediaType;
    This->vt->SetMediaType = CMediaSample_SetMediaType;
    This->vt->IsDiscontinuity = CMediaSample_IsDiscontinuity;
    This->vt->SetDiscontinuity = CMediaSample_SetDiscontinuity;
    This->vt->GetMediaTime = CMediaSample_GetMediaTime;
    This->vt->SetMediaTime = CMediaSample_SetMediaTime;

    This->all = allocator;
    This->size = size;
    This->refcount = 0; // increased by MemAllocator
    This->actual_size = 0;
    This->isPreroll = 0;
    This->isDiscontinuity = 1;
    This->time_start = 0;
    This->time_end = 0;
    This->type_valid = 0;
    This->block = This->own_block;

    This->SetPointer = CMediaSample_SetPointer;
    This->ResetPointer = CMediaSample_ResetPointer;

    Debug printf("CMediaSample_Create(%p) called - sample size %d, buffer %p\n",
		 This, This->size, This->block);

    return This;
}
