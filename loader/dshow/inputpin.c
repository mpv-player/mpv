#include "inputpin.h"
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

static long STDCALL CEnumPins_Skip(IEnumPins* This,
				   /* [in] */ unsigned long cMediaTypes)
{
    Debug unimplemented("CEnumPins_Skip", This);
    return E_NOTIMPL;
}

static long STDCALL CEnumPins_Reset(IEnumPins* This)
{
    Debug printf("CEnumPins_Reset(%p) called\n", This);
    ((CEnumPins*)This)->counter = 0;
    return 0;
}

static long STDCALL CEnumPins_Clone(IEnumPins* This,
				    /* [out] */ IEnumPins** ppEnum)
{
    Debug unimplemented("CEnumPins_Clone", This);
    return E_NOTIMPL;
}

static void CEnumPins_Destroy(CEnumPins* This)
{
    free(This->vt);
    free(This);
}

IMPLEMENT_IUNKNOWN(CEnumPins)

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
 ***********/

static long STDCALL CInputPin_Connect(IPin* This,
				      /* [in] */ IPin* pReceivePin,
				      /* [in] */ AM_MEDIA_TYPE* pmt)
{
    Debug unimplemented("CInputPin_Connect", This);
    return E_NOTIMPL;
}

static long STDCALL CInputPin_ReceiveConnection(IPin* This,
						/* [in] */ IPin* pConnector,
						/* [in] */ const AM_MEDIA_TYPE *pmt)
{
    Debug unimplemented("CInputPin_ReceiveConnection", This);
    return E_NOTIMPL;
}

static long STDCALL CInputPin_Disconnect(IPin* This)
{
    Debug unimplemented("CInputPin_Disconnect", This);
    return E_NOTIMPL;
}

static long STDCALL CInputPin_ConnectedTo(IPin* This,
					  /* [out] */ IPin** pPin)
{
    Debug unimplemented("CInputPin_ConnectedTo", This);
    return E_NOTIMPL;
}

static long STDCALL CInputPin_ConnectionMediaType(IPin* This,
						  /* [out] */ AM_MEDIA_TYPE *pmt)
{
    Debug printf("CInputPin_ConnectionMediaType(%p) called\n", This);
    if (!pmt)
	return E_INVALIDARG;
    *pmt=((CInputPin*)This)->type;
    if (pmt->cbFormat > 0)
    {
	pmt->pbFormat=(char *)CoTaskMemAlloc(pmt->cbFormat);
	memcpy(pmt->pbFormat, ((CInputPin*)This)->type.pbFormat, pmt->cbFormat);
    }
    return 0;
}

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

static long STDCALL CInputPin_QueryDirection(IPin* This,
					      /* [out] */ PIN_DIRECTION *pPinDir)
{
    *pPinDir = PINDIR_OUTPUT;
    Debug printf("CInputPin_QueryDirection(%p) called\n", This);
    return 0;
}

static long STDCALL CInputPin_QueryId(IPin* This,
				       /* [out] */ unsigned short* *Id)
{
    Debug unimplemented("CInputPin_QueryId", This);
    return E_NOTIMPL;
}

static long STDCALL CInputPin_QueryAccept(IPin* This,
					  /* [in] */ const AM_MEDIA_TYPE* pmt)
{
    Debug unimplemented("CInputPin_QueryAccept", This);
    return E_NOTIMPL;
}

static long STDCALL CInputPin_EnumMediaTypes(IPin* This,
					     /* [out] */ IEnumMediaTypes** ppEnum)
{
    Debug unimplemented("CInputPin_EnumMediaTypes", This);
    return E_NOTIMPL;
}

static long STDCALL CInputPin_QueryInternalConnections(IPin* This,
						       /* [out] */ IPin** apPin,
						       /* [out][in] */ unsigned long *nPin)
{
    Debug unimplemented("CInputPin_QueryInternalConnections", This);
    return E_NOTIMPL;
}

static long STDCALL CInputPin_EndOfStream(IPin * This)
{
    Debug unimplemented("CInputPin_EndOfStream", This);
    return E_NOTIMPL;
}


static long STDCALL CInputPin_BeginFlush(IPin * This)
{
    Debug unimplemented("CInputPin_BeginFlush", This);
    return E_NOTIMPL;
}


static long STDCALL CInputPin_EndFlush(IPin* This)
{
    Debug unimplemented("CInputPin_EndFlush", This);
    return E_NOTIMPL;
}

static long STDCALL CInputPin_NewSegment(IPin* This,
					  /* [in] */ REFERENCE_TIME tStart,
					  /* [in] */ REFERENCE_TIME tStop,
					  /* [in] */ double dRate)
{
    Debug unimplemented("CInputPin_NewSegment", This);
    return E_NOTIMPL;
}

static void CInputPin_Destroy(CInputPin* This)
{
    free(This->vt);
    free(This);
}

IMPLEMENT_IUNKNOWN(CInputPin)

CInputPin* CInputPinCreate(CBaseFilter* p, const AM_MEDIA_TYPE* amt)
{
    CInputPin* This = (CInputPin*) malloc(sizeof(CInputPin));

    if (!This)
        return NULL;

    This->refcount = 1;
    This->parent = p;
    This->type = *amt;

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

static long STDCALL CBaseFilter_Stop(IBaseFilter* This)
{
    Debug unimplemented("CBaseFilter_Stop", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter_Pause(IBaseFilter* This)
{
    Debug unimplemented("CBaseFilter_Pause", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter_Run(IBaseFilter* This, REFERENCE_TIME tStart)
{
    Debug unimplemented("CBaseFilter_Run", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter_GetState(IBaseFilter* This,
					 /* [in] */ unsigned long dwMilliSecsTimeout,
					 // /* [out] */ FILTER_STATE *State)
					 void* State)
{
    Debug unimplemented("CBaseFilter_GetState", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter_SetSyncSource(IBaseFilter* This,
					      /* [in] */ IReferenceClock *pClock)
{
    Debug unimplemented("CBaseFilter_SetSyncSource", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter_GetSyncSource(IBaseFilter* This,
					      /* [out] */ IReferenceClock **pClock)
{
    Debug unimplemented("CBaseFilter_GetSyncSource", This);
    return E_NOTIMPL;
}


static long STDCALL CBaseFilter_EnumPins(IBaseFilter* This,
					 /* [out] */ IEnumPins **ppEnum)
{
    Debug printf("CBaseFilter_EnumPins(%p) called\n", This);
    *ppEnum = (IEnumPins*) CEnumPinsCreate(((CBaseFilter*)This)->pin, ((CBaseFilter*)This)->unused_pin);
    return 0;
}

static long STDCALL CBaseFilter_FindPin(IBaseFilter* This,
					/* [string][in] */ const unsigned short* Id,
					/* [out] */ IPin **ppPin)
{
    Debug unimplemented("CBaseFilter_FindPin\n", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter_QueryFilterInfo(IBaseFilter* This,
						// /* [out] */ FILTER_INFO *pInfo)
						void* pInfo)
{
    Debug unimplemented("CBaseFilter_QueryFilterInfo", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter_JoinFilterGraph(IBaseFilter* This,
						/* [in] */ IFilterGraph* pGraph,
						/* [string][in] */ const unsigned short* pName)
{
    Debug unimplemented("CBaseFilter_JoinFilterGraph", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter_QueryVendorInfo(IBaseFilter* This,
						/* [string][out] */ unsigned short** pVendorInfo)
{
    Debug unimplemented("CBaseFilter_QueryVendorInfo", This);
    return E_NOTIMPL;
}

static IPin* CBaseFilter_GetPin(CBaseFilter* This)
{
    return This->pin;
}

static IPin* CBaseFilter_GetUnusedPin(CBaseFilter* This)
{
    return This->unused_pin;
}

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

static long STDCALL CBaseFilter2_Stop(IBaseFilter* This)
{
    Debug unimplemented("CBaseFilter2_Stop", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter2_Pause(IBaseFilter* This)
{
    Debug unimplemented("CBaseFilter2_Pause", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter2_Run(IBaseFilter* This, REFERENCE_TIME tStart)
{
    Debug unimplemented("CBaseFilter2_Run", This);
    return E_NOTIMPL;
}


static long STDCALL CBaseFilter2_GetState(IBaseFilter* This,
					  /* [in] */ unsigned long dwMilliSecsTimeout,
					  // /* [out] */ FILTER_STATE *State)
					  void* State)
{
    Debug unimplemented("CBaseFilter2_GetState", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter2_SetSyncSource(IBaseFilter* This,
					       /* [in] */ IReferenceClock* pClock)
{
    Debug unimplemented("CBaseFilter2_SetSyncSource", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter2_GetSyncSource(IBaseFilter* This,
					       /* [out] */ IReferenceClock** pClock)
{
    Debug unimplemented("CBaseFilter2_GetSyncSource", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter2_EnumPins(IBaseFilter* This,
					  /* [out] */ IEnumPins** ppEnum)
{
    Debug printf("CBaseFilter2_EnumPins(%p) called\n", This);
    *ppEnum = (IEnumPins*) CEnumPinsCreate(((CBaseFilter2*)This)->pin, 0);
    return 0;
}

static long STDCALL CBaseFilter2_FindPin(IBaseFilter* This,
					 /* [string][in] */ const unsigned short* Id,
					 /* [out] */ IPin** ppPin)
{
    Debug unimplemented("CBaseFilter2_FindPin", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter2_QueryFilterInfo(IBaseFilter* This,
						 // /* [out] */ FILTER_INFO *pInfo)
						 void* pInfo)
{
    Debug unimplemented("CBaseFilter2_QueryFilterInfo", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter2_JoinFilterGraph(IBaseFilter* This,
						 /* [in] */ IFilterGraph* pGraph,
						 /* [string][in] */
						  const unsigned short* pName)
{
    Debug unimplemented("CBaseFilter2_JoinFilterGraph", This);
    return E_NOTIMPL;
}

static long STDCALL CBaseFilter2_QueryVendorInfo(IBaseFilter* This,
						 /* [string][out] */
						 unsigned short** pVendorInfo)
{
    Debug unimplemented("CBaseFilter2_QueryVendorInfo", This);
    return E_NOTIMPL;
}

static IPin* CBaseFilter2_GetPin(CBaseFilter2* This)
{
    return This->pin;
}

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
static GUID CBaseFilter2_interf2 =
{0xaae7e4e2, 0x6388, 0x11d1, {0x8d, 0x93, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2}};
static GUID CBaseFilter2_interf3 =
{0x02ef04dd, 0x7580, 0x11d1, {0xbe, 0xce, 0x00, 0xc0, 0x4f, 0xb6, 0xe9, 0x37}};

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


static long STDCALL CRemotePin_ConnectedTo(IPin* This, /* [out] */ IPin** pPin)
{
    Debug printf("CRemotePin_ConnectedTo(%p) called\n", This);
    if (!pPin)
	return E_INVALIDARG;
    *pPin = ((CRemotePin*)This)->remote_pin;
    (*pPin)->vt->AddRef((IUnknown*)(*pPin));
    return 0;
}

static long STDCALL CRemotePin_QueryDirection(IPin* This,
					      /* [out] */ PIN_DIRECTION* pPinDir)
{
    Debug printf("CRemotePin_QueryDirection(%p) called\n", This);
    if (!pPinDir)
	return E_INVALIDARG;
    *pPinDir=PINDIR_INPUT;
    return 0;
}

static long STDCALL CRemotePin_ConnectionMediaType(IPin* This, /* [out] */ AM_MEDIA_TYPE* pmt)
{
    Debug unimplemented("CRemotePin_ConnectionMediaType", This);
    return E_NOTIMPL;
}

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

static void CRemotePin_Destroy(CRemotePin* This)
{
    Debug printf("CRemotePin_Destroy(%p) called\n", This);
    free(This->vt);
    free(This);
}

IMPLEMENT_IUNKNOWN(CRemotePin)

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

// FIXME - not being released!
static void CRemotePin2_Destroy(CRemotePin2* This)
{
    Debug printf("CRemotePin2_Destroy(%p) called\n", This);
    free(This->vt);
    free(This);
}

IMPLEMENT_IUNKNOWN(CRemotePin2)

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
