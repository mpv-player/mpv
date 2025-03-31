//------------------------------------------------------------------------------
// File: AMExtra.cpp
//
// Desc: DirectShow base classes - implements CRenderedInputPin class.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>  // DirectShow base class definitions
#include <mmsystem.h> // Needed for definition of timeGetTime
#include <limits.h>   // Standard data type limit definitions
#include <measure.h>  // Used for time critical log functions

#include "amextra.h"

#pragma warning(disable : 4355)

//  Implements CRenderedInputPin class

CRenderedInputPin::CRenderedInputPin(__in_opt LPCTSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock,
                                     __inout HRESULT *phr, __in_opt LPCWSTR pName)
    : CBaseInputPin(pObjectName, pFilter, pLock, phr, pName)
    , m_bAtEndOfStream(FALSE)
    , m_bCompleteNotified(FALSE)
{
}
#ifdef UNICODE
CRenderedInputPin::CRenderedInputPin(__in_opt LPCSTR pObjectName, __in CBaseFilter *pFilter, __in CCritSec *pLock,
                                     __inout HRESULT *phr, __in_opt LPCWSTR pName)
    : CBaseInputPin(pObjectName, pFilter, pLock, phr, pName)
    , m_bAtEndOfStream(FALSE)
    , m_bCompleteNotified(FALSE)
{
}
#endif

// Flush end of stream condition - caller should do any
// necessary stream level locking before calling this

STDMETHODIMP CRenderedInputPin::EndOfStream()
{
    HRESULT hr = CheckStreaming();

    //  Do EC_COMPLETE handling for rendered pins
    if (S_OK == hr && !m_bAtEndOfStream)
    {
        m_bAtEndOfStream = TRUE;
        FILTER_STATE fs;
        EXECUTE_ASSERT(SUCCEEDED(m_pFilter->GetState(0, &fs)));
        if (fs == State_Running)
        {
            DoCompleteHandling();
        }
    }
    return hr;
}

// Called to complete the flush

STDMETHODIMP CRenderedInputPin::EndFlush()
{
    CAutoLock lck(m_pLock);

    // Clean up renderer state
    m_bAtEndOfStream = FALSE;
    m_bCompleteNotified = FALSE;

    return CBaseInputPin::EndFlush();
}

// Notify of Run() from filter

HRESULT CRenderedInputPin::Run(REFERENCE_TIME tStart)
{
    UNREFERENCED_PARAMETER(tStart);
    m_bCompleteNotified = FALSE;
    if (m_bAtEndOfStream)
    {
        DoCompleteHandling();
    }
    return S_OK;
}

//  Clear status on going into paused state

HRESULT CRenderedInputPin::Active()
{
    m_bAtEndOfStream = FALSE;
    m_bCompleteNotified = FALSE;
    return CBaseInputPin::Active();
}

//  Do stuff to deliver end of stream

void CRenderedInputPin::DoCompleteHandling()
{
    ASSERT(m_bAtEndOfStream);
    if (!m_bCompleteNotified)
    {
        m_bCompleteNotified = TRUE;
        m_pFilter->NotifyEvent(EC_COMPLETE, S_OK, (LONG_PTR)(IBaseFilter *)m_pFilter);
    }
}
