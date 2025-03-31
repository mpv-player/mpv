//------------------------------------------------------------------------------
// File: CtlUtil.cpp
//
// Desc: DirectShow base classes.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

// Base classes implementing IDispatch parsing for the basic control dual
// interfaces. Derive from these and implement just the custom method and
// property methods. We also implement CPosPassThru that can be used by
// renderers and transforms to pass by IMediaPosition and IMediaSeeking

#include <streams.h>
#include <limits.h>
#include "seekpt.h"

// 'bool' non standard reserved word
#pragma warning(disable : 4237)

// --- CBaseDispatch implementation ----------
CBaseDispatch::~CBaseDispatch()
{
    if (m_pti)
    {
        m_pti->Release();
    }
}

// return 1 if we support GetTypeInfo

STDMETHODIMP
CBaseDispatch::GetTypeInfoCount(__out UINT *pctinfo)
{
    CheckPointer(pctinfo, E_POINTER);
    ValidateReadWritePtr(pctinfo, sizeof(UINT *));
    *pctinfo = 1;
    return S_OK;
}

typedef HRESULT(STDAPICALLTYPE *LPLOADTYPELIB)(const OLECHAR FAR *szFile, __deref_out ITypeLib FAR *FAR *pptlib);

typedef HRESULT(STDAPICALLTYPE *LPLOADREGTYPELIB)(REFGUID rguid, WORD wVerMajor, WORD wVerMinor, LCID lcid,
                                                  __deref_out ITypeLib FAR *FAR *pptlib);

// attempt to find our type library

STDMETHODIMP
CBaseDispatch::GetTypeInfo(REFIID riid, UINT itinfo, LCID lcid, __deref_out ITypeInfo **pptinfo)
{
    CheckPointer(pptinfo, E_POINTER);
    ValidateReadWritePtr(pptinfo, sizeof(ITypeInfo *));
    HRESULT hr;

    *pptinfo = NULL;

    // we only support one type element
    if (0 != itinfo)
    {
        return TYPE_E_ELEMENTNOTFOUND;
    }

    if (NULL == pptinfo)
    {
        return E_POINTER;
    }

    // always look for neutral
    if (NULL == m_pti)
    {

        LPLOADTYPELIB lpfnLoadTypeLib;
        LPLOADREGTYPELIB lpfnLoadRegTypeLib;
        ITypeLib *ptlib;
        HINSTANCE hInst;

        static const char szTypeLib[] = "LoadTypeLib";
        static const char szRegTypeLib[] = "LoadRegTypeLib";
        static const WCHAR szControl[] = L"control.tlb";

        //
        // Try to get the Ole32Aut.dll module handle.
        //

        hInst = LoadOLEAut32();
        if (hInst == NULL)
        {
            DWORD dwError = GetLastError();
            return AmHresultFromWin32(dwError);
        }
        lpfnLoadRegTypeLib = (LPLOADREGTYPELIB)GetProcAddress(hInst, szRegTypeLib);
        if (lpfnLoadRegTypeLib == NULL)
        {
            DWORD dwError = GetLastError();
            return AmHresultFromWin32(dwError);
        }

        hr = (*lpfnLoadRegTypeLib)(LIBID_QuartzTypeLib, 1, 0, // version 1.0
                                   lcid, &ptlib);

        if (FAILED(hr))
        {

            // attempt to load directly - this will fill the
            // registry in if it finds it

            lpfnLoadTypeLib = (LPLOADTYPELIB)GetProcAddress(hInst, szTypeLib);
            if (lpfnLoadTypeLib == NULL)
            {
                DWORD dwError = GetLastError();
                return AmHresultFromWin32(dwError);
            }

            hr = (*lpfnLoadTypeLib)(szControl, &ptlib);
            if (FAILED(hr))
            {
                return hr;
            }
        }

        hr = ptlib->GetTypeInfoOfGuid(riid, &m_pti);

        ptlib->Release();

        if (FAILED(hr))
        {
            return hr;
        }
    }

    *pptinfo = m_pti;
    m_pti->AddRef();
    return S_OK;
}

STDMETHODIMP
CBaseDispatch::GetIDsOfNames(REFIID riid, __in_ecount(cNames) LPOLESTR *rgszNames, UINT cNames, LCID lcid,
                             __out_ecount(cNames) DISPID *rgdispid)
{
    // although the IDispatch riid is dead, we use this to pass from
    // the interface implementation class to us the iid we are talking about.

    ITypeInfo *pti;
    HRESULT hr = GetTypeInfo(riid, 0, lcid, &pti);

    if (SUCCEEDED(hr))
    {
        hr = pti->GetIDsOfNames(rgszNames, cNames, rgdispid);

        pti->Release();
    }
    return hr;
}

// --- CMediaControl implementation ---------

CMediaControl::CMediaControl(const TCHAR *name, LPUNKNOWN pUnk)
    : CUnknown(name, pUnk)
{
}

// expose our interfaces IMediaControl and IUnknown

STDMETHODIMP
CMediaControl::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    ValidateReadWritePtr(ppv, sizeof(PVOID));
    if (riid == IID_IMediaControl)
    {
        return GetInterface((IMediaControl *)this, ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

// return 1 if we support GetTypeInfo

STDMETHODIMP
CMediaControl::GetTypeInfoCount(__out UINT *pctinfo)
{
    return m_basedisp.GetTypeInfoCount(pctinfo);
}

// attempt to find our type library

STDMETHODIMP
CMediaControl::GetTypeInfo(UINT itinfo, LCID lcid, __deref_out ITypeInfo **pptinfo)
{
    return m_basedisp.GetTypeInfo(IID_IMediaControl, itinfo, lcid, pptinfo);
}

STDMETHODIMP
CMediaControl::GetIDsOfNames(REFIID riid, __in_ecount(cNames) LPOLESTR *rgszNames, UINT cNames, LCID lcid,
                             __out_ecount(cNames) DISPID *rgdispid)
{
    return m_basedisp.GetIDsOfNames(IID_IMediaControl, rgszNames, cNames, lcid, rgdispid);
}

STDMETHODIMP
CMediaControl::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, __in DISPPARAMS *pdispparams,
                      __out_opt VARIANT *pvarResult, __out_opt EXCEPINFO *pexcepinfo, __out_opt UINT *puArgErr)
{
    // this parameter is a dead leftover from an earlier interface
    if (IID_NULL != riid)
    {
        return DISP_E_UNKNOWNINTERFACE;
    }

    ITypeInfo *pti;
    HRESULT hr = GetTypeInfo(0, lcid, &pti);

    if (FAILED(hr))
    {
        return hr;
    }

    hr = pti->Invoke((IMediaControl *)this, dispidMember, wFlags, pdispparams, pvarResult, pexcepinfo, puArgErr);

    pti->Release();
    return hr;
}

// --- CMediaEvent implementation ----------

CMediaEvent::CMediaEvent(__in_opt LPCTSTR name, __in_opt LPUNKNOWN pUnk)
    : CUnknown(name, pUnk)
{
}

// expose our interfaces IMediaEvent and IUnknown

STDMETHODIMP
CMediaEvent::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    ValidateReadWritePtr(ppv, sizeof(PVOID));
    if (riid == IID_IMediaEvent || riid == IID_IMediaEventEx)
    {
        return GetInterface((IMediaEventEx *)this, ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

// return 1 if we support GetTypeInfo

STDMETHODIMP
CMediaEvent::GetTypeInfoCount(__out UINT *pctinfo)
{
    return m_basedisp.GetTypeInfoCount(pctinfo);
}

// attempt to find our type library

STDMETHODIMP
CMediaEvent::GetTypeInfo(UINT itinfo, LCID lcid, __deref_out ITypeInfo **pptinfo)
{
    return m_basedisp.GetTypeInfo(IID_IMediaEvent, itinfo, lcid, pptinfo);
}

STDMETHODIMP
CMediaEvent::GetIDsOfNames(REFIID riid, __in_ecount(cNames) LPOLESTR *rgszNames, UINT cNames, LCID lcid,
                           __out_ecount(cNames) DISPID *rgdispid)
{
    return m_basedisp.GetIDsOfNames(IID_IMediaEvent, rgszNames, cNames, lcid, rgdispid);
}

STDMETHODIMP
CMediaEvent::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, __in DISPPARAMS *pdispparams,
                    __out_opt VARIANT *pvarResult, __out_opt EXCEPINFO *pexcepinfo, __out_opt UINT *puArgErr)
{
    // this parameter is a dead leftover from an earlier interface
    if (IID_NULL != riid)
    {
        return DISP_E_UNKNOWNINTERFACE;
    }

    ITypeInfo *pti;
    HRESULT hr = GetTypeInfo(0, lcid, &pti);

    if (FAILED(hr))
    {
        return hr;
    }

    hr = pti->Invoke((IMediaEvent *)this, dispidMember, wFlags, pdispparams, pvarResult, pexcepinfo, puArgErr);

    pti->Release();
    return hr;
}

// --- CMediaPosition implementation ----------

CMediaPosition::CMediaPosition(__in_opt LPCTSTR name, __in_opt LPUNKNOWN pUnk)
    : CUnknown(name, pUnk)
{
}

CMediaPosition::CMediaPosition(__in_opt LPCTSTR name, __in_opt LPUNKNOWN pUnk, __inout HRESULT *phr)
    : CUnknown(name, pUnk)
{
    UNREFERENCED_PARAMETER(phr);
}

// expose our interfaces IMediaPosition and IUnknown

STDMETHODIMP
CMediaPosition::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    ValidateReadWritePtr(ppv, sizeof(PVOID));
    if (riid == IID_IMediaPosition)
    {
        return GetInterface((IMediaPosition *)this, ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

// return 1 if we support GetTypeInfo

STDMETHODIMP
CMediaPosition::GetTypeInfoCount(__out UINT *pctinfo)
{
    return m_basedisp.GetTypeInfoCount(pctinfo);
}

// attempt to find our type library

STDMETHODIMP
CMediaPosition::GetTypeInfo(UINT itinfo, LCID lcid, __deref_out ITypeInfo **pptinfo)
{
    return m_basedisp.GetTypeInfo(IID_IMediaPosition, itinfo, lcid, pptinfo);
}

STDMETHODIMP
CMediaPosition::GetIDsOfNames(REFIID riid, __in_ecount(cNames) LPOLESTR *rgszNames, UINT cNames, LCID lcid,
                              __out_ecount(cNames) DISPID *rgdispid)
{
    return m_basedisp.GetIDsOfNames(IID_IMediaPosition, rgszNames, cNames, lcid, rgdispid);
}

STDMETHODIMP
CMediaPosition::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, __in DISPPARAMS *pdispparams,
                       __out_opt VARIANT *pvarResult, __out_opt EXCEPINFO *pexcepinfo, __out_opt UINT *puArgErr)
{
    // this parameter is a dead leftover from an earlier interface
    if (IID_NULL != riid)
    {
        return DISP_E_UNKNOWNINTERFACE;
    }

    ITypeInfo *pti;
    HRESULT hr = GetTypeInfo(0, lcid, &pti);

    if (FAILED(hr))
    {
        return hr;
    }

    hr = pti->Invoke((IMediaPosition *)this, dispidMember, wFlags, pdispparams, pvarResult, pexcepinfo, puArgErr);

    pti->Release();
    return hr;
}

// --- IMediaPosition and IMediaSeeking pass through class ----------

CPosPassThru::CPosPassThru(__in_opt LPCTSTR pName, __in_opt LPUNKNOWN pUnk, __inout HRESULT *phr, IPin *pPin)
    : CMediaPosition(pName, pUnk)
    , m_pPin(pPin)
{
    if (pPin == NULL)
    {
        *phr = E_POINTER;
        return;
    }
}

// Expose our IMediaSeeking and IMediaPosition interfaces

STDMETHODIMP
CPosPassThru::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    CheckPointer(ppv, E_POINTER);
    *ppv = NULL;

    if (riid == IID_IMediaSeeking)
    {
        return GetInterface(static_cast<IMediaSeeking *>(this), ppv);
    }
    return CMediaPosition::NonDelegatingQueryInterface(riid, ppv);
}

// Return the IMediaPosition interface from our peer

HRESULT
CPosPassThru::GetPeer(IMediaPosition **ppMP)
{
    *ppMP = NULL;

    IPin *pConnected;
    HRESULT hr = m_pPin->ConnectedTo(&pConnected);
    if (FAILED(hr))
    {
        return E_NOTIMPL;
    }
    IMediaPosition *pMP;
    hr = pConnected->QueryInterface(IID_IMediaPosition, (void **)&pMP);
    pConnected->Release();
    if (FAILED(hr))
    {
        return E_NOTIMPL;
    }

    *ppMP = pMP;
    return S_OK;
}

// Return the IMediaSeeking interface from our peer

HRESULT
CPosPassThru::GetPeerSeeking(__deref_out IMediaSeeking **ppMS)
{
    *ppMS = NULL;

    IPin *pConnected;
    HRESULT hr = m_pPin->ConnectedTo(&pConnected);
    if (FAILED(hr))
    {
        return E_NOTIMPL;
    }
    IMediaSeeking *pMS;
    hr = pConnected->QueryInterface(IID_IMediaSeeking, (void **)&pMS);
    pConnected->Release();
    if (FAILED(hr))
    {
        return E_NOTIMPL;
    }

    *ppMS = pMS;
    return S_OK;
}

// --- IMediaSeeking methods ----------

STDMETHODIMP
CPosPassThru::GetCapabilities(__out DWORD *pCaps)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = pMS->GetCapabilities(pCaps);
    pMS->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::CheckCapabilities(__inout DWORD *pCaps)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = pMS->CheckCapabilities(pCaps);
    pMS->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::IsFormatSupported(const GUID *pFormat)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = pMS->IsFormatSupported(pFormat);
    pMS->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::QueryPreferredFormat(__out GUID *pFormat)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = pMS->QueryPreferredFormat(pFormat);
    pMS->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::SetTimeFormat(const GUID *pFormat)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = pMS->SetTimeFormat(pFormat);
    pMS->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::GetTimeFormat(__out GUID *pFormat)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = pMS->GetTimeFormat(pFormat);
    pMS->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::IsUsingTimeFormat(const GUID *pFormat)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = pMS->IsUsingTimeFormat(pFormat);
    pMS->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::ConvertTimeFormat(__out LONGLONG *pTarget, __in_opt const GUID *pTargetFormat, LONGLONG Source,
                                __in_opt const GUID *pSourceFormat)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = pMS->ConvertTimeFormat(pTarget, pTargetFormat, Source, pSourceFormat);
    pMS->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::SetPositions(__inout_opt LONGLONG *pCurrent, DWORD CurrentFlags, __inout_opt LONGLONG *pStop,
                           DWORD StopFlags)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = pMS->SetPositions(pCurrent, CurrentFlags, pStop, StopFlags);
    pMS->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::GetPositions(__out_opt LONGLONG *pCurrent, __out_opt LONGLONG *pStop)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = pMS->GetPositions(pCurrent, pStop);
    pMS->Release();
    return hr;
}

HRESULT
CPosPassThru::GetSeekingLongLong(HRESULT (__stdcall IMediaSeeking::*pMethod)(__out LONGLONG *), LONGLONG *pll)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (SUCCEEDED(hr))
    {
        hr = (pMS->*pMethod)(pll);
        pMS->Release();
    }
    return hr;
}

// If we don't have a current position then ask upstream

STDMETHODIMP
CPosPassThru::GetCurrentPosition(__out LONGLONG *pCurrent)
{
    // Can we report the current position
    HRESULT hr = GetMediaTime(pCurrent, NULL);
    if (SUCCEEDED(hr))
        hr = NOERROR;
    else
        hr = GetSeekingLongLong(&IMediaSeeking::GetCurrentPosition, pCurrent);
    return hr;
}

STDMETHODIMP
CPosPassThru::GetStopPosition(__out LONGLONG *pStop)
{
    return GetSeekingLongLong(&IMediaSeeking::GetStopPosition, pStop);
    ;
}

STDMETHODIMP
CPosPassThru::GetDuration(__out LONGLONG *pDuration)
{
    return GetSeekingLongLong(&IMediaSeeking::GetDuration, pDuration);
    ;
}

STDMETHODIMP
CPosPassThru::GetPreroll(__out LONGLONG *pllPreroll)
{
    return GetSeekingLongLong(&IMediaSeeking::GetPreroll, pllPreroll);
    ;
}

STDMETHODIMP
CPosPassThru::GetAvailable(__out_opt LONGLONG *pEarliest, __out_opt LONGLONG *pLatest)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = pMS->GetAvailable(pEarliest, pLatest);
    pMS->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::GetRate(__out double *pdRate)
{
    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pMS->GetRate(pdRate);
    pMS->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::SetRate(double dRate)
{
    if (0.0 == dRate)
    {
        return E_INVALIDARG;
    }

    IMediaSeeking *pMS;
    HRESULT hr = GetPeerSeeking(&pMS);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pMS->SetRate(dRate);
    pMS->Release();
    return hr;
}

// --- IMediaPosition methods ----------

STDMETHODIMP
CPosPassThru::get_Duration(__out REFTIME *plength)
{
    IMediaPosition *pMP;
    HRESULT hr = GetPeer(&pMP);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = pMP->get_Duration(plength);
    pMP->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::get_CurrentPosition(__out REFTIME *pllTime)
{
    IMediaPosition *pMP;
    HRESULT hr = GetPeer(&pMP);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pMP->get_CurrentPosition(pllTime);
    pMP->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::put_CurrentPosition(REFTIME llTime)
{
    IMediaPosition *pMP;
    HRESULT hr = GetPeer(&pMP);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pMP->put_CurrentPosition(llTime);
    pMP->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::get_StopTime(__out REFTIME *pllTime)
{
    IMediaPosition *pMP;
    HRESULT hr = GetPeer(&pMP);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pMP->get_StopTime(pllTime);
    pMP->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::put_StopTime(REFTIME llTime)
{
    IMediaPosition *pMP;
    HRESULT hr = GetPeer(&pMP);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pMP->put_StopTime(llTime);
    pMP->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::get_PrerollTime(__out REFTIME *pllTime)
{
    IMediaPosition *pMP;
    HRESULT hr = GetPeer(&pMP);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pMP->get_PrerollTime(pllTime);
    pMP->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::put_PrerollTime(REFTIME llTime)
{
    IMediaPosition *pMP;
    HRESULT hr = GetPeer(&pMP);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pMP->put_PrerollTime(llTime);
    pMP->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::get_Rate(__out double *pdRate)
{
    IMediaPosition *pMP;
    HRESULT hr = GetPeer(&pMP);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pMP->get_Rate(pdRate);
    pMP->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::put_Rate(double dRate)
{
    if (0.0 == dRate)
    {
        return E_INVALIDARG;
    }

    IMediaPosition *pMP;
    HRESULT hr = GetPeer(&pMP);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pMP->put_Rate(dRate);
    pMP->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::CanSeekForward(__out LONG *pCanSeekForward)
{
    IMediaPosition *pMP;
    HRESULT hr = GetPeer(&pMP);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pMP->CanSeekForward(pCanSeekForward);
    pMP->Release();
    return hr;
}

STDMETHODIMP
CPosPassThru::CanSeekBackward(__out LONG *pCanSeekBackward)
{
    IMediaPosition *pMP;
    HRESULT hr = GetPeer(&pMP);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pMP->CanSeekBackward(pCanSeekBackward);
    pMP->Release();
    return hr;
}

// --- Implements the CRendererPosPassThru class ----------

// Media times (eg current frame, field, sample etc) are passed through the
// filtergraph in media samples. When a renderer gets a sample with media
// times in it, it will call one of the RegisterMediaTime methods we expose
// (one takes an IMediaSample, the other takes the media times direct). We
// store the media times internally and return them in GetCurrentPosition.

CRendererPosPassThru::CRendererPosPassThru(__in_opt LPCTSTR pName, __in_opt LPUNKNOWN pUnk, __inout HRESULT *phr,
                                           IPin *pPin)
    : CPosPassThru(pName, pUnk, phr, pPin)
    , m_StartMedia(0)
    , m_EndMedia(0)
    , m_bReset(TRUE)
{
}

// Sets the media times the object should report

HRESULT
CRendererPosPassThru::RegisterMediaTime(IMediaSample *pMediaSample)
{
    ASSERT(pMediaSample);
    LONGLONG StartMedia;
    LONGLONG EndMedia;

    CAutoLock cAutoLock(&m_PositionLock);

    // Get the media times from the sample

    HRESULT hr = pMediaSample->GetTime(&StartMedia, &EndMedia);
    if (FAILED(hr))
    {
        ASSERT(hr == VFW_E_SAMPLE_TIME_NOT_SET);
        return hr;
    }

    m_StartMedia = StartMedia;
    m_EndMedia = EndMedia;
    m_bReset = FALSE;
    return NOERROR;
}

// Sets the media times the object should report

HRESULT
CRendererPosPassThru::RegisterMediaTime(LONGLONG StartTime, LONGLONG EndTime)
{
    CAutoLock cAutoLock(&m_PositionLock);
    m_StartMedia = StartTime;
    m_EndMedia = EndTime;
    m_bReset = FALSE;
    return NOERROR;
}

// Return the current media times registered in the object

HRESULT
CRendererPosPassThru::GetMediaTime(__out LONGLONG *pStartTime, __out_opt LONGLONG *pEndTime)
{
    ASSERT(pStartTime);

    CAutoLock cAutoLock(&m_PositionLock);
    if (m_bReset == TRUE)
    {
        return E_FAIL;
    }

    // We don't have to return the end time

    HRESULT hr = ConvertTimeFormat(pStartTime, 0, m_StartMedia, &TIME_FORMAT_MEDIA_TIME);
    if (pEndTime && SUCCEEDED(hr))
    {
        hr = ConvertTimeFormat(pEndTime, 0, m_EndMedia, &TIME_FORMAT_MEDIA_TIME);
    }
    return hr;
}

// Resets the media times we hold

HRESULT
CRendererPosPassThru::ResetMediaTime()
{
    CAutoLock cAutoLock(&m_PositionLock);
    m_StartMedia = 0;
    m_EndMedia = 0;
    m_bReset = TRUE;
    return NOERROR;
}

// Intended to be called by the owing filter during EOS processing so
// that the media times can be adjusted to the stop time.  This ensures
// that the GetCurrentPosition will actully get to the stop position.
HRESULT
CRendererPosPassThru::EOS()
{
    HRESULT hr;

    if (m_bReset == TRUE)
        hr = E_FAIL;
    else
    {
        LONGLONG llStop;
        if
            SUCCEEDED(hr = GetStopPosition(&llStop))
            {
                CAutoLock cAutoLock(&m_PositionLock);
                m_StartMedia = m_EndMedia = llStop;
            }
    }
    return hr;
}

// -- CSourceSeeking implementation ------------

CSourceSeeking::CSourceSeeking(__in_opt LPCTSTR pName, __in_opt LPUNKNOWN pUnk, __inout HRESULT *phr,
                               __in CCritSec *pLock)
    : CUnknown(pName, pUnk)
    , m_pLock(pLock)
    , m_rtStart((long)0)
{
    m_rtStop = _I64_MAX / 2;
    m_rtDuration = m_rtStop;
    m_dRateSeeking = 1.0;

    m_dwSeekingCaps = AM_SEEKING_CanSeekForwards | AM_SEEKING_CanSeekBackwards | AM_SEEKING_CanSeekAbsolute |
                      AM_SEEKING_CanGetStopPos | AM_SEEKING_CanGetDuration;
}

HRESULT CSourceSeeking::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    if (riid == IID_IMediaSeeking)
    {
        CheckPointer(ppv, E_POINTER);
        return GetInterface(static_cast<IMediaSeeking *>(this), ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

HRESULT CSourceSeeking::IsFormatSupported(const GUID *pFormat)
{
    CheckPointer(pFormat, E_POINTER);
    // only seeking in time (REFERENCE_TIME units) is supported
    return *pFormat == TIME_FORMAT_MEDIA_TIME ? S_OK : S_FALSE;
}

HRESULT CSourceSeeking::QueryPreferredFormat(__out GUID *pFormat)
{
    CheckPointer(pFormat, E_POINTER);
    *pFormat = TIME_FORMAT_MEDIA_TIME;
    return S_OK;
}

HRESULT CSourceSeeking::SetTimeFormat(const GUID *pFormat)
{
    CheckPointer(pFormat, E_POINTER);

    // nothing to set; just check that it's TIME_FORMAT_TIME
    return *pFormat == TIME_FORMAT_MEDIA_TIME ? S_OK : E_INVALIDARG;
}

HRESULT CSourceSeeking::IsUsingTimeFormat(const GUID *pFormat)
{
    CheckPointer(pFormat, E_POINTER);
    return *pFormat == TIME_FORMAT_MEDIA_TIME ? S_OK : S_FALSE;
}

HRESULT CSourceSeeking::GetTimeFormat(__out GUID *pFormat)
{
    CheckPointer(pFormat, E_POINTER);
    *pFormat = TIME_FORMAT_MEDIA_TIME;
    return S_OK;
}

HRESULT CSourceSeeking::GetDuration(__out LONGLONG *pDuration)
{
    CheckPointer(pDuration, E_POINTER);
    CAutoLock lock(m_pLock);
    *pDuration = m_rtDuration;
    return S_OK;
}

HRESULT CSourceSeeking::GetStopPosition(__out LONGLONG *pStop)
{
    CheckPointer(pStop, E_POINTER);
    CAutoLock lock(m_pLock);
    *pStop = m_rtStop;
    return S_OK;
}

HRESULT CSourceSeeking::GetCurrentPosition(__out LONGLONG *pCurrent)
{
    // GetCurrentPosition is typically supported only in renderers and
    // not in source filters.
    return E_NOTIMPL;
}

HRESULT CSourceSeeking::GetCapabilities(__out DWORD *pCapabilities)
{
    CheckPointer(pCapabilities, E_POINTER);
    *pCapabilities = m_dwSeekingCaps;
    return S_OK;
}

HRESULT CSourceSeeking::CheckCapabilities(__inout DWORD *pCapabilities)
{
    CheckPointer(pCapabilities, E_POINTER);

    // make sure all requested capabilities are in our mask
    return (~m_dwSeekingCaps & *pCapabilities) ? S_FALSE : S_OK;
}

HRESULT CSourceSeeking::ConvertTimeFormat(__out LONGLONG *pTarget, __in_opt const GUID *pTargetFormat, LONGLONG Source,
                                          __in_opt const GUID *pSourceFormat)
{
    CheckPointer(pTarget, E_POINTER);
    // format guids can be null to indicate current format

    // since we only support TIME_FORMAT_MEDIA_TIME, we don't really
    // offer any conversions.
    if (pTargetFormat == 0 || *pTargetFormat == TIME_FORMAT_MEDIA_TIME)
    {
        if (pSourceFormat == 0 || *pSourceFormat == TIME_FORMAT_MEDIA_TIME)
        {
            *pTarget = Source;
            return S_OK;
        }
    }

    return E_INVALIDARG;
}

HRESULT CSourceSeeking::SetPositions(__inout_opt LONGLONG *pCurrent, DWORD CurrentFlags, __inout_opt LONGLONG *pStop,
                                     DWORD StopFlags)
{
    DWORD StopPosBits = StopFlags & AM_SEEKING_PositioningBitsMask;
    DWORD StartPosBits = CurrentFlags & AM_SEEKING_PositioningBitsMask;

    if (StopFlags)
    {
        CheckPointer(pStop, E_POINTER);

        // accept only relative, incremental, or absolute positioning
        if (StopPosBits != StopFlags)
        {
            return E_INVALIDARG;
        }
    }

    if (CurrentFlags)
    {
        CheckPointer(pCurrent, E_POINTER);
        if (StartPosBits != AM_SEEKING_AbsolutePositioning && StartPosBits != AM_SEEKING_RelativePositioning)
        {
            return E_INVALIDARG;
        }
    }

    // scope for autolock
    {
        CAutoLock lock(m_pLock);

        // set start position
        if (StartPosBits == AM_SEEKING_AbsolutePositioning)
        {
            m_rtStart = *pCurrent;
        }
        else if (StartPosBits == AM_SEEKING_RelativePositioning)
        {
            m_rtStart += *pCurrent;
        }

        // set stop position
        if (StopPosBits == AM_SEEKING_AbsolutePositioning)
        {
            m_rtStop = *pStop;
        }
        else if (StopPosBits == AM_SEEKING_IncrementalPositioning)
        {
            m_rtStop = m_rtStart + *pStop;
        }
        else if (StopPosBits == AM_SEEKING_RelativePositioning)
        {
            m_rtStop = m_rtStop + *pStop;
        }
    }

    HRESULT hr = S_OK;
    if (SUCCEEDED(hr) && StopPosBits)
    {
        hr = ChangeStop();
    }
    if (StartPosBits)
    {
        hr = ChangeStart();
    }

    return hr;
}

HRESULT CSourceSeeking::GetPositions(__out_opt LONGLONG *pCurrent, __out_opt LONGLONG *pStop)
{
    if (pCurrent)
    {
        *pCurrent = m_rtStart;
    }
    if (pStop)
    {
        *pStop = m_rtStop;
    }

    return S_OK;
    ;
}

HRESULT CSourceSeeking::GetAvailable(__out_opt LONGLONG *pEarliest, __out_opt LONGLONG *pLatest)
{
    if (pEarliest)
    {
        *pEarliest = 0;
    }
    if (pLatest)
    {
        CAutoLock lock(m_pLock);
        *pLatest = m_rtDuration;
    }
    return S_OK;
}

HRESULT CSourceSeeking::SetRate(double dRate)
{
    {
        CAutoLock lock(m_pLock);
        m_dRateSeeking = dRate;
    }
    return ChangeRate();
}

HRESULT CSourceSeeking::GetRate(__out double *pdRate)
{
    CheckPointer(pdRate, E_POINTER);
    CAutoLock lock(m_pLock);
    *pdRate = m_dRateSeeking;
    return S_OK;
}

HRESULT CSourceSeeking::GetPreroll(__out LONGLONG *pPreroll)
{
    CheckPointer(pPreroll, E_POINTER);
    *pPreroll = 0;
    return S_OK;
}

// --- CSourcePosition implementation ----------

CSourcePosition::CSourcePosition(__in_opt LPCTSTR pName, __in_opt LPUNKNOWN pUnk, __inout HRESULT *phr,
                                 __in CCritSec *pLock)
    : CMediaPosition(pName, pUnk)
    , m_pLock(pLock)
    , m_Start(CRefTime((LONGLONG)0))
{
    m_Stop = _I64_MAX;
    m_Rate = 1.0;
}

STDMETHODIMP
CSourcePosition::get_Duration(__out REFTIME *plength)
{
    CheckPointer(plength, E_POINTER);
    ValidateReadWritePtr(plength, sizeof(REFTIME));
    CAutoLock lock(m_pLock);

    *plength = m_Duration;
    return S_OK;
}

STDMETHODIMP
CSourcePosition::put_CurrentPosition(REFTIME llTime)
{
    m_pLock->Lock();
    m_Start = llTime;
    m_pLock->Unlock();

    return ChangeStart();
}

STDMETHODIMP
CSourcePosition::get_StopTime(__out REFTIME *pllTime)
{
    CheckPointer(pllTime, E_POINTER);
    ValidateReadWritePtr(pllTime, sizeof(REFTIME));
    CAutoLock lock(m_pLock);

    *pllTime = m_Stop;
    return S_OK;
}

STDMETHODIMP
CSourcePosition::put_StopTime(REFTIME llTime)
{
    m_pLock->Lock();
    m_Stop = llTime;
    m_pLock->Unlock();

    return ChangeStop();
}

STDMETHODIMP
CSourcePosition::get_PrerollTime(__out REFTIME *pllTime)
{
    CheckPointer(pllTime, E_POINTER);
    ValidateReadWritePtr(pllTime, sizeof(REFTIME));
    return E_NOTIMPL;
}

STDMETHODIMP
CSourcePosition::put_PrerollTime(REFTIME llTime)
{
    return E_NOTIMPL;
}

STDMETHODIMP
CSourcePosition::get_Rate(__out double *pdRate)
{
    CheckPointer(pdRate, E_POINTER);
    ValidateReadWritePtr(pdRate, sizeof(double));
    CAutoLock lock(m_pLock);

    *pdRate = m_Rate;
    return S_OK;
}

STDMETHODIMP
CSourcePosition::put_Rate(double dRate)
{
    m_pLock->Lock();
    m_Rate = dRate;
    m_pLock->Unlock();

    return ChangeRate();
}

// By default we can seek forwards

STDMETHODIMP
CSourcePosition::CanSeekForward(__out LONG *pCanSeekForward)
{
    CheckPointer(pCanSeekForward, E_POINTER);
    *pCanSeekForward = OATRUE;
    return S_OK;
}

// By default we can seek backwards

STDMETHODIMP
CSourcePosition::CanSeekBackward(__out LONG *pCanSeekBackward)
{
    CheckPointer(pCanSeekBackward, E_POINTER);
    *pCanSeekBackward = OATRUE;
    return S_OK;
}

// --- Implementation of CBasicAudio class ----------

CBasicAudio::CBasicAudio(__in_opt LPCTSTR pName, __in_opt LPUNKNOWN punk)
    : CUnknown(pName, punk)
{
}

// overriden to publicise our interfaces

STDMETHODIMP
CBasicAudio::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    ValidateReadWritePtr(ppv, sizeof(PVOID));
    if (riid == IID_IBasicAudio)
    {
        return GetInterface((IBasicAudio *)this, ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

STDMETHODIMP
CBasicAudio::GetTypeInfoCount(__out UINT *pctinfo)
{
    return m_basedisp.GetTypeInfoCount(pctinfo);
}

STDMETHODIMP
CBasicAudio::GetTypeInfo(UINT itinfo, LCID lcid, __deref_out ITypeInfo **pptinfo)
{
    return m_basedisp.GetTypeInfo(IID_IBasicAudio, itinfo, lcid, pptinfo);
}

STDMETHODIMP
CBasicAudio::GetIDsOfNames(REFIID riid, __in_ecount(cNames) LPOLESTR *rgszNames, UINT cNames, LCID lcid,
                           __out_ecount(cNames) DISPID *rgdispid)
{
    return m_basedisp.GetIDsOfNames(IID_IBasicAudio, rgszNames, cNames, lcid, rgdispid);
}

STDMETHODIMP
CBasicAudio::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, __in DISPPARAMS *pdispparams,
                    __out_opt VARIANT *pvarResult, __out_opt EXCEPINFO *pexcepinfo, __out_opt UINT *puArgErr)
{
    // this parameter is a dead leftover from an earlier interface
    if (IID_NULL != riid)
    {
        return DISP_E_UNKNOWNINTERFACE;
    }

    ITypeInfo *pti;
    HRESULT hr = GetTypeInfo(0, lcid, &pti);

    if (FAILED(hr))
    {
        return hr;
    }

    hr = pti->Invoke((IBasicAudio *)this, dispidMember, wFlags, pdispparams, pvarResult, pexcepinfo, puArgErr);

    pti->Release();
    return hr;
}

// --- IVideoWindow implementation ----------

CBaseVideoWindow::CBaseVideoWindow(__in_opt LPCTSTR pName, __in_opt LPUNKNOWN punk)
    : CUnknown(pName, punk)
{
}

// overriden to publicise our interfaces

STDMETHODIMP
CBaseVideoWindow::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    ValidateReadWritePtr(ppv, sizeof(PVOID));
    if (riid == IID_IVideoWindow)
    {
        return GetInterface((IVideoWindow *)this, ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

STDMETHODIMP
CBaseVideoWindow::GetTypeInfoCount(__out UINT *pctinfo)
{
    return m_basedisp.GetTypeInfoCount(pctinfo);
}

STDMETHODIMP
CBaseVideoWindow::GetTypeInfo(UINT itinfo, LCID lcid, __deref_out ITypeInfo **pptinfo)
{
    return m_basedisp.GetTypeInfo(IID_IVideoWindow, itinfo, lcid, pptinfo);
}

STDMETHODIMP
CBaseVideoWindow::GetIDsOfNames(REFIID riid, __in_ecount(cNames) LPOLESTR *rgszNames, UINT cNames, LCID lcid,
                                __out_ecount(cNames) DISPID *rgdispid)
{
    return m_basedisp.GetIDsOfNames(IID_IVideoWindow, rgszNames, cNames, lcid, rgdispid);
}

STDMETHODIMP
CBaseVideoWindow::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, __in DISPPARAMS *pdispparams,
                         __out_opt VARIANT *pvarResult, __out_opt EXCEPINFO *pexcepinfo, __out_opt UINT *puArgErr)
{
    // this parameter is a dead leftover from an earlier interface
    if (IID_NULL != riid)
    {
        return DISP_E_UNKNOWNINTERFACE;
    }

    ITypeInfo *pti;
    HRESULT hr = GetTypeInfo(0, lcid, &pti);

    if (FAILED(hr))
    {
        return hr;
    }

    hr = pti->Invoke((IVideoWindow *)this, dispidMember, wFlags, pdispparams, pvarResult, pexcepinfo, puArgErr);

    pti->Release();
    return hr;
}

// --- IBasicVideo implementation ----------

CBaseBasicVideo::CBaseBasicVideo(__in_opt LPCTSTR pName, __in_opt LPUNKNOWN punk)
    : CUnknown(pName, punk)
{
}

// overriden to publicise our interfaces

STDMETHODIMP
CBaseBasicVideo::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    ValidateReadWritePtr(ppv, sizeof(PVOID));
    if (riid == IID_IBasicVideo || riid == IID_IBasicVideo2)
    {
        return GetInterface(static_cast<IBasicVideo2 *>(this), ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

STDMETHODIMP
CBaseBasicVideo::GetTypeInfoCount(__out UINT *pctinfo)
{
    return m_basedisp.GetTypeInfoCount(pctinfo);
}

STDMETHODIMP
CBaseBasicVideo::GetTypeInfo(UINT itinfo, LCID lcid, __deref_out ITypeInfo **pptinfo)
{
    return m_basedisp.GetTypeInfo(IID_IBasicVideo, itinfo, lcid, pptinfo);
}

STDMETHODIMP
CBaseBasicVideo::GetIDsOfNames(REFIID riid, __in_ecount(cNames) LPOLESTR *rgszNames, UINT cNames, LCID lcid,
                               __out_ecount(cNames) DISPID *rgdispid)
{
    return m_basedisp.GetIDsOfNames(IID_IBasicVideo, rgszNames, cNames, lcid, rgdispid);
}

STDMETHODIMP
CBaseBasicVideo::Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, __in DISPPARAMS *pdispparams,
                        __out_opt VARIANT *pvarResult, __out_opt EXCEPINFO *pexcepinfo, __out_opt UINT *puArgErr)
{
    // this parameter is a dead leftover from an earlier interface
    if (IID_NULL != riid)
    {
        return DISP_E_UNKNOWNINTERFACE;
    }

    ITypeInfo *pti;
    HRESULT hr = GetTypeInfo(0, lcid, &pti);

    if (FAILED(hr))
    {
        return hr;
    }

    hr = pti->Invoke((IBasicVideo *)this, dispidMember, wFlags, pdispparams, pvarResult, pexcepinfo, puArgErr);

    pti->Release();
    return hr;
}

// --- Implementation of Deferred Commands ----------

CDispParams::CDispParams(UINT nArgs, __in_ecount(nArgs) VARIANT *pArgs, __inout_opt HRESULT *phr)
{
    cNamedArgs = 0;
    rgdispidNamedArgs = NULL;
    cArgs = nArgs;

    if (cArgs)
    {
        rgvarg = new VARIANT[cArgs];
        if (NULL == rgvarg)
        {
            cArgs = 0;
            if (phr)
            {
                *phr = E_OUTOFMEMORY;
            }
            return;
        }

        for (UINT i = 0; i < cArgs; i++)
        {

            //  Why aren't we using VariantCopy?

            VARIANT *pDest = &rgvarg[i];
            VARIANT *pSrc = &pArgs[i];

            pDest->vt = pSrc->vt;
            switch (pDest->vt)
            {

            case VT_I4: pDest->lVal = pSrc->lVal; break;

            case VT_UI1: pDest->bVal = pSrc->bVal; break;

            case VT_I2: pDest->iVal = pSrc->iVal; break;

            case VT_R4: pDest->fltVal = pSrc->fltVal; break;

            case VT_R8: pDest->dblVal = pSrc->dblVal; break;

            case VT_BOOL: pDest->boolVal = pSrc->boolVal; break;

            case VT_ERROR: pDest->scode = pSrc->scode; break;

            case VT_CY: pDest->cyVal = pSrc->cyVal; break;

            case VT_DATE: pDest->date = pSrc->date; break;

            case VT_BSTR:
                if ((PVOID)pSrc->bstrVal == NULL)
                {
                    pDest->bstrVal = NULL;
                }
                else
                {

                    // a BSTR is a WORD followed by a UNICODE string.
                    // the pointer points just after the WORD

                    WORD len = *(WORD *)(pSrc->bstrVal - (sizeof(WORD) / sizeof(OLECHAR)));
                    OLECHAR *pch = new OLECHAR[len + (sizeof(WORD) / sizeof(OLECHAR))];
                    if (pch)
                    {
                        WORD *pui = (WORD *)pch;
                        *pui = len;
                        pDest->bstrVal = pch + (sizeof(WORD) / sizeof(OLECHAR));
                        CopyMemory(pDest->bstrVal, pSrc->bstrVal, len * sizeof(OLECHAR));
                    }
                    else
                    {
                        cArgs = i;
                        if (phr)
                        {
                            *phr = E_OUTOFMEMORY;
                        }
                    }
                }
                break;

            case VT_UNKNOWN:
                pDest->punkVal = pSrc->punkVal;
                pDest->punkVal->AddRef();
                break;

            case VT_DISPATCH:
                pDest->pdispVal = pSrc->pdispVal;
                pDest->pdispVal->AddRef();
                break;

            default:
                // a type we haven't got round to adding yet!
                ASSERT(0);
                break;
            }
        }
    }
    else
    {
        rgvarg = NULL;
    }
}

CDispParams::~CDispParams()
{
    for (UINT i = 0; i < cArgs; i++)
    {
        switch (rgvarg[i].vt)
        {
        case VT_BSTR:
            //  Explicitly cast BSTR to PVOID to tell code scanning tools we really mean to test the pointer
            if ((PVOID)rgvarg[i].bstrVal != NULL)
            {
                OLECHAR *pch = rgvarg[i].bstrVal - (sizeof(WORD) / sizeof(OLECHAR));
                delete pch;
            }
            break;

        case VT_UNKNOWN: rgvarg[i].punkVal->Release(); break;

        case VT_DISPATCH: rgvarg[i].pdispVal->Release(); break;
        }
    }
    delete[] rgvarg;
}

// lifetime is controlled by refcounts (see defer.h)

CDeferredCommand::CDeferredCommand(__inout CCmdQueue *pQ, __in_opt LPUNKNOWN pUnk, __inout HRESULT *phr,
                                   __in LPUNKNOWN pUnkExecutor, REFTIME time, __in GUID *iid, long dispidMethod,
                                   short wFlags, long nArgs, __in_ecount(nArgs) VARIANT *pDispParams,
                                   __out VARIANT *pvarResult, __out short *puArgErr, BOOL bStream)
    : CUnknown(NAME("DeferredCommand"), pUnk)
    , m_pQueue(pQ)
    , m_pUnk(pUnkExecutor)
    , m_iid(iid)
    , m_dispidMethod(dispidMethod)
    , m_wFlags(wFlags)
    , m_DispParams(nArgs, pDispParams, phr)
    , m_pvarResult(pvarResult)
    , m_bStream(bStream)
    , m_hrResult(E_ABORT)

{
    // convert REFTIME to REFERENCE_TIME
    COARefTime convertor(time);
    m_time = convertor;

    // no check of time validity - it's ok to queue a command that's
    // already late

    // check iid is supportable on pUnk by QueryInterface for it
    IUnknown *pInterface;
    HRESULT hr = m_pUnk->QueryInterface(GetIID(), (void **)&pInterface);
    if (FAILED(hr))
    {
        *phr = hr;
        return;
    }
    pInterface->Release();

    // !!! check dispidMethod and param/return types using typelib
    ITypeInfo *pti;
    hr = m_Dispatch.GetTypeInfo(*iid, 0, 0, &pti);
    if (FAILED(hr))
    {
        *phr = hr;
        return;
    }
    // !!! some sort of ITypeInfo validity check here
    pti->Release();

    // Fix up the dispid for put and get
    if (wFlags == DISPATCH_PROPERTYPUT)
    {
        m_DispParams.cNamedArgs = 1;
        m_DispId = DISPID_PROPERTYPUT;
        m_DispParams.rgdispidNamedArgs = &m_DispId;
    }

    // all checks ok - add to queue
    hr = pQ->Insert(this);
    if (FAILED(hr))
    {
        *phr = hr;
    }
}

// refcounts are held by caller of InvokeAt... and by list. So if
// we get here, we can't be on the list

#if 0
CDeferredCommand::~CDeferredCommand()
{
    // this assert is invalid since if the queue is deleted while we are
    // still on the queue, we will have been removed by the queue and this
    // m_pQueue will not have been modified.
    // ASSERT(m_pQueue == NULL);

    // we don't hold a ref count on pUnk, which is the object that should
    // execute the command.
    // This is because there would otherwise be a circular refcount problem
    // since pUnk probably owns the CmdQueue object that has a refcount
    // on us.
    // The lifetime of pUnk is guaranteed by it being part of, or lifetime
    // controlled by, our parent object. As long as we are on the list, pUnk
    // must be valid. Once we are off the list, we do not use pUnk.

}
#endif

// overriden to publicise our interfaces

STDMETHODIMP
CDeferredCommand::NonDelegatingQueryInterface(REFIID riid, __out void **ppv)
{
    ValidateReadWritePtr(ppv, sizeof(PVOID));
    if (riid == IID_IDeferredCommand)
    {
        return GetInterface((IDeferredCommand *)this, ppv);
    }
    else
    {
        return CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
}

// remove from q. this will reduce the refcount by one (since the q
// holds a count) but can't make us go away since he must have a
// refcount in order to call this method.

STDMETHODIMP
CDeferredCommand::Cancel()
{
    if (m_pQueue == NULL)
    {
        return VFW_E_ALREADY_CANCELLED;
    }

    HRESULT hr = m_pQueue->Remove(this);
    if (FAILED(hr))
    {
        return hr;
    }

    m_pQueue = NULL;
    return S_OK;
}

STDMETHODIMP
CDeferredCommand::Confidence(__out LONG *pConfidence)
{
    return E_NOTIMPL;
}

STDMETHODIMP
CDeferredCommand::GetHResult(__out HRESULT *phrResult)
{
    CheckPointer(phrResult, E_POINTER);
    ValidateReadWritePtr(phrResult, sizeof(HRESULT));

    if (m_pQueue != NULL)
    {
        return E_ABORT;
    }
    *phrResult = m_hrResult;
    return S_OK;
}

// set the time to be a new time (checking that it is valid) and
// then requeue

STDMETHODIMP
CDeferredCommand::Postpone(REFTIME newtime)
{

    // check that this time is not past
    // convert REFTIME to REFERENCE_TIME
    COARefTime convertor(newtime);

    // check that the time has not passed
    if (m_pQueue->CheckTime(convertor, IsStreamTime()))
    {
        return VFW_E_TIME_ALREADY_PASSED;
    }

    // extract from list
    HRESULT hr = m_pQueue->Remove(this);
    if (FAILED(hr))
    {
        return hr;
    }

    // change time
    m_time = convertor;

    // requeue
    hr = m_pQueue->Insert(this);

    return hr;
}

HRESULT
CDeferredCommand::Invoke()
{
    // check that we are still outstanding
    if (m_pQueue == NULL)
    {
        return VFW_E_ALREADY_CANCELLED;
    }

    // get the type info
    ITypeInfo *pti;
    HRESULT hr = m_Dispatch.GetTypeInfo(GetIID(), 0, 0, &pti);
    if (FAILED(hr))
    {
        return hr;
    }

    // qi for the expected interface and then invoke it. Note that we have to
    // treat the returned interface as IUnknown since we don't know its type.
    IUnknown *pInterface;

    hr = m_pUnk->QueryInterface(GetIID(), (void **)&pInterface);
    if (FAILED(hr))
    {
        pti->Release();
        return hr;
    }

    EXCEPINFO expinfo;
    UINT uArgErr;
    m_hrResult = pti->Invoke(pInterface, GetMethod(), GetFlags(), GetParams(), GetResult(), &expinfo, &uArgErr);

    // release the interface we QI'd for
    pInterface->Release();
    pti->Release();

    // remove from list whether or not successful
    // or we loop indefinitely
    hr = m_pQueue->Remove(this);
    m_pQueue = NULL;
    return hr;
}

// --- CCmdQueue methods ----------

CCmdQueue::CCmdQueue(__inout_opt HRESULT *phr)
    : m_listPresentation(NAME("Presentation time command list"))
    , m_listStream(NAME("Stream time command list"))
    , m_evDue(TRUE, phr)
    , // manual reset
    m_dwAdvise(0)
    , m_pClock(NULL)
    , m_bRunning(FALSE)
{
}

CCmdQueue::~CCmdQueue()
{
    // empty all our lists

    // we hold a refcount on each, so traverse and Release each
    // entry then RemoveAll to empty the list
    POSITION pos = m_listPresentation.GetHeadPosition();

    while (pos)
    {
        CDeferredCommand *pCmd = m_listPresentation.GetNext(pos);
        pCmd->Release();
    }
    m_listPresentation.RemoveAll();

    pos = m_listStream.GetHeadPosition();

    while (pos)
    {
        CDeferredCommand *pCmd = m_listStream.GetNext(pos);
        pCmd->Release();
    }
    m_listStream.RemoveAll();

    if (m_pClock)
    {
        if (m_dwAdvise)
        {
            m_pClock->Unadvise(m_dwAdvise);
            m_dwAdvise = 0;
        }
        m_pClock->Release();
    }
}

// returns a new CDeferredCommand object that will be initialised with
// the parameters and will be added to the queue during construction.
// returns S_OK if successfully created otherwise an error and
// no object has been queued.

HRESULT
CCmdQueue::New(__out CDeferredCommand **ppCmd,
               __in LPUNKNOWN pUnk, // this object will execute command
               REFTIME time, __in GUID *iid, long dispidMethod, short wFlags, long cArgs,
               __in_ecount(cArgs) VARIANT *pDispParams, __out VARIANT *pvarResult, __out short *puArgErr, BOOL bStream)
{
    CAutoLock lock(&m_Lock);

    HRESULT hr = S_OK;
    *ppCmd = NULL;

    CDeferredCommand *pCmd;
    pCmd = new CDeferredCommand(this,
                                NULL, // not aggregated
                                &hr,
                                pUnk, // this guy will execute
                                time, iid, dispidMethod, wFlags, cArgs, pDispParams, pvarResult, puArgErr, bStream);

    if (pCmd == NULL)
    {
        hr = E_OUTOFMEMORY;
    }
    else
    {
        *ppCmd = pCmd;
    }
    return hr;
}

HRESULT
CCmdQueue::Insert(__in CDeferredCommand *pCmd)
{
    CAutoLock lock(&m_Lock);

    // addref the item
    pCmd->AddRef();

    CGenericList<CDeferredCommand> *pList;
    if (pCmd->IsStreamTime())
    {
        pList = &m_listStream;
    }
    else
    {
        pList = &m_listPresentation;
    }
    POSITION pos = pList->GetHeadPosition();

    // seek past all items that are before us
    while (pos && (pList->GetValid(pos)->GetTime() <= pCmd->GetTime()))
    {

        pList->GetNext(pos);
    }

    // now at end of list or in front of items that come later
    if (!pos)
    {
        pList->AddTail(pCmd);
    }
    else
    {
        pList->AddBefore(pos, pCmd);
    }

    SetTimeAdvise();
    return S_OK;
}

HRESULT
CCmdQueue::Remove(__in CDeferredCommand *pCmd)
{
    CAutoLock lock(&m_Lock);
    HRESULT hr = S_OK;

    CGenericList<CDeferredCommand> *pList;
    if (pCmd->IsStreamTime())
    {
        pList = &m_listStream;
    }
    else
    {
        pList = &m_listPresentation;
    }
    POSITION pos = pList->GetHeadPosition();

    // traverse the list
    while (pos && (pList->GetValid(pos) != pCmd))
    {
        pList->GetNext(pos);
    }

    // did we drop off the end?
    if (!pos)
    {
        hr = VFW_E_NOT_FOUND;
    }
    else
    {

        // found it - now take off list
        pList->Remove(pos);

        // Insert did an AddRef, so release it
        pCmd->Release();

        // check that timer request is still for earliest time
        SetTimeAdvise();
    }
    return hr;
}

// set the clock used for timing

HRESULT
CCmdQueue::SetSyncSource(__in_opt IReferenceClock *pClock)
{
    CAutoLock lock(&m_Lock);

    // addref the new clock first in case they are the same
    if (pClock)
    {
        pClock->AddRef();
    }

    // kill any advise on the old clock
    if (m_pClock)
    {
        if (m_dwAdvise)
        {
            m_pClock->Unadvise(m_dwAdvise);
            m_dwAdvise = 0;
        }
        m_pClock->Release();
    }
    m_pClock = pClock;

    // set up a new advise
    SetTimeAdvise();
    return S_OK;
}

// set up a timer event with the reference clock

void CCmdQueue::SetTimeAdvise(void)
{
    // make sure we have a clock to use
    if (!m_pClock)
    {
        return;
    }

    // reset the event whenever we are requesting a new signal
    m_evDue.Reset();

    // time 0 is earliest
    CRefTime current;

    // find the earliest presentation time
    POSITION pos = m_listPresentation.GetHeadPosition();
    if (pos != NULL)
    {
        current = m_listPresentation.GetValid(pos)->GetTime();
    }

    // if we're running, check the stream times too
    if (m_bRunning)
    {

        CRefTime t;
        pos = m_listStream.GetHeadPosition();
        if (NULL != pos)
        {
            t = m_listStream.GetValid(pos)->GetTime();

            // add on stream time offset to get presentation time
            t += m_StreamTimeOffset;

            // is this earlier?
            if ((current == TimeZero) || (t < current))
            {
                current = t;
            }
        }
    }

    // need to change?
    if ((current > TimeZero) && (current != m_tCurrentAdvise))
    {
        if (m_dwAdvise)
        {
            m_pClock->Unadvise(m_dwAdvise);
            // reset the event whenever we are requesting a new signal
            m_evDue.Reset();
        }

        // ask for time advice - the first two params are either
        // stream time offset and stream time or
        // presentation time and 0. we always use the latter
        HRESULT hr = m_pClock->AdviseTime((REFERENCE_TIME)current, TimeZero, (HEVENT)HANDLE(m_evDue), &m_dwAdvise);

        ASSERT(SUCCEEDED(hr));
        m_tCurrentAdvise = current;
    }
}

// switch to run mode. Streamtime to Presentation time mapping known.

HRESULT
CCmdQueue::Run(REFERENCE_TIME tStreamTimeOffset)
{
    CAutoLock lock(&m_Lock);

    m_StreamTimeOffset = tStreamTimeOffset;
    m_bRunning = TRUE;

    // ensure advise is accurate
    SetTimeAdvise();
    return S_OK;
}

// switch to Stopped or Paused mode. Time mapping not known.

HRESULT
CCmdQueue::EndRun()
{
    CAutoLock lock(&m_Lock);

    m_bRunning = FALSE;

    // check timer setting - stream times
    SetTimeAdvise();
    return S_OK;
}

// return a pointer to the next due command. Blocks for msTimeout
// milliseconds until there is a due command.
// Stream-time commands will only become due between Run and Endrun calls.
// The command remains queued until invoked or cancelled.
// Returns E_ABORT if timeout occurs, otherwise S_OK (or other error).
//
// returns an AddRef'd object

HRESULT
CCmdQueue::GetDueCommand(__out CDeferredCommand **ppCmd, long msTimeout)
{
    // loop until we timeout or find a due command
    for (;;)
    {

        {
            CAutoLock lock(&m_Lock);

            // find the earliest command
            CDeferredCommand *pCmd = NULL;

            // check the presentation time and the
            // stream time list to find the earliest

            POSITION pos = m_listPresentation.GetHeadPosition();

            if (NULL != pos)
            {
                pCmd = m_listPresentation.GetValid(pos);
            }

            if (m_bRunning)
            {
                pos = m_listStream.GetHeadPosition();
                if (NULL != pos)
                {
                    CDeferredCommand *pStrm = m_listStream.GetValid(pos);

                    CRefTime t = pStrm->GetTime() + m_StreamTimeOffset;
                    if (!pCmd || (t < pCmd->GetTime()))
                    {
                        pCmd = pStrm;
                    }
                }
            }

            //	if we have found one, is it due?
            if (pCmd)
            {
                if (CheckTime(pCmd->GetTime(), pCmd->IsStreamTime()))
                {

                    // yes it's due - addref it
                    pCmd->AddRef();
                    *ppCmd = pCmd;
                    return S_OK;
                }
            }
        }

        // block until the advise is signalled
        if (WaitForSingleObject(m_evDue, msTimeout) != WAIT_OBJECT_0)
        {
            return E_ABORT;
        }
    }
}

// return a pointer to a command that will be due for a given time.
// Pass in a stream time here. The stream time offset will be passed
// in via the Run method.
// Commands remain queued until invoked or cancelled.
// This method will not block. It will report E_ABORT if there are no
// commands due yet.
//
// returns an AddRef'd object

HRESULT
CCmdQueue::GetCommandDueFor(REFERENCE_TIME rtStream, __out CDeferredCommand **ppCmd)
{
    CAutoLock lock(&m_Lock);

    CRefTime tStream(rtStream);

    // find the earliest stream and presentation time commands
    CDeferredCommand *pStream = NULL;
    POSITION pos = m_listStream.GetHeadPosition();
    if (NULL != pos)
    {
        pStream = m_listStream.GetValid(pos);
    }
    CDeferredCommand *pPresent = NULL;
    pos = m_listPresentation.GetHeadPosition();
    if (NULL != pos)
    {
        pPresent = m_listPresentation.GetValid(pos);
    }

    // is there a presentation time that has passed already
    if (pPresent && CheckTime(pPresent->GetTime(), FALSE))
    {
        pPresent->AddRef();
        *ppCmd = pPresent;
        return S_OK;
    }

    // is there a stream time command due before this stream time
    if (pStream && (pStream->GetTime() <= tStream))
    {
        pStream->AddRef();
        *ppCmd = pStream;
        return S_OK;
    }

    // if we are running, we can map presentation times to
    // stream time. In this case, is there a presentation time command
    // that will be due before this stream time is presented?
    if (m_bRunning && pPresent)
    {

        // this stream time will appear at...
        tStream += m_StreamTimeOffset;

        // due before that?
        if (pPresent->GetTime() <= tStream)
        {
            *ppCmd = pPresent;
            return S_OK;
        }
    }

    // no commands due yet
    return VFW_E_NOT_FOUND;
}
