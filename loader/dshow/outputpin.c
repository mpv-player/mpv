/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#include "loader/wine/winerror.h"
#include "loader/wine/windef.h"
#include "outputpin.h"
#include "mediatype.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static inline int output_unimplemented(const char* s, void* p)
{
    Debug printf("%s(%p) called (UNIMPLEMENTED)", s, p);
    return E_NOTIMPL;
}

/**
    An object beyond interface IEnumMediaTypes.
    Returned by COutputPin through call IPin::EnumMediaTypes().
*/
typedef struct CEnumMediaTypes
{
    IEnumMediaTypes_vt* vt;
    DECLARE_IUNKNOWN();
    AM_MEDIA_TYPE type;
    GUID interfaces[2];
} CEnumMediaTypes;

/**
   IMemOutput interface implementation
*/
struct COutputMemPin
{
    IMemInputPin_vt* vt;
    DECLARE_IUNKNOWN();
    char** frame_pointer;
    long* frame_size_pointer;
    MemAllocator* pAllocator;
    COutputPin* parent;
};

/**
 * \brief IEnumMediaTypes:Next (retrives a specified number of media types )
 *
 * \param[in]  This pointer to CEnumMediaTypes object
 * \param[in]  cMediaTypes number of media types to retrive
 * \param[out] ppMediaTypes array of AM_MEDIA_TYPE structure pointers of size cMediaTypes
 * \param[out] pcFetched address of variables that receives number of returned media types
 *
 * \return S_OK - success
 * \return S_FALSE - did not return as meny structures as requested
 * \return E_INVALIDARG Invalid argument
 * \return E_POINTER Null pointer
 * \return VFW_E_ENUM_OUT_OF_SYNC - pin's state has changed and is now inconsistent with enumerator
 *
 */
static HRESULT STDCALL CEnumMediaTypes_Next(IEnumMediaTypes * This,
					    /* [in] */ ULONG cMediaTypes,
					    /* [size_is][out] */ AM_MEDIA_TYPE **ppMediaTypes,
					    /* [out] */ ULONG *pcFetched)
{
    AM_MEDIA_TYPE* type = &((CEnumMediaTypes*)This)->type;
    Debug printf("CEnumMediaTypes::Next(%p) called\n", This);
    if (!ppMediaTypes)
	return E_INVALIDARG;
    if (!pcFetched && (cMediaTypes!=1))
	return E_INVALIDARG;
    if (cMediaTypes <= 0)
	return 0;

    if (pcFetched)
	*pcFetched=1;
    ppMediaTypes[0] = CreateMediaType(type);

    if (cMediaTypes == 1)
	return 0;
    return 1;
}

/* I expect that these methods are unused. */

/**
 * \brief IEnumMediaTypes::Skip (skips over a specified number of media types)
 *
 * \param[in]  This pointer to CEnumMEdiaTypes object
 * \param[in]  cMediaTypes number of media types to skip
 *
 * \return S_OK - success
 * \return S_FALSE - skipped past the end of the sequence
 * \return VFW_E_ENUM_OUT_OF_SYNC - pin's state has changed and is now inconsistent with enumerator
 *
 */
static HRESULT STDCALL CEnumMediaTypes_Skip(IEnumMediaTypes * This,
					    /* [in] */ ULONG cMediaTypes)
{
    return output_unimplemented("CEnumMediaTypes::Skip", This);
}

/**
 * \brief IEnumMediaTypes::Reset (resets enumeration sequence to beginning)
 *
 * \param[in]  This pointer to CEnumMEdiaTypes object
 *
 * \return S_OK - success
 *
 */
static HRESULT STDCALL CEnumMediaTypes_Reset(IEnumMediaTypes * This)
{
    Debug printf("CEnumMediaTypes::Reset(%p) called\n", This);
    return 0;
}

/**
 * \brief IEnumMediaTypes::Clone (makes a copy of enumerator, returned object
 *        starts at the same position as original)
 *
 * \param[in]  This pointer to CEnumMEdiaTypes object
 * \param[out] ppEnum address of variable that receives pointer to IEnumMediaTypes interface
 *
 * \return S_OK - success
 * \return E_OUTOFMEMRY - Insufficient memory
 * \return E_POINTER - Null pointer
 * \return VFW_E_ENUM_OUT_OF_SYNC - pin's state has changed and is now inconsistent with enumerator
 *
 */
static HRESULT STDCALL CEnumMediaTypes_Clone(IEnumMediaTypes * This,
				      /* [out] */ IEnumMediaTypes **ppEnum)
{
    Debug printf("CEnumMediaTypes::Clone(%p) called\n", This);
    return E_NOTIMPL;
}

/**
 * \brief CEnumMediaTypes destructor
 *
 * \param[in]  This pointer to CEnumMediaTypes object
 *
 */
static void CEnumMediaTypes_Destroy(CEnumMediaTypes* This)
{
    FreeMediaType(&(This->type));
    free(This->vt);
    free(This);
}

// IEnumMediaTypes->IUnknown methods
IMPLEMENT_IUNKNOWN(CEnumMediaTypes)

/**
 * \brief CEnumMediaTypes constructor
 *
 * \param[in]  amt media type for enumerating
 *
 * \return pointer to CEnumMEdiaTypes object or NULL if error occured
 *
 */
static CEnumMediaTypes* CEnumMediaTypesCreate(const AM_MEDIA_TYPE* amt)
{
    CEnumMediaTypes *This = malloc(sizeof(CEnumMediaTypes)) ;

    if (!This)
        return NULL;

    This->vt = malloc(sizeof(IEnumMediaTypes_vt));
    if (!This->vt)
    {
	free(This);
	return NULL;
    }

    This->refcount = 1;
    CopyMediaType(&(This->type),amt);

    This->vt->QueryInterface = CEnumMediaTypes_QueryInterface;
    This->vt->AddRef = CEnumMediaTypes_AddRef;
    This->vt->Release = CEnumMediaTypes_Release;
    This->vt->Next = CEnumMediaTypes_Next;
    This->vt->Skip = CEnumMediaTypes_Skip;
    This->vt->Reset = CEnumMediaTypes_Reset;
    This->vt->Clone = CEnumMediaTypes_Clone;

    This->interfaces[0] = IID_IUnknown;
    This->interfaces[1] = IID_IEnumMediaTypes;

    return This;
}


/*************
 * COutputPin
 *
 * WARNING:
 * This is implementation of INPUT pin in DirectShow's terms
 *
 *************/


/**
 *
 * \brief IUnknown::QueryInterface (query object for interface)
 * \param[in]  This pointer to IUnknown interface
 * \param[in]  iid  GUID of requested interface
 * \param[out] ppv  receives pointer to interface
 *
 * \return S_OK - success (and *ppv contains valid pointer)
 * \return E_NOINTERFACE - interface not found (and *ppv was set NULL)
 *
 * \note
 * Make sure to call Release on received interface when you are done
 *
 */
static HRESULT STDCALL COutputPin_QueryInterface(IUnknown* This, const GUID* iid, void** ppv)
{
    COutputPin* p = (COutputPin*) This;

    Debug printf("COutputPin_QueryInterface(%p) called\n", This);
    if (!ppv)
	return E_INVALIDARG;

    if (memcmp(iid, &IID_IUnknown, 16) == 0)
    {
	*ppv = p;
	p->vt->AddRef(This);
        return 0;
    }
    if (memcmp(iid, &IID_IMemInputPin, 16) == 0)
    {
	*ppv = p->mempin;
	p->mempin->vt->AddRef((IUnknown*)*ppv);
	return 0;
    }

    Debug printf("Unknown interface : %08x-%04x-%04x-%02x%02x-"
		 "%02x%02x%02x%02x%02x%02x\n",
		 iid->f1,  iid->f2,  iid->f3,
		 (unsigned char)iid->f4[1], (unsigned char)iid->f4[0],
		 (unsigned char)iid->f4[2], (unsigned char)iid->f4[3],
		 (unsigned char)iid->f4[4], (unsigned char)iid->f4[5],
		 (unsigned char)iid->f4[6], (unsigned char)iid->f4[7]);
    return E_NOINTERFACE;
}

// IPin methods

/**
 * \brief IPin::Connect (connects pin to another pin)
 *
 * \param[in] This          pointer to IPin interface
 * \param[in] pReceivePin   pointer to IPin interface of remote pin
 * \param[in] pmt suggested media type for link. Can be NULL (any media type)
 *
 * \return S_OK - success.
 * \return VFW_E_ALREADY_CONNECTED - pin already connected
 * \return VFW_E_NOT_STOPPED - filter is active
 * \return VFW_E_TYPE_NOT_ACCEPT - type is not acceptable
 * \return Apropriate error code otherwise.
 *
 */
static HRESULT STDCALL COutputPin_Connect(IPin * This,
				    /* [in] */ IPin *pReceivePin,
				    /* [in] */ /* const */ AM_MEDIA_TYPE *pmt)
{
    Debug printf("COutputPin_Connect(%p) called\n",This);
/*
    *pmt=((COutputPin*)This)->type;
    if(pmt->cbFormat>0)
    {
	pmt->pbFormat=malloc(pmt->cbFormat);
	memcpy(pmt->pbFormat, ((COutputPin*)This)->type.pbFormat, pmt->cbFormat);
    }
*/
    //return E_NOTIMPL;
    return 0;// XXXXXXXXXXXXX CHECKME XXXXXXXXXXXXXXX
    // if I put return 0; here, it crashes
}

/**
 * \brief IPin::ReceiveConnection (accepts a connection from another pin)
 *
 * \param[in] This       pointer to IPin interface
 * \param[in] pConnector connecting pin's IPin interface
 * \param[in] pmt        suggested media type for connection
 *
 * \return S_OK - success
 * \return E_POINTER - Null pointer
 * \return VFW_E_ALREADY_CONNECTED - pin already connected
 * \return VFW_E_NOT_STOPPED - filter is active
 * \return VFW_E_TYPE_NOT_ACCEPT - type is not acceptable
 *
 * \note
 * When returning S_OK method should also do the following:
 *  - store media type and return the same type in IPin::ConnectionMediaType
 *  - store pConnector and return it in IPin::ConnectedTo
 *
 */
static HRESULT STDCALL COutputPin_ReceiveConnection(IPin * This,
						    /* [in] */ IPin *pConnector,
						    /* [in] */ const AM_MEDIA_TYPE *pmt)
{
    Debug printf("COutputPin_ReceiveConnection(%p) called\n", This);
    ((COutputPin*)This)->remote = pConnector;
    return 0;
}

/**
 * \brief IPin::Disconnect (accepts a connection from another pin)
 *
 * \param[in] This pointer to IPin interface
 *
 * \return S_OK - success
 * \return S_FALSE - pin was not connected
 * \return VFW_E_NOT_STOPPED - filter is active
 *
 * \note
 *   To break connection you have to also call Disconnect on other pin
 */
static HRESULT STDCALL COutputPin_Disconnect(IPin * This)
{
    Debug printf("COutputPin_Disconnect(%p) called\n", This);
    return 1;
}

/**
 * \brief IPin::ConnectedTo (retrieves pointer to the connected pin, if such exist)
 *
 * \param[in]  This pointer to IPin interface
 * \param[out] pPin pointer to remote pin's IPin interface
 *
 * \return S_OK - success
 * \return E_POINTER - Null pointer
 * \return VFW_E_NOT_CONNECTED - pin is not connected
 *
 * \note
 * Caller must call Release on received IPin, when done
 */
static HRESULT STDCALL COutputPin_ConnectedTo(IPin * This,
					/* [out] */ IPin **pPin)
{
    Debug printf("COutputPin_ConnectedTo(%p) called\n", This);
    if (!pPin)
	return E_INVALIDARG;
    *pPin = ((COutputPin*)This)->remote;
    return 0;
}

/**
 * \brief IPin::ConnectionMediaType (retrieves media type for connection, if such exist)
 *
 * \param[in]  This pointer to IPin interface
 * \param[out] pmt pointer to AM_MEDIA_TYPE,  that receives connection media type
 *
 * \return S_OK - success
 * \return E_POINTER - Null pointer
 * \return VFW_E_NOT_CONNECTED - pin is not connected
 *
 */
static HRESULT STDCALL COutputPin_ConnectionMediaType(IPin * This,
						      /* [out] */ AM_MEDIA_TYPE *pmt)
{
    Debug printf("COutputPin_ConnectionMediaType(%p) called\n",This);
    if (!pmt)
	return E_INVALIDARG;
    CopyMediaType(pmt,&(((COutputPin*)This)->type));
    return 0;
}

/**
 * \brief IPin::QueryPinInfo (retrieves information about the pin)
 *
 * \param[in]  This  pointer to IPin interface
 * \param[out] pInfo pointer to PIN_INFO structure, that receives pin info
 *
 * \return S_OK - success
 * \return E_POINTER - Null pointer
 *
 * \note
 * If pInfo->pFilter is not NULL, then caller must call Release on pInfo->pFilter when done
 *
 */
static HRESULT STDCALL COutputPin_QueryPinInfo(IPin * This,
					       /* [out] */ PIN_INFO *pInfo)
{
    return output_unimplemented("COutputPin_QueryPinInfo", This);
}

/**
 * \brief IPin::QueryDirection (retrieves pin direction)
 *
 * \param[in]  This    pointer to IPin interface
 * \param[out] pPinDir pointer to variable, that receives pin direction (PINDIR_INPUT,PINDIR_OUTPUT)
 *
 * \return S_OK - success
 * \return E_POINTER - Null pointer
 *
 */
static HRESULT STDCALL COutputPin_QueryDirection(IPin * This,
					   /* [out] */ PIN_DIRECTION *pPinDir)
{
    Debug printf("COutputPin_QueryDirection(%p) called\n", This);
    if (!pPinDir)
	return E_INVALIDARG;
    *pPinDir = PINDIR_INPUT;
    return 0;
}

/**
 * \brief IPin::QueryId (retrieves pin identificator)
 *
 * \param[in]  This pointer to IPin interface
 * \param[out] Id   adress of variable, that receives string with pin's Id.
 *
 * \return S_OK - success
 * \return E_OUTOFMEMORY - Insufficient memory
 * \return E_POINTER     - Null pointer
 *
 * \note
 * Pin's Id is not the same as pin's name
 *
 */
static HRESULT STDCALL COutputPin_QueryId(IPin * This,
					  /* [out] */ LPWSTR *Id)
{
    return output_unimplemented("COutputPin_QueryId", This);
}

/**
 * \brief IPin::QueryAccept (determines can media type be accepted or not)
 *
 * \param[in] This  pointer to IPin interface
 * \param[in] pmt   Media type to check
 *
 * \return S_OK - success
 * \return S_FALSE - pin rejects media type
 *
 */
static HRESULT STDCALL COutputPin_QueryAccept(IPin * This,
					      /* [in] */ const AM_MEDIA_TYPE *pmt)
{
    return output_unimplemented("COutputPin_QueryAccept", This);
}

/**
 * \brief IPin::EnumMediaTypes (enumerates the pin's preferred media types)
 *
 * \param[in] This  pointer to IPin interface
 * \param[out] ppEnum adress of variable that receives pointer to IEnumMEdiaTypes interface
 *
 * \return S_OK - success
 * \return E_OUTOFMEMORY - Insufficient memory
 * \return E_POINTER     - Null pointer
 *
 * \note
 * Caller must call Release on received interface when done
 *
 */
static HRESULT STDCALL COutputPin_EnumMediaTypes(IPin * This,
					   /* [out] */ IEnumMediaTypes **ppEnum)
{
    Debug printf("COutputPin_EnumMediaTypes(%p) called\n",This);
    if (!ppEnum)
	return E_INVALIDARG;
    *ppEnum = (IEnumMediaTypes*) CEnumMediaTypesCreate(&((COutputPin*)This)->type);
    return 0;
}

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
static HRESULT STDCALL COutputPin_QueryInternalConnections(IPin * This,
						     /* [out] */ IPin **apPin,
						     /* [out][in] */ ULONG *nPin)
{
    return output_unimplemented("COutputPin_QueryInternalConnections", This);
}

/**
 * \brief IPin::EndOfStream (notifies pin, that no data is expected, until new run command)
 *
 * \param[in] This  pointer to IPin interface
 *
 * \return S_OK - success
 * \return E_UNEXPECTED - The pin is output pin
 *
 * \note
 * IMemoryInputPin::Receive,IMemoryInputPin::ReceiveMultiple, IMemoryInputPin::EndOfStream,
 * IMemAllocator::GetBuffer runs in different (streaming) thread then other
 * methods (application thread).
 * IMemoryInputPin::NewSegment runs either in streaming or application thread.
 * Developer must use critical sections for thread-safing work.
 *
 */
static HRESULT STDCALL COutputPin_EndOfStream(IPin * This)
{
    return output_unimplemented("COutputPin_EndOfStream", This);
}

/**
 * \brief IPin::BeginFlush (begins a flush operation)
 *
 * \param[in] This  pointer to IPin interface
 *
 * \return S_OK - success
 * \return E_UNEXPECTED - The pin is output pin
 *
 */
static HRESULT STDCALL COutputPin_BeginFlush(IPin * This)
{
    return output_unimplemented("COutputPin_BeginFlush", This);
}

/**
 * \brief IPin::EndFlush (ends a flush operation)
 *
 * \param[in] This  pointer to IPin interface
 *
 * \return S_OK - success
 * \return E_UNEXPECTED - The pin is output pin
 *
 */
static HRESULT STDCALL COutputPin_EndFlush(IPin * This)
{
    return output_unimplemented("COutputPin_EndFlush", This);
}

/**
 * \brief IPin::NewSegment (media sample received after this call grouped as segment with common
 *        start,stop time and rate)
 *
 * \param[in] This   pointer to IPin interface
 * \param[in] tStart start time of new segment
 * \param[in] tStop  end time of new segment
 * \param[in] dRate  rate at wich segment should be processed
 *
 * \return S_OK - success
 * \return E_UNEXPECTED - The pin is output pin
 *
 */
static HRESULT STDCALL COutputPin_NewSegment(IPin * This,
				       /* [in] */ REFERENCE_TIME tStart,
				       /* [in] */ REFERENCE_TIME tStop,
				       /* [in] */ double dRate)
{
    Debug printf("COutputPin_NewSegment(%Ld,%Ld,%f) called\n",
		 tStart, tStop, dRate);
    return 0;
}



// IMemInputPin->IUnknown methods

/**
 * \brief IUnknown::QueryInterface (query object for interface)
 *
 * \param[in]  This pointer to IUnknown interface
 * \param[in]  iid  GUID of requested interface
 * \param[out] ppv  receives pointer to interface
 *
 * \return S_OK - success (and *ppv contains valid pointer)
 * \return E_NOINTERFACE - interface not found (and *ppv was set NULL)
 *
 * \note
 * Make sure to call Release on received interface when you are done
 *
 */
static HRESULT STDCALL COutputMemPin_QueryInterface(IUnknown* This, const GUID* iid, void** ppv)
{
    COutputMemPin* p = (COutputMemPin*)This;

    Debug printf("COutputMemPin_QueryInterface(%p) called\n", This);
    if (!ppv)
	return E_INVALIDARG;

    if(!memcmp(iid, &IID_IUnknown, 16))
    {
	*ppv = p;
	p->vt->AddRef(This);
	return 0;
    }
    /*if(!memcmp(iid, &IID_IPin, 16))
    {
	COutputPin* ptr=(COutputPin*)(This-1);
	*ppv=(void*)ptr;
	AddRef((IUnknown*)ptr);
	return 0;
    }*/
    if(!memcmp(iid, &IID_IMemInputPin, 16))
    {
	*ppv = p;
	p->vt->AddRef(This);
	return 0;
    }
    Debug printf("Unknown interface : %08x-%04x-%04x-%02x%02x-" \
		 "%02x%02x%02x%02x%02x%02x\n",
		 iid->f1,  iid->f2,  iid->f3,
		 (unsigned char)iid->f4[1], (unsigned char)iid->f4[0],
		 (unsigned char)iid->f4[2], (unsigned char)iid->f4[3],
		 (unsigned char)iid->f4[4], (unsigned char)iid->f4[5],
		 (unsigned char)iid->f4[6], (unsigned char)iid->f4[7]);
    return E_NOINTERFACE;
}

// IMemInputPin methods

/**
 * \brief IMemInputPin::GetAllocator (retrives memory allocator, proposed by pin)
 *
 * \param[in]  This pointer to IMemInputPin interface
 * \param[out]  ppAllocator  address of variable that receives allocator's IMemAllocator interface
 *
 * \return S_OK - success
 * \return VFW_E_NO_ALLOCATOR - No allocator
 *
 * \note
 * Make sure to call Release on received interface when you are done
 *
 */
static HRESULT STDCALL COutputMemPin_GetAllocator(IMemInputPin* This,
					 /* [out] */ IMemAllocator** ppAllocator)
{
    Debug printf("COutputMemPin_GetAllocator(%p, %p) called\n", This->vt, ppAllocator);
    *ppAllocator = (IMemAllocator*) MemAllocatorCreate();
    return 0;
}

/**
 *
 * \brief IMemInputPin::NotifyAllocator (specifies an allocator for the connection)
 *
 * \param[in]  This pointer to IMemInputPin interface
 * \param[in]  pAllocator  allocator's IMemAllocator interface
 * \param[in]  bReadOnly specifies whether samples from allocator are readonly
 *
 * \return S_OK - success
 * \return Apropriate error code otherwise
 *
 */
static HRESULT STDCALL COutputMemPin_NotifyAllocator(IMemInputPin* This,
						  /* [in] */ IMemAllocator* pAllocator,
						  /* [in] */ int bReadOnly)
{
    Debug printf("COutputMemPin_NotifyAllocator(%p, %p) called\n", This, pAllocator);
    ((COutputMemPin*)This)->pAllocator = (MemAllocator*) pAllocator;
    return 0;
}

/**
 * \brief IMemInputPin::GetAllocatorRequirements (retrieves allocator properties requested by
 *        input pin)
 *
 * \param[in]  This pointer to IMemInputPin interface
 * \param[out]  pProps pointer to a structure that receives allocator properties
 *
 * \return S_OK - success
 * \return E_NOTIMPL - Not implemented
 * \return E_POINTER - Null pointer
 *
 */
static HRESULT STDCALL COutputMemPin_GetAllocatorRequirements(IMemInputPin* This,
							   /* [out] */ ALLOCATOR_PROPERTIES* pProps)
{
    return output_unimplemented("COutputMemPin_GetAllocatorRequirements", This);
}

/**
 * \brief IMemInputPin::Receive (receives the next media sample int thre stream)
 *
 * \param[in]  This pointer to IMemInputPin interface
 * \param[in]  pSample pointer to sample's IMediaSample interface
 *
 * \return S_OK - success
 * \return S_FALSE - The sample was rejected
 * \return E_POINTER - Null pointer
 * \return VFW_E_INVALIDMEDIATYPE - invalid media type
 * \return VFW_E_RUNTIME_ERROR - run-time error occured
 * \return VFW_E_WRONG_STATE - pin is stopped
 *
 * \remarks
 * Method san do on of the following:
 * - reject sample
 * - accept sample and process it in another thread
 * - accept sample and process it before returning
 *
 * In second case method should increase reference count for sample (through AddRef)
 * In the last case method might block indefinitely. If this might
 * happen IMemInpuPin::ReceiveCAnBlock returns S_OK
 *
 * \note
 * IMemoryInputPin::Receive,IMemoryInputPin::ReceiveMultiple, IMemoryInputPin::EndOfStream,
 * IMemAllocator::GetBuffer runs in different (streaming) thread then other
 * methods (application thread).
 * IMemoryInputPin::NewSegment runs either in streaming or application thread.
 * Developer must use critical sections for thread-safing work.
 *
 */
static HRESULT STDCALL COutputMemPin_Receive(IMemInputPin* This,
					  /* [in] */ IMediaSample* pSample)
{
    Debug printf("COutputMemPin_Receive(%p) called\n", This);
    if (!pSample)
	return E_INVALIDARG;

    if(((COutputMemPin*)This)->parent->SampleProc)
        return ((COutputMemPin*)This)->parent->SampleProc(((COutputMemPin*)This)->parent->pUserData,pSample);
    //reject sample
    return S_FALSE;
}

/**
 * \brief IMemInputPin::ReceiveMultiple (receives multiple samples in the stream)
 *
 * \param[in]  This pointer to IMemInputPin interface
 * \param[in]  pSamples          pointer to array with samples
 * \param[in]  nSamples          number of samples in array
 * \param[out] nSamplesProcessed number of processed samples
 *
 * \return S_OK - success
 * \return S_FALSE - The sample was rejected
 * \return E_POINTER - Null pointer
 * \return VFW_E_INVALIDMEDIATYPE - invalid media type
 * \return VFW_E_RUNTIME_ERROR - run-time error occured
 * \return VFW_E_WRONG_STATE - pin is stopped
 *
 * \remarks
 * This method behaves like IMemInputPin::Receive but for array of samples
 *
 * \note
 * IMemoryInputPin::Receive,IMemoryInputPin::ReceiveMultiple, IMemoryInputPin::EndOfStream,
 * IMemAllocator::GetBuffer runs in different (streaming) thread then other
 * methods (application thread).
 * IMemoryInputPin::NewSegment runs either in streaming or application thread.
 * Developer must use critical sections for thread-safing work.
 *
 */
static HRESULT STDCALL COutputMemPin_ReceiveMultiple(IMemInputPin * This,
					    /* [size_is][in] */ IMediaSample **pSamples,
					    /* [in] */ long nSamples,
					    /* [out] */ long *nSamplesProcessed)
{
    HRESULT hr;
    Debug printf("COutputMemPin_ReceiveMultiple(%p) %ld\n", This,nSamples);
    for(*nSamplesProcessed=0; *nSamplesProcessed < nSamples; *nSamplesProcessed++) {
         hr = This->vt->Receive(This,pSamples[*nSamplesProcessed]);
         if (hr != S_OK) break;
    }
    return hr;
}

/**
 * \brief IMemInputPin::ReceiveCanBlock (determines whether IMemInputPin:::Receive might block)
 *
 * \param[in]  This pointer to IMemInputPin interface
 *
 * \return S_OK - the pin might block
 * \return S_FALSE - the pin will not block
 *
 */
static HRESULT STDCALL COutputMemPin_ReceiveCanBlock(IMemInputPin * This)
{
    return output_unimplemented("COutputMemPin_ReceiveCanBlock", This);
}

/**
 * \brief COutputPin::SetNewFormat(sets new media format for the pin)
 *
 * \param[in]  This pointer to COutputPin class
 * \param[in]  amt  new media format
 *
 */
static void COutputPin_SetNewFormat(COutputPin* This, const AM_MEDIA_TYPE* amt)
{
    CopyMediaType(&(This->type),amt);
}

/**
 * \brief COutputPin destructor
 *
 * \param[in]  This pointer to COutputPin class
 *
 */
static void COutputPin_Destroy(COutputPin* This)
{
    if (This->mempin->vt)
	free(This->mempin->vt);
    if (This->mempin)
	free(This->mempin);
    if (This->vt)
	free(This->vt);
    FreeMediaType(&(This->type));
    free(This);
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
static HRESULT STDCALL COutputPin_AddRef(IUnknown* This)
{
    Debug printf("COutputPin_AddRef(%p) called (%d)\n", This, ((COutputPin*)This)->refcount);
    ((COutputPin*)This)->refcount++;
    return 0;
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
static HRESULT STDCALL COutputPin_Release(IUnknown* This)
{
    Debug printf("COutputPin_Release(%p) called (%d)\n", This, ((COutputPin*)This)->refcount);
    if (--((COutputPin*)This)->refcount <= 0)
	COutputPin_Destroy((COutputPin*)This);

    return 0;
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
static HRESULT STDCALL COutputMemPin_AddRef(IUnknown* This)
{
    COutputMemPin* p = (COutputMemPin*) This;
    Debug printf("COutputMemPin_AddRef(%p) called (%p, %d)\n", p, p->parent, p->parent->refcount);
    p->parent->refcount++;
    return 0;
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
static HRESULT STDCALL COutputMemPin_Release(IUnknown* This)
{
    COutputMemPin* p = (COutputMemPin*) This;
    Debug printf("COutputMemPin_Release(%p) called (%p,   %d)\n",
		 p, p->parent, p->parent->refcount);
    if (--p->parent->refcount <= 0)
	COutputPin_Destroy(p->parent);
    return 0;
}

/**
 * \brief COutputPin constructor
 *
 * \param[in]  amt media type for pin
 *
 * \return pointer to COutputPin if success
 * \return NULL if error occured
 *
 */
COutputPin* COutputPinCreate(const AM_MEDIA_TYPE* amt,SAMPLEPROC SampleProc,void* pUserData)
{
    COutputPin* This = malloc(sizeof(COutputPin));
    IMemInputPin_vt* ivt;

    if (!This)
        return NULL;

    This->vt = malloc(sizeof(IPin_vt));
    This->mempin = malloc(sizeof(COutputMemPin));
    ivt = malloc(sizeof(IMemInputPin_vt));

    if (!This->vt || !This->mempin || !ivt)
    {
        COutputPin_Destroy(This);
        free(ivt);
	return NULL;
    }

    This->SampleProc=SampleProc;
    This->pUserData=pUserData;

    This->mempin->vt = ivt;

    This->refcount = 1;
    This->remote = 0;
    CopyMediaType(&(This->type),amt);

    This->vt->QueryInterface = COutputPin_QueryInterface;
    This->vt->AddRef = COutputPin_AddRef;
    This->vt->Release = COutputPin_Release;
    This->vt->Connect = COutputPin_Connect;
    This->vt->ReceiveConnection = COutputPin_ReceiveConnection;
    This->vt->Disconnect = COutputPin_Disconnect;
    This->vt->ConnectedTo = COutputPin_ConnectedTo;
    This->vt->ConnectionMediaType = COutputPin_ConnectionMediaType;
    This->vt->QueryPinInfo = COutputPin_QueryPinInfo;
    This->vt->QueryDirection = COutputPin_QueryDirection;
    This->vt->QueryId = COutputPin_QueryId;
    This->vt->QueryAccept = COutputPin_QueryAccept;
    This->vt->EnumMediaTypes = COutputPin_EnumMediaTypes;
    This->vt->QueryInternalConnections = COutputPin_QueryInternalConnections;
    This->vt->EndOfStream = COutputPin_EndOfStream;
    This->vt->BeginFlush = COutputPin_BeginFlush;
    This->vt->EndFlush = COutputPin_EndFlush;
    This->vt->NewSegment = COutputPin_NewSegment;

    This->mempin->vt->QueryInterface = COutputMemPin_QueryInterface;
    This->mempin->vt->AddRef = COutputMemPin_AddRef;
    This->mempin->vt->Release = COutputMemPin_Release;
    This->mempin->vt->GetAllocator = COutputMemPin_GetAllocator;
    This->mempin->vt->NotifyAllocator = COutputMemPin_NotifyAllocator;
    This->mempin->vt->GetAllocatorRequirements = COutputMemPin_GetAllocatorRequirements;
    This->mempin->vt->Receive = COutputMemPin_Receive;
    This->mempin->vt->ReceiveMultiple = COutputMemPin_ReceiveMultiple;
    This->mempin->vt->ReceiveCanBlock = COutputMemPin_ReceiveCanBlock;

    This->mempin->frame_size_pointer = 0;
    This->mempin->frame_pointer = 0;
    This->mempin->pAllocator = 0;
    This->mempin->refcount = 1;
    This->mempin->parent = This;

    This->SetNewFormat = COutputPin_SetNewFormat;

    return This;
}
