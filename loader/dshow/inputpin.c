/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 */

#include "inputpin.h"
#include "mediatype.h"
#include "wine/winerror.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static inline int unimplemented(const char* s, void* p)
{
    Debug printf("%s(%p) called (UNIMPLEMENTED)", s, p);
    return E_NOTIMPL;
}

/***********
 * EnumPins
 ***********/

typedef struct
{
    IEnumPins_vt* vt;
    DECLARE_IUNKNOWN();
    IPin* pin1;
    IPin* pin2;
    int counter;
    GUID interfaces[2];
} CEnumPins;

/**
 * \brief IEnumPins:Next (retrives a specified number of pins )
 *
 * \param[in]  This pointer to CEnumPins object
 * \param[in]  cMediaTypes number of pins to retrive
 * \param[out] ppMediaTypes array of IPin interface pointers of size cMediaTypes
 * \param[out] pcFetched address of variables that receives number of returned pins
 *
 * \return S_OK - success
 * \return S_FALSE - did not return as meny pins as requested
 * \return E_INVALIDARG Invalid argument
 * \return E_POINTER Null pointer
 * \return VFW_E_ENUM_OUT_OF_SYNC - filter's state has changed and is now inconsistent with enumerator
 *
 */
static long STDCALL CEnumPins_Next(IEnumPins* This,
				   /* [in] */ unsigned long cMediaTypes,
				   /* [size_is][out] */ IPin** ppMediaTypes,
				   /* [out] */ unsigned long* pcFetched)
{
    CEnumPins* pin = (CEnumPins*)This;

    Debug printf("CEnumPins_Next(%p) called\n", This);
    if (!ppMediaTypes)
	return E_INVALIDARG;
    if (!pcFetched && (cMediaTypes!=1))
	return E_INVALIDARG;
    if (cMediaTypes<=0)
	return 0;

    //lcounter = ((CEnumPins*)This)->counter;
    //lpin1 = ((CEnumPins*)This)->pin1;
    //lpin2 = ((CEnumPins*)This)->pin2;
    if (((pin->counter == 2) && pin->pin2)
	|| ((pin->counter == 1) && !pin->pin2))
    {
	if (pcFetched)
	    *pcFetched=0;
	return 1;
    }

    if (pcFetched)
	*pcFetched=1;
    if (pin->counter==0)
    {
	*ppMediaTypes = pin->pin1;
	pin->pin1->vt->AddRef((IUnknown*)pin->pin1);
    }
    else
    {
	*ppMediaTypes = pin->pin2;
	pin->pin2->vt->AddRef((IUnknown*)pin->pin2);
    }
    pin->counter++;
    if (cMediaTypes == 1)
	return 0;
    return 1;
}

/**
 * \brief IEnumPins::Skip (skips over a specified number of pins)
 *
 * \param[in]  This pointer to CEnumPinss object
 * \param[in]  cMediaTypes number of pins to skip
 *
 * \return S_OK - success
 * \return S_FALSE - skipped past the end of the sequence
 * \return VFW_E_ENUM_OUT_OF_SYNC - filter's state has changed and is now inconsistent with enumerator
 *
 */
static long STDCALL CEnumPins_Skip(IEnumPins* This,
				   /* [in] */ unsigned long cMediaTypes)
{
    Debug unimplemented("CEnumPins_Skip", This);
    return E_NOTIMPL;
}

/**
 * \brief IEnumPins::Reset (resets enumeration sequence to beginning)
 *
 * \param[in]  This pointer to CEnumPins object
 *
 * \return S_OK - success
 *
 */
static long STDCALL CEnumPins_Reset(IEnumPins* This)
{
    Debug printf("CEnumPins_Reset(%p) called\n", This);
    ((CEnumPins*)This)->counter = 0;
    return 0;
}

/**
 * \brief IEnumPins::Clone (makes a copy of enumerator, returned object
 *        starts at the same position as original)
 *
 * \param[in]  This pointer to CEnumPins object
 * \param[out] ppEnum address of variable that receives pointer to IEnumPins interface
 *
 * \return S_OK - success
 * \return E_OUTOFMEMRY - Insufficient memory
 * \return E_POINTER - Null pointer
 * \return VFW_E_ENUM_OUT_OF_SYNC - filter's state has changed and is now inconsistent with enumerator
 *
 */
static long STDCALL CEnumPins_Clone(IEnumPins* This,
				    /* [out] */ IEnumPins** ppEnum)
{
    Debug unimplemented("CEnumPins_Clone", This);
    return E_NOTIMPL;
}

/**
 * \brief CEnumPins destructor
 *
 * \param[in]  This pointer to CEnumPins object
 *
 */
static void CEnumPins_Destroy(CEnumPins* This)
{
    free(This->vt);
    free(This);
}

IMPLEMENT_IUNKNOWN(CEnumPins)

/**
 * \brief CEnumPins constructor
 *
 * \param[in]  p first pin for enumerator
 * \param[in]  pp second pin for enumerator
 *
 * \return pointer to CEnumPins object or NULL if error occured
 *
 */
static CEnumPins* CEnumPinsCreate(IPin* p, IPin* pp)
{
    CEnumPins* This = (CEnumPins*) malloc(sizeof(CEnumPins));

    if (!This)
        return NULL;

    This->refcount = 1;
    This->pin1 = p;
    This->pin2 = pp;
    This->counter = 0;

    This->vt = (IEnumPins_vt*) malloc(sizeof(IEnumPins_vt));
    if (!This->vt)
    {
	free(This);
        return NULL;
    }
    This->vt->QueryInterface = CEnumPins_QueryInterface;
    This->vt->AddRef = CEnumPins_AddRef;
    This->vt->Release = CEnumPins_Release;
    This->vt->Next = CEnumPins_Next;
    This->vt->Skip = CEnumPins_Skip;
    This->vt->Reset = CEnumPins_Reset;
    This->vt->Clone = CEnumPins_Clone;

    This->interfaces[0] = IID_IUnknown;
    This->interfaces[1] = IID_IEnumPins;

    return This;
}



/***********
 * InputPin
 *
 * WARNING:
 * This is implementation of OUTPUT pin in DirectShow's terms
 *
 ***********/

/**
 * \brief IPin::Connect (connects pin to another pin)
 *
 * \param[in] This          pointer to IPin interface
 * \param[in] pReceivePin   pointer to IPin interface of remote pin
 * \param[in] pmt           suggested media type for link. Can be NULL (any media type)
 *
 * \return S_OK - success.
 * \return VFW_E_ALREADY_CONNECTED - pin already connected
 * \return VFW_E_NOT_STOPPED - filter is active
 * \return VFW_E_TYPE_NOT_ACCEPT - type is not acceptable
 * \return Apropriate error code otherwise.
 *
 */
static long STDCALL CInputPin_Connect(IPin* This,
				      /* [in] */ IPin* pReceivePin,
				      /* [in] */ AM_MEDIA_TYPE* pmt)
{
    Debug unimplemented("CInputPin_Connect", This);
    return E_NOTIMPL;
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
static long STDCALL CInputPin_ReceiveConnection(IPin* This,
						/* [in] */ IPin* pConnector,
						/* [in] */ const AM_MEDIA_TYPE *pmt)
{
    Debug unimplemented("CInputPin_ReceiveConnection", This);
    return E_NOTIMPL;
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
static long STDCALL CInputPin_Disconnect(IPin* This)
{
    Debug unimplemented("CInputPin_Disconnect", This);
    return E_NOTIMPL;
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
static long STDCALL CInputPin_ConnectedTo(IPin* This,
					  /* [out] */ IPin** pPin)
{
    Debug unimplemented("CInputPin_ConnectedTo", This);
    return E_NOTIMPL;
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
static long STDCALL CInputPin_ConnectionMediaType(IPin* This,
						  /* [out] */ AM_MEDIA_TYPE *pmt)
{
    Debug printf("CInputPin_ConnectionMediaType(%p) called\n", This);
    if (!pmt)
	return E_INVALIDARG;
    CopyMediaType(pmt,&(((CInputPin*)This)->type));
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
static long STDCALL CInputPin_QueryPinInfo(IPin* This,
					   /* [out] */ PIN_INFO *pInfo)
{
    CBaseFilter* lparent=((CInputPin*)This)->parent;
    Debug printf("CInputPin_QueryPinInfo(%p) called\n", This);
    pInfo->dir = PINDIR_OUTPUT;
    pInfo->pFilter = (IBaseFilter*) lparent;
    lparent->vt->AddRef((IUnknown*)lparent);
    pInfo->achName[0] = 0;
    return 0;
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
static long STDCALL CInputPin_QueryDirection(IPin* This,
					      /* [out] */ PIN_DIRECTION *pPinDir)
{
    *pPinDir = PINDIR_OUTPUT;
    Debug printf("CInputPin_QueryDirection(%p) called\n", This);
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
static long STDCALL CInputPin_QueryId(IPin* This,
				       /* [out] */ unsigned short* *Id)
{
    Debug unimplemented("CInputPin_QueryId", This);
    return E_NOTIMPL;
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
static long STDCALL CInputPin_QueryAccept(IPin* This,
					  /* [in] */ const AM_MEDIA_TYPE* pmt)
{
    Debug unimplemented("CInputPin_QueryAccept", This);
    return E_NOTIMPL;
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
static long STDCALL CInputPin_EnumMediaTypes(IPin* This,
					     /* [out] */ IEnumMediaTypes** ppEnum)
{
    Debug unimplemented("CInputPin_EnumMediaTypes", This);
    return E_NOTIMPL;
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
static long STDCALL CInputPin_QueryInternalConnections(IPin* This,
						       /* [out] */ IPin** apPin,
						       /* [out][in] */ unsigned long *nPin)
{
    Debug unimplemented("CInputPin_QueryInternalConnections", This);
    return E_NOTIMPL;
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
static long STDCALL CInputPin_EndOfStream(IPin * This)
{
    Debug unimplemented("CInputPin_EndOfStream", This);
    return E_NOTIMPL;
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
static long STDCALL CInputPin_BeginFlush(IPin * This)
{
    Debug unimplemented("CInputPin_BeginFlush", This);
    return E_NOTIMPL;
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
static long STDCALL CInputPin_EndFlush(IPin* This)
{
    Debug unimplemented("CInputPin_EndFlush", This);
    return E_NOTIMPL;
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
static long STDCALL CInputPin_NewSegment(IPin* This,
					  /* [in] */ REFERENCE_TIME tStart,
					  /* [in] */ REFERENCE_TIME tStop,
					  /* [in] */ double dRate)
{
    Debug unimplemented("CInputPin_NewSegment", This);
    return E_NOTIMPL;
}

/**
 * \brief CInputPin destructor
 *
 * \param[in]  This pointer to CInputPin class
 *
 */
static void CInputPin_Destroy(CInputPin* This)
{
    free(This->vt);
    FreeMediaType(&(This->type));
    free(This);
}

IMPLEMENT_IUNKNOWN(CInputPin)

/**
 * \brief CInputPin constructor
 *
 * \param[in]  amt media type for pin
 *
 * \return pointer to CInputPin if success
 * \return NULL if error occured
 *
 */
CInputPin* CInputPinCreate(CBaseFilter* p, const AM_MEDIA_TYPE* amt)
{
    CInputPin* This = (CInputPin*) malloc(sizeof(CInputPin));

    if (!This)
        return NULL;

    This->refcount = 1;
    This->parent = p;
    CopyMediaType(&(This->type),amt);

    This->vt= (IPin_vt*) malloc(sizeof(IPin_vt));

    if (!This->vt)
    {
	free(This);
	return NULL;
    }

    This->vt->QueryInterface = CInputPin_QueryInterface;
    This->vt->AddRef = CInputPin_AddRef;
    This->vt->Release = CInputPin_Release;
    This->vt->Connect = CInputPin_Connect;
    This->vt->ReceiveConnection = CInputPin_ReceiveConnection;
    This->vt->Disconnect = CInputPin_Disconnect;
    This->vt->ConnectedTo = CInputPin_ConnectedTo;
    This->vt->ConnectionMediaType = CInputPin_ConnectionMediaType;
    This->vt->QueryPinInfo = CInputPin_QueryPinInfo;
    This->vt->QueryDirection = CInputPin_QueryDirection;
    This->vt->QueryId = CInputPin_QueryId;
    This->vt->QueryAccept = CInputPin_QueryAccept;
    This->vt->EnumMediaTypes = CInputPin_EnumMediaTypes;
    This->vt->QueryInternalConnections = CInputPin_QueryInternalConnections;
    This->vt->EndOfStream = CInputPin_EndOfStream;
    This->vt->BeginFlush = CInputPin_BeginFlush;
    This->vt->EndFlush = CInputPin_EndFlush;
    This->vt->NewSegment = CInputPin_NewSegment;

    This->interfaces[0]=IID_IUnknown;

    return This;
}


/*************
 * BaseFilter
 *************/

static long STDCALL CBaseFilter_GetClassID(IBaseFilter * This,
					   /* [out] */ CLSID *pClassID)
{
    Debug unimplemented("CBaseFilter_GetClassID", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediaFilter::Stop  (stops the filter)
 *
 * \param[in] This pointer to IBaseFilter interface
 *
 * \return S_OK success
 * \return S_FALSE transition is not complete
 *
 * \remarks
 * When filter is stopped it does onot deliver or process any samples and rejects any samples
 * from upstream filter. 
 * Transition may be asynchronous. In this case method should return S_FALSE.
 * Method always sets filter's state to State_Stopped even if error occured.
 *
 */
static long STDCALL CBaseFilter_Stop(IBaseFilter* This)
{
    Debug unimplemented("CBaseFilter_Stop", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediaFilter::Pause (pauses filter)
 *
 * \param[in] This pointer to IBaseFilter interface
 *
 * \return S_OK success
 * \return S_FALSE transition is not complete
 *
 * \remarks
 * When filter is paused it can receive, process and deliver samples.
 * Live source filters do not deliver any samples while paused.
 * Transition may be asynchronous. In this case method should return S_FALSE.
 * Method always sets filter's state to State_Stopped even if error occured.
 *
 */
static long STDCALL CBaseFilter_Pause(IBaseFilter* This)
{
    Debug unimplemented("CBaseFilter_Pause", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediaFilter::Run (runs the filter)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[in] tStart Reference time corresponding to stream time 0.
 *
 * \return S_OK success
 * \return S_FALSE transition is not complete
 *
 * \remarks
 * When filter is running it can receive, process and deliver samples. Source filters
 * generatesnew  samples, and renderers renders them.
 * Stream time is calculated as the current reference time minus tStart.
 * Graph Manager sets tStart slightly in the future according to graph latency.
 *
 */
static long STDCALL CBaseFilter_Run(IBaseFilter* This, REFERENCE_TIME tStart)
{
    Debug unimplemented("CBaseFilter_Run", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediaFilter::GetState (retrieves the filter's state (running, stopped or paused))
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[in] dwMilliSecsTimeout Timeout interval in milliseconds. To block indifinitely pass
 *            INFINITE.
 * \param[out] State pointer to variable that receives a member of FILTER_STATE enumeration.
 *
 * \return S_OK success
 * \return E_POINTER Null pointer
 * \return VFW_S_STATE_INTERMEDATE Intermediate state
 * \return VFW_S_CANT_CUE The filter is active, but cannot deliver data.
 *
 */
static long STDCALL CBaseFilter_GetState(IBaseFilter* This,
					 /* [in] */ unsigned long dwMilliSecsTimeout,
					 // /* [out] */ FILTER_STATE *State)
					 void* State)
{
    Debug unimplemented("CBaseFilter_GetState", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediaFilter::SetSyncSource (sets the reference clock)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[in] pClock IReferenceClock interface of reference clock
 *
 * \return S_OK success
 * \return apripriate error otherwise
 *
 */
static long STDCALL CBaseFilter_SetSyncSource(IBaseFilter* This,
					      /* [in] */ IReferenceClock *pClock)
{
    Debug unimplemented("CBaseFilter_SetSyncSource", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediafilter::GetSyncSource (gets current reference clock)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[out] pClock address of variable that receives pointer to clock's 
 *  IReferenceClock interface 
 *
 * \return S_OK success
 * \return E_POINTER Null pointer
 *
 */
static long STDCALL CBaseFilter_GetSyncSource(IBaseFilter* This,
					      /* [out] */ IReferenceClock **pClock)
{
    Debug unimplemented("CBaseFilter_GetSyncSource", This);
    return E_NOTIMPL;
}


/**
 * \brief IBaseFilter::EnumPins (enumerates the pins of this filter)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[out] ppEnum address of variable that receives pointer to IEnumPins interface
 *
 * \return S_OK success
 * \return E_OUTOFMEMORY Insufficient memory
 * \return E_POINTER Null pointer
 *
 */
static long STDCALL CBaseFilter_EnumPins(IBaseFilter* This,
					 /* [out] */ IEnumPins **ppEnum)
{
    Debug printf("CBaseFilter_EnumPins(%p) called\n", This);
    *ppEnum = (IEnumPins*) CEnumPinsCreate(((CBaseFilter*)This)->pin, ((CBaseFilter*)This)->unused_pin);
    return 0;
}

/**
 * \brief IBaseFilter::FindPin (retrieves the pin with specified id)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[in] Id  constant wide string, containing pin id
 * \param[out] ppPin address of variable that receives pointer to pin's IPin interface
 *
 * \return S_OK success
 * \return E_POINTER Null pointer
 * \return VFW_E_NOT_FOUND Could not find a pin with specified id
 *
 * \note
 * Be sure to release the interface after use.
 *
 */
static long STDCALL CBaseFilter_FindPin(IBaseFilter* This,
					/* [string][in] */ const unsigned short* Id,
					/* [out] */ IPin **ppPin)
{
    Debug unimplemented("CBaseFilter_FindPin\n", This);
    return E_NOTIMPL;
}

/**
 * \brief IBaseFilter::QueryFilterInfo (retrieves information aboud the filter)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[out] pInfo pointer to FILTER_INFO structure
 *
 * \return S_OK success
 * \return E_POINTER Null pointer
 *
 * \note
 * If pGraph member of FILTER_INFO is not NULL, be sure to release IFilterGraph interface after use.
 *
 */
static long STDCALL CBaseFilter_QueryFilterInfo(IBaseFilter* This,
						// /* [out] */ FILTER_INFO *pInfo)
						void* pInfo)
{
    Debug unimplemented("CBaseFilter_QueryFilterInfo", This);
    return E_NOTIMPL;
}

/**
 * \brief IBaseFilter::JoinFilterGraph (notifies the filter that it has joined of left filter graph)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[in] pInfo pointer to graph's IFilterGraph interface or NULL if filter is leaving graph
 * \param[in] pName pointer to wide character string that specifies a name for the filter
 *
 * \return S_OK success
 * \return apropriate error code otherwise
 *
 * \remarks
 * Filter should not call to graph's AddRef method.
 * The IFilterGraph is guaranteed to be valid until graph manager calls this method again with 
 * the value NULL.
 *
 */
static long STDCALL CBaseFilter_JoinFilterGraph(IBaseFilter* This,
						/* [in] */ IFilterGraph* pGraph,
						/* [string][in] */ const unsigned short* pName)
{
    Debug unimplemented("CBaseFilter_JoinFilterGraph", This);
    return E_NOTIMPL;
}

/**
 * \brief IBaseFilter::QueryVendorInfo (retrieves a string containing vendor info)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[out] address of variable that receives pointer to a string containing vendor info
 *
 * \return S_OK success
 * \return E_POINTER Null pointer
 * \return E_NOTIMPL Not implemented
 *
 * \remarks
 * Call to CoTaskMemFree to free memory allocated for string
 *
 */
static long STDCALL CBaseFilter_QueryVendorInfo(IBaseFilter* This,
						/* [string][out] */ unsigned short** pVendorInfo)
{
    Debug unimplemented("CBaseFilter_QueryVendorInfo", This);
    return E_NOTIMPL;
}

/**
 * \brief CBaseFilter::GetPin (gets used pin)
 *
 * \param[in] This pointer to CBaseFilter object
 *
 * \return pointer to used pin's IPin interface
 *
 */
static IPin* CBaseFilter_GetPin(CBaseFilter* This)
{
    return This->pin;
}

/**
 * \brief CBaseFilter::GetUnusedPin (gets used pin)
 *
 * \param[in] This pointer to CBaseFilter object
 *
 * \return pointer to unused pin's IPin interface
 *
 */
static IPin* CBaseFilter_GetUnusedPin(CBaseFilter* This)
{
    return This->unused_pin;
}

/**
 * \brief CBaseFilter destructor
 *
 * \param[in] This pointer to CBaseFilter object
 *
 */
static void CBaseFilter_Destroy(CBaseFilter* This)
{
    if (This->vt)
	free(This->vt);
    if (This->pin)
	This->pin->vt->Release((IUnknown*)This->pin);
    if (This->unused_pin)
	This->unused_pin->vt->Release((IUnknown*)This->unused_pin);
    free(This);
}

IMPLEMENT_IUNKNOWN(CBaseFilter)

/**
 * \brief CBaseFilter constructor
 *
 * \param[in] type Pointer to media type for connection
 * \param[in] parent Pointer to parent CBaseFilter2 object
 *
 * \return pointer to CBaseFilter object or NULL if error occured
 *
 */
CBaseFilter* CBaseFilterCreate(const AM_MEDIA_TYPE* type, CBaseFilter2* parent)
{
    CBaseFilter* This = (CBaseFilter*) malloc(sizeof(CBaseFilter));
    if (!This)
	return NULL;

    This->refcount = 1;

    This->pin = (IPin*) CInputPinCreate(This, type);
    This->unused_pin = (IPin*) CRemotePinCreate(This, parent->GetPin(parent));

    This->vt = (IBaseFilter_vt*) malloc(sizeof(IBaseFilter_vt));
    if (!This->vt || !This->pin || !This->unused_pin)
    {
        CBaseFilter_Destroy(This);
        return NULL;
    }

    This->vt->QueryInterface = CBaseFilter_QueryInterface;
    This->vt->AddRef = CBaseFilter_AddRef;
    This->vt->Release = CBaseFilter_Release;
    This->vt->GetClassID = CBaseFilter_GetClassID;
    This->vt->Stop = CBaseFilter_Stop;
    This->vt->Pause = CBaseFilter_Pause;
    This->vt->Run = CBaseFilter_Run;
    This->vt->GetState = CBaseFilter_GetState;
    This->vt->SetSyncSource = CBaseFilter_SetSyncSource;
    This->vt->GetSyncSource = CBaseFilter_GetSyncSource;
    This->vt->EnumPins = CBaseFilter_EnumPins;
    This->vt->FindPin = CBaseFilter_FindPin;
    This->vt->QueryFilterInfo = CBaseFilter_QueryFilterInfo;
    This->vt->JoinFilterGraph = CBaseFilter_JoinFilterGraph;
    This->vt->QueryVendorInfo = CBaseFilter_QueryVendorInfo;

    This->interfaces[0] = IID_IUnknown;
    This->interfaces[1] = IID_IBaseFilter;

    This->GetPin = CBaseFilter_GetPin;
    This->GetUnusedPin = CBaseFilter_GetUnusedPin;

    return This;
}


/**************
 * BaseFilter2
 **************/


static long STDCALL CBaseFilter2_GetClassID(IBaseFilter* This,
					     /* [out] */ CLSID* pClassID)
{
    Debug unimplemented("CBaseFilter2_GetClassID", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediaFilter::Stop  (stops the filter)
 *
 * \param[in] This pointer to IBaseFilter interface
 *
 * \return S_OK success
 * \return S_FALSE transition is not complete
 *
 * \remarks
 * When filter is stopped it does onot deliver or process any samples and rejects any samples
 * from upstream filter. 
 * Transition may be asynchronous. In this case method should return S_FALSE.
 * Method always sets filter's state to State_Stopped even if error occured.
 *
 */
static long STDCALL CBaseFilter2_Stop(IBaseFilter* This)
{
    Debug unimplemented("CBaseFilter2_Stop", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediaFilter::Pause (pauses filter)
 *
 * \param[in] This pointer to IBaseFilter interface
 *
 * \return S_OK success
 * \return S_FALSE transition is not complete
 *
 * \remarks
 * When filter is paused it can receive, process and deliver samples.
 * Live source filters do not deliver any samples while paused.
 * Transition may be asynchronous. In this case method should return S_FALSE.
 * Method always sets filter's state to State_Stopped even if error occured.
 *
 */
static long STDCALL CBaseFilter2_Pause(IBaseFilter* This)
{
    Debug unimplemented("CBaseFilter2_Pause", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediaFilter::Run (runs the filter)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[in] tStart Reference time corresponding to stream time 0.
 *
 * \return S_OK success
 * \return S_FALSE transition is not complete
 *
 * \remarks
 * When filter is running it can receive, process and deliver samples. Source filters
 * generatesnew  samples, and renderers renders them.
 * Stream time is calculated as the current reference time minus tStart.
 * Graph Manager sets tStart slightly in the future according to graph latency.
 *
 */
static long STDCALL CBaseFilter2_Run(IBaseFilter* This, REFERENCE_TIME tStart)
{
    Debug unimplemented("CBaseFilter2_Run", This);
    return E_NOTIMPL;
}


/**
 * \brief IMediaFilter::GetState (retrieves the filter's state (running, stopped or paused))
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[in] dwMilliSecsTimeout Timeout interval in milliseconds. To block indifinitely pass
 *            INFINITE.
 * \param[out] State pointer to variable that receives a member of FILTER_STATE enumeration.
 *
 * \return S_OK success
 * \return E_POINTER Null pointer
 * \return VFW_S_STATE_INTERMEDATE Intermediate state
 * \return VFW_S_CANT_CUE The filter is active, but cannot deliver data.
 *
 */
static long STDCALL CBaseFilter2_GetState(IBaseFilter* This,
					  /* [in] */ unsigned long dwMilliSecsTimeout,
					  // /* [out] */ FILTER_STATE *State)
					  void* State)
{
    Debug unimplemented("CBaseFilter2_GetState", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediaFilter::SetSyncSource (sets the reference clock)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[in] pClock IReferenceClock interface of reference clock
 *
 * \return S_OK success
 * \return apripriate error otherwise
 *
 */
static long STDCALL CBaseFilter2_SetSyncSource(IBaseFilter* This,
					       /* [in] */ IReferenceClock* pClock)
{
    Debug unimplemented("CBaseFilter2_SetSyncSource", This);
    return E_NOTIMPL;
}

/**
 * \brief IMediafilter::GetSyncSource (gets current reference clock)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[out] pClock address of variable that receives pointer to clock's 
 *  IReferenceClock interface 
 *
 * \return S_OK success
 * \return E_POINTER Null pointer
 *
 */
static long STDCALL CBaseFilter2_GetSyncSource(IBaseFilter* This,
					       /* [out] */ IReferenceClock** pClock)
{
    Debug unimplemented("CBaseFilter2_GetSyncSource", This);
    return E_NOTIMPL;
}

/**
 * \brief IBaseFilter::EnumPins (enumerates the pins of this filter)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[out] ppEnum address of variable that receives pointer to IEnumPins interface
 *
 * \return S_OK success
 * \return E_OUTOFMEMORY Insufficient memory
 * \return E_POINTER Null pointer
 *
 */
static long STDCALL CBaseFilter2_EnumPins(IBaseFilter* This,
					  /* [out] */ IEnumPins** ppEnum)
{
    Debug printf("CBaseFilter2_EnumPins(%p) called\n", This);
    *ppEnum = (IEnumPins*) CEnumPinsCreate(((CBaseFilter2*)This)->pin, 0);
    return 0;
}

/**
 * \brief IBaseFilter::FindPin (retrieves the pin with specified id)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[in] Id  constant wide string, containing pin id
 * \param[out] ppPin address of variable that receives pointer to pin's IPin interface
 *
 * \return S_OK success
 * \return E_POINTER Null pointer
 * \return VFW_E_NOT_FOUND Could not find a pin with specified id
 *
 * \note
 * Be sure to release the interface after use.
 *
 */
static long STDCALL CBaseFilter2_FindPin(IBaseFilter* This,
					 /* [string][in] */ const unsigned short* Id,
					 /* [out] */ IPin** ppPin)
{
    Debug unimplemented("CBaseFilter2_FindPin", This);
    return E_NOTIMPL;
}

/**
 * \brief IBaseFilter::QueryFilterInfo (retrieves information aboud the filter)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[out] pInfo pointer to FILTER_INFO structure
 *
 * \return S_OK success
 * \return E_POINTER Null pointer
 *
 * \note
 * If pGraph member of FILTER_INFO is not NULL, be sure to release IFilterGraph interface after use.
 *
 */
static long STDCALL CBaseFilter2_QueryFilterInfo(IBaseFilter* This,
						 // /* [out] */ FILTER_INFO *pInfo)
						 void* pInfo)
{
    Debug unimplemented("CBaseFilter2_QueryFilterInfo", This);
    return E_NOTIMPL;
}

/**
 * \brief IBaseFilter::JoinFilterGraph (notifies the filter that it has joined of left filter graph)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[in] pInfo pointer to graph's IFilterGraph interface or NULL if filter is leaving graph
 * \param[in] pName pointer to wide character string that specifies a name for the filter
 *
 * \return S_OK success
 * \return apropriate error code otherwise
 *
 * \remarks
 * Filter should not call to graph's AddRef method.
 * The IFilterGraph is guaranteed to be valid until graph manager calls this method again with 
 * the value NULL.
 *
 */
static long STDCALL CBaseFilter2_JoinFilterGraph(IBaseFilter* This,
						 /* [in] */ IFilterGraph* pGraph,
						 /* [string][in] */
						  const unsigned short* pName)
{
    Debug unimplemented("CBaseFilter2_JoinFilterGraph", This);
    return E_NOTIMPL;
}

/**
 * \brief IBaseFilter::QueryVendorInfo (retrieves a string containing vendor info)
 *
 * \param[in] This pointer to IBaseFilter interface
 * \param[out] address of variable that receives pointer to a string containing vendor info
 *
 * \return S_OK success
 * \return E_POINTER Null pointer
 * \return E_NOTIMPL Not implemented
 *
 * \remarks
 * Call to CoTaskMemFree to free memory allocated for string
 *
 */
static long STDCALL CBaseFilter2_QueryVendorInfo(IBaseFilter* This,
						 /* [string][out] */
						 unsigned short** pVendorInfo)
{
    Debug unimplemented("CBaseFilter2_QueryVendorInfo", This);
    return E_NOTIMPL;
}

/**
 * \brief CBaseFilter2::GetPin (gets used pin)
 *
 * \param[in] This pointer to CBaseFilter2 object
 *
 * \return pointer to used pin's IPin interface
 *
 */
static IPin* CBaseFilter2_GetPin(CBaseFilter2* This)
{
    return This->pin;
}

/**
 * \brief CBaseFilter2 destructor
 *
 * \param[in] This pointer to CBaseFilter2 object
 *
 */
static void CBaseFilter2_Destroy(CBaseFilter2* This)
{
    Debug printf("CBaseFilter2_Destroy(%p) called\n", This);
    if (This->pin)
	This->pin->vt->Release((IUnknown*) This->pin);
    if (This->vt)
	free(This->vt);
    free(This);
}

IMPLEMENT_IUNKNOWN(CBaseFilter2)

static GUID CBaseFilter2_interf1 =
{0x76c61a30, 0xebe1, 0x11cf, {0x89, 0xf9, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb}};
/// IID_IAMNetShowPreroll
static GUID CBaseFilter2_interf2 =
{0xaae7e4e2, 0x6388, 0x11d1, {0x8d, 0x93, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2}};
/// IID_IAMRebuild
static GUID CBaseFilter2_interf3 =
{0x02ef04dd, 0x7580, 0x11d1, {0xbe, 0xce, 0x00, 0xc0, 0x4f, 0xb6, 0xe9, 0x37}};

/**
 * \brief CBaseFilter2 constructor
 *
 * \return pointer to CBaseFilter2 object or NULL if error occured
 *
 */
CBaseFilter2* CBaseFilter2Create()
{
    CBaseFilter2* This = (CBaseFilter2*) malloc(sizeof(CBaseFilter2));

    if (!This)
	return NULL;

    This->refcount = 1;
    This->pin = (IPin*) CRemotePin2Create(This);

    This->vt = (IBaseFilter_vt*) malloc(sizeof(IBaseFilter_vt));

    if (!This->pin || !This->vt)
    {
	CBaseFilter2_Destroy(This);
        return NULL;
    }

    memset(This->vt, 0, sizeof(IBaseFilter_vt));
    This->vt->QueryInterface = CBaseFilter2_QueryInterface;
    This->vt->AddRef = CBaseFilter2_AddRef;
    This->vt->Release = CBaseFilter2_Release;
    This->vt->GetClassID = CBaseFilter2_GetClassID;
    This->vt->Stop = CBaseFilter2_Stop;
    This->vt->Pause = CBaseFilter2_Pause;
    This->vt->Run = CBaseFilter2_Run;
    This->vt->GetState = CBaseFilter2_GetState;
    This->vt->SetSyncSource = CBaseFilter2_SetSyncSource;
    This->vt->GetSyncSource = CBaseFilter2_GetSyncSource;
    This->vt->EnumPins = CBaseFilter2_EnumPins;
    This->vt->FindPin = CBaseFilter2_FindPin;
    This->vt->QueryFilterInfo = CBaseFilter2_QueryFilterInfo;
    This->vt->JoinFilterGraph = CBaseFilter2_JoinFilterGraph;
    This->vt->QueryVendorInfo = CBaseFilter2_QueryVendorInfo;

    This->GetPin = CBaseFilter2_GetPin;

    This->interfaces[0] = IID_IUnknown;
    This->interfaces[1] = IID_IBaseFilter;
    This->interfaces[2] = CBaseFilter2_interf1;
    This->interfaces[3] = CBaseFilter2_interf2;
    This->interfaces[4] = CBaseFilter2_interf3;

    return This;
}


/*************
 * CRemotePin
 *************/


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
static long STDCALL CRemotePin_ConnectedTo(IPin* This, /* [out] */ IPin** pPin)
{
    Debug printf("CRemotePin_ConnectedTo(%p) called\n", This);
    if (!pPin)
	return E_INVALIDARG;
    *pPin = ((CRemotePin*)This)->remote_pin;
    (*pPin)->vt->AddRef((IUnknown*)(*pPin));
    return 0;
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
static long STDCALL CRemotePin_QueryDirection(IPin* This,
					      /* [out] */ PIN_DIRECTION* pPinDir)
{
    Debug printf("CRemotePin_QueryDirection(%p) called\n", This);
    if (!pPinDir)
	return E_INVALIDARG;
    *pPinDir=PINDIR_INPUT;
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
static long STDCALL CRemotePin_ConnectionMediaType(IPin* This, /* [out] */ AM_MEDIA_TYPE* pmt)
{
    Debug unimplemented("CRemotePin_ConnectionMediaType", This);
    return E_NOTIMPL;
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
static long STDCALL CRemotePin_QueryPinInfo(IPin* This, /* [out] */ PIN_INFO* pInfo)
{
    CBaseFilter* lparent = ((CRemotePin*)This)->parent;
    Debug printf("CRemotePin_QueryPinInfo(%p) called\n", This);
    pInfo->dir= PINDIR_INPUT;
    pInfo->pFilter = (IBaseFilter*) lparent;
    lparent->vt->AddRef((IUnknown*)lparent);
    pInfo->achName[0]=0;
    return 0;
}

/**
 * \brief CRemotePin destructor
 *
 * \param[in] This pointer to CRemotePin object
 *
 */
static void CRemotePin_Destroy(CRemotePin* This)
{
    Debug printf("CRemotePin_Destroy(%p) called\n", This);
    free(This->vt);
    free(This);
}

IMPLEMENT_IUNKNOWN(CRemotePin)

/**
 * \brief CRemotePin constructor
 *
 * \param[in] pt parent filter
 * \param[in] rpin remote pin
 *
 * \return pointer to CRemotePin or NULL if error occured
 *
 */
CRemotePin* CRemotePinCreate(CBaseFilter* pt, IPin* rpin)
{
    CRemotePin* This = (CRemotePin*) malloc(sizeof(CRemotePin));

    if (!This)
        return NULL;

    Debug printf("CRemotePinCreate() called -> %p\n", This);

    This->parent = pt;
    This->remote_pin = rpin;
    This->refcount = 1;

    This->vt = (IPin_vt*) malloc(sizeof(IPin_vt));

    if (!This->vt)
    {
	free(This);
	return NULL;
    }

    memset(This->vt, 0, sizeof(IPin_vt));
    This->vt->QueryInterface = CRemotePin_QueryInterface;
    This->vt->AddRef = CRemotePin_AddRef;
    This->vt->Release = CRemotePin_Release;
    This->vt->QueryDirection = CRemotePin_QueryDirection;
    This->vt->ConnectedTo = CRemotePin_ConnectedTo;
    This->vt->ConnectionMediaType = CRemotePin_ConnectionMediaType;
    This->vt->QueryPinInfo = CRemotePin_QueryPinInfo;

    This->interfaces[0] = IID_IUnknown;

    return This;
}


/*************
 * CRemotePin2
 *************/


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
static long STDCALL CRemotePin2_QueryPinInfo(IPin* This,
					     /* [out] */ PIN_INFO* pInfo)
{
    CBaseFilter2* lparent=((CRemotePin2*)This)->parent;
    Debug printf("CRemotePin2_QueryPinInfo(%p) called\n", This);
    pInfo->pFilter=(IBaseFilter*)lparent;
    lparent->vt->AddRef((IUnknown*)lparent);
    pInfo->dir=PINDIR_OUTPUT;
    pInfo->achName[0]=0;
    return 0;
}

/**
 * \brief CremotePin2 destructor
 *
 * \param This pointer to CRemotePin2 object
 *
 *  FIXME - not being released!
 */
static void CRemotePin2_Destroy(CRemotePin2* This)
{
    Debug printf("CRemotePin2_Destroy(%p) called\n", This);
    free(This->vt);
    free(This);
}

IMPLEMENT_IUNKNOWN(CRemotePin2)

/**
 * \brief CRemotePin2 contructor
 *
 * \param[in] p pointer to parent CBaseFilter2 object
 *
 * \return pointer to CRemotePin2 object or NULL if error occured
 *
 */
CRemotePin2* CRemotePin2Create(CBaseFilter2* p)
{
    CRemotePin2* This = (CRemotePin2*) malloc(sizeof(CRemotePin2));

    if (!This)
        return NULL;

    Debug printf("CRemotePin2Create() called -> %p\n", This);

    This->parent = p;
    This->refcount = 1;

    This->vt = (IPin_vt*) malloc(sizeof(IPin_vt));

    if (!This->vt)
    {
	free(This);
        return NULL;
    }

    memset(This->vt, 0, sizeof(IPin_vt));
    This->vt->QueryInterface = CRemotePin2_QueryInterface;
    This->vt->AddRef = CRemotePin2_AddRef;
    This->vt->Release = CRemotePin2_Release;
    This->vt->QueryPinInfo = CRemotePin2_QueryPinInfo;

    This->interfaces[0] = IID_IUnknown;

    return This;
}
