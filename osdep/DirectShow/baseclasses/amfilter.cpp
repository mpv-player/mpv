//------------------------------------------------------------------------------
// File: AMFilter.cpp
//
// Desc: DirectShow base classes - implements class hierarchy for streams
//       architecture.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

//=====================================================================
//=====================================================================
// The following classes are declared in this header:
//
//
// CBaseMediaFilter            Basic IMediaFilter support (abstract class)
// CBaseFilter                 Support for IBaseFilter (incl. IMediaFilter)
// CEnumPins                   Enumerate input and output pins
// CEnumMediaTypes             Enumerate the preferred pin formats
// CBasePin                    Abstract base class for IPin interface
//    CBaseOutputPin           Adds data provider member functions
//    CBaseInputPin            Implements IMemInputPin interface
// CMediaSample                Basic transport unit for IMemInputPin
// CBaseAllocator              General list guff for most allocators
//    CMemAllocator            Implements memory buffer allocation
//
//=====================================================================
//=====================================================================

#include <streams.h>
#include <strsafe.h>

#ifdef DXMPERF
#include "dxmperf.h"
#endif // DXMPERF

//=====================================================================
// Helpers
//=====================================================================
STDAPI CreateMemoryAllocator(__deref_out IMemAllocator **ppAllocator)
{
    return CoCreateInstance(CLSID_MemoryAllocator, 0, CLSCTX_INPROC_SERVER, IID_IMemAllocator, (void **)ppAllocator);
}

//  Put this one here rather than in ctlutil.cpp to avoid linking
//  anything brought in by ctlutil.cpp
STDAPI CreatePosPassThru(__in_opt LPUNKNOWN pAgg, BOOL bRenderer, IPin *pPin, __deref_out IUnknown **ppPassThru)
{
    *ppPassThru = NULL;
    IUnknown *pUnkSeek;
    HRESULT hr = CoCreateInstance(CLSID_SeekingPassThru, pAgg, CLSCTX_INPROC_SERVER, IID_IUnknown, (void **)&pUnkSeek);
    if (FAILED(hr))
    {
        return hr;
    }

    ISeekingPassThru *pPassThru;
    hr = pUnkSeek->QueryInterface(IID_ISeekingPassThru, (void **)&pPassThru);
    if (FAILED(hr))
    {
        pUnkSeek->Release();
        return hr;
    }
    hr = pPassThru->Init(bRenderer, pPin);
    pPassThru->Release();
    if (FAILED(hr))
    {
        pUnkSeek->Release();
        return hr;
    }
    *ppPassThru = pUnkSeek;
    return S_OK;
}

#define CONNECT_TRACE_LEVEL 3

//=====================================================================
//=====================================================================
// Implements CBaseMediaFilter
//=====================================================================
//=====================================================================

/* Constructor */

CBaseMediaFilter::CBaseMediaFilter(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN pUnk, __in CCritSec *pLock,
                                   REFCLSID clsid)
    : CUnknown(pName, pUnk)
    , m_pLock(pLock)
    , m_clsid(clsid)
    , m_State(State_Stopped)
    , m_pClock(NULL)
{
}

/* Destructor */

CBaseMediaFilter::~CBaseMediaFilter()
{
    // must be stopped, but can't call Stop here since
    // our critsec has been destroyed.

    /* Release any clock we were using */

    if (m_pClock)
    {
        m_pClock->Release();
        m_pClock = NULL;
    }
}

/* Override this to say what interfaces we support and where */

STDMETHODIMP
CBaseMediaFilter::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    if (riid == IID_IMediaFilter)
    {
        return GetInterface((IMediaFilter *)this, ppv);
    }
    else if (riid == IID_IPersist)
    {
        return GetInterface((IPersist *)this, ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

/* Return the filter's clsid */
STDMETHODIMP
CBaseMediaFilter::GetClassID(__out CLSID *pClsID)
{
    CheckPointer(pClsID, E_POINTER);
    ValidateReadWritePtr(pClsID, sizeof(CLSID));
    *pClsID = m_clsid;
    return NOERROR;
}

/* Override this if your state changes are not done synchronously */

STDMETHODIMP
CBaseMediaFilter::GetState(DWORD dwMSecs, __out FILTER_STATE *State)
{
    UNREFERENCED_PARAMETER(dwMSecs);
    CheckPointer(State, E_POINTER);
    ValidateReadWritePtr(State, sizeof(FILTER_STATE));

    *State = m_State;
    return S_OK;
}

/* Set the clock we will use for synchronisation */

STDMETHODIMP
CBaseMediaFilter::SetSyncSource(__inout_opt IReferenceClock *pClock)
{
    CAutoLock cObjectLock(m_pLock);

    // Ensure the new one does not go away - even if the same as the old
    if (pClock)
    {
        pClock->AddRef();
    }

    // if we have a clock, release it
    if (m_pClock)
    {
        m_pClock->Release();
    }

    // Set the new reference clock (might be NULL)
    // Should we query it to ensure it is a clock?  Consider for a debug build.
    m_pClock = pClock;

    return NOERROR;
}

/* Return the clock we are using for synchronisation */
STDMETHODIMP
CBaseMediaFilter::GetSyncSource(__deref_out_opt IReferenceClock **pClock)
{
    CheckPointer(pClock, E_POINTER);
    ValidateReadWritePtr(pClock, sizeof(IReferenceClock *));
    CAutoLock cObjectLock(m_pLock);

    if (m_pClock)
    {
        // returning an interface... addref it...
        m_pClock->AddRef();
    }
    *pClock = (IReferenceClock *)m_pClock;
    return NOERROR;
}

/* Put the filter into a stopped state */

STDMETHODIMP
CBaseMediaFilter::Stop()
{
    CAutoLock cObjectLock(m_pLock);

    m_State = State_Stopped;
    return S_OK;
}

/* Put the filter into a paused state */

STDMETHODIMP
CBaseMediaFilter::Pause()
{
    CAutoLock cObjectLock(m_pLock);

    m_State = State_Paused;
    return S_OK;
}

// Put the filter into a running state.

// The time parameter is the offset to be added to the samples'
// stream time to get the reference time at which they should be presented.
//
// you can either add these two and compare it against the reference clock,
// or you can call CBaseMediaFilter::StreamTime and compare that against
// the sample timestamp.

STDMETHODIMP
CBaseMediaFilter::Run(REFERENCE_TIME tStart)
{
    CAutoLock cObjectLock(m_pLock);

    // remember the stream time offset
    m_tStart = tStart;

    if (m_State == State_Stopped)
    {
        HRESULT hr = Pause();

        if (FAILED(hr))
        {
            return hr;
        }
    }
    m_State = State_Running;
    return S_OK;
}

//
// return the current stream time - samples with start timestamps of this
// time or before should be rendered by now
HRESULT
CBaseMediaFilter::StreamTime(CRefTime &rtStream)
{
    // Caller must lock for synchronization
    // We can't grab the filter lock because we want to be able to call
    // this from worker threads without deadlocking

    if (m_pClock == NULL)
    {
        return VFW_E_NO_CLOCK;
    }

    // get the current reference time
    HRESULT hr = m_pClock->GetTime((REFERENCE_TIME *)&rtStream);
    if (FAILED(hr))
    {
        return hr;
    }

    // subtract the stream offset to get stream time
    rtStream -= m_tStart;

    return S_OK;
}

//=====================================================================
//=====================================================================
// Implements CBaseFilter
//=====================================================================
//=====================================================================

/* Override this to say what interfaces we support and where */

STDMETHODIMP CBaseFilter::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    /* Do we have this interface */

    if (riid == IID_IBaseFilter)
    {
        return GetInterface((IBaseFilter *)this, ppv);
    }
    else if (riid == IID_IMediaFilter)
    {
        return GetInterface((IMediaFilter *)this, ppv);
    }
    else if (riid == IID_IPersist)
    {
        return GetInterface((IPersist *)this, ppv);
    }
    else if (riid == IID_IAMovieSetup)
    {
        return GetInterface((IAMovieSetup *)this, ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

#ifdef DEBUG
STDMETHODIMP_(ULONG) CBaseFilter::NonDelegatingRelease()
{
    if (m_cRef == 1)
    {
        KASSERT(m_pGraph == NULL);
    }
    return CUnknown::NonDelegatingRelease();
}
#endif

/* Constructor */

CBaseFilter::CBaseFilter(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN pUnk, __in CCritSec *pLock, REFCLSID clsid)
    : CUnknown(pName, pUnk)
    , m_pLock(pLock)
    , m_clsid(clsid)
    , m_State(State_Stopped)
    , m_pClock(NULL)
    , m_pGraph(NULL)
    , m_pSink(NULL)
    , m_pName(NULL)
    , m_PinVersion(1)
{
#ifdef DXMPERF
    PERFLOG_CTOR(pName ? pName : L"CBaseFilter", (IBaseFilter *)this);
#endif // DXMPERF

    ASSERT(pLock != NULL);
}

/* Passes in a redundant HRESULT argument */

CBaseFilter::CBaseFilter(__in_opt LPCTSTR pName, __in_opt LPUNKNOWN pUnk, __in CCritSec *pLock, REFCLSID clsid,
                         __inout HRESULT *phr)
    : CUnknown(pName, pUnk)
    , m_pLock(pLock)
    , m_clsid(clsid)
    , m_State(State_Stopped)
    , m_pClock(NULL)
    , m_pGraph(NULL)
    , m_pSink(NULL)
    , m_pName(NULL)
    , m_PinVersion(1)
{
#ifdef DXMPERF
    PERFLOG_CTOR(pName ? pName : L"CBaseFilter", (IBaseFilter *)this);
#endif // DXMPERF

    ASSERT(pLock != NULL);
    UNREFERENCED_PARAMETER(phr);
}

#ifdef UNICODE
CBaseFilter::CBaseFilter(__in_opt LPCSTR pName, __in_opt LPUNKNOWN pUnk, __in CCritSec *pLock, REFCLSID clsid)
    : CUnknown(pName, pUnk)
    , m_pLock(pLock)
    , m_clsid(clsid)
    , m_State(State_Stopped)
    , m_pClock(NULL)
    , m_pGraph(NULL)
    , m_pSink(NULL)
    , m_pName(NULL)
    , m_PinVersion(1)
{
#ifdef DXMPERF
    PERFLOG_CTOR(L"CBaseFilter", (IBaseFilter *)this);
#endif // DXMPERF

    ASSERT(pLock != NULL);
}
CBaseFilter::CBaseFilter(__in_opt LPCSTR pName, __in_opt LPUNKNOWN pUnk, __in CCritSec *pLock, REFCLSID clsid,
                         __inout HRESULT *phr)
    : CUnknown(pName, pUnk)
    , m_pLock(pLock)
    , m_clsid(clsid)
    , m_State(State_Stopped)
    , m_pClock(NULL)
    , m_pGraph(NULL)
    , m_pSink(NULL)
    , m_pName(NULL)
    , m_PinVersion(1)
{
#ifdef DXMPERF
    PERFLOG_CTOR(L"CBaseFilter", (IBaseFilter *)this);
#endif // DXMPERF

    ASSERT(pLock != NULL);
    UNREFERENCED_PARAMETER(phr);
}
#endif

/* Destructor */

CBaseFilter::~CBaseFilter()
{
#ifdef DXMPERF
    PERFLOG_DTOR(L"CBaseFilter", (IBaseFilter *)this);
#endif // DXMPERF

    // NOTE we do NOT hold references on the filtergraph for m_pGraph or m_pSink
    // When we did we had the circular reference problem.  Nothing would go away.

    delete[] m_pName;

    // must be stopped, but can't call Stop here since
    // our critsec has been destroyed.

    /* Release any clock we were using */
    if (m_pClock)
    {
        m_pClock->Release();
        m_pClock = NULL;
    }
}

/* Return the filter's clsid */
STDMETHODIMP
CBaseFilter::GetClassID(__out CLSID *pClsID)
{
    CheckPointer(pClsID, E_POINTER);
    ValidateReadWritePtr(pClsID, sizeof(CLSID));
    *pClsID = m_clsid;
    return NOERROR;
}

/* Override this if your state changes are not done synchronously */
STDMETHODIMP
CBaseFilter::GetState(DWORD dwMSecs, __out FILTER_STATE *State)
{
    UNREFERENCED_PARAMETER(dwMSecs);
    CheckPointer(State, E_POINTER);
    ValidateReadWritePtr(State, sizeof(FILTER_STATE));

    *State = m_State;
    return S_OK;
}

/* Set the clock we will use for synchronisation */

STDMETHODIMP
CBaseFilter::SetSyncSource(__in_opt IReferenceClock *pClock)
{
    CAutoLock cObjectLock(m_pLock);

    // Ensure the new one does not go away - even if the same as the old
    if (pClock)
    {
        pClock->AddRef();
    }

    // if we have a clock, release it
    if (m_pClock)
    {
        m_pClock->Release();
    }

    // Set the new reference clock (might be NULL)
    // Should we query it to ensure it is a clock?  Consider for a debug build.
    m_pClock = pClock;

    return NOERROR;
}

/* Return the clock we are using for synchronisation */
STDMETHODIMP
CBaseFilter::GetSyncSource(__deref_out_opt IReferenceClock **pClock)
{
    CheckPointer(pClock, E_POINTER);
    ValidateReadWritePtr(pClock, sizeof(IReferenceClock *));
    CAutoLock cObjectLock(m_pLock);

    if (m_pClock)
    {
        // returning an interface... addref it...
        m_pClock->AddRef();
    }
    *pClock = (IReferenceClock *)m_pClock;
    return NOERROR;
}

// override CBaseMediaFilter Stop method, to deactivate any pins this
// filter has.
STDMETHODIMP
CBaseFilter::Stop()
{
    CAutoLock cObjectLock(m_pLock);
    HRESULT hr = NOERROR;

    // notify all pins of the state change
    if (m_State != State_Stopped)
    {
        int cPins = GetPinCount();
        for (int c = 0; c < cPins; c++)
        {

            CBasePin *pPin = GetPin(c);
            if (NULL == pPin)
            {
                break;
            }

            // Disconnected pins are not activated - this saves pins worrying
            // about this state themselves. We ignore the return code to make
            // sure everyone is inactivated regardless. The base input pin
            // class can return an error if it has no allocator but Stop can
            // be used to resync the graph state after something has gone bad

            if (pPin->IsConnected())
            {
                HRESULT hrTmp = pPin->Inactive();
                if (FAILED(hrTmp) && SUCCEEDED(hr))
                {
                    hr = hrTmp;
                }
            }
        }
    }

#ifdef DXMPERF
    PERFLOG_STOP(m_pName ? m_pName : L"CBaseFilter", (IBaseFilter *)this, m_State);
#endif // DXMPERF

    m_State = State_Stopped;
    return hr;
}

// override CBaseMediaFilter Pause method to activate any pins
// this filter has (also called from Run)

STDMETHODIMP
CBaseFilter::Pause()
{
    CAutoLock cObjectLock(m_pLock);

    // notify all pins of the change to active state
    if (m_State == State_Stopped)
    {
        int cPins = GetPinCount();
        for (int c = 0; c < cPins; c++)
        {

            CBasePin *pPin = GetPin(c);
            if (NULL == pPin)
            {
                break;
            }

            // Disconnected pins are not activated - this saves pins
            // worrying about this state themselves

            if (pPin->IsConnected())
            {
                HRESULT hr = pPin->Active();
                if (FAILED(hr))
                {
                    return hr;
                }
            }
        }
    }

#ifdef DXMPERF
    PERFLOG_PAUSE(m_pName ? m_pName : L"CBaseFilter", (IBaseFilter *)this, m_State);
#endif // DXMPERF

    m_State = State_Paused;
    return S_OK;
}

// Put the filter into a running state.

// The time parameter is the offset to be added to the samples'
// stream time to get the reference time at which they should be presented.
//
// you can either add these two and compare it against the reference clock,
// or you can call CBaseFilter::StreamTime and compare that against
// the sample timestamp.

STDMETHODIMP
CBaseFilter::Run(REFERENCE_TIME tStart)
{
    CAutoLock cObjectLock(m_pLock);

    // remember the stream time offset
    m_tStart = tStart;

    if (m_State == State_Stopped)
    {
        HRESULT hr = Pause();

        if (FAILED(hr))
        {
            return hr;
        }
    }
    // notify all pins of the change to active state
    if (m_State != State_Running)
    {
        int cPins = GetPinCount();
        for (int c = 0; c < cPins; c++)
        {

            CBasePin *pPin = GetPin(c);
            if (NULL == pPin)
            {
                break;
            }

            // Disconnected pins are not activated - this saves pins
            // worrying about this state themselves

            if (pPin->IsConnected())
            {
                HRESULT hr = pPin->Run(tStart);
                if (FAILED(hr))
                {
                    return hr;
                }
            }
        }
    }

#ifdef DXMPERF
    PERFLOG_RUN(m_pName ? m_pName : L"CBaseFilter", (IBaseFilter *)this, tStart, m_State);
#endif // DXMPERF

    m_State = State_Running;
    return S_OK;
}

//
// return the current stream time - samples with start timestamps of this
// time or before should be rendered by now
HRESULT
CBaseFilter::StreamTime(CRefTime &rtStream)
{
    // Caller must lock for synchronization
    // We can't grab the filter lock because we want to be able to call
    // this from worker threads without deadlocking

    if (m_pClock == NULL)
    {
        return VFW_E_NO_CLOCK;
    }

    // get the current reference time
    HRESULT hr = m_pClock->GetTime((REFERENCE_TIME *)&rtStream);
    if (FAILED(hr))
    {
        return hr;
    }

    // subtract the stream offset to get stream time
    rtStream -= m_tStart;

    return S_OK;
}

/* Create an enumerator for the pins attached to this filter */

STDMETHODIMP
CBaseFilter::EnumPins(__deref_out IEnumPins **ppEnum)
{
    CheckPointer(ppEnum, E_POINTER);
    ValidateReadWritePtr(ppEnum, sizeof(IEnumPins *));

    /* Create a new ref counted enumerator */

    *ppEnum = new CEnumPins(this, NULL);

    return *ppEnum == NULL ? E_OUTOFMEMORY : NOERROR;
}

// default behaviour of FindPin is to assume pins are named
// by their pin names
STDMETHODIMP
CBaseFilter::FindPin(LPCWSTR Id, __deref_out IPin **ppPin)
{
    CheckPointer(ppPin, E_POINTER);
    ValidateReadWritePtr(ppPin, sizeof(IPin *));

    //  We're going to search the pin list so maintain integrity
    CAutoLock lck(m_pLock);
    int iCount = GetPinCount();
    for (int i = 0; i < iCount; i++)
    {
        CBasePin *pPin = GetPin(i);
        if (NULL == pPin)
        {
            break;
        }

        if (0 == lstrcmpW(pPin->Name(), Id))
        {
            //  Found one that matches
            //
            //  AddRef() and return it
            *ppPin = pPin;
            pPin->AddRef();
            return S_OK;
        }
    }
    *ppPin = NULL;
    return VFW_E_NOT_FOUND;
}

/* Return information about this filter */

STDMETHODIMP
CBaseFilter::QueryFilterInfo(__out FILTER_INFO *pInfo)
{
    CheckPointer(pInfo, E_POINTER);
    ValidateReadWritePtr(pInfo, sizeof(FILTER_INFO));

    if (m_pName)
    {
        (void)StringCchCopyW(pInfo->achName, NUMELMS(pInfo->achName), m_pName);
    }
    else
    {
        pInfo->achName[0] = L'\0';
    }
    pInfo->pGraph = m_pGraph;
    if (m_pGraph)
        m_pGraph->AddRef();
    return NOERROR;
}

/* Provide the filter with a filter graph */

STDMETHODIMP
CBaseFilter::JoinFilterGraph(__inout_opt IFilterGraph *pGraph, __in_opt LPCWSTR pName)
{
    CAutoLock cObjectLock(m_pLock);

    // NOTE: we no longer hold references on the graph (m_pGraph, m_pSink)

    m_pGraph = pGraph;
    if (m_pGraph)
    {
        HRESULT hr = m_pGraph->QueryInterface(IID_IMediaEventSink, (void **)&m_pSink);
        if (FAILED(hr))
        {
            ASSERT(m_pSink == NULL);
        }
        else
            m_pSink->Release(); // we do NOT keep a reference on it.
    }
    else
    {
        // if graph pointer is null, then we should
        // also release the IMediaEventSink on the same object - we don't
        // refcount it, so just set it to null
        m_pSink = NULL;
    }

    if (m_pName)
    {
        delete[] m_pName;
        m_pName = NULL;
    }

    if (pName)
    {
        size_t namelen;
        HRESULT hr = StringCchLengthW(pName, STRSAFE_MAX_CCH, &namelen);
        if (FAILED(hr))
        {
            return hr;
        }
        m_pName = new WCHAR[namelen + 1];
        if (m_pName)
        {
            (void)StringCchCopyW(m_pName, namelen + 1, pName);
        }
        else
        {
            return E_OUTOFMEMORY;
        }
    }

#ifdef DXMPERF
    PERFLOG_JOINGRAPH(m_pName ? m_pName : L"CBaseFilter", (IBaseFilter *)this, pGraph);
#endif // DXMPERF

    return NOERROR;
}

// return a Vendor information string. Optional - may return E_NOTIMPL.
// memory returned should be freed using CoTaskMemFree
// default implementation returns E_NOTIMPL
STDMETHODIMP
CBaseFilter::QueryVendorInfo(__deref_out LPWSTR *pVendorInfo)
{
    UNREFERENCED_PARAMETER(pVendorInfo);
    return E_NOTIMPL;
}

// send an event notification to the filter graph if we know about it.
// returns S_OK if delivered, S_FALSE if the filter graph does not sink
// events, or an error otherwise.
HRESULT
CBaseFilter::NotifyEvent(long EventCode, LONG_PTR EventParam1, LONG_PTR EventParam2)
{
    // Snapshot so we don't have to lock up
    IMediaEventSink *pSink = m_pSink;
    if (pSink)
    {
        if (EC_COMPLETE == EventCode)
        {
            EventParam2 = (LONG_PTR)(IBaseFilter *)this;
        }

        return pSink->Notify(EventCode, EventParam1, EventParam2);
    }
    else
    {
        return E_NOTIMPL;
    }
}

// Request reconnect
// pPin is the pin to reconnect
// pmt is the type to reconnect with - can be NULL
// Calls ReconnectEx on the filter graph
HRESULT
CBaseFilter::ReconnectPin(IPin *pPin, __in_opt AM_MEDIA_TYPE const *pmt)
{
    IFilterGraph2 *pGraph2;
    if (m_pGraph != NULL)
    {
        HRESULT hr = m_pGraph->QueryInterface(IID_IFilterGraph2, (void **)&pGraph2);
        if (SUCCEEDED(hr))
        {
            hr = pGraph2->ReconnectEx(pPin, pmt);
            pGraph2->Release();
            return hr;
        }
        else
        {
            return m_pGraph->Reconnect(pPin);
        }
    }
    else
    {
        return E_NOINTERFACE;
    }
}

/* This is the same idea as the media type version does for type enumeration
   on pins but for the list of pins available. So if the list of pins you
   provide changes dynamically then either override this virtual function
   to provide the version number, or more simply call IncrementPinVersion */

LONG CBaseFilter::GetPinVersion()
{
    return m_PinVersion;
}

/* Increment the current pin version cookie */

void CBaseFilter::IncrementPinVersion()
{
    InterlockedIncrement(&m_PinVersion);
}

/* register filter */

STDMETHODIMP CBaseFilter::Register()
{
    // get setup data, if it exists
    //
    LPAMOVIESETUP_FILTER psetupdata = GetSetupData();

    // check we've got data
    //
    if (NULL == psetupdata)
        return S_FALSE;

    // init is ref counted so call just in case
    // we're being called cold.
    //
    HRESULT hr = CoInitialize((LPVOID)NULL);
    ASSERT(SUCCEEDED(hr));

    // get hold of IFilterMapper
    //
    IFilterMapper *pIFM;
    hr = CoCreateInstance(CLSID_FilterMapper, NULL, CLSCTX_INPROC_SERVER, IID_IFilterMapper, (void **)&pIFM);
    if (SUCCEEDED(hr))
    {
        hr = AMovieSetupRegisterFilter(psetupdata, pIFM, TRUE);
        pIFM->Release();
    }

    // and clear up
    //
    CoFreeUnusedLibraries();
    CoUninitialize();

    return NOERROR;
}

/* unregister filter */

STDMETHODIMP CBaseFilter::Unregister()
{
    // get setup data, if it exists
    //
    LPAMOVIESETUP_FILTER psetupdata = GetSetupData();

    // check we've got data
    //
    if (NULL == psetupdata)
        return S_FALSE;

    // OLE init is ref counted so call
    // just in case we're being called cold.
    //
    HRESULT hr = CoInitialize((LPVOID)NULL);
    ASSERT(SUCCEEDED(hr));

    // get hold of IFilterMapper
    //
    IFilterMapper *pIFM;
    hr = CoCreateInstance(CLSID_FilterMapper, NULL, CLSCTX_INPROC_SERVER, IID_IFilterMapper, (void **)&pIFM);
    if (SUCCEEDED(hr))
    {
        hr = AMovieSetupRegisterFilter(psetupdata, pIFM, FALSE);

        // release interface
        //
        pIFM->Release();
    }

    // clear up
    //
    CoFreeUnusedLibraries();
    CoUninitialize();

    // handle one acceptable "error" - that
    // of filter not being registered!
    // (couldn't find a suitable #define'd
    // name for the error!)
    //
    if (0x80070002 == hr)
        return NOERROR;
    else
        return hr;
}

//=====================================================================
//=====================================================================
// Implements CEnumPins
//=====================================================================
//=====================================================================

CEnumPins::CEnumPins(__in CBaseFilter *pFilter, __in_opt CEnumPins *pEnumPins)
    : m_Position(0)
    , m_PinCount(0)
    , m_pFilter(pFilter)
    , m_cRef(1)
    , // Already ref counted
    m_PinCache(NAME("Pin Cache"))
{

#ifdef DEBUG
    m_dwCookie = DbgRegisterObjectCreation("CEnumPins", 0);
#endif

    /* We must be owned by a filter derived from CBaseFilter */

    ASSERT(pFilter != NULL);

    /* Hold a reference count on our filter */
    m_pFilter->AddRef();

    /* Are we creating a new enumerator */

    if (pEnumPins == NULL)
    {
        m_Version = m_pFilter->GetPinVersion();
        m_PinCount = m_pFilter->GetPinCount();
    }
    else
    {
        ASSERT(m_Position <= m_PinCount);
        m_Position = pEnumPins->m_Position;
        m_PinCount = pEnumPins->m_PinCount;
        m_Version = pEnumPins->m_Version;
        m_PinCache.AddTail(&(pEnumPins->m_PinCache));
    }
}

/* Destructor releases the reference count on our filter NOTE since we hold
   a reference count on the filter who created us we know it is safe to
   release it, no access can be made to it afterwards though as we have just
   caused the last reference count to go and the object to be deleted */

CEnumPins::~CEnumPins()
{
    m_pFilter->Release();

#ifdef DEBUG
    DbgRegisterObjectDestruction(m_dwCookie);
#endif
}

/* Override this to say what interfaces we support where */

STDMETHODIMP
CEnumPins::QueryInterface(REFIID riid, __deref_out void **ppv)
{
    CheckPointer(ppv, E_POINTER);

    /* Do we have this interface */

    if (riid == IID_IEnumPins || riid == IID_IUnknown)
    {
        return GetInterface((IEnumPins *)this, ppv);
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

STDMETHODIMP_(ULONG)
CEnumPins::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG)
CEnumPins::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}

/* One of an enumerator's basic member functions allows us to create a cloned
   interface that initially has the same state. Since we are taking a snapshot
   of an object (current position and all) we must lock access at the start */

STDMETHODIMP
CEnumPins::Clone(__deref_out IEnumPins **ppEnum)
{
    CheckPointer(ppEnum, E_POINTER);
    ValidateReadWritePtr(ppEnum, sizeof(IEnumPins *));
    HRESULT hr = NOERROR;

    /* Check we are still in sync with the filter */
    if (AreWeOutOfSync() == TRUE)
    {
        *ppEnum = NULL;
        hr = VFW_E_ENUM_OUT_OF_SYNC;
    }
    else
    {
        *ppEnum = new CEnumPins(m_pFilter, this);
        if (*ppEnum == NULL)
        {
            hr = E_OUTOFMEMORY;
        }
    }
    return hr;
}

/* Return the next pin after the current position */

STDMETHODIMP
CEnumPins::Next(ULONG cPins,                       // place this many pins...
                __out_ecount(cPins) IPin **ppPins, // ...in this array
                __out_opt ULONG *pcFetched)        // actual count passed returned here
{
    CheckPointer(ppPins, E_POINTER);
    ValidateReadWritePtr(ppPins, cPins * sizeof(IPin *));

    ASSERT(ppPins);

    if (pcFetched != NULL)
    {
        ValidateWritePtr(pcFetched, sizeof(ULONG));
        *pcFetched = 0; // default unless we succeed
    }
    // now check that the parameter is valid
    else if (cPins > 1)
    { // pcFetched == NULL
        return E_INVALIDARG;
    }
    ULONG cFetched = 0; // increment as we get each one.

    /* Check we are still in sync with the filter */
    if (AreWeOutOfSync() == TRUE)
    {
        // If we are out of sync, we should refresh the enumerator.
        // This will reset the position and update the other members, but
        // will not clear cache of pins we have already returned.
        Refresh();
    }

    /* Return each pin interface NOTE GetPin returns CBasePin * not addrefed
       so we must QI for the IPin (which increments its reference count)
       If while we are retrieving a pin from the filter an error occurs we
       assume that our internal state is stale with respect to the filter
       (for example someone has deleted a pin) so we
       return VFW_E_ENUM_OUT_OF_SYNC                            */

    while (cFetched < cPins && m_PinCount > m_Position)
    {

        /* Get the next pin object from the filter */

        CBasePin *pPin = m_pFilter->GetPin(m_Position++);
        if (pPin == NULL)
        {
            // If this happend, and it's not the first time through, then we've got a problem,
            // since we should really go back and release the iPins, which we have previously
            // AddRef'ed.
            ASSERT(cFetched == 0);
            return VFW_E_ENUM_OUT_OF_SYNC;
        }

        /* We only want to return this pin, if it is not in our cache */
        if (0 == m_PinCache.Find(pPin))
        {
            /* From the object get an IPin interface */

            *ppPins = pPin;
            pPin->AddRef();

            cFetched++;
            ppPins++;

            m_PinCache.AddTail(pPin);
        }
    }

    if (pcFetched != NULL)
    {
        *pcFetched = cFetched;
    }

    return (cPins == cFetched ? NOERROR : S_FALSE);
}

/* Skip over one or more entries in the enumerator */

STDMETHODIMP
CEnumPins::Skip(ULONG cPins)
{
    /* Check we are still in sync with the filter */
    if (AreWeOutOfSync() == TRUE)
    {
        return VFW_E_ENUM_OUT_OF_SYNC;
    }

    /* Work out how many pins are left to skip over */
    /* We could position at the end if we are asked to skip too many... */
    /* ..which would match the base implementation for CEnumMediaTypes::Skip */

    ULONG PinsLeft = m_PinCount - m_Position;
    if (cPins > PinsLeft)
    {
        return S_FALSE;
    }
    m_Position += cPins;
    return NOERROR;
}

/* Set the current position back to the start */
/* Reset has 4 simple steps:
 *
 * Set position to head of list
 * Sync enumerator with object being enumerated
 * Clear the cache of pins already returned
 * return S_OK
 */

STDMETHODIMP
CEnumPins::Reset()
{
    m_Version = m_pFilter->GetPinVersion();
    m_PinCount = m_pFilter->GetPinCount();

    m_Position = 0;

    // Clear the cache
    m_PinCache.RemoveAll();

    return S_OK;
}

/* Set the current position back to the start */
/* Refresh has 3 simple steps:
 *
 * Set position to head of list
 * Sync enumerator with object being enumerated
 * return S_OK
 */

STDMETHODIMP
CEnumPins::Refresh()
{
    m_Version = m_pFilter->GetPinVersion();
    m_PinCount = m_pFilter->GetPinCount();

    m_Position = 0;
    return S_OK;
}

//=====================================================================
//=====================================================================
// Implements CEnumMediaTypes
//=====================================================================
//=====================================================================

CEnumMediaTypes::CEnumMediaTypes(__in CBasePin *pPin, __in_opt CEnumMediaTypes *pEnumMediaTypes)
    : m_Position(0)
    , m_pPin(pPin)
    , m_cRef(1)
{

#ifdef DEBUG
    m_dwCookie = DbgRegisterObjectCreation("CEnumMediaTypes", 0);
#endif

    /* We must be owned by a pin derived from CBasePin */

    ASSERT(pPin != NULL);

    /* Hold a reference count on our pin */
    m_pPin->AddRef();

    /* Are we creating a new enumerator */

    if (pEnumMediaTypes == NULL)
    {
        m_Version = m_pPin->GetMediaTypeVersion();
        return;
    }

    m_Position = pEnumMediaTypes->m_Position;
    m_Version = pEnumMediaTypes->m_Version;
}

/* Destructor releases the reference count on our base pin. NOTE since we hold
   a reference count on the pin who created us we know it is safe to release
   it, no access can be made to it afterwards though as we might have just
   caused the last reference count to go and the object to be deleted */

CEnumMediaTypes::~CEnumMediaTypes()
{
#ifdef DEBUG
    DbgRegisterObjectDestruction(m_dwCookie);
#endif
    m_pPin->Release();
}

/* Override this to say what interfaces we support where */

STDMETHODIMP
CEnumMediaTypes::QueryInterface(REFIID riid, __deref_out void **ppv)
{
    CheckPointer(ppv, E_POINTER);

    /* Do we have this interface */

    if (riid == IID_IEnumMediaTypes || riid == IID_IUnknown)
    {
        return GetInterface((IEnumMediaTypes *)this, ppv);
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

STDMETHODIMP_(ULONG)
CEnumMediaTypes::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG)
CEnumMediaTypes::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}

/* One of an enumerator's basic member functions allows us to create a cloned
   interface that initially has the same state. Since we are taking a snapshot
   of an object (current position and all) we must lock access at the start */

STDMETHODIMP
CEnumMediaTypes::Clone(__deref_out IEnumMediaTypes **ppEnum)
{
    CheckPointer(ppEnum, E_POINTER);
    ValidateReadWritePtr(ppEnum, sizeof(IEnumMediaTypes *));
    HRESULT hr = NOERROR;

    /* Check we are still in sync with the pin */
    if (AreWeOutOfSync() == TRUE)
    {
        *ppEnum = NULL;
        hr = VFW_E_ENUM_OUT_OF_SYNC;
    }
    else
    {

        *ppEnum = new CEnumMediaTypes(m_pPin, this);

        if (*ppEnum == NULL)
        {
            hr = E_OUTOFMEMORY;
        }
    }
    return hr;
}

/* Enumerate the next pin(s) after the current position. The client using this
   interface passes in a pointer to an array of pointers each of which will
   be filled in with a pointer to a fully initialised media type format
   Return NOERROR if it all works,
          S_FALSE if fewer than cMediaTypes were enumerated.
          VFW_E_ENUM_OUT_OF_SYNC if the enumerator has been broken by
                                 state changes in the filter
   The actual count always correctly reflects the number of types in the array.
*/

STDMETHODIMP
CEnumMediaTypes::Next(ULONG cMediaTypes,                                      // place this many types...
                      __out_ecount(cMediaTypes) AM_MEDIA_TYPE **ppMediaTypes, // ...in this array
                      __out ULONG *pcFetched)                                 // actual count passed
{
    CheckPointer(ppMediaTypes, E_POINTER);
    ValidateReadWritePtr(ppMediaTypes, cMediaTypes * sizeof(AM_MEDIA_TYPE *));
    /* Check we are still in sync with the pin */
    if (AreWeOutOfSync() == TRUE)
    {
        return VFW_E_ENUM_OUT_OF_SYNC;
    }

    if (pcFetched != NULL)
    {
        ValidateWritePtr(pcFetched, sizeof(ULONG));
        *pcFetched = 0; // default unless we succeed
    }
    // now check that the parameter is valid
    else if (cMediaTypes > 1)
    { // pcFetched == NULL
        return E_INVALIDARG;
    }
    ULONG cFetched = 0; // increment as we get each one.

    /* Return each media type by asking the filter for them in turn - If we
       have an error code retured to us while we are retrieving a media type
       we assume that our internal state is stale with respect to the filter
       (for example the window size changing) so we return
       VFW_E_ENUM_OUT_OF_SYNC */

    while (cMediaTypes)
    {

        CMediaType cmt;

        HRESULT hr = m_pPin->GetMediaType(m_Position++, &cmt);
        if (S_OK != hr)
        {
            break;
        }

        /* We now have a CMediaType object that contains the next media type
           but when we assign it to the array position we CANNOT just assign
           the AM_MEDIA_TYPE structure because as soon as the object goes out of
           scope it will delete the memory we have just copied. The function
           we use is CreateMediaType which allocates a task memory block */

        /*  Transfer across the format block manually to save an allocate
            and free on the format block and generally go faster */

        *ppMediaTypes = (AM_MEDIA_TYPE *)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
        if (*ppMediaTypes == NULL)
        {
            break;
        }

        /*  Do a regular copy */
        **ppMediaTypes = cmt;

        /*  Make sure the destructor doesn't free these */
        cmt.pbFormat = NULL;
        cmt.cbFormat = NULL;
        cmt.pUnk = NULL;

        ppMediaTypes++;
        cFetched++;
        cMediaTypes--;
    }

    if (pcFetched != NULL)
    {
        *pcFetched = cFetched;
    }

    return (cMediaTypes == 0 ? NOERROR : S_FALSE);
}

/* Skip over one or more entries in the enumerator */

STDMETHODIMP
CEnumMediaTypes::Skip(ULONG cMediaTypes)
{
    //  If we're skipping 0 elements we're guaranteed to skip the
    //  correct number of elements
    if (cMediaTypes == 0)
    {
        return S_OK;
    }

    /* Check we are still in sync with the pin */
    if (AreWeOutOfSync() == TRUE)
    {
        return VFW_E_ENUM_OUT_OF_SYNC;
    }

    m_Position += cMediaTypes;

    /*  See if we're over the end */
    CMediaType cmt;
    return S_OK == m_pPin->GetMediaType(m_Position - 1, &cmt) ? S_OK : S_FALSE;
}

/* Set the current position back to the start */
/* Reset has 3 simple steps:
 *
 * set position to head of list
 * sync enumerator with object being enumerated
 * return S_OK
 */

STDMETHODIMP
CEnumMediaTypes::Reset()

{
    m_Position = 0;

    // Bring the enumerator back into step with the current state.  This
    // may be a noop but ensures that the enumerator will be valid on the
    // next call.
    m_Version = m_pPin->GetMediaTypeVersion();
    return NOERROR;
}

//=====================================================================
//=====================================================================
// Implements CBasePin
//=====================================================================
//=====================================================================

/* NOTE The implementation of this class calls the CUnknown constructor with
   a NULL outer unknown pointer. This has the effect of making us a self
   contained class, ie any QueryInterface, AddRef or Release calls will be
   routed to the class's NonDelegatingUnknown methods. You will typically
   find that the classes that do this then override one or more of these
   virtual functions to provide more specialised behaviour. A good example
   of this is where a class wants to keep the QueryInterface internal but
   still wants its lifetime controlled by the external object */

/* Constructor */

CBasePin::CBasePin(__in_opt LPCTSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock, __inout HRESULT *phr,
                   __in_opt LPCWSTR pName, PIN_DIRECTION dir)
    : CUnknown(pObjectName, NULL)
    , m_pFilter(pFilter)
    , m_pLock(pLock)
    , m_pName(NULL)
    , m_Connected(NULL)
    , m_dir(dir)
    , m_bRunTimeError(FALSE)
    , m_pQSink(NULL)
    , m_TypeVersion(1)
    , m_tStart()
    , m_tStop(MAX_TIME)
    , m_bCanReconnectWhenActive(false)
    , m_bTryMyTypesFirst(false)
    , m_dRate(1.0)
{
    /*  WARNING - pFilter is often not a properly constituted object at
        this state (in particular QueryInterface may not work) - this
        is because its owner is often its containing object and we
        have been called from the containing object's constructor so
        the filter's owner has not yet had its CUnknown constructor
        called
    */
#ifdef DXMPERF
    PERFLOG_CTOR(pName ? pName : L"CBasePin", (IPin *)this);
#endif // DXMPERF

    ASSERT(pFilter != NULL);
    ASSERT(pLock != NULL);

    if (pName)
    {
        size_t cchName;
        HRESULT hr = StringCchLengthW(pName, STRSAFE_MAX_CCH, &cchName);
        if (SUCCEEDED(hr))
        {
            m_pName = new WCHAR[cchName + 1];
            if (m_pName)
            {
                (void)StringCchCopyW(m_pName, cchName + 1, pName);
            }
        }
    }

#ifdef DEBUG
    m_cRef = 0;
#endif
}

#ifdef UNICODE
CBasePin::CBasePin(__in_opt LPCSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock, __inout HRESULT *phr,
                   __in_opt LPCWSTR pName, PIN_DIRECTION dir)
    : CUnknown(pObjectName, NULL)
    , m_pFilter(pFilter)
    , m_pLock(pLock)
    , m_pName(NULL)
    , m_Connected(NULL)
    , m_dir(dir)
    , m_bRunTimeError(FALSE)
    , m_pQSink(NULL)
    , m_TypeVersion(1)
    , m_tStart()
    , m_tStop(MAX_TIME)
    , m_bCanReconnectWhenActive(false)
    , m_bTryMyTypesFirst(false)
    , m_dRate(1.0)
{
    /*  WARNING - pFilter is often not a properly constituted object at
        this state (in particular QueryInterface may not work) - this
        is because its owner is often its containing object and we
        have been called from the containing object's constructor so
        the filter's owner has not yet had its CUnknown constructor
        called
    */
#ifdef DXMPERF
    PERFLOG_CTOR(pName ? pName : L"CBasePin", (IPin *)this);
#endif // DXMPERF

    ASSERT(pFilter != NULL);
    ASSERT(pLock != NULL);

    if (pName)
    {
        size_t cchName;
        HRESULT hr = StringCchLengthW(pName, STRSAFE_MAX_CCH, &cchName);
        if (SUCCEEDED(hr))
        {
            m_pName = new WCHAR[cchName + 1];
            if (m_pName)
            {
                (void)StringCchCopyW(m_pName, cchName + 1, pName);
            }
        }
    }

#ifdef DEBUG
    m_cRef = 0;
#endif
}
#endif

/* Destructor since a connected pin holds a reference count on us there is
   no way that we can be deleted unless we are not currently connected */

CBasePin::~CBasePin()
{
#ifdef DXMPERF
    PERFLOG_DTOR(m_pName ? m_pName : L"CBasePin", (IPin *)this);
#endif // DXMPERF

    //  We don't call disconnect because if the filter is going away
    //  all the pins must have a reference count of zero so they must
    //  have been disconnected anyway - (but check the assumption)
    ASSERT(m_Connected == FALSE);

    delete[] m_pName;

    // check the internal reference count is consistent
    ASSERT(m_cRef == 0);
}

/* Override this to say what interfaces we support and where */

STDMETHODIMP
CBasePin::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    /* Do we have this interface */

    if (riid == IID_IPin)
    {
        return GetInterface((IPin *)this, ppv);
    }
    else if (riid == IID_IQualityControl)
    {
        return GetInterface((IQualityControl *)this, ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

/* Override to increment the owning filter's reference count */

STDMETHODIMP_(ULONG)
CBasePin::NonDelegatingAddRef()
{
    ASSERT(InterlockedIncrement(&m_cRef) > 0);
    return m_pFilter->AddRef();
}

/* Override to decrement the owning filter's reference count */

STDMETHODIMP_(ULONG)
CBasePin::NonDelegatingRelease()
{
    ASSERT(InterlockedDecrement(&m_cRef) >= 0);
    return m_pFilter->Release();
}

/* Displays pin connection information */

#ifdef DEBUG
void CBasePin::DisplayPinInfo(IPin *pReceivePin)
{

    if (DbgCheckModuleLevel(LOG_TRACE, CONNECT_TRACE_LEVEL))
    {
        PIN_INFO ConnectPinInfo;
        PIN_INFO ReceivePinInfo;

        if (FAILED(QueryPinInfo(&ConnectPinInfo)))
        {
            StringCchCopyW(ConnectPinInfo.achName, sizeof(ConnectPinInfo.achName) / sizeof(WCHAR), L"Bad Pin");
        }
        else
        {
            QueryPinInfoReleaseFilter(ConnectPinInfo);
        }

        if (FAILED(pReceivePin->QueryPinInfo(&ReceivePinInfo)))
        {
            StringCchCopyW(ReceivePinInfo.achName, sizeof(ReceivePinInfo.achName) / sizeof(WCHAR), L"Bad Pin");
        }
        else
        {
            QueryPinInfoReleaseFilter(ReceivePinInfo);
        }

        DbgLog((LOG_TRACE, CONNECT_TRACE_LEVEL, TEXT("Trying to connect Pins :")));
        DbgLog((LOG_TRACE, CONNECT_TRACE_LEVEL, TEXT("    <%ls>"), ConnectPinInfo.achName));
        DbgLog((LOG_TRACE, CONNECT_TRACE_LEVEL, TEXT("    <%ls>"), ReceivePinInfo.achName));
    }
}
#endif

/* Displays general information on the pin media type */

#ifdef DEBUG
void CBasePin::DisplayTypeInfo(IPin *pPin, const CMediaType *pmt)
{
    UNREFERENCED_PARAMETER(pPin);
    if (DbgCheckModuleLevel(LOG_TRACE, CONNECT_TRACE_LEVEL))
    {
        DbgLog((LOG_TRACE, CONNECT_TRACE_LEVEL, TEXT("Trying media type:")));
        DbgLog((LOG_TRACE, CONNECT_TRACE_LEVEL, TEXT("    major type:  %hs"), GuidNames[*pmt->Type()]));
        DbgLog((LOG_TRACE, CONNECT_TRACE_LEVEL, TEXT("    sub type  :  %hs"), GuidNames[*pmt->Subtype()]));
    }
}
#endif

/* Asked to connect to a pin. A pin is always attached to an owning filter
   object so we always delegate our locking to that object. We first of all
   retrieve a media type enumerator for the input pin and see if we accept
   any of the formats that it would ideally like, failing that we retrieve
   our enumerator and see if it will accept any of our preferred types */

STDMETHODIMP
CBasePin::Connect(IPin *pReceivePin,
                  __in_opt const AM_MEDIA_TYPE *pmt // optional media type
)
{
    CheckPointer(pReceivePin, E_POINTER);
    ValidateReadPtr(pReceivePin, sizeof(IPin));
    CAutoLock cObjectLock(m_pLock);
    DisplayPinInfo(pReceivePin);

    /* See if we are already connected */

    if (m_Connected)
    {
        DbgLog((LOG_TRACE, CONNECT_TRACE_LEVEL, TEXT("Already connected")));
        return VFW_E_ALREADY_CONNECTED;
    }

    /* See if the filter is active */
    if (!IsStopped() && !m_bCanReconnectWhenActive)
    {
        return VFW_E_NOT_STOPPED;
    }

    // Find a mutually agreeable media type -
    // Pass in the template media type. If this is partially specified,
    // each of the enumerated media types will need to be checked against
    // it. If it is non-null and fully specified, we will just try to connect
    // with this.

    const CMediaType *ptype = (CMediaType *)pmt;
    HRESULT hr = AgreeMediaType(pReceivePin, ptype);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, CONNECT_TRACE_LEVEL, TEXT("Failed to agree type")));

        // Since the procedure is already returning an error code, there
        // is nothing else this function can do to report the error.
        EXECUTE_ASSERT(SUCCEEDED(BreakConnect()));

#ifdef DXMPERF
        PERFLOG_CONNECT((IPin *)this, pReceivePin, hr, pmt);
#endif // DXMPERF

        return hr;
    }

    DbgLog((LOG_TRACE, CONNECT_TRACE_LEVEL, TEXT("Connection succeeded")));

#ifdef DXMPERF
    PERFLOG_CONNECT((IPin *)this, pReceivePin, NOERROR, pmt);
#endif // DXMPERF

    return NOERROR;
}

// given a specific media type, attempt a connection (includes
// checking that the type is acceptable to this pin)
HRESULT
CBasePin::AttemptConnection(IPin *pReceivePin,    // connect to this pin
                            const CMediaType *pmt // using this type
)
{
    // The caller should hold the filter lock becasue this function
    // uses m_Connected.  The caller should also hold the filter lock
    // because this function calls SetMediaType(), IsStopped() and
    // CompleteConnect().
    ASSERT(CritCheckIn(m_pLock));

    // Check that the connection is valid  -- need to do this for every
    // connect attempt since BreakConnect will undo it.
    HRESULT hr = CheckConnect(pReceivePin);
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, CONNECT_TRACE_LEVEL, TEXT("CheckConnect failed")));

        // Since the procedure is already returning an error code, there
        // is nothing else this function can do to report the error.
        EXECUTE_ASSERT(SUCCEEDED(BreakConnect()));

        return hr;
    }

    DisplayTypeInfo(pReceivePin, pmt);

    /* Check we will accept this media type */

    hr = CheckMediaType(pmt);
    if (hr == NOERROR)
    {

        /*  Make ourselves look connected otherwise ReceiveConnection
            may not be able to complete the connection
        */
        m_Connected = pReceivePin;
        m_Connected->AddRef();
        hr = SetMediaType(pmt);
        if (SUCCEEDED(hr))
        {
            /* See if the other pin will accept this type */

            hr = pReceivePin->ReceiveConnection((IPin *)this, pmt);
            if (SUCCEEDED(hr))
            {
                /* Complete the connection */

                hr = CompleteConnect(pReceivePin);
                if (SUCCEEDED(hr))
                {
                    return hr;
                }
                else
                {
                    DbgLog((LOG_TRACE, CONNECT_TRACE_LEVEL, TEXT("Failed to complete connection")));
                    pReceivePin->Disconnect();
                }
            }
        }
    }
    else
    {
        // we cannot use this media type

        // return a specific media type error if there is one
        // or map a general failure code to something more helpful
        // (in particular S_FALSE gets changed to an error code)
        if (SUCCEEDED(hr) || (hr == E_FAIL) || (hr == E_INVALIDARG))
        {
            hr = VFW_E_TYPE_NOT_ACCEPTED;
        }
    }

    // BreakConnect and release any connection here in case CheckMediaType
    // failed, or if we set anything up during a call back during
    // ReceiveConnection.

    // Since the procedure is already returning an error code, there
    // is nothing else this function can do to report the error.
    EXECUTE_ASSERT(SUCCEEDED(BreakConnect()));

    /*  If failed then undo our state */
    if (m_Connected)
    {
        m_Connected->Release();
        m_Connected = NULL;
    }

    return hr;
}

/* Given an enumerator we cycle through all the media types it proposes and
   firstly suggest them to our derived pin class and if that succeeds try
   them with the pin in a ReceiveConnection call. This means that if our pin
   proposes a media type we still check in here that we can support it. This
   is deliberate so that in simple cases the enumerator can hold all of the
   media types even if some of them are not really currently available */

HRESULT CBasePin::TryMediaTypes(IPin *pReceivePin, __in_opt const CMediaType *pmt, IEnumMediaTypes *pEnum)
{
    /* Reset the current enumerator position */

    HRESULT hr = pEnum->Reset();
    if (FAILED(hr))
    {
        return hr;
    }

    CMediaType *pMediaType = NULL;
    ULONG ulMediaCount = 0;

    // attempt to remember a specific error code if there is one
    HRESULT hrFailure = S_OK;

    for (;;)
    {

        /* Retrieve the next media type NOTE each time round the loop the
           enumerator interface will allocate another AM_MEDIA_TYPE structure
           If we are successful then we copy it into our output object, if
           not then we must delete the memory allocated before returning */

        hr = pEnum->Next(1, (AM_MEDIA_TYPE **)&pMediaType, &ulMediaCount);
        if (hr != S_OK)
        {
            if (S_OK == hrFailure)
            {
                hrFailure = VFW_E_NO_ACCEPTABLE_TYPES;
            }
            return hrFailure;
        }

        ASSERT(ulMediaCount == 1);
        ASSERT(pMediaType);

        // check that this matches the partial type (if any)

        if (pMediaType && ((pmt == NULL) || pMediaType->MatchesPartial(pmt)))
        {

            hr = AttemptConnection(pReceivePin, pMediaType);

            // attempt to remember a specific error code
            if (FAILED(hr) && SUCCEEDED(hrFailure) && (hr != E_FAIL) && (hr != E_INVALIDARG) &&
                (hr != VFW_E_TYPE_NOT_ACCEPTED))
            {
                hrFailure = hr;
            }
        }
        else
        {
            hr = VFW_E_NO_ACCEPTABLE_TYPES;
        }

        if (pMediaType)
        {
            DeleteMediaType(pMediaType);
            pMediaType = NULL;
        }

        if (S_OK == hr)
        {
            return hr;
        }
    }
}

/* This is called to make the connection, including the taask of finding
   a media type for the pin connection. pmt is the proposed media type
   from the Connect call: if this is fully specified, we will try that.
   Otherwise we enumerate and try all the input pin's types first and
   if that fails we then enumerate and try all our preferred media types.
   For each media type we check it against pmt (if non-null and partially
   specified) as well as checking that both pins will accept it.
 */

HRESULT CBasePin::AgreeMediaType(IPin *pReceivePin, const CMediaType *pmt)
{
    ASSERT(pReceivePin);
    IEnumMediaTypes *pEnumMediaTypes = NULL;

    // if the media type is fully specified then use that
    if ((pmt != NULL) && (!pmt->IsPartiallySpecified()))
    {

        // if this media type fails, then we must fail the connection
        // since if pmt is nonnull we are only allowed to connect
        // using a type that matches it.

        return AttemptConnection(pReceivePin, pmt);
    }

    /* Try the other pin's enumerator */

    HRESULT hrFailure = VFW_E_NO_ACCEPTABLE_TYPES;

    for (int i = 0; i < 2; i++)
    {
        HRESULT hr;
        if (i == (int)m_bTryMyTypesFirst)
        {
            hr = pReceivePin->EnumMediaTypes(&pEnumMediaTypes);
        }
        else
        {
            hr = EnumMediaTypes(&pEnumMediaTypes);
        }
        if (SUCCEEDED(hr))
        {
            ASSERT(pEnumMediaTypes);
            hr = TryMediaTypes(pReceivePin, pmt, pEnumMediaTypes);
            pEnumMediaTypes->Release();
            if (SUCCEEDED(hr))
            {
                return NOERROR;
            }
            else
            {
                // try to remember specific error codes if there are any
                if ((hr != E_FAIL) && (hr != E_INVALIDARG) && (hr != VFW_E_TYPE_NOT_ACCEPTED))
                {
                    hrFailure = hr;
                }
            }
        }
    }

    return hrFailure;
}

/* Called when we want to complete a connection to another filter. Failing
   this will also fail the connection and disconnect the other pin as well */

HRESULT
CBasePin::CompleteConnect(IPin *pReceivePin)
{
    UNREFERENCED_PARAMETER(pReceivePin);
    return NOERROR;
}

/* This is called to set the format for a pin connection - CheckMediaType
   will have been called to check the connection format and if it didn't
   return an error code then this (virtual) function will be invoked */

HRESULT
CBasePin::SetMediaType(const CMediaType *pmt)
{
    HRESULT hr = m_mt.Set(*pmt);
    if (FAILED(hr))
    {
        return hr;
    }

    return NOERROR;
}

/* This is called during Connect() to provide a virtual method that can do
   any specific check needed for connection such as QueryInterface. This
   base class method just checks that the pin directions don't match */

HRESULT
CBasePin::CheckConnect(IPin *pPin)
{
    /* Check that pin directions DONT match */

    PIN_DIRECTION pd;
    pPin->QueryDirection(&pd);

    ASSERT((pd == PINDIR_OUTPUT) || (pd == PINDIR_INPUT));
    ASSERT((m_dir == PINDIR_OUTPUT) || (m_dir == PINDIR_INPUT));

    // we should allow for non-input and non-output connections?
    if (pd == m_dir)
    {
        return VFW_E_INVALID_DIRECTION;
    }
    return NOERROR;
}

/* This is called when we realise we can't make a connection to the pin and
   must undo anything we did in CheckConnect - override to release QIs done */

HRESULT
CBasePin::BreakConnect()
{
    return NOERROR;
}

/* Called normally by an output pin on an input pin to try and establish a
   connection.
*/

STDMETHODIMP
CBasePin::ReceiveConnection(IPin *pConnector,        // this is the pin who we will connect to
                            const AM_MEDIA_TYPE *pmt // this is the media type we will exchange
)
{
    CheckPointer(pConnector, E_POINTER);
    CheckPointer(pmt, E_POINTER);
    ValidateReadPtr(pConnector, sizeof(IPin));
    ValidateReadPtr(pmt, sizeof(AM_MEDIA_TYPE));
    CAutoLock cObjectLock(m_pLock);

    /* Are we already connected */
    if (m_Connected)
    {
        return VFW_E_ALREADY_CONNECTED;
    }

    /* See if the filter is active */
    if (!IsStopped() && !m_bCanReconnectWhenActive)
    {
        return VFW_E_NOT_STOPPED;
    }

    HRESULT hr = CheckConnect(pConnector);
    if (FAILED(hr))
    {
        // Since the procedure is already returning an error code, there
        // is nothing else this function can do to report the error.
        EXECUTE_ASSERT(SUCCEEDED(BreakConnect()));

#ifdef DXMPERF
        PERFLOG_RXCONNECT(pConnector, (IPin *)this, hr, pmt);
#endif // DXMPERF

        return hr;
    }

    /* Ask derived class if this media type is ok */

    CMediaType *pcmt = (CMediaType *)pmt;
    hr = CheckMediaType(pcmt);
    if (hr != NOERROR)
    {
        // no -we don't support this media type

        // Since the procedure is already returning an error code, there
        // is nothing else this function can do to report the error.
        EXECUTE_ASSERT(SUCCEEDED(BreakConnect()));

        // return a specific media type error if there is one
        // or map a general failure code to something more helpful
        // (in particular S_FALSE gets changed to an error code)
        if (SUCCEEDED(hr) || (hr == E_FAIL) || (hr == E_INVALIDARG))
        {
            hr = VFW_E_TYPE_NOT_ACCEPTED;
        }

#ifdef DXMPERF
        PERFLOG_RXCONNECT(pConnector, (IPin *)this, hr, pmt);
#endif // DXMPERF

        return hr;
    }

    /* Complete the connection */

    m_Connected = pConnector;
    m_Connected->AddRef();
    hr = SetMediaType(pcmt);
    if (SUCCEEDED(hr))
    {
        hr = CompleteConnect(pConnector);
        if (SUCCEEDED(hr))
        {

#ifdef DXMPERF
            PERFLOG_RXCONNECT(pConnector, (IPin *)this, NOERROR, pmt);
#endif // DXMPERF

            return NOERROR;
        }
    }

    DbgLog(
        (LOG_TRACE, CONNECT_TRACE_LEVEL, TEXT("Failed to set the media type or failed to complete the connection.")));
    m_Connected->Release();
    m_Connected = NULL;

    // Since the procedure is already returning an error code, there
    // is nothing else this function can do to report the error.
    EXECUTE_ASSERT(SUCCEEDED(BreakConnect()));

#ifdef DXMPERF
    PERFLOG_RXCONNECT(pConnector, (IPin *)this, hr, pmt);
#endif // DXMPERF

    return hr;
}

/* Called when we want to terminate a pin connection */

STDMETHODIMP
CBasePin::Disconnect()
{
    CAutoLock cObjectLock(m_pLock);

    /* See if the filter is active */
    if (!IsStopped())
    {
        return VFW_E_NOT_STOPPED;
    }

    return DisconnectInternal();
}

STDMETHODIMP
CBasePin::DisconnectInternal()
{
    ASSERT(CritCheckIn(m_pLock));

    if (m_Connected)
    {
        HRESULT hr = BreakConnect();
        if (FAILED(hr))
        {

#ifdef DXMPERF
            PERFLOG_DISCONNECT((IPin *)this, m_Connected, hr);
#endif // DXMPERF

            // There is usually a bug in the program if BreakConnect() fails.
            DbgBreak("WARNING: BreakConnect() failed in CBasePin::Disconnect().");
            return hr;
        }

        m_Connected->Release();
        m_Connected = NULL;

#ifdef DXMPERF
        PERFLOG_DISCONNECT((IPin *)this, m_Connected, S_OK);
#endif // DXMPERF

        return S_OK;
    }
    else
    {
        // no connection - not an error

#ifdef DXMPERF
        PERFLOG_DISCONNECT((IPin *)this, m_Connected, S_FALSE);
#endif // DXMPERF

        return S_FALSE;
    }
}

/* Return an AddRef()'d pointer to the connected pin if there is one */
STDMETHODIMP
CBasePin::ConnectedTo(__deref_out IPin **ppPin)
{
    CheckPointer(ppPin, E_POINTER);
    ValidateReadWritePtr(ppPin, sizeof(IPin *));
    //
    //  It's pointless to lock here.
    //  The caller should ensure integrity.
    //

    IPin *pPin = m_Connected;
    *ppPin = pPin;
    if (pPin != NULL)
    {
        pPin->AddRef();
        return S_OK;
    }
    else
    {
        ASSERT(*ppPin == NULL);
        return VFW_E_NOT_CONNECTED;
    }
}

/* Return the media type of the connection */
STDMETHODIMP
CBasePin::ConnectionMediaType(__out AM_MEDIA_TYPE *pmt)
{
    CheckPointer(pmt, E_POINTER);
    ValidateReadWritePtr(pmt, sizeof(AM_MEDIA_TYPE));
    CAutoLock cObjectLock(m_pLock);

    /*  Copy constructor of m_mt allocates the memory */
    if (IsConnected())
    {
        CopyMediaType(pmt, &m_mt);
        return S_OK;
    }
    else
    {
        ((CMediaType *)pmt)->InitMediaType();
        return VFW_E_NOT_CONNECTED;
    }
}

/* Return information about the filter we are connect to */

STDMETHODIMP
CBasePin::QueryPinInfo(__out PIN_INFO *pInfo)
{
    CheckPointer(pInfo, E_POINTER);
    ValidateReadWritePtr(pInfo, sizeof(PIN_INFO));

    pInfo->pFilter = m_pFilter;
    if (m_pFilter)
    {
        m_pFilter->AddRef();
    }

    if (m_pName)
    {
        (void)StringCchCopyW(pInfo->achName, NUMELMS(pInfo->achName), m_pName);
    }
    else
    {
        pInfo->achName[0] = L'\0';
    }

    pInfo->dir = m_dir;

    return NOERROR;
}

STDMETHODIMP
CBasePin::QueryDirection(__out PIN_DIRECTION *pPinDir)
{
    CheckPointer(pPinDir, E_POINTER);
    ValidateReadWritePtr(pPinDir, sizeof(PIN_DIRECTION));

    *pPinDir = m_dir;
    return NOERROR;
}

// Default QueryId to return the pin's name
STDMETHODIMP
CBasePin::QueryId(__deref_out LPWSTR *Id)
{
    //  We're not going away because someone's got a pointer to us
    //  so there's no need to lock

    return AMGetWideString(Name(), Id);
}

/* Does this pin support this media type WARNING this interface function does
   not lock the main object as it is meant to be asynchronous by nature - if
   the media types you support depend on some internal state that is updated
   dynamically then you will need to implement locking in a derived class */

STDMETHODIMP
CBasePin::QueryAccept(const AM_MEDIA_TYPE *pmt)
{
    CheckPointer(pmt, E_POINTER);
    ValidateReadPtr(pmt, sizeof(AM_MEDIA_TYPE));

    /* The CheckMediaType method is valid to return error codes if the media
       type is horrible, an example might be E_INVALIDARG. What we do here
       is map all the error codes into either S_OK or S_FALSE regardless */

    HRESULT hr = CheckMediaType((CMediaType *)pmt);
    if (FAILED(hr))
    {
        return S_FALSE;
    }
    // note that the only defined success codes should be S_OK and S_FALSE...
    return hr;
}

/* This can be called to return an enumerator for the pin's list of preferred
   media types. An input pin is not obliged to have any preferred formats
   although it can do. For example, the window renderer has a preferred type
   which describes a video image that matches the current window size. All
   output pins should expose at least one preferred format otherwise it is
   possible that neither pin has any types and so no connection is possible */

STDMETHODIMP
CBasePin::EnumMediaTypes(__deref_out IEnumMediaTypes **ppEnum)
{
    CheckPointer(ppEnum, E_POINTER);
    ValidateReadWritePtr(ppEnum, sizeof(IEnumMediaTypes *));

    /* Create a new ref counted enumerator */

    *ppEnum = new CEnumMediaTypes(this, NULL);

    if (*ppEnum == NULL)
    {
        return E_OUTOFMEMORY;
    }

    return NOERROR;
}

/* This is a virtual function that returns a media type corresponding with
   place iPosition in the list. This base class simply returns an error as
   we support no media types by default but derived classes should override */

HRESULT CBasePin::GetMediaType(int iPosition, __inout CMediaType *pMediaType)
{
    UNREFERENCED_PARAMETER(iPosition);
    UNREFERENCED_PARAMETER(pMediaType);
    return E_UNEXPECTED;
}

/* This is a virtual function that returns the current media type version.
   The base class initialises the media type enumerators with the value 1
   By default we always returns that same value. A Derived class may change
   the list of media types available and after doing so it should increment
   the version either in a method derived from this, or more simply by just
   incrementing the m_TypeVersion base pin variable. The type enumerators
   call this when they want to see if their enumerations are out of date */

LONG CBasePin::GetMediaTypeVersion()
{
    return m_TypeVersion;
}

/* Increment the cookie representing the current media type version */

void CBasePin::IncrementTypeVersion()
{
    InterlockedIncrement(&m_TypeVersion);
}

/* Called by IMediaFilter implementation when the state changes from Stopped
   to either paused or running and in derived classes could do things like
   commit memory and grab hardware resource (the default is to do nothing) */

HRESULT
CBasePin::Active(void)
{
    return NOERROR;
}

/* Called by IMediaFilter implementation when the state changes from
   to either paused to running and in derived classes could do things like
   commit memory and grab hardware resource (the default is to do nothing) */

HRESULT
CBasePin::Run(REFERENCE_TIME tStart)
{
    UNREFERENCED_PARAMETER(tStart);
    return NOERROR;
}

/* Also called by the IMediaFilter implementation when the state changes to
   Stopped at which point you should decommit allocators and free hardware
   resources you grabbed in the Active call (default is also to do nothing) */

HRESULT
CBasePin::Inactive(void)
{
    m_bRunTimeError = FALSE;
    return NOERROR;
}

// Called when no more data will arrive
STDMETHODIMP
CBasePin::EndOfStream(void)
{
    return S_OK;
}

STDMETHODIMP
CBasePin::SetSink(IQualityControl *piqc)
{
    CAutoLock cObjectLock(m_pLock);
    if (piqc)
        ValidateReadPtr(piqc, sizeof(IQualityControl));
    m_pQSink = piqc;
    return NOERROR;
} // SetSink

STDMETHODIMP
CBasePin::Notify(IBaseFilter *pSender, Quality q)
{
    UNREFERENCED_PARAMETER(q);
    UNREFERENCED_PARAMETER(pSender);
    DbgBreak("IQualityControl::Notify not over-ridden from CBasePin.  (IGNORE is OK)");
    return E_NOTIMPL;
} // Notify

// NewSegment notifies of the start/stop/rate applying to the data
// about to be received. Default implementation records data and
// returns S_OK.
// Override this to pass downstream.
STDMETHODIMP
CBasePin::NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    m_tStart = tStart;
    m_tStop = tStop;
    m_dRate = dRate;

    return S_OK;
}

//=====================================================================
//=====================================================================
// Implements CBaseOutputPin
//=====================================================================
//=====================================================================

CBaseOutputPin::CBaseOutputPin(__in_opt LPCTSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock,
                               __inout HRESULT *phr, __in_opt LPCWSTR pName)
    : CBasePin(pObjectName, pFilter, pLock, phr, pName, PINDIR_OUTPUT)
    , m_pAllocator(NULL)
    , m_pInputPin(NULL)
{
    ASSERT(pFilter);
}

#ifdef UNICODE
CBaseOutputPin::CBaseOutputPin(__in_opt LPCSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock,
                               __inout HRESULT *phr, __in_opt LPCWSTR pName)
    : CBasePin(pObjectName, pFilter, pLock, phr, pName, PINDIR_OUTPUT)
    , m_pAllocator(NULL)
    , m_pInputPin(NULL)
{
    ASSERT(pFilter);
}
#endif

/*   This is called after a media type has been proposed

     Try to complete the connection by agreeing the allocator
*/
HRESULT
CBaseOutputPin::CompleteConnect(IPin *pReceivePin)
{
    UNREFERENCED_PARAMETER(pReceivePin);
    return DecideAllocator(m_pInputPin, &m_pAllocator);
}

/* This method is called when the output pin is about to try and connect to
   an input pin. It is at this point that you should try and grab any extra
   interfaces that you need, in this case IMemInputPin. Because this is
   only called if we are not currently connected we do NOT need to call
   BreakConnect. This also makes it easier to derive classes from us as
   BreakConnect is only called when we actually have to break a connection
   (or a partly made connection) and not when we are checking a connection */

/* Overriden from CBasePin */

HRESULT
CBaseOutputPin::CheckConnect(IPin *pPin)
{
    HRESULT hr = CBasePin::CheckConnect(pPin);
    if (FAILED(hr))
    {
        return hr;
    }

    // get an input pin and an allocator interface
    hr = pPin->QueryInterface(IID_IMemInputPin, (void **)&m_pInputPin);
    if (FAILED(hr))
    {
        return hr;
    }
    return NOERROR;
}

/* Overriden from CBasePin */

HRESULT
CBaseOutputPin::BreakConnect()
{
    /* Release any allocator we hold */

    if (m_pAllocator)
    {
        // Always decommit the allocator because a downstream filter may or
        // may not decommit the connection's allocator.  A memory leak could
        // occur if the allocator is not decommited when a connection is broken.
        HRESULT hr = m_pAllocator->Decommit();
        if (FAILED(hr))
        {
            return hr;
        }

        m_pAllocator->Release();
        m_pAllocator = NULL;
    }

    /* Release any input pin interface we hold */

    if (m_pInputPin)
    {
        m_pInputPin->Release();
        m_pInputPin = NULL;
    }
    return NOERROR;
}

/* This is called when the input pin didn't give us a valid allocator */

HRESULT
CBaseOutputPin::InitAllocator(__deref_out IMemAllocator **ppAlloc)
{
    return CreateMemoryAllocator(ppAlloc);
}

/* Decide on an allocator, override this if you want to use your own allocator
   Override DecideBufferSize to call SetProperties. If the input pin fails
   the GetAllocator call then this will construct a CMemAllocator and call
   DecideBufferSize on that, and if that fails then we are completely hosed.
   If the you succeed the DecideBufferSize call, we will notify the input
   pin of the selected allocator. NOTE this is called during Connect() which
   therefore looks after grabbing and locking the object's critical section */

// We query the input pin for its requested properties and pass this to
// DecideBufferSize to allow it to fulfill requests that it is happy
// with (eg most people don't care about alignment and are thus happy to
// use the downstream pin's alignment request).

HRESULT
CBaseOutputPin::DecideAllocator(IMemInputPin *pPin, __deref_out IMemAllocator **ppAlloc)
{
    HRESULT hr = NOERROR;
    *ppAlloc = NULL;

    // get downstream prop request
    // the derived class may modify this in DecideBufferSize, but
    // we assume that he will consistently modify it the same way,
    // so we only get it once
    ALLOCATOR_PROPERTIES prop;
    ZeroMemory(&prop, sizeof(prop));

    // whatever he returns, we assume prop is either all zeros
    // or he has filled it out.
    pPin->GetAllocatorRequirements(&prop);

    // if he doesn't care about alignment, then set it to 1
    if (prop.cbAlign == 0)
    {
        prop.cbAlign = 1;
    }

    /* Try the allocator provided by the input pin */

    hr = pPin->GetAllocator(ppAlloc);
    if (SUCCEEDED(hr))
    {

        hr = DecideBufferSize(*ppAlloc, &prop);
        if (SUCCEEDED(hr))
        {
            hr = pPin->NotifyAllocator(*ppAlloc, FALSE);
            if (SUCCEEDED(hr))
            {
                return NOERROR;
            }
        }
    }

    /* If the GetAllocator failed we may not have an interface */

    if (*ppAlloc)
    {
        (*ppAlloc)->Release();
        *ppAlloc = NULL;
    }

    /* Try the output pin's allocator by the same method */

    hr = InitAllocator(ppAlloc);
    if (SUCCEEDED(hr))
    {

        // note - the properties passed here are in the same
        // structure as above and may have been modified by
        // the previous call to DecideBufferSize
        hr = DecideBufferSize(*ppAlloc, &prop);
        if (SUCCEEDED(hr))
        {
            hr = pPin->NotifyAllocator(*ppAlloc, FALSE);
            if (SUCCEEDED(hr))
            {
                return NOERROR;
            }
        }
    }

    /* Likewise we may not have an interface to release */

    if (*ppAlloc)
    {
        (*ppAlloc)->Release();
        *ppAlloc = NULL;
    }
    return hr;
}

/* This returns an empty sample buffer from the allocator WARNING the same
   dangers and restrictions apply here as described below for Deliver() */

HRESULT
CBaseOutputPin::GetDeliveryBuffer(__deref_out IMediaSample **ppSample, __in_opt REFERENCE_TIME *pStartTime,
                                  __in_opt REFERENCE_TIME *pEndTime, DWORD dwFlags)
{
    if (m_pAllocator != NULL)
    {
        return m_pAllocator->GetBuffer(ppSample, pStartTime, pEndTime, dwFlags);
    }
    else
    {
        return E_NOINTERFACE;
    }
}

/* Deliver a filled-in sample to the connected input pin. NOTE the object must
   have locked itself before calling us otherwise we may get halfway through
   executing this method only to find the filter graph has got in and
   disconnected us from the input pin. If the filter has no worker threads
   then the lock is best applied on Receive(), otherwise it should be done
   when the worker thread is ready to deliver. There is a wee snag to worker
   threads that this shows up. The worker thread must lock the object when
   it is ready to deliver a sample, but it may have to wait until a state
   change has completed, but that may never complete because the state change
   is waiting for the worker thread to complete. The way to handle this is for
   the state change code to grab the critical section, then set an abort event
   for the worker thread, then release the critical section and wait for the
   worker thread to see the event we set and then signal that it has finished
   (with another event). At which point the state change code can complete */

// note (if you've still got any breath left after reading that) that you
// need to release the sample yourself after this call. if the connected
// input pin needs to hold onto the sample beyond the call, it will addref
// the sample itself.

// of course you must release this one and call GetDeliveryBuffer for the
// next. You cannot reuse it directly.

HRESULT
CBaseOutputPin::Deliver(IMediaSample *pSample)
{
    if (m_pInputPin == NULL)
    {
        return VFW_E_NOT_CONNECTED;
    }

#ifdef DXMPERF
    PERFLOG_DELIVER(m_pName ? m_pName : L"CBaseOutputPin", (IPin *)this, (IPin *)m_pInputPin, pSample, &m_mt);
#endif // DXMPERF

    return m_pInputPin->Receive(pSample);
}

// called from elsewhere in our filter to pass EOS downstream to
// our connected input pin
HRESULT
CBaseOutputPin::DeliverEndOfStream(void)
{
    // remember this is on IPin not IMemInputPin
    if (m_Connected == NULL)
    {
        return VFW_E_NOT_CONNECTED;
    }
    return m_Connected->EndOfStream();
}

/* Commit the allocator's memory, this is called through IMediaFilter
   which is responsible for locking the object before calling us */

HRESULT
CBaseOutputPin::Active(void)
{
    if (m_pAllocator == NULL)
    {
        return VFW_E_NO_ALLOCATOR;
    }
    return m_pAllocator->Commit();
}

/* Free up or unprepare allocator's memory, this is called through
   IMediaFilter which is responsible for locking the object first */

HRESULT
CBaseOutputPin::Inactive(void)
{
    m_bRunTimeError = FALSE;
    if (m_pAllocator == NULL)
    {
        return VFW_E_NO_ALLOCATOR;
    }
    return m_pAllocator->Decommit();
}

// we have a default handling of EndOfStream which is to return
// an error, since this should be called on input pins only
STDMETHODIMP
CBaseOutputPin::EndOfStream(void)
{
    return E_UNEXPECTED;
}

// BeginFlush should be called on input pins only
STDMETHODIMP
CBaseOutputPin::BeginFlush(void)
{
    return E_UNEXPECTED;
}

// EndFlush should be called on input pins only
STDMETHODIMP
CBaseOutputPin::EndFlush(void)
{
    return E_UNEXPECTED;
}

// call BeginFlush on the connected input pin
HRESULT
CBaseOutputPin::DeliverBeginFlush(void)
{
    // remember this is on IPin not IMemInputPin
    if (m_Connected == NULL)
    {
        return VFW_E_NOT_CONNECTED;
    }
    return m_Connected->BeginFlush();
}

// call EndFlush on the connected input pin
HRESULT
CBaseOutputPin::DeliverEndFlush(void)
{
    // remember this is on IPin not IMemInputPin
    if (m_Connected == NULL)
    {
        return VFW_E_NOT_CONNECTED;
    }
    return m_Connected->EndFlush();
}
// deliver NewSegment to connected pin
HRESULT
CBaseOutputPin::DeliverNewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate)
{
    if (m_Connected == NULL)
    {
        return VFW_E_NOT_CONNECTED;
    }
    return m_Connected->NewSegment(tStart, tStop, dRate);
}

//=====================================================================
//=====================================================================
// Implements CBaseInputPin
//=====================================================================
//=====================================================================

/* Constructor creates a default allocator object */

CBaseInputPin::CBaseInputPin(__in_opt LPCTSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock,
                             __inout HRESULT *phr, __in_opt LPCWSTR pPinName)
    : CBasePin(pObjectName, pFilter, pLock, phr, pPinName, PINDIR_INPUT)
    , m_pAllocator(NULL)
    , m_bReadOnly(FALSE)
    , m_bFlushing(FALSE)
{
    ZeroMemory(&m_SampleProps, sizeof(m_SampleProps));
}

#ifdef UNICODE
CBaseInputPin::CBaseInputPin(__in LPCSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock,
                             __inout HRESULT *phr, __in_opt LPCWSTR pPinName)
    : CBasePin(pObjectName, pFilter, pLock, phr, pPinName, PINDIR_INPUT)
    , m_pAllocator(NULL)
    , m_bReadOnly(FALSE)
    , m_bFlushing(FALSE)
{
    ZeroMemory(&m_SampleProps, sizeof(m_SampleProps));
}
#endif

/* Destructor releases it's reference count on the default allocator */

CBaseInputPin::~CBaseInputPin()
{
    if (m_pAllocator != NULL)
    {
        m_pAllocator->Release();
        m_pAllocator = NULL;
    }
}

// override this to publicise our interfaces
STDMETHODIMP
CBaseInputPin::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    /* Do we know about this interface */

    if (riid == IID_IMemInputPin)
    {
        return GetInterface((IMemInputPin *)this, ppv);
    }
    else
    {
        return CBasePin::NonDelegatingQueryInterface(riid, ppv);
    }
}

/* Return the allocator interface that this input pin would like the output
   pin to use. NOTE subsequent calls to GetAllocator should all return an
   interface onto the SAME object so we create one object at the start

   Note:
       The allocator is Release()'d on disconnect and replaced on
       NotifyAllocator().

   Override this to provide your own allocator.
*/

STDMETHODIMP
CBaseInputPin::GetAllocator(__deref_out IMemAllocator **ppAllocator)
{
    CheckPointer(ppAllocator, E_POINTER);
    ValidateReadWritePtr(ppAllocator, sizeof(IMemAllocator *));
    CAutoLock cObjectLock(m_pLock);

    if (m_pAllocator == NULL)
    {
        HRESULT hr = CreateMemoryAllocator(&m_pAllocator);
        if (FAILED(hr))
        {
            return hr;
        }
    }
    ASSERT(m_pAllocator != NULL);
    *ppAllocator = m_pAllocator;
    m_pAllocator->AddRef();
    return NOERROR;
}

/* Tell the input pin which allocator the output pin is actually going to use
   Override this if you care - NOTE the locking we do both here and also in
   GetAllocator is unnecessary but derived classes that do something useful
   will undoubtedly have to lock the object so this might help remind people */

STDMETHODIMP
CBaseInputPin::NotifyAllocator(IMemAllocator *pAllocator, BOOL bReadOnly)
{
    CheckPointer(pAllocator, E_POINTER);
    ValidateReadPtr(pAllocator, sizeof(IMemAllocator));
    CAutoLock cObjectLock(m_pLock);

    IMemAllocator *pOldAllocator = m_pAllocator;
    pAllocator->AddRef();
    m_pAllocator = pAllocator;

    if (pOldAllocator != NULL)
    {
        pOldAllocator->Release();
    }

    // the readonly flag indicates whether samples from this allocator should
    // be regarded as readonly - if true, then inplace transforms will not be
    // allowed.
    m_bReadOnly = (BYTE)bReadOnly;
    return NOERROR;
}

HRESULT
CBaseInputPin::BreakConnect()
{
    /* We don't need our allocator any more */
    if (m_pAllocator)
    {
        // Always decommit the allocator because a downstream filter may or
        // may not decommit the connection's allocator.  A memory leak could
        // occur if the allocator is not decommited when a pin is disconnected.
        HRESULT hr = m_pAllocator->Decommit();
        if (FAILED(hr))
        {
            return hr;
        }

        m_pAllocator->Release();
        m_pAllocator = NULL;
    }

    return S_OK;
}

/* Do something with this media sample - this base class checks to see if the
   format has changed with this media sample and if so checks that the filter
   will accept it, generating a run time error if not. Once we have raised a
   run time error we set a flag so that no more samples will be accepted

   It is important that any filter should override this method and implement
   synchronization so that samples are not processed when the pin is
   disconnected etc
*/

STDMETHODIMP
CBaseInputPin::Receive(IMediaSample *pSample)
{
    CheckPointer(pSample, E_POINTER);
    ValidateReadPtr(pSample, sizeof(IMediaSample));
    ASSERT(pSample);

    HRESULT hr = CheckStreaming();
    if (S_OK != hr)
    {
        return hr;
    }

#ifdef DXMPERF
    PERFLOG_RECEIVE(m_pName ? m_pName : L"CBaseInputPin", (IPin *)m_Connected, (IPin *)this, pSample, &m_mt);
#endif // DXMPERF

    /* Check for IMediaSample2 */
    IMediaSample2 *pSample2;
    if (SUCCEEDED(pSample->QueryInterface(IID_IMediaSample2, (void **)&pSample2)))
    {
        hr = pSample2->GetProperties(sizeof(m_SampleProps), (PBYTE)&m_SampleProps);
        pSample2->Release();
        if (FAILED(hr))
        {
            return hr;
        }
    }
    else
    {
        /*  Get the properties the hard way */
        m_SampleProps.cbData = sizeof(m_SampleProps);
        m_SampleProps.dwTypeSpecificFlags = 0;
        m_SampleProps.dwStreamId = AM_STREAM_MEDIA;
        m_SampleProps.dwSampleFlags = 0;
        if (S_OK == pSample->IsDiscontinuity())
        {
            m_SampleProps.dwSampleFlags |= AM_SAMPLE_DATADISCONTINUITY;
        }
        if (S_OK == pSample->IsPreroll())
        {
            m_SampleProps.dwSampleFlags |= AM_SAMPLE_PREROLL;
        }
        if (S_OK == pSample->IsSyncPoint())
        {
            m_SampleProps.dwSampleFlags |= AM_SAMPLE_SPLICEPOINT;
        }
        if (SUCCEEDED(pSample->GetTime(&m_SampleProps.tStart, &m_SampleProps.tStop)))
        {
            m_SampleProps.dwSampleFlags |= AM_SAMPLE_TIMEVALID | AM_SAMPLE_STOPVALID;
        }
        if (S_OK == pSample->GetMediaType(&m_SampleProps.pMediaType))
        {
            m_SampleProps.dwSampleFlags |= AM_SAMPLE_TYPECHANGED;
        }
        pSample->GetPointer(&m_SampleProps.pbBuffer);
        m_SampleProps.lActual = pSample->GetActualDataLength();
        m_SampleProps.cbBuffer = pSample->GetSize();
    }

    /* Has the format changed in this sample */

    if (!(m_SampleProps.dwSampleFlags & AM_SAMPLE_TYPECHANGED))
    {
        return NOERROR;
    }

    /* Check the derived class accepts this format */
    /* This shouldn't fail as the source must call QueryAccept first */

    hr = CheckMediaType((CMediaType *)m_SampleProps.pMediaType);

    if (hr == NOERROR)
    {
        return NOERROR;
    }

    /* Raise a runtime error if we fail the media type */

    m_bRunTimeError = TRUE;
    EndOfStream();
    m_pFilter->NotifyEvent(EC_ERRORABORT, VFW_E_TYPE_NOT_ACCEPTED, 0);
    return VFW_E_INVALIDMEDIATYPE;
}

/*  Receive multiple samples */
STDMETHODIMP
CBaseInputPin::ReceiveMultiple(__in_ecount(nSamples) IMediaSample **pSamples, long nSamples,
                               __out long *nSamplesProcessed)
{
    CheckPointer(pSamples, E_POINTER);
    ValidateReadPtr(pSamples, nSamples * sizeof(IMediaSample *));

    HRESULT hr = S_OK;
    *nSamplesProcessed = 0;
    while (nSamples-- > 0)
    {
        hr = Receive(pSamples[*nSamplesProcessed]);

        /*  S_FALSE means don't send any more */
        if (hr != S_OK)
        {
            break;
        }
        (*nSamplesProcessed)++;
    }
    return hr;
}

/*  See if Receive() might block */
STDMETHODIMP
CBaseInputPin::ReceiveCanBlock()
{
    /*  Ask all the output pins if they block
        If there are no output pin assume we do block
    */
    int cPins = m_pFilter->GetPinCount();
    int cOutputPins = 0;
    for (int c = 0; c < cPins; c++)
    {
        CBasePin *pPin = m_pFilter->GetPin(c);
        if (NULL == pPin)
        {
            break;
        }
        PIN_DIRECTION pd;
        HRESULT hr = pPin->QueryDirection(&pd);
        if (FAILED(hr))
        {
            return hr;
        }

        if (pd == PINDIR_OUTPUT)
        {

            IPin *pConnected;
            hr = pPin->ConnectedTo(&pConnected);
            if (SUCCEEDED(hr))
            {
                ASSERT(pConnected != NULL);
                cOutputPins++;
                IMemInputPin *pInputPin;
                hr = pConnected->QueryInterface(IID_IMemInputPin, (void **)&pInputPin);
                pConnected->Release();
                if (SUCCEEDED(hr))
                {
                    hr = pInputPin->ReceiveCanBlock();
                    pInputPin->Release();
                    if (hr != S_FALSE)
                    {
                        return S_OK;
                    }
                }
                else
                {
                    /*  There's a transport we don't understand here */
                    return S_OK;
                }
            }
        }
    }
    return cOutputPins == 0 ? S_OK : S_FALSE;
}

// Default handling for BeginFlush - call at the beginning
// of your implementation (makes sure that all Receive calls
// fail). After calling this, you need to free any queued data
// and then call downstream.
STDMETHODIMP
CBaseInputPin::BeginFlush(void)
{
    //  BeginFlush is NOT synchronized with streaming but is part of
    //  a control action - hence we synchronize with the filter
    CAutoLock lck(m_pLock);

    // if we are already in mid-flush, this is probably a mistake
    // though not harmful - try to pick it up for now so I can think about it
    ASSERT(!m_bFlushing);

    // first thing to do is ensure that no further Receive calls succeed
    m_bFlushing = TRUE;

    // now discard any data and call downstream - must do that
    // in derived classes
    return S_OK;
}

// default handling for EndFlush - call at end of your implementation
// - before calling this, ensure that there is no queued data and no thread
// pushing any more without a further receive, then call downstream,
// then call this method to clear the m_bFlushing flag and re-enable
// receives
STDMETHODIMP
CBaseInputPin::EndFlush(void)
{
    //  Endlush is NOT synchronized with streaming but is part of
    //  a control action - hence we synchronize with the filter
    CAutoLock lck(m_pLock);

    // almost certainly a mistake if we are not in mid-flush
    ASSERT(m_bFlushing);

    // before calling, sync with pushing thread and ensure
    // no more data is going downstream, then call EndFlush on
    // downstream pins.

    // now re-enable Receives
    m_bFlushing = FALSE;

    // No more errors
    m_bRunTimeError = FALSE;

    return S_OK;
}

STDMETHODIMP
CBaseInputPin::Notify(IBaseFilter *pSender, Quality q)
{
    UNREFERENCED_PARAMETER(q);
    CheckPointer(pSender, E_POINTER);
    ValidateReadPtr(pSender, sizeof(IBaseFilter));
    DbgBreak("IQuality::Notify called on an input pin");
    return NOERROR;
} // Notify

/* Free up or unprepare allocator's memory, this is called through
   IMediaFilter which is responsible for locking the object first */

HRESULT
CBaseInputPin::Inactive(void)
{
    m_bRunTimeError = FALSE;
    if (m_pAllocator == NULL)
    {
        return VFW_E_NO_ALLOCATOR;
    }

    m_bFlushing = FALSE;

    return m_pAllocator->Decommit();
}

// what requirements do we have of the allocator - override if you want
// to support other people's allocators but need a specific alignment
// or prefix.
STDMETHODIMP
CBaseInputPin::GetAllocatorRequirements(__out ALLOCATOR_PROPERTIES *pProps)
{
    UNREFERENCED_PARAMETER(pProps);
    return E_NOTIMPL;
}

//  Check if it's OK to process data
//
HRESULT
CBaseInputPin::CheckStreaming()
{
    //  Shouldn't be able to get any data if we're not connected!
    ASSERT(IsConnected());

    //  Don't process stuff in Stopped state
    if (IsStopped())
    {
        return VFW_E_WRONG_STATE;
    }
    if (m_bFlushing)
    {
        return S_FALSE;
    }
    if (m_bRunTimeError)
    {
        return VFW_E_RUNTIME_ERROR;
    }
    return S_OK;
}

// Pass on the Quality notification q to
// a. Our QualityControl sink (if we have one) or else
// b. to our upstream filter
// and if that doesn't work, throw it away with a bad return code
HRESULT
CBaseInputPin::PassNotify(Quality &q)
{
    // We pass the message on, which means that we find the quality sink
    // for our input pin and send it there

    // DbgLog((LOG_TRACE,3,TEXT("Passing Quality notification through transform")));
    if (m_pQSink != NULL)
    {
        return m_pQSink->Notify(m_pFilter, q);
    }
    else
    {
        // no sink set, so pass it upstream
        HRESULT hr;
        IQualityControl *pIQC;

        hr = VFW_E_NOT_FOUND; // default
        if (m_Connected)
        {
            m_Connected->QueryInterface(IID_IQualityControl, (void **)&pIQC);

            if (pIQC != NULL)
            {
                hr = pIQC->Notify(m_pFilter, q);
                pIQC->Release();
            }
        }
        return hr;
    }

} // PassNotify

//=====================================================================
//=====================================================================
// Memory allocation class, implements CMediaSample
//=====================================================================
//=====================================================================

/* NOTE The implementation of this class calls the CUnknown constructor with
   a NULL outer unknown pointer. This has the effect of making us a self
   contained class, ie any QueryInterface, AddRef or Release calls will be
   routed to the class's NonDelegatingUnknown methods. You will typically
   find that the classes that do this then override one or more of these
   virtual functions to provide more specialised behaviour. A good example
   of this is where a class wants to keep the QueryInterface internal but
   still wants it's lifetime controlled by the external object */

/* The last two parameters have default values of NULL and zero */

CMediaSample::CMediaSample(__in_opt LPCTSTR pName, __in_opt CBaseAllocator *pAllocator, __inout_opt HRESULT *phr,
                           __in_bcount_opt(length) LPBYTE pBuffer,
                           LONG length)
    : m_pBuffer(pBuffer)
    , // Initialise the buffer
    m_cbBuffer(length)
    , // And it's length
    m_lActual(length)
    , // By default, actual = length
    m_pMediaType(NULL)
    , // No media type change
    m_dwFlags(0)
    , // Nothing set
    m_cRef(0)
    , // 0 ref count
    m_dwTypeSpecificFlags(0)
    , // Type specific flags
    m_dwStreamId(AM_STREAM_MEDIA)
    ,                        // Stream id
    m_pAllocator(pAllocator) // Allocator
{
#ifdef DXMPERF
    PERFLOG_CTOR(pName ? pName : L"CMediaSample", (IMediaSample *)this);
#endif // DXMPERF

    /* We must have an owner and it must also be derived from class
       CBaseAllocator BUT we do not hold a reference count on it */

    ASSERT(pAllocator);

    if (length < 0)
    {
        *phr = VFW_E_BUFFER_OVERFLOW;
        m_cbBuffer = 0;
    }
}

#ifdef UNICODE
CMediaSample::CMediaSample(__in_opt LPCSTR pName, __in_opt CBaseAllocator *pAllocator, __inout_opt HRESULT *phr,
                           __in_bcount_opt(length) LPBYTE pBuffer,
                           LONG length)
    : m_pBuffer(pBuffer)
    , // Initialise the buffer
    m_cbBuffer(length)
    , // And it's length
    m_lActual(length)
    , // By default, actual = length
    m_pMediaType(NULL)
    , // No media type change
    m_dwFlags(0)
    , // Nothing set
    m_cRef(0)
    , // 0 ref count
    m_dwTypeSpecificFlags(0)
    , // Type specific flags
    m_dwStreamId(AM_STREAM_MEDIA)
    ,                        // Stream id
    m_pAllocator(pAllocator) // Allocator
{
#ifdef DXMPERF
    PERFLOG_CTOR(L"CMediaSample", (IMediaSample *)this);
#endif // DXMPERF

    /* We must have an owner and it must also be derived from class
       CBaseAllocator BUT we do not hold a reference count on it */

    ASSERT(pAllocator);
}
#endif

/* Destructor deletes the media type memory */

CMediaSample::~CMediaSample()
{
#ifdef DXMPERF
    PERFLOG_DTOR(L"CMediaSample", (IMediaSample *)this);
#endif // DXMPERF

    if (m_pMediaType)
    {
        DeleteMediaType(m_pMediaType);
    }
}

/* Override this to publicise our interfaces */

STDMETHODIMP
CMediaSample::QueryInterface(REFIID riid, __deref_out void **ppv)
{
    if (riid == IID_IMediaSample || riid == IID_IMediaSample2 || riid == IID_IUnknown)
    {
        return GetInterface((IMediaSample *)this, ppv);
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

STDMETHODIMP_(ULONG)
CMediaSample::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

// --  CMediaSample lifetimes --
//
// On final release of this sample buffer it is not deleted but
// returned to the freelist of the owning memory allocator
//
// The allocator may be waiting for the last buffer to be placed on the free
// list in order to decommit all the memory, so the ReleaseBuffer() call may
// result in this sample being deleted. We also need to hold a refcount on
// the allocator to stop that going away until we have finished with this.
// However, we cannot release the allocator before the ReleaseBuffer, as the
// release may cause us to be deleted. Similarly we can't do it afterwards.
//
// Thus we must leave it to the allocator to hold an addref on our behalf.
// When he issues us in GetBuffer, he addref's himself. When ReleaseBuffer
// is called, he releases himself, possibly causing us and him to be deleted.

STDMETHODIMP_(ULONG)
CMediaSample::Release()
{
    /* Decrement our own private reference count */
    LONG lRef;
    if (m_cRef == 1)
    {
        lRef = 0;
        m_cRef = 0;
    }
    else
    {
        lRef = InterlockedDecrement(&m_cRef);
    }
    ASSERT(lRef >= 0);

    DbgLog((LOG_MEMORY, 3, TEXT("    Unknown %X ref-- = %d"), this, m_cRef));

    /* Did we release our final reference count */
    if (lRef == 0)
    {
        /* Free all resources */
        if (m_dwFlags & Sample_TypeChanged)
        {
            SetMediaType(NULL);
        }
        ASSERT(m_pMediaType == NULL);
        m_dwFlags = 0;
        m_dwTypeSpecificFlags = 0;
        m_dwStreamId = AM_STREAM_MEDIA;

        /* This may cause us to be deleted */
        // Our refcount is reliably 0 thus no-one will mess with us
        m_pAllocator->ReleaseBuffer(this);
    }
    return (ULONG)lRef;
}

// set the buffer pointer and length. Used by allocators that
// want variable sized pointers or pointers into already-read data.
// This is only available through a CMediaSample* not an IMediaSample*
// and so cannot be changed by clients.
HRESULT
CMediaSample::SetPointer(__in_bcount(cBytes) BYTE *ptr, LONG cBytes)
{
    if (cBytes < 0)
    {
        return VFW_E_BUFFER_OVERFLOW;
    }
    m_pBuffer = ptr;     // new buffer area (could be null)
    m_cbBuffer = cBytes; // length of buffer
    m_lActual = cBytes;  // length of data in buffer (assume full)

    return S_OK;
}

// get me a read/write pointer to this buffer's memory. I will actually
// want to use sizeUsed bytes.
STDMETHODIMP
CMediaSample::GetPointer(__deref_out BYTE **ppBuffer)
{
    ValidateReadWritePtr(ppBuffer, sizeof(BYTE *));

    // creator must have set pointer either during
    // constructor or by SetPointer
    ASSERT(m_pBuffer);

    *ppBuffer = m_pBuffer;
    return NOERROR;
}

// return the size in bytes of this buffer
STDMETHODIMP_(LONG)
CMediaSample::GetSize(void)
{
    return m_cbBuffer;
}

// get the stream time at which this sample should start and finish.
STDMETHODIMP
CMediaSample::GetTime(__out REFERENCE_TIME *pTimeStart, // put time here
                      __out REFERENCE_TIME *pTimeEnd)
{
    ValidateReadWritePtr(pTimeStart, sizeof(REFERENCE_TIME));
    ValidateReadWritePtr(pTimeEnd, sizeof(REFERENCE_TIME));

    if (!(m_dwFlags & Sample_StopValid))
    {
        if (!(m_dwFlags & Sample_TimeValid))
        {
            return VFW_E_SAMPLE_TIME_NOT_SET;
        }
        else
        {
            *pTimeStart = m_Start;

            //  Make sure old stuff works
            *pTimeEnd = m_Start + 1;
            return VFW_S_NO_STOP_TIME;
        }
    }

    *pTimeStart = m_Start;
    *pTimeEnd = m_End;
    return NOERROR;
}

// Set the stream time at which this sample should start and finish.
// NULL pointers means the time is reset
STDMETHODIMP
CMediaSample::SetTime(__in_opt REFERENCE_TIME *pTimeStart, __in_opt REFERENCE_TIME *pTimeEnd)
{
    if (pTimeStart == NULL)
    {
        ASSERT(pTimeEnd == NULL);
        m_dwFlags &= ~(Sample_TimeValid | Sample_StopValid);
    }
    else
    {
        if (pTimeEnd == NULL)
        {
            m_Start = *pTimeStart;
            m_dwFlags |= Sample_TimeValid;
            m_dwFlags &= ~Sample_StopValid;
        }
        else
        {
            ValidateReadPtr(pTimeStart, sizeof(REFERENCE_TIME));
            ValidateReadPtr(pTimeEnd, sizeof(REFERENCE_TIME));
            ASSERT(*pTimeEnd >= *pTimeStart);

            m_Start = *pTimeStart;
            m_End = *pTimeEnd;
            m_dwFlags |= Sample_TimeValid | Sample_StopValid;
        }
    }
    return NOERROR;
}

// get the media times (eg bytes) for this sample
STDMETHODIMP
CMediaSample::GetMediaTime(__out LONGLONG *pTimeStart, __out LONGLONG *pTimeEnd)
{
    ValidateReadWritePtr(pTimeStart, sizeof(LONGLONG));
    ValidateReadWritePtr(pTimeEnd, sizeof(LONGLONG));

    if (!(m_dwFlags & Sample_MediaTimeValid))
    {
        return VFW_E_MEDIA_TIME_NOT_SET;
    }

    *pTimeStart = m_MediaStart;
    *pTimeEnd = (m_MediaStart + m_MediaEnd);
    return NOERROR;
}

// Set the media times for this sample
STDMETHODIMP
CMediaSample::SetMediaTime(__in_opt LONGLONG *pTimeStart, __in_opt LONGLONG *pTimeEnd)
{
    if (pTimeStart == NULL)
    {
        ASSERT(pTimeEnd == NULL);
        m_dwFlags &= ~Sample_MediaTimeValid;
    }
    else
    {
        if (NULL == pTimeEnd)
        {
            return E_POINTER;
        }
        ValidateReadPtr(pTimeStart, sizeof(LONGLONG));
        ValidateReadPtr(pTimeEnd, sizeof(LONGLONG));
        ASSERT(*pTimeEnd >= *pTimeStart);

        m_MediaStart = *pTimeStart;
        m_MediaEnd = (LONG)(*pTimeEnd - *pTimeStart);
        m_dwFlags |= Sample_MediaTimeValid;
    }
    return NOERROR;
}

STDMETHODIMP
CMediaSample::IsSyncPoint(void)
{
    if (m_dwFlags & Sample_SyncPoint)
    {
        return S_OK;
    }
    else
    {
        return S_FALSE;
    }
}

STDMETHODIMP
CMediaSample::SetSyncPoint(BOOL bIsSyncPoint)
{
    if (bIsSyncPoint)
    {
        m_dwFlags |= Sample_SyncPoint;
    }
    else
    {
        m_dwFlags &= ~Sample_SyncPoint;
    }
    return NOERROR;
}

// returns S_OK if there is a discontinuity in the data (this same is
// not a continuation of the previous stream of data
// - there has been a seek).
STDMETHODIMP
CMediaSample::IsDiscontinuity(void)
{
    if (m_dwFlags & Sample_Discontinuity)
    {
        return S_OK;
    }
    else
    {
        return S_FALSE;
    }
}

// set the discontinuity property - TRUE if this sample is not a
// continuation, but a new sample after a seek.
STDMETHODIMP
CMediaSample::SetDiscontinuity(BOOL bDiscont)
{
    // should be TRUE or FALSE
    if (bDiscont)
    {
        m_dwFlags |= Sample_Discontinuity;
    }
    else
    {
        m_dwFlags &= ~Sample_Discontinuity;
    }
    return S_OK;
}

STDMETHODIMP
CMediaSample::IsPreroll(void)
{
    if (m_dwFlags & Sample_Preroll)
    {
        return S_OK;
    }
    else
    {
        return S_FALSE;
    }
}

STDMETHODIMP
CMediaSample::SetPreroll(BOOL bIsPreroll)
{
    if (bIsPreroll)
    {
        m_dwFlags |= Sample_Preroll;
    }
    else
    {
        m_dwFlags &= ~Sample_Preroll;
    }
    return NOERROR;
}

STDMETHODIMP_(LONG)
CMediaSample::GetActualDataLength(void)
{
    return m_lActual;
}

STDMETHODIMP
CMediaSample::SetActualDataLength(LONG lActual)
{
    if (lActual > m_cbBuffer || lActual < 0)
    {
        ASSERT(lActual <= GetSize());
        return VFW_E_BUFFER_OVERFLOW;
    }
    m_lActual = lActual;
    return NOERROR;
}

/* These allow for limited format changes in band */

STDMETHODIMP
CMediaSample::GetMediaType(__deref_out AM_MEDIA_TYPE **ppMediaType)
{
    ValidateReadWritePtr(ppMediaType, sizeof(AM_MEDIA_TYPE *));
    ASSERT(ppMediaType);

    /* Do we have a new media type for them */

    if (!(m_dwFlags & Sample_TypeChanged))
    {
        ASSERT(m_pMediaType == NULL);
        *ppMediaType = NULL;
        return S_FALSE;
    }

    ASSERT(m_pMediaType);

    /* Create a copy of our media type */

    *ppMediaType = CreateMediaType(m_pMediaType);
    if (*ppMediaType == NULL)
    {
        return E_OUTOFMEMORY;
    }
    return NOERROR;
}

/* Mark this sample as having a different format type */

STDMETHODIMP
CMediaSample::SetMediaType(__in_opt AM_MEDIA_TYPE *pMediaType)
{
    /* Delete the current media type */

    if (m_pMediaType)
    {
        DeleteMediaType(m_pMediaType);
        m_pMediaType = NULL;
    }

    /* Mechanism for resetting the format type */

    if (pMediaType == NULL)
    {
        m_dwFlags &= ~Sample_TypeChanged;
        return NOERROR;
    }

    ASSERT(pMediaType);
    ValidateReadPtr(pMediaType, sizeof(AM_MEDIA_TYPE));

    /* Take a copy of the media type */

    m_pMediaType = CreateMediaType(pMediaType);
    if (m_pMediaType == NULL)
    {
        m_dwFlags &= ~Sample_TypeChanged;
        return E_OUTOFMEMORY;
    }

    m_dwFlags |= Sample_TypeChanged;
    return NOERROR;
}

// Set and get properties (IMediaSample2)
STDMETHODIMP CMediaSample::GetProperties(DWORD cbProperties, __out_bcount(cbProperties) BYTE *pbProperties)
{
    if (0 != cbProperties)
    {
        CheckPointer(pbProperties, E_POINTER);
        //  Return generic stuff up to the length
        AM_SAMPLE2_PROPERTIES Props;
        Props.cbData = min(cbProperties, sizeof(Props));
        Props.dwSampleFlags = m_dwFlags & ~Sample_MediaTimeValid;
        Props.dwTypeSpecificFlags = m_dwTypeSpecificFlags;
        Props.pbBuffer = m_pBuffer;
        Props.cbBuffer = m_cbBuffer;
        Props.lActual = m_lActual;
        Props.tStart = m_Start;
        Props.tStop = m_End;
        Props.dwStreamId = m_dwStreamId;
        if (m_dwFlags & AM_SAMPLE_TYPECHANGED)
        {
            Props.pMediaType = m_pMediaType;
        }
        else
        {
            Props.pMediaType = NULL;
        }
        CopyMemory(pbProperties, &Props, Props.cbData);
    }
    return S_OK;
}

#define CONTAINS_FIELD(type, field, offset) ((FIELD_OFFSET(type, field) + sizeof(((type *)0)->field)) <= offset)

HRESULT CMediaSample::SetProperties(DWORD cbProperties, __in_bcount(cbProperties) const BYTE *pbProperties)
{

    /*  Generic properties */
    AM_MEDIA_TYPE *pMediaType = NULL;

    if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, cbData, cbProperties))
    {
        CheckPointer(pbProperties, E_POINTER);
        AM_SAMPLE2_PROPERTIES *pProps = (AM_SAMPLE2_PROPERTIES *)pbProperties;

        /*  Don't use more data than is actually there */
        if (pProps->cbData < cbProperties)
        {
            cbProperties = pProps->cbData;
        }
        /*  We only handle IMediaSample2 */
        if (cbProperties > sizeof(*pProps) || pProps->cbData > sizeof(*pProps))
        {
            return E_INVALIDARG;
        }
        /*  Do checks first, the assignments (for backout) */
        if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, dwSampleFlags, cbProperties))
        {
            /*  Check the flags */
            if (pProps->dwSampleFlags & (~Sample_ValidFlags | Sample_MediaTimeValid))
            {
                return E_INVALIDARG;
            }
            /*  Check a flag isn't being set for a property
                not being provided
            */
            if ((pProps->dwSampleFlags & AM_SAMPLE_TIMEVALID) && !(m_dwFlags & AM_SAMPLE_TIMEVALID) &&
                !CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, tStop, cbProperties))
            {
                return E_INVALIDARG;
            }
        }
        /*  NB - can't SET the pointer or size */
        if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, pbBuffer, cbProperties))
        {

            /*  Check pbBuffer */
            if (pProps->pbBuffer != 0 && pProps->pbBuffer != m_pBuffer)
            {
                return E_INVALIDARG;
            }
        }
        if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, cbBuffer, cbProperties))
        {

            /*  Check cbBuffer */
            if (pProps->cbBuffer != 0 && pProps->cbBuffer != m_cbBuffer)
            {
                return E_INVALIDARG;
            }
        }
        if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, cbBuffer, cbProperties) &&
            CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, lActual, cbProperties))
        {

            /*  Check lActual */
            if (pProps->cbBuffer < pProps->lActual)
            {
                return E_INVALIDARG;
            }
        }

        if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, pMediaType, cbProperties))
        {

            /*  Check pMediaType */
            if (pProps->dwSampleFlags & AM_SAMPLE_TYPECHANGED)
            {
                CheckPointer(pProps->pMediaType, E_POINTER);
                pMediaType = CreateMediaType(pProps->pMediaType);
                if (pMediaType == NULL)
                {
                    return E_OUTOFMEMORY;
                }
            }
        }

        /*  Now do the assignments */
        if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, dwStreamId, cbProperties))
        {
            m_dwStreamId = pProps->dwStreamId;
        }
        if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, dwSampleFlags, cbProperties))
        {
            /*  Set the flags */
            m_dwFlags = pProps->dwSampleFlags | (m_dwFlags & Sample_MediaTimeValid);
            m_dwTypeSpecificFlags = pProps->dwTypeSpecificFlags;
        }
        else
        {
            if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, dwTypeSpecificFlags, cbProperties))
            {
                m_dwTypeSpecificFlags = pProps->dwTypeSpecificFlags;
            }
        }

        if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, lActual, cbProperties))
        {
            /*  Set lActual */
            m_lActual = pProps->lActual;
        }

        if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, tStop, cbProperties))
        {

            /*  Set the times */
            m_End = pProps->tStop;
        }
        if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, tStart, cbProperties))
        {

            /*  Set the times */
            m_Start = pProps->tStart;
        }

        if (CONTAINS_FIELD(AM_SAMPLE2_PROPERTIES, pMediaType, cbProperties))
        {
            /*  Set pMediaType */
            if (pProps->dwSampleFlags & AM_SAMPLE_TYPECHANGED)
            {
                if (m_pMediaType != NULL)
                {
                    DeleteMediaType(m_pMediaType);
                }
                m_pMediaType = pMediaType;
            }
        }

        /*  Fix up the type changed flag to correctly reflect the current state
            If, for instance the input contained no type change but the
            output does then if we don't do this we'd lose the
            output media type.
        */
        if (m_pMediaType)
        {
            m_dwFlags |= Sample_TypeChanged;
        }
        else
        {
            m_dwFlags &= ~Sample_TypeChanged;
        }
    }

    return S_OK;
}

//
// The streaming thread calls IPin::NewSegment(), IPin::EndOfStream(),
// IMemInputPin::Receive() and IMemInputPin::ReceiveMultiple() on the
// connected input pin.  The application thread calls Block().  The
// following class members can only be called by the streaming thread.
//
//    Deliver()
//    DeliverNewSegment()
//    StartUsingOutputPin()
//    StopUsingOutputPin()
//    ChangeOutputFormat()
//    ChangeMediaType()
//    DynamicReconnect()
//
// The following class members can only be called by the application thread.
//
//    Block()
//    SynchronousBlockOutputPin()
//    AsynchronousBlockOutputPin()
//

CDynamicOutputPin::CDynamicOutputPin(__in_opt LPCTSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock,
                                     __inout HRESULT *phr, __in_opt LPCWSTR pName)
    : CBaseOutputPin(pObjectName, pFilter, pLock, phr, pName)
    , m_hStopEvent(NULL)
    , m_pGraphConfig(NULL)
    , m_bPinUsesReadOnlyAllocator(FALSE)
    , m_BlockState(NOT_BLOCKED)
    , m_hUnblockOutputPinEvent(NULL)
    , m_hNotifyCallerPinBlockedEvent(NULL)
    , m_dwBlockCallerThreadID(0)
    , m_dwNumOutstandingOutputPinUsers(0)
{
    HRESULT hr = Initialize();
    if (FAILED(hr))
    {
        *phr = hr;
        return;
    }
}

#ifdef UNICODE
CDynamicOutputPin::CDynamicOutputPin(__in_opt LPCSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock,
                                     __inout HRESULT *phr, __in_opt LPCWSTR pName)
    : CBaseOutputPin(pObjectName, pFilter, pLock, phr, pName)
    , m_hStopEvent(NULL)
    , m_pGraphConfig(NULL)
    , m_bPinUsesReadOnlyAllocator(FALSE)
    , m_BlockState(NOT_BLOCKED)
    , m_hUnblockOutputPinEvent(NULL)
    , m_hNotifyCallerPinBlockedEvent(NULL)
    , m_dwBlockCallerThreadID(0)
    , m_dwNumOutstandingOutputPinUsers(0)
{
    HRESULT hr = Initialize();
    if (FAILED(hr))
    {
        *phr = hr;
        return;
    }
}
#endif

CDynamicOutputPin::~CDynamicOutputPin()
{
    if (NULL != m_hUnblockOutputPinEvent)
    {
        // This call should not fail because we have access to m_hUnblockOutputPinEvent
        // and m_hUnblockOutputPinEvent is a valid event.
        EXECUTE_ASSERT(::CloseHandle(m_hUnblockOutputPinEvent));
    }

    if (NULL != m_hNotifyCallerPinBlockedEvent)
    {
        // This call should not fail because we have access to m_hNotifyCallerPinBlockedEvent
        // and m_hNotifyCallerPinBlockedEvent is a valid event.
        EXECUTE_ASSERT(::CloseHandle(m_hNotifyCallerPinBlockedEvent));
    }
}

HRESULT CDynamicOutputPin::Initialize(void)
{
    m_hUnblockOutputPinEvent = ::CreateEvent(NULL,  // The event will have the default security descriptor.
                                             TRUE,  // This is a manual reset event.
                                             TRUE,  // The event is initially signaled.
                                             NULL); // The event is not named.

    // CreateEvent() returns NULL if an error occurs.
    if (NULL == m_hUnblockOutputPinEvent)
    {
        return AmGetLastErrorToHResult();
    }

    //  Set flag to say we can reconnect while streaming.
    SetReconnectWhenActive(true);

    return S_OK;
}

STDMETHODIMP CDynamicOutputPin::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    if (riid == IID_IPinFlowControl)
    {
        return GetInterface(static_cast<IPinFlowControl *>(this), ppv);
    }
    else
    {
        return CBaseOutputPin::NonDelegatingQueryInterface(riid, ppv);
    }
}

STDMETHODIMP CDynamicOutputPin::Disconnect(void)
{
    CAutoLock cObjectLock(m_pLock);
    return DisconnectInternal();
}

STDMETHODIMP CDynamicOutputPin::Block(DWORD dwBlockFlags, HANDLE hEvent)
{
    const DWORD VALID_FLAGS = AM_PIN_FLOW_CONTROL_BLOCK;

    // Check for illegal flags.
    if (dwBlockFlags & ~VALID_FLAGS)
    {
        return E_INVALIDARG;
    }

    // Make sure the event is unsignaled.
    if ((dwBlockFlags & AM_PIN_FLOW_CONTROL_BLOCK) && (NULL != hEvent))
    {
        if (!::ResetEvent(hEvent))
        {
            return AmGetLastErrorToHResult();
        }
    }

    // No flags are set if we are unblocking the output pin.
    if (0 == dwBlockFlags)
    {

        // This parameter should be NULL because unblock operations are always synchronous.
        // There is no need to notify the caller when the event is done.
        if (NULL != hEvent)
        {
            return E_INVALIDARG;
        }
    }

#ifdef DEBUG
    AssertValid();
#endif // DEBUG

    HRESULT hr;

    if (dwBlockFlags & AM_PIN_FLOW_CONTROL_BLOCK)
    {
        // IPinFlowControl::Block()'s hEvent parameter is NULL if the block is synchronous.
        // If hEvent is not NULL, the block is asynchronous.
        if (NULL == hEvent)
        {
            hr = SynchronousBlockOutputPin();
        }
        else
        {
            hr = AsynchronousBlockOutputPin(hEvent);
        }
    }
    else
    {
        hr = UnblockOutputPin();
    }

#ifdef DEBUG
    AssertValid();
#endif // DEBUG

    if (FAILED(hr))
    {
        return hr;
    }

    return S_OK;
}

HRESULT CDynamicOutputPin::SynchronousBlockOutputPin(void)
{
    HANDLE hNotifyCallerPinBlockedEvent = ::CreateEvent(NULL,  // The event will have the default security attributes.
                                                        FALSE, // This is an automatic reset event.
                                                        FALSE, // The event is initially unsignaled.
                                                        NULL); // The event is not named.

    // CreateEvent() returns NULL if an error occurs.
    if (NULL == hNotifyCallerPinBlockedEvent)
    {
        return AmGetLastErrorToHResult();
    }

    HRESULT hr = AsynchronousBlockOutputPin(hNotifyCallerPinBlockedEvent);
    if (FAILED(hr))
    {
        // This call should not fail because we have access to hNotifyCallerPinBlockedEvent
        // and hNotifyCallerPinBlockedEvent is a valid event.
        EXECUTE_ASSERT(::CloseHandle(hNotifyCallerPinBlockedEvent));

        return hr;
    }

    hr = WaitEvent(hNotifyCallerPinBlockedEvent);

    // This call should not fail because we have access to hNotifyCallerPinBlockedEvent
    // and hNotifyCallerPinBlockedEvent is a valid event.
    EXECUTE_ASSERT(::CloseHandle(hNotifyCallerPinBlockedEvent));

    if (FAILED(hr))
    {
        return hr;
    }

    return S_OK;
}

HRESULT CDynamicOutputPin::AsynchronousBlockOutputPin(HANDLE hNotifyCallerPinBlockedEvent)
{
    // This function holds the m_BlockStateLock because it uses
    // m_dwBlockCallerThreadID, m_BlockState and
    // m_hNotifyCallerPinBlockedEvent.
    CAutoLock alBlockStateLock(&m_BlockStateLock);

    if (NOT_BLOCKED != m_BlockState)
    {
        if (m_dwBlockCallerThreadID == ::GetCurrentThreadId())
        {
            return VFW_E_PIN_ALREADY_BLOCKED_ON_THIS_THREAD;
        }
        else
        {
            return VFW_E_PIN_ALREADY_BLOCKED;
        }
    }

    BOOL fSuccess = ::DuplicateHandle(::GetCurrentProcess(), hNotifyCallerPinBlockedEvent, ::GetCurrentProcess(),
                                      &m_hNotifyCallerPinBlockedEvent, EVENT_MODIFY_STATE, FALSE, 0);
    if (!fSuccess)
    {
        return AmGetLastErrorToHResult();
    }

    m_BlockState = PENDING;
    m_dwBlockCallerThreadID = ::GetCurrentThreadId();

    // The output pin cannot be blocked if the streaming thread is
    // calling IPin::NewSegment(), IPin::EndOfStream(), IMemInputPin::Receive()
    // or IMemInputPin::ReceiveMultiple() on the connected input pin.  Also, it
    // cannot be blocked if the streaming thread is calling DynamicReconnect(),
    // ChangeMediaType() or ChangeOutputFormat().
    if (!StreamingThreadUsingOutputPin())
    {

        // The output pin can be immediately blocked.
        BlockOutputPin();
    }

    return S_OK;
}

void CDynamicOutputPin::BlockOutputPin(void)
{
    // The caller should always hold the m_BlockStateLock because this function
    // uses m_BlockState and m_hNotifyCallerPinBlockedEvent.
    ASSERT(CritCheckIn(&m_BlockStateLock));

    // This function should not be called if the streaming thread is modifying
    // the connection state or it's passing data downstream.
    ASSERT(!StreamingThreadUsingOutputPin());

    // This should not fail because we successfully created the event
    // and we have the security permissions to change it's state.
    EXECUTE_ASSERT(::ResetEvent(m_hUnblockOutputPinEvent));

    // This event should not fail because AsynchronousBlockOutputPin() successfully
    // duplicated this handle and we have the appropriate security permissions.
    EXECUTE_ASSERT(::SetEvent(m_hNotifyCallerPinBlockedEvent));
    EXECUTE_ASSERT(::CloseHandle(m_hNotifyCallerPinBlockedEvent));

    m_BlockState = BLOCKED;
    m_hNotifyCallerPinBlockedEvent = NULL;
}

HRESULT CDynamicOutputPin::UnblockOutputPin(void)
{
    // UnblockOutputPin() holds the m_BlockStateLock because it
    // uses m_BlockState, m_dwBlockCallerThreadID and
    // m_hNotifyCallerPinBlockedEvent.
    CAutoLock alBlockStateLock(&m_BlockStateLock);

    if (NOT_BLOCKED == m_BlockState)
    {
        return S_FALSE;
    }

    // This should not fail because we successfully created the event
    // and we have the security permissions to change it's state.
    EXECUTE_ASSERT(::SetEvent(m_hUnblockOutputPinEvent));

    // Cancel the block operation if it's still pending.
    if (NULL != m_hNotifyCallerPinBlockedEvent)
    {
        // This event should not fail because AsynchronousBlockOutputPin() successfully
        // duplicated this handle and we have the appropriate security permissions.
        EXECUTE_ASSERT(::SetEvent(m_hNotifyCallerPinBlockedEvent));
        EXECUTE_ASSERT(::CloseHandle(m_hNotifyCallerPinBlockedEvent));
    }

    m_BlockState = NOT_BLOCKED;
    m_dwBlockCallerThreadID = 0;
    m_hNotifyCallerPinBlockedEvent = NULL;

    return S_OK;
}

HRESULT CDynamicOutputPin::StartUsingOutputPin(void)
{
    // The caller should not hold m_BlockStateLock.  If the caller does,
    // a deadlock could occur.
    ASSERT(CritCheckOut(&m_BlockStateLock));

    CAutoLock alBlockStateLock(&m_BlockStateLock);

#ifdef DEBUG
    AssertValid();
#endif // DEBUG

    // Are we in the middle of a block operation?
    while (BLOCKED == m_BlockState)
    {
        m_BlockStateLock.Unlock();

        // If this ASSERT fires, a deadlock could occur.  The caller should make sure
        // that this thread never acquires the Block State lock more than once.
        ASSERT(CritCheckOut(&m_BlockStateLock));

        // WaitForMultipleObjects() returns WAIT_OBJECT_0 if the unblock event
        // is fired.  It returns WAIT_OBJECT_0 + 1 if the stop event if fired.
        // See the Windows SDK documentation for more information on
        // WaitForMultipleObjects().
        const DWORD UNBLOCK = WAIT_OBJECT_0;
        const DWORD STOP = WAIT_OBJECT_0 + 1;

        HANDLE ahWaitEvents[] = {m_hUnblockOutputPinEvent, m_hStopEvent};
        DWORD dwNumWaitEvents = sizeof(ahWaitEvents) / sizeof(HANDLE);

        DWORD dwReturnValue = ::WaitForMultipleObjects(dwNumWaitEvents, ahWaitEvents, FALSE, INFINITE);

        m_BlockStateLock.Lock();

#ifdef DEBUG
        AssertValid();
#endif // DEBUG

        switch (dwReturnValue)
        {
        case UNBLOCK: break;

        case STOP: return VFW_E_STATE_CHANGED;

        case WAIT_FAILED: return AmGetLastErrorToHResult();

        default:
            DbgBreak("An Unexpected case occured in CDynamicOutputPin::StartUsingOutputPin().");
            return E_UNEXPECTED;
        }
    }

    m_dwNumOutstandingOutputPinUsers++;

#ifdef DEBUG
    AssertValid();
#endif // DEBUG

    return S_OK;
}

void CDynamicOutputPin::StopUsingOutputPin(void)
{
    CAutoLock alBlockStateLock(&m_BlockStateLock);

#ifdef DEBUG
    AssertValid();
#endif // DEBUG

    m_dwNumOutstandingOutputPinUsers--;

    if ((m_dwNumOutstandingOutputPinUsers == 0) && (NOT_BLOCKED != m_BlockState))
    {
        BlockOutputPin();
    }

#ifdef DEBUG
    AssertValid();
#endif // DEBUG
}

bool CDynamicOutputPin::StreamingThreadUsingOutputPin(void)
{
    CAutoLock alBlockStateLock(&m_BlockStateLock);

    return (m_dwNumOutstandingOutputPinUsers > 0);
}

void CDynamicOutputPin::SetConfigInfo(IGraphConfig *pGraphConfig, HANDLE hStopEvent)
{
    // This pointer is not addrefed because filters are not allowed to
    // hold references to the filter graph manager.  See the documentation for
    // IBaseFilter::JoinFilterGraph() in the Direct Show SDK for more information.
    m_pGraphConfig = pGraphConfig;

    m_hStopEvent = hStopEvent;
}

HRESULT CDynamicOutputPin::Active(void)
{
    // Make sure the user initialized the object by calling SetConfigInfo().
    if ((NULL == m_hStopEvent) || (NULL == m_pGraphConfig))
    {
        DbgBreak(ERROR
                 : CDynamicOutputPin::Active() failed because m_pGraphConfig and
                       m_hStopEvent were not initialized.Call SetConfigInfo() to initialize them.);
        return E_FAIL;
    }

    // If this ASSERT fires, the user may have passed an invalid event handle to SetConfigInfo().
    // The ASSERT can also fire if the event if destroyed and then Active() is called.  An event
    // handle is invalid if 1) the event does not exist or the user does not have the security
    // permissions to use the event.
    EXECUTE_ASSERT(ResetEvent(m_hStopEvent));

    return CBaseOutputPin::Active();
}

HRESULT CDynamicOutputPin::Inactive(void)
{
    // If this ASSERT fires, the user may have passed an invalid event handle to SetConfigInfo().
    // The ASSERT can also fire if the event if destroyed and then Active() is called.  An event
    // handle is invalid if 1) the event does not exist or the user does not have the security
    // permissions to use the event.
    EXECUTE_ASSERT(SetEvent(m_hStopEvent));

    return CBaseOutputPin::Inactive();
}

HRESULT CDynamicOutputPin::DeliverBeginFlush(void)
{
    // If this ASSERT fires, the user may have passed an invalid event handle to SetConfigInfo().
    // The ASSERT can also fire if the event if destroyed and then DeliverBeginFlush() is called.
    // An event handle is invalid if 1) the event does not exist or the user does not have the security
    // permissions to use the event.
    EXECUTE_ASSERT(SetEvent(m_hStopEvent));

    return CBaseOutputPin::DeliverBeginFlush();
}

HRESULT CDynamicOutputPin::DeliverEndFlush(void)
{
    // If this ASSERT fires, the user may have passed an invalid event handle to SetConfigInfo().
    // The ASSERT can also fire if the event if destroyed and then DeliverBeginFlush() is called.
    // An event handle is invalid if 1) the event does not exist or the user does not have the security
    // permissions to use the event.
    EXECUTE_ASSERT(ResetEvent(m_hStopEvent));

    return CBaseOutputPin::DeliverEndFlush();
}

// ChangeOutputFormat() either dynamicly changes the connection's format type or it dynamicly
// reconnects the output pin.
HRESULT CDynamicOutputPin::ChangeOutputFormat(const AM_MEDIA_TYPE *pmt, REFERENCE_TIME tSegmentStart,
                                              REFERENCE_TIME tSegmentStop, double dSegmentRate)
{
    // The caller should call StartUsingOutputPin() before calling this
    // method.
    ASSERT(StreamingThreadUsingOutputPin());

    // Callers should always pass a valid media type to ChangeOutputFormat() .
    ASSERT(NULL != pmt);

    CMediaType cmt(*pmt);
    HRESULT hr = ChangeMediaType(&cmt);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = DeliverNewSegment(tSegmentStart, tSegmentStop, dSegmentRate);
    if (FAILED(hr))
    {
        return hr;
    }

    return S_OK;
}

HRESULT CDynamicOutputPin::ChangeMediaType(const CMediaType *pmt)
{
    // The caller should call StartUsingOutputPin() before calling this
    // method.
    ASSERT(StreamingThreadUsingOutputPin());

    // This function assumes the filter graph is running.
    ASSERT(!IsStopped());

    if (!IsConnected())
    {
        return VFW_E_NOT_CONNECTED;
    }

    /*  First check if the downstream pin will accept a dynamic
        format change
    */
    QzCComPtr<IPinConnection> pConnection;

    m_Connected->QueryInterface(IID_IPinConnection, (void **)&pConnection);
    if (pConnection != NULL)
    {

        if (S_OK == pConnection->DynamicQueryAccept(pmt))
        {

            HRESULT hr = ChangeMediaTypeHelper(pmt);
            if (FAILED(hr))
            {
                return hr;
            }

            return S_OK;
        }
    }

    /*  Can't do the dynamic connection */
    return DynamicReconnect(pmt);
}

HRESULT CDynamicOutputPin::ChangeMediaTypeHelper(const CMediaType *pmt)
{
    // The caller should call StartUsingOutputPin() before calling this
    // method.
    ASSERT(StreamingThreadUsingOutputPin());

    HRESULT hr = m_Connected->ReceiveConnection(this, pmt);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = SetMediaType(pmt);
    if (FAILED(hr))
    {
        return hr;
    }

    // Does this pin use the local memory transport?
    if (NULL != m_pInputPin)
    {
        // This function assumes that m_pInputPin and m_Connected are
        // two different interfaces to the same object.
        ASSERT(::IsEqualObject(m_Connected, m_pInputPin));

        ALLOCATOR_PROPERTIES apInputPinRequirements;
        apInputPinRequirements.cbAlign = 0;
        apInputPinRequirements.cbBuffer = 0;
        apInputPinRequirements.cbPrefix = 0;
        apInputPinRequirements.cBuffers = 0;

        m_pInputPin->GetAllocatorRequirements(&apInputPinRequirements);

        // A zero allignment does not make any sense.
        if (0 == apInputPinRequirements.cbAlign)
        {
            apInputPinRequirements.cbAlign = 1;
        }

        hr = m_pAllocator->Decommit();
        if (FAILED(hr))
        {
            return hr;
        }

        hr = DecideBufferSize(m_pAllocator, &apInputPinRequirements);
        if (FAILED(hr))
        {
            return hr;
        }

        hr = m_pAllocator->Commit();
        if (FAILED(hr))
        {
            return hr;
        }

        hr = m_pInputPin->NotifyAllocator(m_pAllocator, m_bPinUsesReadOnlyAllocator);
        if (FAILED(hr))
        {
            return hr;
        }
    }

    return S_OK;
}

// this method has to be called from the thread that is pushing data,
// and it's the caller's responsibility to make sure that the thread
// has no outstand samples because they cannot be delivered after a
// reconnect
//
HRESULT CDynamicOutputPin::DynamicReconnect(const CMediaType *pmt)
{
    // The caller should call StartUsingOutputPin() before calling this
    // method.
    ASSERT(StreamingThreadUsingOutputPin());

    if ((m_pGraphConfig == NULL) || (NULL == m_hStopEvent))
    {
        return E_FAIL;
    }

    HRESULT hr =
        m_pGraphConfig->Reconnect(this, NULL, pmt, NULL, m_hStopEvent, AM_GRAPH_CONFIG_RECONNECT_CACHE_REMOVED_FILTERS);

    return hr;
}

HRESULT CDynamicOutputPin::CompleteConnect(IPin *pReceivePin)
{
    HRESULT hr = CBaseOutputPin::CompleteConnect(pReceivePin);
    if (SUCCEEDED(hr))
    {
        if (!IsStopped() && m_pAllocator)
        {
            hr = m_pAllocator->Commit();
            ASSERT(hr != VFW_E_ALREADY_COMMITTED);
        }
    }

    return hr;
}

#ifdef DEBUG
void CDynamicOutputPin::AssertValid(void)
{
    // Make sure the object was correctly initialized.

    // This ASSERT only fires if the object failed to initialize
    // and the user ignored the constructor's return code (phr).
    ASSERT(NULL != m_hUnblockOutputPinEvent);

    // If either of these ASSERTs fire, the user did not correctly call
    // SetConfigInfo().
    ASSERT(NULL != m_hStopEvent);
    ASSERT(NULL != m_pGraphConfig);

    // Make sure the block state is consistent.

    CAutoLock alBlockStateLock(&m_BlockStateLock);

    // BLOCK_STATE variables only have three legal values: PENDING, BLOCKED and NOT_BLOCKED.
    ASSERT((NOT_BLOCKED == m_BlockState) || (PENDING == m_BlockState) || (BLOCKED == m_BlockState));

    // m_hNotifyCallerPinBlockedEvent is only needed when a block operation cannot complete
    // immediately.
    ASSERT(((NULL == m_hNotifyCallerPinBlockedEvent) && (PENDING != m_BlockState)) ||
           ((NULL != m_hNotifyCallerPinBlockedEvent) && (PENDING == m_BlockState)));

    // m_dwBlockCallerThreadID should always be 0 if the pin is not blocked and
    // the user is not trying to block the pin.
    ASSERT((0 == m_dwBlockCallerThreadID) || (NOT_BLOCKED != m_BlockState));

    // If this ASSERT fires, the streaming thread is using the output pin and the
    // output pin is blocked.
    ASSERT(((0 != m_dwNumOutstandingOutputPinUsers) && (BLOCKED != m_BlockState)) ||
           ((0 == m_dwNumOutstandingOutputPinUsers) && (NOT_BLOCKED != m_BlockState)) ||
           ((0 == m_dwNumOutstandingOutputPinUsers) && (NOT_BLOCKED == m_BlockState)));
}
#endif // DEBUG

HRESULT CDynamicOutputPin::WaitEvent(HANDLE hEvent)
{
    const DWORD EVENT_SIGNALED = WAIT_OBJECT_0;

    DWORD dwReturnValue = ::WaitForSingleObject(hEvent, INFINITE);

    switch (dwReturnValue)
    {
    case EVENT_SIGNALED: return S_OK;

    case WAIT_FAILED: return AmGetLastErrorToHResult();

    default: DbgBreak("An Unexpected case occured in CDynamicOutputPin::WaitEvent()."); return E_UNEXPECTED;
    }
}

//=====================================================================
//=====================================================================
// Implements CBaseAllocator
//=====================================================================
//=====================================================================

/* Constructor overrides the default settings for the free list to request
   that it be alertable (ie the list can be cast to a handle which can be
   passed to WaitForSingleObject). Both of the allocator lists also ask for
   object locking, the all list matches the object default settings but I
   have included them here just so it is obvious what kind of list it is */

CBaseAllocator::CBaseAllocator(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN pUnk, __inout HRESULT *phr, BOOL bEvent,
                               BOOL fEnableReleaseCallback)
    : CUnknown(pName, pUnk)
    , m_lAllocated(0)
    , m_bChanged(FALSE)
    , m_bCommitted(FALSE)
    , m_bDecommitInProgress(FALSE)
    , m_lSize(0)
    , m_lCount(0)
    , m_lAlignment(0)
    , m_lPrefix(0)
    , m_hSem(NULL)
    , m_lWaiting(0)
    , m_fEnableReleaseCallback(fEnableReleaseCallback)
    , m_pNotify(NULL)
{
#ifdef DXMPERF
    PERFLOG_CTOR(pName ? pName : L"CBaseAllocator", (IMemAllocator *)this);
#endif // DXMPERF

    if (bEvent)
    {
        m_hSem = CreateSemaphore(NULL, 0, 0x7FFFFFFF, NULL);
        if (m_hSem == NULL)
        {
            *phr = E_OUTOFMEMORY;
            return;
        }
    }
}

#ifdef UNICODE
CBaseAllocator::CBaseAllocator(__in_opt LPCSTR pName, __inout_opt LPUNKNOWN pUnk, __inout HRESULT *phr, BOOL bEvent,
                               BOOL fEnableReleaseCallback)
    : CUnknown(pName, pUnk)
    , m_lAllocated(0)
    , m_bChanged(FALSE)
    , m_bCommitted(FALSE)
    , m_bDecommitInProgress(FALSE)
    , m_lSize(0)
    , m_lCount(0)
    , m_lAlignment(0)
    , m_lPrefix(0)
    , m_hSem(NULL)
    , m_lWaiting(0)
    , m_fEnableReleaseCallback(fEnableReleaseCallback)
    , m_pNotify(NULL)
{
#ifdef DXMPERF
    PERFLOG_CTOR(L"CBaseAllocator", (IMemAllocator *)this);
#endif // DXMPERF

    if (bEvent)
    {
        m_hSem = CreateSemaphore(NULL, 0, 0x7FFFFFFF, NULL);
        if (m_hSem == NULL)
        {
            *phr = E_OUTOFMEMORY;
            return;
        }
    }
}
#endif

/* Destructor */

CBaseAllocator::~CBaseAllocator()
{
    // we can't call Decommit here since that would mean a call to a
    // pure virtual in destructor.
    // We must assume that the derived class has gone into decommit state in
    // its destructor.
#ifdef DXMPERF
    PERFLOG_DTOR(L"CBaseAllocator", (IMemAllocator *)this);
#endif // DXMPERF

    ASSERT(!m_bCommitted);
    if (m_hSem != NULL)
    {
        EXECUTE_ASSERT(CloseHandle(m_hSem));
    }
    if (m_pNotify)
    {
        m_pNotify->Release();
    }
}

/* Override this to publicise our interfaces */

STDMETHODIMP
CBaseAllocator::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    /* Do we know about this interface */

    if (riid == IID_IMemAllocator || riid == IID_IMemAllocatorCallbackTemp && m_fEnableReleaseCallback)
    {
        return GetInterface((IMemAllocatorCallbackTemp *)this, ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

/* This sets the size and count of the required samples. The memory isn't
   actually allocated until Commit() is called, if memory has already been
   allocated then assuming no samples are outstanding the user may call us
   to change the buffering, the memory will be released in Commit() */

STDMETHODIMP
CBaseAllocator::SetProperties(__in ALLOCATOR_PROPERTIES *pRequest, __out ALLOCATOR_PROPERTIES *pActual)
{
    CheckPointer(pRequest, E_POINTER);
    CheckPointer(pActual, E_POINTER);
    ValidateReadWritePtr(pActual, sizeof(ALLOCATOR_PROPERTIES));
    CAutoLock cObjectLock(this);

    ZeroMemory(pActual, sizeof(ALLOCATOR_PROPERTIES));

    ASSERT(pRequest->cbBuffer > 0);

    /*  Check the alignment requested */
    if (pRequest->cbAlign != 1)
    {
        DbgLog((LOG_ERROR, 2, TEXT("Alignment requested was 0x%x, not 1"), pRequest->cbAlign));
        return VFW_E_BADALIGN;
    }

    /* Can't do this if already committed, there is an argument that says we
       should not reject the SetProperties call if there are buffers still
       active. However this is called by the source filter, which is the same
       person who is holding the samples. Therefore it is not unreasonable
       for them to free all their samples before changing the requirements */

    if (m_bCommitted)
    {
        return VFW_E_ALREADY_COMMITTED;
    }

    /* Must be no outstanding buffers */

    if (m_lAllocated != m_lFree.GetCount())
    {
        return VFW_E_BUFFERS_OUTSTANDING;
    }

    /* There isn't any real need to check the parameters as they
       will just be rejected when the user finally calls Commit */

    pActual->cbBuffer = m_lSize = pRequest->cbBuffer;
    pActual->cBuffers = m_lCount = pRequest->cBuffers;
    pActual->cbAlign = m_lAlignment = pRequest->cbAlign;
    pActual->cbPrefix = m_lPrefix = pRequest->cbPrefix;

    m_bChanged = TRUE;
    return NOERROR;
}

STDMETHODIMP
CBaseAllocator::GetProperties(__out ALLOCATOR_PROPERTIES *pActual)
{
    CheckPointer(pActual, E_POINTER);
    ValidateReadWritePtr(pActual, sizeof(ALLOCATOR_PROPERTIES));

    CAutoLock cObjectLock(this);
    pActual->cbBuffer = m_lSize;
    pActual->cBuffers = m_lCount;
    pActual->cbAlign = m_lAlignment;
    pActual->cbPrefix = m_lPrefix;
    return NOERROR;
}

// get container for a sample. Blocking, synchronous call to get the
// next free buffer (as represented by an IMediaSample interface).
// on return, the time etc properties will be invalid, but the buffer
// pointer and size will be correct.

HRESULT CBaseAllocator::GetBuffer(__deref_out IMediaSample **ppBuffer, __in_opt REFERENCE_TIME *pStartTime,
                                  __in_opt REFERENCE_TIME *pEndTime, DWORD dwFlags)
{
    UNREFERENCED_PARAMETER(pStartTime);
    UNREFERENCED_PARAMETER(pEndTime);
    UNREFERENCED_PARAMETER(dwFlags);
    CMediaSample *pSample;

    *ppBuffer = NULL;
    for (;;)
    {
        { // scope for lock
            CAutoLock cObjectLock(this);

            /* Check we are committed */
            if (!m_bCommitted)
            {
                return VFW_E_NOT_COMMITTED;
            }
            pSample = (CMediaSample *)m_lFree.RemoveHead();
            if (pSample == NULL)
            {
                SetWaiting();
            }
        }

        /* If we didn't get a sample then wait for the list to signal */

        if (pSample)
        {
            break;
        }
        if (dwFlags & AM_GBF_NOWAIT)
        {
            return VFW_E_TIMEOUT;
        }
        ASSERT(m_hSem != NULL);
        WaitForSingleObject(m_hSem, INFINITE);
    }

    /* Addref the buffer up to one. On release
       back to zero instead of being deleted, it will requeue itself by
       calling the ReleaseBuffer member function. NOTE the owner of a
       media sample must always be derived from CBaseAllocator */

    ASSERT(pSample->m_cRef == 0);
    pSample->m_cRef = 1;
    *ppBuffer = pSample;

#ifdef DXMPERF
    PERFLOG_GETBUFFER((IMemAllocator *)this, pSample);
#endif // DXMPERF

    return NOERROR;
}

/* Final release of a CMediaSample will call this */

STDMETHODIMP
CBaseAllocator::ReleaseBuffer(IMediaSample *pSample)
{
    CheckPointer(pSample, E_POINTER);
    ValidateReadPtr(pSample, sizeof(IMediaSample));

#ifdef DXMPERF
    PERFLOG_RELBUFFER((IMemAllocator *)this, pSample);
#endif // DXMPERF

    BOOL bRelease = FALSE;
    {
        CAutoLock cal(this);

        /* Put back on the free list */

        m_lFree.Add((CMediaSample *)pSample);
        if (m_lWaiting != 0)
        {
            NotifySample();
        }

        // if there is a pending Decommit, then we need to complete it by
        // calling Free() when the last buffer is placed on the free list

        LONG l1 = m_lFree.GetCount();
        if (m_bDecommitInProgress && (l1 == m_lAllocated))
        {
            Free();
            m_bDecommitInProgress = FALSE;
            bRelease = TRUE;
        }
    }

    if (m_pNotify)
    {

        ASSERT(m_fEnableReleaseCallback);

        //
        // Note that this is not synchronized with setting up a notification
        // method.
        //
        m_pNotify->NotifyRelease();
    }

    /* For each buffer there is one AddRef, made in GetBuffer and released
       here. This may cause the allocator and all samples to be deleted */

    if (bRelease)
    {
        Release();
    }
    return NOERROR;
}

STDMETHODIMP
CBaseAllocator::SetNotify(IMemAllocatorNotifyCallbackTemp *pNotify)
{
    ASSERT(m_fEnableReleaseCallback);
    CAutoLock lck(this);
    if (pNotify)
    {
        pNotify->AddRef();
    }
    if (m_pNotify)
    {
        m_pNotify->Release();
    }
    m_pNotify = pNotify;
    return S_OK;
}

STDMETHODIMP
CBaseAllocator::GetFreeCount(__out LONG *plBuffersFree)
{
    ASSERT(m_fEnableReleaseCallback);
    CAutoLock cObjectLock(this);
    *plBuffersFree = m_lCount - m_lAllocated + m_lFree.GetCount();
    return NOERROR;
}

void CBaseAllocator::NotifySample()
{
    if (m_lWaiting != 0)
    {
        ASSERT(m_hSem != NULL);
        ReleaseSemaphore(m_hSem, m_lWaiting, 0);
        m_lWaiting = 0;
    }
}

STDMETHODIMP
CBaseAllocator::Commit()
{
    /* Check we are not decommitted */
    CAutoLock cObjectLock(this);

    // cannot need to alloc or re-alloc if we are committed
    if (m_bCommitted)
    {
        return NOERROR;
    }

    // is there a pending decommit ? if so, just cancel it
    if (m_bDecommitInProgress)
    {
        m_bDecommitInProgress = FALSE;
        m_bCommitted = TRUE;

        // don't call Alloc at this point. He cannot allow SetProperties
        // between Decommit and the last free, so the buffer size cannot have
        // changed. And because some of the buffers are not free yet, he
        // cannot re-alloc anyway.
        return NOERROR;
    }

    DbgLog((LOG_MEMORY, 1, TEXT("Allocating: %ldx%ld"), m_lCount, m_lSize));

    // actually need to allocate the samples
    HRESULT hr = Alloc();
    if (FAILED(hr))
    {
        m_bCommitted = FALSE;
        return hr;
    }

    /* Allow GetBuffer calls */
    m_bCommitted = TRUE;

    AddRef();
    return NOERROR;
}

STDMETHODIMP
CBaseAllocator::Decommit()
{
    BOOL bRelease = FALSE;
    {
        /* Check we are not already decommitted */
        CAutoLock cObjectLock(this);
        if (m_bCommitted == FALSE)
        {
            if (m_bDecommitInProgress == FALSE)
            {
                return NOERROR;
            }
        }

        /* No more GetBuffer calls will succeed */
        m_bCommitted = FALSE;

        // are any buffers outstanding?
        if (m_lFree.GetCount() < m_lAllocated)
        {
            // please complete the decommit when last buffer is freed
            m_bDecommitInProgress = TRUE;
        }
        else
        {
            m_bDecommitInProgress = FALSE;

            // need to complete the decommit here as there are no
            // outstanding buffers

            Free();
            bRelease = TRUE;
        }

        // Tell anyone waiting that they can go now so we can
        // reject their call
#pragma warning(push)
#ifndef _PREFAST_
#pragma warning(disable : 4068)
#endif
#pragma prefast(                \
    suppress                    \
    : __WARNING_DEREF_NULL_PTR, \
      "Suppress warning related to Free() invalidating 'this' which is no applicable to CBaseAllocator::Free()")
        NotifySample();

#pragma warning(pop)
    }

    if (bRelease)
    {
        Release();
    }
    return NOERROR;
}

/* Base definition of allocation which checks we are ok to go ahead and do
   the full allocation. We return S_FALSE if the requirements are the same */

HRESULT
CBaseAllocator::Alloc(void)
{
    /* Error if he hasn't set the size yet */
    if (m_lCount <= 0 || m_lSize <= 0 || m_lAlignment <= 0)
    {
        return VFW_E_SIZENOTSET;
    }

    /* should never get here while buffers outstanding */
    ASSERT(m_lFree.GetCount() == m_lAllocated);

    /* If the requirements haven't changed then don't reallocate */
    if (m_bChanged == FALSE)
    {
        return S_FALSE;
    }

    return NOERROR;
}

/*  Implement CBaseAllocator::CSampleList::Remove(pSample)
    Removes pSample from the list
*/
void CBaseAllocator::CSampleList::Remove(__inout CMediaSample *pSample)
{
    CMediaSample **pSearch;
    for (pSearch = &m_List; *pSearch != NULL; pSearch = &(CBaseAllocator::NextSample(*pSearch)))
    {
        if (*pSearch == pSample)
        {
            *pSearch = CBaseAllocator::NextSample(pSample);
            CBaseAllocator::NextSample(pSample) = NULL;
            m_nOnList--;
            return;
        }
    }
    DbgBreak("Couldn't find sample in list");
}

//=====================================================================
//=====================================================================
// Implements CMemAllocator
//=====================================================================
//=====================================================================

/* This goes in the factory template table to create new instances */
CUnknown *CMemAllocator::CreateInstance(__inout_opt LPUNKNOWN pUnk, __inout HRESULT *phr)
{
    CUnknown *pUnkRet = new CMemAllocator(NAME("CMemAllocator"), pUnk, phr);
    return pUnkRet;
}

CMemAllocator::CMemAllocator(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN pUnk, __inout HRESULT *phr)
    : CBaseAllocator(pName, pUnk, phr, TRUE, TRUE)
    , m_pBuffer(NULL)
{
}

#ifdef UNICODE
CMemAllocator::CMemAllocator(__in_opt LPCSTR pName, __inout_opt LPUNKNOWN pUnk, __inout HRESULT *phr)
    : CBaseAllocator(pName, pUnk, phr, TRUE, TRUE)
    , m_pBuffer(NULL)
{
}
#endif

/* This sets the size and count of the required samples. The memory isn't
   actually allocated until Commit() is called, if memory has already been
   allocated then assuming no samples are outstanding the user may call us
   to change the buffering, the memory will be released in Commit() */
STDMETHODIMP
CMemAllocator::SetProperties(__in ALLOCATOR_PROPERTIES *pRequest, __out ALLOCATOR_PROPERTIES *pActual)
{
    CheckPointer(pActual, E_POINTER);
    ValidateReadWritePtr(pActual, sizeof(ALLOCATOR_PROPERTIES));
    CAutoLock cObjectLock(this);

    ZeroMemory(pActual, sizeof(ALLOCATOR_PROPERTIES));

    ASSERT(pRequest->cbBuffer > 0);

    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);

    /*  Check the alignment request is a power of 2 */
    if ((-pRequest->cbAlign & pRequest->cbAlign) != pRequest->cbAlign)
    {
        DbgLog((LOG_ERROR, 1, TEXT("Alignment requested 0x%x not a power of 2!"), pRequest->cbAlign));
    }
    /*  Check the alignment requested */
    if (pRequest->cbAlign == 0 || (SysInfo.dwAllocationGranularity & (pRequest->cbAlign - 1)) != 0)
    {
        DbgLog((LOG_ERROR, 1, TEXT("Invalid alignment 0x%x requested - granularity = 0x%x"), pRequest->cbAlign,
                SysInfo.dwAllocationGranularity));
        return VFW_E_BADALIGN;
    }

    /* Can't do this if already committed, there is an argument that says we
       should not reject the SetProperties call if there are buffers still
       active. However this is called by the source filter, which is the same
       person who is holding the samples. Therefore it is not unreasonable
       for them to free all their samples before changing the requirements */

    if (m_bCommitted == TRUE)
    {
        return VFW_E_ALREADY_COMMITTED;
    }

    /* Must be no outstanding buffers */

    if (m_lFree.GetCount() < m_lAllocated)
    {
        return VFW_E_BUFFERS_OUTSTANDING;
    }

    /* There isn't any real need to check the parameters as they
       will just be rejected when the user finally calls Commit */

    // round length up to alignment - remember that prefix is included in
    // the alignment
    LONG lSize = pRequest->cbBuffer + pRequest->cbPrefix;
    LONG lRemainder = lSize % pRequest->cbAlign;
    if (lRemainder != 0)
    {
        lSize = lSize - lRemainder + pRequest->cbAlign;
    }
    pActual->cbBuffer = m_lSize = (lSize - pRequest->cbPrefix);

    pActual->cBuffers = m_lCount = pRequest->cBuffers;
    pActual->cbAlign = m_lAlignment = pRequest->cbAlign;
    pActual->cbPrefix = m_lPrefix = pRequest->cbPrefix;

    m_bChanged = TRUE;
    return NOERROR;
}

// override this to allocate our resources when Commit is called.
//
// note that our resources may be already allocated when this is called,
// since we don't free them on Decommit. We will only be called when in
// decommit state with all buffers free.
//
// object locked by caller
HRESULT
CMemAllocator::Alloc(void)
{
    CAutoLock lck(this);

    /* Check he has called SetProperties */
    HRESULT hr = CBaseAllocator::Alloc();
    if (FAILED(hr))
    {
        return hr;
    }

    /* If the requirements haven't changed then don't reallocate */
    if (hr == S_FALSE)
    {
        ASSERT(m_pBuffer);
        return NOERROR;
    }
    ASSERT(hr == S_OK); // we use this fact in the loop below

    /* Free the old resources */
    if (m_pBuffer)
    {
        ReallyFree();
    }

    /* Make sure we've got reasonable values */
    if (m_lSize < 0 || m_lPrefix < 0 || m_lCount < 0)
    {
        return E_OUTOFMEMORY;
    }

    /* Compute the aligned size */
    LONG lAlignedSize = m_lSize + m_lPrefix;

    /*  Check overflow */
    if (lAlignedSize < m_lSize)
    {
        return E_OUTOFMEMORY;
    }

    if (m_lAlignment > 1)
    {
        LONG lRemainder = lAlignedSize % m_lAlignment;
        if (lRemainder != 0)
        {
            LONG lNewSize = lAlignedSize + m_lAlignment - lRemainder;
            if (lNewSize < lAlignedSize)
            {
                return E_OUTOFMEMORY;
            }
            lAlignedSize = lNewSize;
        }
    }

    /* Create the contiguous memory block for the samples
       making sure it's properly aligned (64K should be enough!)
    */
    ASSERT(lAlignedSize % m_lAlignment == 0);

    LONGLONG lToAllocate = m_lCount * (LONGLONG)lAlignedSize;

    /*  Check overflow */
    if (lToAllocate > MAXLONG)
    {
        return E_OUTOFMEMORY;
    }

    m_pBuffer = (PBYTE)VirtualAlloc(NULL, (LONG)lToAllocate, MEM_COMMIT, PAGE_READWRITE);

    if (m_pBuffer == NULL)
    {
        return E_OUTOFMEMORY;
    }

    LPBYTE pNext = m_pBuffer;
    CMediaSample *pSample;

    ASSERT(m_lAllocated == 0);

    // Create the new samples - we have allocated m_lSize bytes for each sample
    // plus m_lPrefix bytes per sample as a prefix. We set the pointer to
    // the memory after the prefix - so that GetPointer() will return a pointer
    // to m_lSize bytes.
    for (; m_lAllocated < m_lCount; m_lAllocated++, pNext += lAlignedSize)
    {

        pSample = new CMediaSample(NAME("Default memory media sample"), this, &hr,
                                   pNext + m_lPrefix, // GetPointer() value
                                   m_lSize);          // not including prefix

        ASSERT(SUCCEEDED(hr));
        if (pSample == NULL)
        {
            return E_OUTOFMEMORY;
        }

        // This CANNOT fail
        m_lFree.Add(pSample);
    }

    m_bChanged = FALSE;
    return NOERROR;
}

// override this to free up any resources we have allocated.
// called from the base class on Decommit when all buffers have been
// returned to the free list.
//
// caller has already locked the object.

// in our case, we keep the memory until we are deleted, so
// we do nothing here. The memory is deleted in the destructor by
// calling ReallyFree()
void CMemAllocator::Free(void)
{
    return;
}

// called from the destructor (and from Alloc if changing size/count) to
// actually free up the memory
void CMemAllocator::ReallyFree(void)
{
    /* Should never be deleting this unless all buffers are freed */

    ASSERT(m_lAllocated == m_lFree.GetCount());

    /* Free up all the CMediaSamples */

    CMediaSample *pSample;
    for (;;)
    {
        pSample = m_lFree.RemoveHead();
        if (pSample != NULL)
        {
            delete pSample;
        }
        else
        {
            break;
        }
    }

    m_lAllocated = 0;

    // free the block of buffer memory
    if (m_pBuffer)
    {
        EXECUTE_ASSERT(VirtualFree(m_pBuffer, 0, MEM_RELEASE));
        m_pBuffer = NULL;
    }
}

/* Destructor frees our memory resources */

CMemAllocator::~CMemAllocator()
{
    Decommit();
    ReallyFree();
}

// ------------------------------------------------------------------------
// filter registration through IFilterMapper. used if IFilterMapper is
// not found (Quartz 1.0 install)

STDAPI
AMovieSetupRegisterFilter(const AMOVIESETUP_FILTER *const psetupdata, IFilterMapper *pIFM, BOOL bRegister)
{
    DbgLog((LOG_TRACE, 3, TEXT("= AMovieSetupRegisterFilter")));

    // check we've got data
    //
    if (NULL == psetupdata)
        return S_FALSE;

    // unregister filter
    // (as pins are subkeys of filter's CLSID key
    // they do not need to be removed separately).
    //
    DbgLog((LOG_TRACE, 3, TEXT("= = unregister filter")));
    HRESULT hr = pIFM->UnregisterFilter(*(psetupdata->clsID));

    if (bRegister)
    {
        // register filter
        //
        DbgLog((LOG_TRACE, 3, TEXT("= = register filter")));
        hr = pIFM->RegisterFilter(*(psetupdata->clsID), psetupdata->strName, psetupdata->dwMerit);
        if (SUCCEEDED(hr))
        {
            // all its pins
            //
            DbgLog((LOG_TRACE, 3, TEXT("= = register filter pins")));
            for (UINT m1 = 0; m1 < psetupdata->nPins; m1++)
            {
                hr = pIFM->RegisterPin(
                    *(psetupdata->clsID), psetupdata->lpPin[m1].strName, psetupdata->lpPin[m1].bRendered,
                    psetupdata->lpPin[m1].bOutput, psetupdata->lpPin[m1].bZero, psetupdata->lpPin[m1].bMany,
                    *(psetupdata->lpPin[m1].clsConnectsToFilter), psetupdata->lpPin[m1].strConnectsToPin);

                if (SUCCEEDED(hr))
                {
                    // and each pin's media types
                    //
                    DbgLog((LOG_TRACE, 3, TEXT("= = register filter pin types")));
                    for (UINT m2 = 0; m2 < psetupdata->lpPin[m1].nMediaTypes; m2++)
                    {
                        hr = pIFM->RegisterPinType(*(psetupdata->clsID), psetupdata->lpPin[m1].strName,
                                                   *(psetupdata->lpPin[m1].lpMediaType[m2].clsMajorType),
                                                   *(psetupdata->lpPin[m1].lpMediaType[m2].clsMinorType));
                        if (FAILED(hr))
                            break;
                    }
                    if (FAILED(hr))
                        break;
                }
                if (FAILED(hr))
                    break;
            }
        }
    }

    // handle one acceptable "error" - that
    // of filter not being registered!
    // (couldn't find a suitable #define'd
    // name for the error!)
    //
    if (0x80070002 == hr)
        return NOERROR;
    else
        return hr;
}

//  Remove warnings about unreferenced inline functions
#pragma warning(disable : 4514)
