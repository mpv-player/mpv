#ifndef DS_INPUTPIN_H
#define DS_INPUTPIN_H

#include "interfaces.h"
#include "guids.h"
#include "iunk.h"

class CBaseFilter2;
class CBaseFilter: public IBaseFilter
{
    IPin* pin;
    IPin* unused_pin;
    static GUID interfaces[];
    DECLARE_IUNKNOWN(CBaseFilter)
public:
    CBaseFilter(const AM_MEDIA_TYPE& vhdr, CBaseFilter2* parent);
    ~CBaseFilter(){delete vt;pin->vt->Release((IUnknown*)pin);unused_pin->vt->Release((IUnknown*)unused_pin);}
    
    IPin* GetPin() {return pin;}
    IPin* GetUnusedPin() {return unused_pin;}

    static long STDCALL GetClassID (IBaseFilter * This,
				    /* [out] */ CLSID *pClassID);
    
    static long STDCALL Stop (IBaseFilter * This);
    static long STDCALL Pause (IBaseFilter * This);
    static long STDCALL Run (IBaseFilter * This, REFERENCE_TIME tStart);
    static long STDCALL GetState (IBaseFilter * This,
				  /* [in] */ unsigned long dwMilliSecsTimeout,
				  // /* [out] */ FILTER_STATE *State);
				  void* State);

    static long STDCALL SetSyncSource (IBaseFilter * This,
				       /* [in] */ IReferenceClock *pClock);
    static long STDCALL GetSyncSource (IBaseFilter * This,
				       /* [out] */ IReferenceClock **pClock);
    static long STDCALL EnumPins (IBaseFilter * This,
				  /* [out] */ IEnumPins **ppEnum);
    static long STDCALL FindPin (IBaseFilter * This,
				 /* [string][in] */ const unsigned short* Id,
				 /* [out] */ IPin **ppPin);
    static long STDCALL QueryFilterInfo (IBaseFilter * This,
					 ///* [out] */ FILTER_INFO *pInfo);
					 void* pInfo);
    static long STDCALL JoinFilterGraph (IBaseFilter * This,
					 /* [in] */ IFilterGraph *pGraph,
					 /* [string][in] */
					 const unsigned short* pName);
    static long STDCALL QueryVendorInfo (IBaseFilter * This,
					 /* [string][out] */
					 unsigned short* *pVendorInfo);
};

class CInputPin: public IPin
{
    AM_MEDIA_TYPE type;
    CBaseFilter* parent;
    static GUID interfaces[];
    DECLARE_IUNKNOWN(CInputPin)
public:
    CInputPin(CBaseFilter* parent, const AM_MEDIA_TYPE& vhdr);
    ~CInputPin(){delete vt;}
//    IPin* GetPin();

//    static long STDCALL QueryInterface(IUnknown* This, GUID* iid, void** ppv);
//    static long STDCALL AddRef(IUnknown* This);
//    static long STDCALL Release(IUnknown* This);

    static long STDCALL Connect ( 
	IPin * This,
        /* [in] */ IPin *pReceivePin,
        /* [in] */ AM_MEDIA_TYPE *pmt);
        
    static long STDCALL ReceiveConnection ( 
        IPin * This,
        /* [in] */ IPin *pConnector,
        /* [in] */ const AM_MEDIA_TYPE *pmt);
    
    static long STDCALL Disconnect ( 
        IPin * This);
    
    static long STDCALL ConnectedTo ( 
        IPin * This,
        /* [out] */ IPin **pPin);
    
    static long STDCALL ConnectionMediaType ( 
        IPin * This,
        /* [out] */ AM_MEDIA_TYPE *pmt);
    
    static long STDCALL QueryPinInfo ( 
        IPin * This,
        /* [out] */ PIN_INFO *pInfo);
    
    static long STDCALL QueryDirection ( 
        IPin * This,
        /* [out] */ PIN_DIRECTION *pPinDir);
    
    static long STDCALL QueryId ( 
        IPin * This,
        /* [out] */ unsigned short* *Id);
    
    static long STDCALL QueryAccept ( 
        IPin * This,
        /* [in] */ const AM_MEDIA_TYPE *pmt);
    
    static long STDCALL EnumMediaTypes ( 
        IPin * This,
        /* [out] */ IEnumMediaTypes **ppEnum);
    
    static long STDCALL QueryInternalConnections ( 
        IPin * This,
        /* [out] */ IPin **apPin,
        /* [out][in] */ unsigned long *nPin);
    
    static long STDCALL EndOfStream ( 
        IPin * This);
    
    static long STDCALL BeginFlush ( 
        IPin * This);
    
    static long STDCALL EndFlush ( 
        IPin * This);
    
    static long STDCALL NewSegment ( 
        IPin * This,
        /* [in] */ REFERENCE_TIME tStart,
        /* [in] */ REFERENCE_TIME tStop,
        /* [in] */ double dRate);
};

class CBaseFilter2: public IBaseFilter
{
    IPin* pin;
    static GUID interfaces[];
    DECLARE_IUNKNOWN(CBaseFilter2)
public:
    CBaseFilter2();
    ~CBaseFilter2(){delete vt;pin->vt->Release((IUnknown*)pin);}
    IPin* GetPin() {return pin;}
    
//    static long STDCALL QueryInterface(IUnknown* This, GUID* iid, void** ppv);
//    static long STDCALL AddRef(IUnknown* This);
//    static long STDCALL Release(IUnknown* This);
    static long STDCALL GetClassID ( 
        IBaseFilter * This,
        /* [out] */ CLSID *pClassID);
    
    static long STDCALL Stop ( 
        IBaseFilter * This);
    
    static long STDCALL Pause ( 
        IBaseFilter * This);
    
    static long STDCALL Run ( 
        IBaseFilter * This,
        REFERENCE_TIME tStart);
    
    static long STDCALL GetState ( 
        IBaseFilter * This,
        /* [in] */ unsigned long dwMilliSecsTimeout,
//        /* [out] */ FILTER_STATE *State);
    	void* State);
    
    static long STDCALL SetSyncSource ( 
        IBaseFilter * This,
        /* [in] */ IReferenceClock *pClock);
    
    static long STDCALL GetSyncSource ( 
        IBaseFilter * This,
        /* [out] */ IReferenceClock **pClock);
    
    static long STDCALL EnumPins ( 
        IBaseFilter * This,
        /* [out] */ IEnumPins **ppEnum);
    
    static long STDCALL FindPin ( 
        IBaseFilter * This,
        /* [string][in] */ const unsigned short* Id,
        /* [out] */ IPin **ppPin);
    
    static long STDCALL QueryFilterInfo ( 
        IBaseFilter * This,
//        /* [out] */ FILTER_INFO *pInfo);
	void* pInfo);
    
    static long STDCALL JoinFilterGraph ( 
        IBaseFilter * This,
        /* [in] */ IFilterGraph *pGraph,
        /* [string][in] */ const unsigned short* pName);
    
    static long STDCALL QueryVendorInfo ( 
        IBaseFilter * This,
        /* [string][out] */ unsigned short* *pVendorInfo);
};


struct CRemotePin: public IPin
{
    CBaseFilter* parent;
    IPin* remote_pin;
    static GUID interfaces[];
    DECLARE_IUNKNOWN(CRemotePin)
    CRemotePin(CBaseFilter* pt, IPin* rpin);
    ~CRemotePin(){delete vt;}
};

struct CRemotePin2: public IPin
{
    CBaseFilter2* parent;
    static GUID interfaces[];
    DECLARE_IUNKNOWN(CRemotePin2)
    CRemotePin2(CBaseFilter2* parent);
    ~CRemotePin2(){delete vt;}
};

#endif /* DS_INPUTPIN_H */
