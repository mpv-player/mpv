#include "inputpin.h"
#include "wine/winerror.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

GUID CInputPin::interfaces[]=
{
    IID_IUnknown,
};
IMPLEMENT_IUNKNOWN(CInputPin)

GUID CRemotePin::interfaces[]=
{
    IID_IUnknown,
};
IMPLEMENT_IUNKNOWN(CRemotePin)

GUID CRemotePin2::interfaces[]=
{
    IID_IUnknown,
};
IMPLEMENT_IUNKNOWN(CRemotePin2)

GUID CBaseFilter::interfaces[]=
{
    IID_IUnknown,
    IID_IBaseFilter,
};
IMPLEMENT_IUNKNOWN(CBaseFilter)

GUID CBaseFilter2::interfaces[]=
{
    IID_IUnknown,
    IID_IBaseFilter,
    {0x76c61a30, 0xebe1, 0x11cf, {0x89, 0xf9, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb}},
    {0xaae7e4e2, 0x6388, 0x11d1, {0x8d, 0x93, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2}},
    {0x02ef04dd, 0x7580, 0x11d1, {0xbe, 0xce, 0x00, 0xc0, 0x4f, 0xb6, 0xe9, 0x37}},
};
IMPLEMENT_IUNKNOWN(CBaseFilter2)

class CEnumPins: public IEnumPins
{
    IPin* pin1;
    IPin* pin2;
    int counter;
    static GUID interfaces[];
    DECLARE_IUNKNOWN(CEnumPins)
public:
    CEnumPins(IPin*, IPin* =0);
    ~CEnumPins(){delete vt;}
    static long STDCALL Next (IEnumPins * This,
			      /* [in] */ unsigned long cMediaTypes,
			      /* [size_is][out] */ IPin **ppMediaTypes,
			      /* [out] */ unsigned long *pcFetched);
    static long STDCALL Skip (IEnumPins * This,
			      /* [in] */ unsigned long cMediaTypes);
    static long STDCALL Reset (IEnumPins * This);
    static long STDCALL Clone (IEnumPins * This,
			       /* [out] */ IEnumPins **ppEnum);
};

GUID CEnumPins::interfaces[]=
{
    IID_IUnknown,
    IID_IEnumPins,
};
IMPLEMENT_IUNKNOWN(CEnumPins)

CEnumPins::CEnumPins(IPin* p, IPin* pp): pin1(p), pin2(pp), counter(0), refcount(1)
{
    vt=new IEnumPins_vt;
    vt->QueryInterface = QueryInterface;
    vt->AddRef = AddRef;
    vt->Release = Release;
    vt->Next = Next;
    vt->Skip = Skip;
    vt->Reset = Reset;
    vt->Clone = Clone;
}

long STDCALL CEnumPins::Next(IEnumPins * This,
			     /* [in] */ unsigned long cMediaTypes,
			     /* [size_is][out] */ IPin **ppMediaTypes,
			     /* [out] */ unsigned long *pcFetched)
{
    Debug printf("CEnumPins::Next() called\n");
    if (!ppMediaTypes)
	return E_INVALIDARG;
    if (!pcFetched && (cMediaTypes!=1))
	return E_INVALIDARG;
    if (cMediaTypes<=0)
	return 0;
    int& lcounter=((CEnumPins*)This)->counter;

    IPin* lpin1=((CEnumPins*)This)->pin1;
    IPin* lpin2=((CEnumPins*)This)->pin2;
    if (((lcounter == 2) && lpin2) || ((lcounter == 1) && !lpin2))
    {
	if (pcFetched)
	    *pcFetched=0;
	return 1;
    }

    if (pcFetched)
	*pcFetched=1;
    if (lcounter==0)
    {
	*ppMediaTypes = lpin1;
	lpin1->vt->AddRef((IUnknown*)lpin1);
    }
    else
    {
	*ppMediaTypes = lpin2;
	lpin2->vt->AddRef((IUnknown*)lpin2);
    }
    lcounter++;
    if (cMediaTypes == 1)
	return 0;
    return 1;
}

long STDCALL CEnumPins::Skip(IEnumPins * This,
			     /* [in] */ unsigned long cMediaTypes)
{
    Debug printf("CEnumPins::Skip() called\n");
    return E_NOTIMPL;
}

long STDCALL CEnumPins::Reset(IEnumPins * This)
{
    Debug printf("CEnumPins::Reset() called\n");
    ((CEnumPins*)This)->counter=0;
    return 0;
}

long STDCALL CEnumPins::Clone(IEnumPins * This,
			      /* [out] */ IEnumPins **ppEnum)
{
    Debug printf("CEnumPins::Clone() called\n");
    return E_NOTIMPL;
}

CInputPin::CInputPin(CBaseFilter* p, const AM_MEDIA_TYPE& vh)
     : type(vh)
{
    refcount = 1;
    parent = p;
    vt=new IPin_vt;
    vt->QueryInterface = QueryInterface;
    vt->AddRef = AddRef;
    vt->Release = Release;
    vt->Connect = Connect;
    vt->ReceiveConnection = ReceiveConnection;
    vt->Disconnect=Disconnect;
    vt->ConnectedTo = ConnectedTo;
    vt->ConnectionMediaType = ConnectionMediaType;
    vt->QueryPinInfo = QueryPinInfo;
    vt->QueryDirection = QueryDirection;
    vt->QueryId = QueryId;
    vt->QueryAccept = QueryAccept;
    vt->EnumMediaTypes = EnumMediaTypes;
    vt->QueryInternalConnections = QueryInternalConnections;
    vt->EndOfStream = EndOfStream;
    vt->BeginFlush = BeginFlush;
    vt->EndFlush = EndFlush;
    vt->NewSegment = NewSegment;
}

long STDCALL CInputPin::Connect (
    IPin * This,
    /* [in] */ IPin *pReceivePin,
    /* [in] */ AM_MEDIA_TYPE *pmt)
{
    Debug printf("CInputPin::Connect() called\n");
    return E_NOTIMPL;
}

long STDCALL CInputPin::ReceiveConnection(IPin * This,
					  /* [in] */ IPin *pConnector,
					  /* [in] */ const AM_MEDIA_TYPE *pmt)
{
    Debug printf("CInputPin::ReceiveConnection() called\n");
    return E_NOTIMPL;
}

long STDCALL CInputPin::Disconnect(IPin * This)
{
    Debug printf("CInputPin::Disconnect() called\n");
    return E_NOTIMPL;
}

long STDCALL CInputPin::ConnectedTo(IPin * This, /* [out] */ IPin **pPin)
{
    Debug printf("CInputPin::ConnectedTo() called\n");
    return E_NOTIMPL;
}

long STDCALL CInputPin::ConnectionMediaType(IPin * This,
					    /* [out] */ AM_MEDIA_TYPE *pmt)
{
    Debug printf("CInputPin::ConnectionMediaType() called\n");
    if(!pmt)return E_INVALIDARG;
    *pmt=((CInputPin*)This)->type;
    if(pmt->cbFormat>0)
    {
	pmt->pbFormat=(char *)CoTaskMemAlloc(pmt->cbFormat);
	memcpy(pmt->pbFormat, ((CInputPin*)This)->type.pbFormat, pmt->cbFormat);
    }
    return 0;
}

long STDCALL CInputPin::QueryPinInfo(IPin * This, /* [out] */ PIN_INFO *pInfo)
{
    Debug printf("CInputPin::QueryPinInfo() called\n");
    pInfo->dir=PINDIR_OUTPUT;
    CBaseFilter* lparent=((CInputPin*)This)->parent;
    pInfo->pFilter = lparent;
    lparent->vt->AddRef((IUnknown*)lparent);
    pInfo->achName[0]=0;
    return 0;
}

long STDCALL CInputPin::QueryDirection(IPin * This,
				       /* [out] */ PIN_DIRECTION *pPinDir)
{
    *pPinDir=PINDIR_OUTPUT;
    Debug printf("CInputPin::QueryDirection() called\n");
    return 0;
}

long STDCALL CInputPin::QueryId(IPin * This, /* [out] */ unsigned short* *Id)
{
    Debug printf("CInputPin::QueryId() called\n");
    return E_NOTIMPL;
}

long STDCALL CInputPin::QueryAccept(IPin * This,
				    /* [in] */ const AM_MEDIA_TYPE *pmt)
{
    Debug printf("CInputPin::QueryAccept() called\n");
    return E_NOTIMPL;
}


long STDCALL CInputPin::EnumMediaTypes (
    IPin * This,
    /* [out] */ IEnumMediaTypes **ppEnum)
{
    Debug printf("CInputPin::EnumMediaTypes() called\n");
    return E_NOTIMPL;
}


long STDCALL CInputPin::QueryInternalConnections(IPin * This,
						 /* [out] */ IPin **apPin,
						 /* [out][in] */ unsigned long *nPin)
{
    Debug printf("CInputPin::QueryInternalConnections() called\n");
    return E_NOTIMPL;
}

long STDCALL CInputPin::EndOfStream (IPin * This)
{
    Debug printf("CInputPin::EndOfStream() called\n");
    return E_NOTIMPL;
}


long STDCALL CInputPin::BeginFlush(IPin * This)
{
    Debug printf("CInputPin::BeginFlush() called\n");
    return E_NOTIMPL;
}


long STDCALL CInputPin::EndFlush(IPin * This)
{
    Debug printf("CInputPin::EndFlush() called\n");
    return E_NOTIMPL;
}

long STDCALL CInputPin::NewSegment(IPin * This,
				   /* [in] */ REFERENCE_TIME tStart,
				   /* [in] */ REFERENCE_TIME tStop,
				   /* [in] */ double dRate)
{
    Debug printf("CInputPin::NewSegment() called\n");
    return E_NOTIMPL;
}

CBaseFilter::CBaseFilter(const AM_MEDIA_TYPE& type, CBaseFilter2* parent)
{
    refcount = 1;
    pin=new CInputPin(this, type);
    unused_pin=new CRemotePin(this, parent->GetPin());
    vt=new IBaseFilter_vt;
    vt->QueryInterface = QueryInterface;
    vt->AddRef = AddRef;
    vt->Release = Release;
    vt->GetClassID = GetClassID;
    vt->Stop = Stop;
    vt->Pause = Pause;
    vt->Run = Run;
    vt->GetState = GetState;
    vt->SetSyncSource = SetSyncSource;
    vt->GetSyncSource = GetSyncSource;
    vt->EnumPins = EnumPins;
    vt->FindPin = FindPin;
    vt->QueryFilterInfo = QueryFilterInfo;
    vt->JoinFilterGraph = JoinFilterGraph;
    vt->QueryVendorInfo = QueryVendorInfo;
}

long STDCALL CBaseFilter::GetClassID(IBaseFilter * This,
				      /* [out] */ CLSID *pClassID)
{
    Debug printf("CBaseFilter::GetClassID() called\n");
    return E_NOTIMPL;
}

long STDCALL CBaseFilter::Stop(IBaseFilter * This)
{
    Debug printf("CBaseFilter::Stop() called\n");
    return E_NOTIMPL;
}

long STDCALL CBaseFilter::Pause(IBaseFilter * This)
{
    Debug printf("CBaseFilter::Pause() called\n");
    return E_NOTIMPL;
}

long STDCALL CBaseFilter::Run(IBaseFilter * This,
			      REFERENCE_TIME tStart)
{
    Debug printf("CBaseFilter::Run() called\n");
    return E_NOTIMPL;
}

long STDCALL CBaseFilter::GetState(IBaseFilter * This,
				   /* [in] */ unsigned long dwMilliSecsTimeout,
				   // /* [out] */ FILTER_STATE *State)
				   void* State)
{
    Debug printf("CBaseFilter::GetState() called\n");
    return E_NOTIMPL;
}

long STDCALL CBaseFilter::SetSyncSource(IBaseFilter * This,
					/* [in] */ IReferenceClock *pClock)
{
    Debug printf("CBaseFilter::SetSyncSource() called\n");
    return E_NOTIMPL;
}

long STDCALL CBaseFilter::GetSyncSource (
        IBaseFilter * This,
        /* [out] */ IReferenceClock **pClock)
{
    Debug printf("CBaseFilter::GetSyncSource() called\n");
    return E_NOTIMPL;
}


long STDCALL CBaseFilter::EnumPins (
        IBaseFilter * This,
        /* [out] */ IEnumPins **ppEnum)
{
    Debug printf("CBaseFilter::EnumPins() called\n");
    *ppEnum=new CEnumPins(((CBaseFilter*)This)->pin, ((CBaseFilter*)This)->unused_pin);
    return 0;
}


long STDCALL CBaseFilter::FindPin (
        IBaseFilter * This,
        /* [string][in] */ const unsigned short* Id,
        /* [out] */ IPin **ppPin)
{
    Debug printf("CBaseFilter::FindPin() called\n");
    return E_NOTIMPL;
}


long STDCALL CBaseFilter::QueryFilterInfo (
        IBaseFilter * This,
//        /* [out] */ FILTER_INFO *pInfo)
	void* pInfo)
{
    Debug printf("CBaseFilter::QueryFilterInfo() called\n");
    return E_NOTIMPL;
}


long STDCALL CBaseFilter::JoinFilterGraph (
        IBaseFilter * This,
        /* [in] */ IFilterGraph *pGraph,
        /* [string][in] */ const unsigned short* pName)
{
    Debug printf("CBaseFilter::JoinFilterGraph() called\n");
    return E_NOTIMPL;
}


long STDCALL CBaseFilter::QueryVendorInfo (
        IBaseFilter * This,
        /* [string][out] */ unsigned short* *pVendorInfo)
{
    Debug printf("CBaseFilter::QueryVendorInfo() called\n");
    return E_NOTIMPL;
}


CBaseFilter2::CBaseFilter2() : refcount(1)
{
    pin=new CRemotePin2(this);
    vt=new IBaseFilter_vt;
    memset(vt, 0, sizeof (IBaseFilter_vt));
    vt->QueryInterface = QueryInterface;
    vt->AddRef = AddRef;
    vt->Release = Release;
    vt->GetClassID = GetClassID;
    vt->Stop = Stop;
    vt->Pause = Pause;
    vt->Run = Run;
    vt->GetState = GetState;
    vt->SetSyncSource = SetSyncSource;
    vt->GetSyncSource = GetSyncSource;
    vt->EnumPins = EnumPins;
    vt->FindPin = FindPin;
    vt->QueryFilterInfo = QueryFilterInfo;
    vt->JoinFilterGraph = JoinFilterGraph;
    vt->QueryVendorInfo = QueryVendorInfo;
}




long STDCALL CBaseFilter2::GetClassID (
        IBaseFilter * This,
        /* [out] */ CLSID *pClassID)
{
    Debug printf("CBaseFilter2::GetClassID() called\n");
    return E_NOTIMPL;
}

long STDCALL CBaseFilter2::Stop (
        IBaseFilter * This)
{
    Debug printf("CBaseFilter2::Stop() called\n");
    return E_NOTIMPL;
}


long STDCALL CBaseFilter2::Pause (IBaseFilter * This)
{
    Debug printf("CBaseFilter2::Pause() called\n");
    return E_NOTIMPL;
}

long STDCALL CBaseFilter2::Run (IBaseFilter * This, REFERENCE_TIME tStart)
{
    Debug printf("CBaseFilter2::Run() called\n");
    return E_NOTIMPL;
}


long STDCALL CBaseFilter2::GetState (
        IBaseFilter * This,
        /* [in] */ unsigned long dwMilliSecsTimeout,
//        /* [out] */ FILTER_STATE *State)
    	void* State)
{
    Debug printf("CBaseFilter2::GetState() called\n");
    return E_NOTIMPL;
}


long STDCALL CBaseFilter2::SetSyncSource (
        IBaseFilter * This,
        /* [in] */ IReferenceClock *pClock)
{
    Debug printf("CBaseFilter2::SetSyncSource() called\n");
    return E_NOTIMPL;
}


long STDCALL CBaseFilter2::GetSyncSource (
        IBaseFilter * This,
        /* [out] */ IReferenceClock **pClock)
{
    Debug printf("CBaseFilter2::GetSyncSource() called\n");
    return E_NOTIMPL;
}


long STDCALL CBaseFilter2::EnumPins (
        IBaseFilter * This,
        /* [out] */ IEnumPins **ppEnum)
{
    Debug printf("CBaseFilter2::EnumPins() called\n");
    *ppEnum=new CEnumPins(((CBaseFilter2*)This)->pin);
    return 0;
}


long STDCALL CBaseFilter2::FindPin (
        IBaseFilter * This,
        /* [string][in] */ const unsigned short* Id,
        /* [out] */ IPin **ppPin)
{
    Debug printf("CBaseFilter2::FindPin() called\n");
    return E_NOTIMPL;
}


long STDCALL CBaseFilter2::QueryFilterInfo (
        IBaseFilter * This,
//        /* [out] */ FILTER_INFO *pInfo)
	void* pInfo)
{
    Debug printf("CBaseFilter2::QueryFilterInfo() called\n");
    return E_NOTIMPL;
}


long STDCALL CBaseFilter2::JoinFilterGraph(IBaseFilter * This,
					   /* [in] */ IFilterGraph *pGraph,
					   /* [string][in] */
					   const unsigned short* pName)
{
    Debug printf("CBaseFilter2::JoinFilterGraph() called\n");
    return E_NOTIMPL;
}

long STDCALL CBaseFilter2::QueryVendorInfo(IBaseFilter * This,
					   /* [string][out] */
					   unsigned short* *pVendorInfo)
{
    Debug printf("CBaseFilter2::QueryVendorInfo() called\n");
    return E_NOTIMPL;
}

static long STDCALL CRemotePin_ConnectedTo(IPin * This, /* [out] */ IPin **pPin)
{
    Debug printf("CRemotePin::ConnectedTo called\n");
    if (!pPin)
	return E_INVALIDARG;
    *pPin=((CRemotePin*)This)->remote_pin;
    (*pPin)->vt->AddRef((IUnknown*)(*pPin));
    return 0;
}

static long STDCALL CRemotePin_QueryDirection(IPin * This,
					      /* [out] */ PIN_DIRECTION *pPinDir)
{
    Debug printf("CRemotePin::QueryDirection called\n");
    if (!pPinDir)
	return E_INVALIDARG;
    *pPinDir=PINDIR_INPUT;
    return 0;
}

static long STDCALL CRemotePin_ConnectionMediaType(IPin* This, /* [out] */ AM_MEDIA_TYPE* pmt)
{
    Debug printf("CRemotePin::ConnectionMediaType() called\n");
    return E_NOTIMPL;
}

static long STDCALL CRemotePin_QueryPinInfo(IPin* This, /* [out] */ PIN_INFO* pInfo)
{
    Debug printf("CRemotePin::QueryPinInfo() called\n");
    pInfo->dir=PINDIR_INPUT;
    CBaseFilter* lparent = ((CRemotePin*)This)->parent;
    pInfo->pFilter = lparent;
    lparent->vt->AddRef((IUnknown*)lparent);
    pInfo->achName[0]=0;
    return 0;
}


static long STDCALL CRemotePin2_QueryPinInfo(IPin * This,
				       /* [out] */ PIN_INFO *pInfo)
{
    Debug printf("CRemotePin2::QueryPinInfo called\n");
    CBaseFilter2* lparent=((CRemotePin2*)This)->parent;
    pInfo->pFilter=(IBaseFilter*)lparent;
    lparent->vt->AddRef((IUnknown*)lparent);
    pInfo->dir=PINDIR_OUTPUT;
    pInfo->achName[0]=0;
    return 0;
}

CRemotePin::CRemotePin(CBaseFilter* pt, IPin* rpin): parent(pt), remote_pin(rpin),
    refcount(1)
{
    vt = new IPin_vt;
    memset(vt, 0, sizeof(IPin_vt));
    vt->QueryInterface = QueryInterface;
    vt->AddRef = AddRef;
    vt->Release = Release;
    vt->QueryDirection = CRemotePin_QueryDirection;
    vt->ConnectedTo = CRemotePin_ConnectedTo;
    vt->ConnectionMediaType = CRemotePin_ConnectionMediaType;
    vt->QueryPinInfo = CRemotePin_QueryPinInfo;
}

CRemotePin2::CRemotePin2(CBaseFilter2* p):parent(p),
    refcount(1)
{
    vt = new IPin_vt;
    memset(vt, 0, sizeof(IPin_vt));
    vt->QueryInterface = QueryInterface;
    vt->AddRef = AddRef;
    vt->Release = Release;
    vt->QueryPinInfo = CRemotePin2_QueryPinInfo;
}
