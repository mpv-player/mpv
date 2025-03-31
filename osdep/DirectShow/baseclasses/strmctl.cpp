//------------------------------------------------------------------------------
// File: StrmCtl.cpp
//
// Desc: DirectShow base classes.
//
// Copyright (c) 1996-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>
#include <strmctl.h>

CBaseStreamControl::CBaseStreamControl(__inout HRESULT *phr)
    : m_StreamState(STREAM_FLOWING)
    , m_StreamStateOnStop(STREAM_FLOWING) // means no pending stop
    , m_tStartTime(MAX_TIME)
    , m_tStopTime(MAX_TIME)
    , m_StreamEvent(FALSE, phr)
    , m_dwStartCookie(0)
    , m_dwStopCookie(0)
    , m_pRefClock(NULL)
    , m_FilterState(State_Stopped)
    , m_bIsFlushing(FALSE)
    , m_bStopSendExtra(FALSE)
{
}

CBaseStreamControl::~CBaseStreamControl()
{
    // Make sure we release the clock.
    SetSyncSource(NULL);
    return;
}

STDMETHODIMP CBaseStreamControl::StopAt(const REFERENCE_TIME *ptStop, BOOL bSendExtra, DWORD dwCookie)
{
    CAutoLock lck(&m_CritSec);
    m_bStopSendExtra = FALSE; // reset
    m_bStopExtraSent = FALSE;
    if (ptStop)
    {
        if (*ptStop == MAX_TIME)
        {
            DbgLog((LOG_TRACE, 2, TEXT("StopAt: Cancel stop")));
            CancelStop();
            // If there's now a command to start in the future, we assume
            // they want to be stopped when the graph is first run
            if (m_FilterState == State_Stopped && m_tStartTime < MAX_TIME)
            {
                m_StreamState = STREAM_DISCARDING;
                DbgLog((LOG_TRACE, 2, TEXT("graph will begin by DISCARDING")));
            }
            return NOERROR;
        }
        DbgLog((LOG_TRACE, 2, TEXT("StopAt: %dms extra=%d"), (int)(*ptStop / 10000), bSendExtra));
        // if the first command is to stop in the future, then we assume they
        // want to be started when the graph is first run
        if (m_FilterState == State_Stopped && m_tStartTime > *ptStop)
        {
            m_StreamState = STREAM_FLOWING;
            DbgLog((LOG_TRACE, 2, TEXT("graph will begin by FLOWING")));
        }
        m_bStopSendExtra = bSendExtra;
        m_tStopTime = *ptStop;
        m_dwStopCookie = dwCookie;
        m_StreamStateOnStop = STREAM_DISCARDING;
    }
    else
    {
        DbgLog((LOG_TRACE, 2, TEXT("StopAt: now")));
        // sending an extra frame when told to stop now would mess people up
        m_bStopSendExtra = FALSE;
        m_tStopTime = MAX_TIME;
        m_dwStopCookie = 0;
        m_StreamState = STREAM_DISCARDING;
        m_StreamStateOnStop = STREAM_FLOWING; // no pending stop
    }
    // we might change our mind what to do with a sample we're blocking
    m_StreamEvent.Set();
    return NOERROR;
}

STDMETHODIMP CBaseStreamControl::StartAt(const REFERENCE_TIME *ptStart, DWORD dwCookie)
{
    CAutoLock lck(&m_CritSec);
    if (ptStart)
    {
        if (*ptStart == MAX_TIME)
        {
            DbgLog((LOG_TRACE, 2, TEXT("StartAt: Cancel start")));
            CancelStart();
            // If there's now a command to stop in the future, we assume
            // they want to be started when the graph is first run
            if (m_FilterState == State_Stopped && m_tStopTime < MAX_TIME)
            {
                DbgLog((LOG_TRACE, 2, TEXT("graph will begin by FLOWING")));
                m_StreamState = STREAM_FLOWING;
            }
            return NOERROR;
        }
        DbgLog((LOG_TRACE, 2, TEXT("StartAt: %dms"), (int)(*ptStart / 10000)));
        // if the first command is to start in the future, then we assume they
        // want to be stopped when the graph is first run
        if (m_FilterState == State_Stopped && m_tStopTime >= *ptStart)
        {
            DbgLog((LOG_TRACE, 2, TEXT("graph will begin by DISCARDING")));
            m_StreamState = STREAM_DISCARDING;
        }
        m_tStartTime = *ptStart;
        m_dwStartCookie = dwCookie;
        // if (m_tStopTime == m_tStartTime) CancelStop();
    }
    else
    {
        DbgLog((LOG_TRACE, 2, TEXT("StartAt: now")));
        m_tStartTime = MAX_TIME;
        m_dwStartCookie = 0;
        m_StreamState = STREAM_FLOWING;
    }
    // we might change our mind what to do with a sample we're blocking
    m_StreamEvent.Set();
    return NOERROR;
}

//  Retrieve information about current settings
STDMETHODIMP CBaseStreamControl::GetInfo(__out AM_STREAM_INFO *pInfo)
{
    if (pInfo == NULL)
        return E_POINTER;

    pInfo->tStart = m_tStartTime;
    pInfo->tStop = m_tStopTime;
    pInfo->dwStartCookie = m_dwStartCookie;
    pInfo->dwStopCookie = m_dwStopCookie;
    pInfo->dwFlags = m_bStopSendExtra ? AM_STREAM_INFO_STOP_SEND_EXTRA : 0;
    pInfo->dwFlags |= m_tStartTime == MAX_TIME ? 0 : AM_STREAM_INFO_START_DEFINED;
    pInfo->dwFlags |= m_tStopTime == MAX_TIME ? 0 : AM_STREAM_INFO_STOP_DEFINED;
    switch (m_StreamState)
    {
    default: DbgBreak("Invalid stream state");
    case STREAM_FLOWING: break;
    case STREAM_DISCARDING: pInfo->dwFlags |= AM_STREAM_INFO_DISCARDING; break;
    }
    return S_OK;
}

void CBaseStreamControl::ExecuteStop()
{
    ASSERT(CritCheckIn(&m_CritSec));
    m_StreamState = m_StreamStateOnStop;
    if (m_dwStopCookie && m_pSink)
    {
        DbgLog((LOG_TRACE, 2, TEXT("*sending EC_STREAM_CONTROL_STOPPED (%d)"), m_dwStopCookie));
        m_pSink->Notify(EC_STREAM_CONTROL_STOPPED, (LONG_PTR)this, m_dwStopCookie);
    }
    CancelStop(); // This will do the tidy up
}

void CBaseStreamControl::ExecuteStart()
{
    ASSERT(CritCheckIn(&m_CritSec));
    m_StreamState = STREAM_FLOWING;
    if (m_dwStartCookie)
    {
        DbgLog((LOG_TRACE, 2, TEXT("*sending EC_STREAM_CONTROL_STARTED (%d)"), m_dwStartCookie));
        m_pSink->Notify(EC_STREAM_CONTROL_STARTED, (LONG_PTR)this, m_dwStartCookie);
    }
    CancelStart(); // This will do the tidy up
}

void CBaseStreamControl::CancelStop()
{
    ASSERT(CritCheckIn(&m_CritSec));
    m_tStopTime = MAX_TIME;
    m_dwStopCookie = 0;
    m_StreamStateOnStop = STREAM_FLOWING;
}

void CBaseStreamControl::CancelStart()
{
    ASSERT(CritCheckIn(&m_CritSec));
    m_tStartTime = MAX_TIME;
    m_dwStartCookie = 0;
}

// This guy will return one of the three StreamControlState's.  Here's what the caller
// should do for each one:
//
// STREAM_FLOWING:      Proceed as usual (render or pass the sample on)
// STREAM_DISCARDING:   Calculate the time 'til *pSampleStart and wait that long
//                      for the event handle (GetStreamEventHandle()).  If the
//                      wait expires, throw the sample away.  If the event
//			fires, call me back, I've changed my mind.
//			I use pSampleStart (not Stop) so that live sources don't
// 			block for the duration of their samples, since the clock
//			will always read approximately pSampleStart when called

// All through this code, you'll notice the following rules:
// - When start and stop time are the same, it's as if start was first
// - An event is considered inside the sample when it's >= sample start time
//   but < sample stop time
// - if any part of the sample is supposed to be sent, we'll send the whole
//   thing since we don't break it into smaller pieces
// - If we skip over a start or stop without doing it, we still signal the event
//   and reset ourselves in case somebody's waiting for the event, and to make
//   sure we notice that the event is past and should be forgotten
// Here are the 19 cases that have to be handled (x=start o=stop <-->=sample):
//
// 1.	xo<-->		start then stop
// 2.	ox<-->		stop then start
// 3.	 x<o->		start
// 4.	 o<x->		stop then start
// 5.	 x<-->o		start
// 6.	 o<-->x		stop
// 7.	  <x->o		start
// 8.	  <o->x		no change
// 9.	  <xo>		start
// 10.	  <ox>		stop then start
// 11.	  <-->xo	no change
// 12.	  <-->ox	no change
// 13.	 x<-->		start
// 14.    <x->		start
// 15.    <-->x		no change
// 16.   o<-->		stop
// 17.	  <o->		no change
// 18.	  <-->o		no change
// 19.    <-->		no change

enum CBaseStreamControl::StreamControlState CBaseStreamControl::CheckSampleTimes(
    __in const REFERENCE_TIME *pSampleStart, __in const REFERENCE_TIME *pSampleStop)
{
    CAutoLock lck(&m_CritSec);

    ASSERT(!m_bIsFlushing);
    ASSERT(pSampleStart && pSampleStop);

    // Don't ask me how I came up with the code below to handle all 19 cases
    // - DannyMi

    if (m_tStopTime >= *pSampleStart)
    {
        if (m_tStartTime >= *pSampleStop)
            return m_StreamState; // cases  8 11 12 15 17 18 19
        if (m_tStopTime < m_tStartTime)
            ExecuteStop(); // case 10
        ExecuteStart();    // cases 3 5 7 9 13 14
        return m_StreamState;
    }

    if (m_tStartTime >= *pSampleStop)
    {
        ExecuteStop(); // cases 6 16
        return m_StreamState;
    }

    if (m_tStartTime <= m_tStopTime)
    {
        ExecuteStart();
        ExecuteStop();
        return m_StreamState; // case 1
    }
    else
    {
        ExecuteStop();
        ExecuteStart();
        return m_StreamState; // cases 2 4
    }
}

enum CBaseStreamControl::StreamControlState CBaseStreamControl::CheckStreamState(IMediaSample *pSample)
{

    REFERENCE_TIME rtBufferStart, rtBufferStop;
    const BOOL bNoBufferTimes = pSample == NULL || FAILED(pSample->GetTime(&rtBufferStart, &rtBufferStop));

    StreamControlState state;
    LONG lWait;

    do
    {
        // something has to break out of the blocking
        if (m_bIsFlushing || m_FilterState == State_Stopped)
            return STREAM_DISCARDING;

        if (bNoBufferTimes)
        {
            //  Can't do anything until we get a time stamp
            state = m_StreamState;
            break;
        }
        else
        {
            state = CheckSampleTimes(&rtBufferStart, &rtBufferStop);
            if (state == STREAM_FLOWING)
                break;

            // we aren't supposed to send this, but we've been
            // told to send one more than we were supposed to
            // (and the stop isn't still pending and we're streaming)
            if (m_bStopSendExtra && !m_bStopExtraSent && m_tStopTime == MAX_TIME && m_FilterState != State_Stopped)
            {
                m_bStopExtraSent = TRUE;
                DbgLog((LOG_TRACE, 2, TEXT("%d sending an EXTRA frame"), m_dwStopCookie));
                state = STREAM_FLOWING;
                break;
            }
        }

        // We're in discarding mode

        // If we've no clock, discard as fast as we can
        if (!m_pRefClock)
        {
            break;

            // If we're paused, we can't discard in a timely manner because
            // there's no such thing as stream times.  We must block until
            // we run or stop, or we'll end up throwing the whole stream away
            // as quickly as possible
        }
        else if (m_FilterState == State_Paused)
        {
            lWait = INFINITE;
        }
        else
        {
            // wait until it's time for the sample until we say "discard"
            // ("discard in a timely fashion")
            REFERENCE_TIME rtNow;
            EXECUTE_ASSERT(SUCCEEDED(m_pRefClock->GetTime(&rtNow)));
            rtNow -= m_tRunStart;                          // Into relative ref-time
            lWait = LONG((rtBufferStart - rtNow) / 10000); // 100ns -> ms
            if (lWait < 10)
                break; // Not worth waiting - discard early
        }

    } while (WaitForSingleObject(GetStreamEventHandle(), lWait) != WAIT_TIMEOUT);

    return state;
}

void CBaseStreamControl::NotifyFilterState(FILTER_STATE new_state, REFERENCE_TIME tStart)
{
    CAutoLock lck(&m_CritSec);

    // or we will get confused
    if (m_FilterState == new_state)
        return;

    switch (new_state)
    {
    case State_Stopped:

        DbgLog((LOG_TRACE, 2, TEXT("Filter is STOPPED")));

        // execute any pending starts and stops in the right order,
        // to make sure all notifications get sent, and we end up
        // in the right state to begin next time (??? why not?)

        if (m_tStartTime != MAX_TIME && m_tStopTime == MAX_TIME)
        {
            ExecuteStart();
        }
        else if (m_tStopTime != MAX_TIME && m_tStartTime == MAX_TIME)
        {
            ExecuteStop();
        }
        else if (m_tStopTime != MAX_TIME && m_tStartTime != MAX_TIME)
        {
            if (m_tStartTime <= m_tStopTime)
            {
                ExecuteStart();
                ExecuteStop();
            }
            else
            {
                ExecuteStop();
                ExecuteStart();
            }
        }
        // always start off flowing when the graph starts streaming
        // unless told otherwise
        m_StreamState = STREAM_FLOWING;
        m_FilterState = new_state;
        break;

    case State_Running:

        DbgLog((LOG_TRACE, 2, TEXT("Filter is RUNNING")));

        m_tRunStart = tStart;
        // fall-through

    default: // case State_Paused:
        m_FilterState = new_state;
    }
    // unblock!
    m_StreamEvent.Set();
}

void CBaseStreamControl::Flushing(BOOL bInProgress)
{
    CAutoLock lck(&m_CritSec);
    m_bIsFlushing = bInProgress;
    m_StreamEvent.Set();
}
