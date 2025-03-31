//------------------------------------------------------------------------------
// File: RefClock.cpp
//
// Desc: DirectShow base classes - implements the IReferenceClock interface.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>
#include <limits.h>

#ifdef DXMPERF
#include "dxmperf.h"
#endif // DXMPERF

// 'this' used in constructor list
#pragma warning(disable : 4355)

STDMETHODIMP CBaseReferenceClock::NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv)
{
    HRESULT hr;

    if (riid == IID_IReferenceClock)
    {
        hr = GetInterface((IReferenceClock *)this, ppv);
    }
    else if (riid == IID_IReferenceClockTimerControl)
    {
        hr = GetInterface((IReferenceClockTimerControl *)this, ppv);
    }
    else
    {
        hr = CUnknown::NonDelegatingQueryInterface(riid, ppv);
    }
    return hr;
}

CBaseReferenceClock::~CBaseReferenceClock()
{
#ifdef DXMPERF
    PERFLOG_DTOR(L"CBaseReferenceClock", (IReferenceClock *)this);
#endif // DXMPERF

    if (m_TimerResolution)
        timeEndPeriod(m_TimerResolution);

    if (m_pSchedule)
    {
        m_pSchedule->DumpLinkedList();
    }

    if (m_hThread)
    {
        m_bAbort = TRUE;
        TriggerThread();
        WaitForSingleObject(m_hThread, INFINITE);
        EXECUTE_ASSERT(CloseHandle(m_hThread));
        m_hThread = 0;
        EXECUTE_ASSERT(CloseHandle(m_pSchedule->GetEvent()));
        delete m_pSchedule;
    }
}

// A derived class may supply a hThreadEvent if it has its own thread that will take care
// of calling the schedulers Advise method.  (Refere to CBaseReferenceClock::AdviseThread()
// to see what such a thread has to do.)
CBaseReferenceClock::CBaseReferenceClock(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN pUnk, __inout HRESULT *phr,
                                         __inout_opt CAMSchedule *pShed)
    : CUnknown(pName, pUnk)
    , m_rtLastGotTime(0)
    , m_TimerResolution(0)
    , m_bAbort(FALSE)
    , m_pSchedule(pShed ? pShed : new CAMSchedule(CreateEvent(NULL, FALSE, FALSE, NULL)))
    , m_hThread(0)
{

#ifdef DXMPERF
    PERFLOG_CTOR(pName ? pName : L"CBaseReferenceClock", (IReferenceClock *)this);
#endif // DXMPERF

    ASSERT(m_pSchedule);
    if (!m_pSchedule)
    {
        *phr = E_OUTOFMEMORY;
    }
    else
    {
        // Set up the highest resolution timer we can manage
        TIMECAPS tc;
        m_TimerResolution = (TIMERR_NOERROR == timeGetDevCaps(&tc, sizeof(tc))) ? tc.wPeriodMin : 1;

        timeBeginPeriod(m_TimerResolution);

        /* Initialise our system times - the derived clock should set the right values */
        m_dwPrevSystemTime = timeGetTime();
        m_rtPrivateTime = (UNITS / MILLISECONDS) * m_dwPrevSystemTime;

#ifdef PERF
        m_idGetSystemTime = MSR_REGISTER(TEXT("CBaseReferenceClock::GetTime"));
#endif

        if (!pShed)
        {
            DWORD ThreadID;
            m_hThread = ::CreateThread(NULL,                 // Security attributes
                                       (DWORD)0,             // Initial stack size
                                       AdviseThreadFunction, // Thread start address
                                       (LPVOID)this,         // Thread parameter
                                       (DWORD)0,             // Creation flags
                                       &ThreadID);           // Thread identifier

            if (m_hThread)
            {
                SetThreadPriority(m_hThread, THREAD_PRIORITY_TIME_CRITICAL);
            }
            else
            {
                *phr = E_FAIL;
                EXECUTE_ASSERT(CloseHandle(m_pSchedule->GetEvent()));
                delete m_pSchedule;
                m_pSchedule = NULL;
            }
        }
    }
}

void CBaseReferenceClock::Restart(IN REFERENCE_TIME rtMinTime)
{
    Lock();
    m_rtLastGotTime = rtMinTime;
    Unlock();
}

STDMETHODIMP CBaseReferenceClock::GetTime(__out REFERENCE_TIME *pTime)
{
    HRESULT hr;
    if (pTime)
    {
        REFERENCE_TIME rtNow;
        Lock();
        rtNow = GetPrivateTime();
        if (rtNow > m_rtLastGotTime)
        {
            m_rtLastGotTime = rtNow;
            hr = S_OK;
        }
        else
        {
            hr = S_FALSE;
        }
        *pTime = m_rtLastGotTime;
        Unlock();
        MSR_INTEGER(m_idGetSystemTime, LONG((*pTime) / (UNITS / MILLISECONDS)));

#ifdef DXMPERF
        PERFLOG_GETTIME((IReferenceClock *)this, *pTime);
#endif // DXMPERF
    }
    else
        hr = E_POINTER;

    return hr;
}

/* Ask for an async notification that a time has elapsed */

STDMETHODIMP CBaseReferenceClock::AdviseTime(REFERENCE_TIME baseTime,          // base reference time
                                             REFERENCE_TIME streamTime,        // stream offset time
                                             HEVENT hEvent,                    // advise via this event
                                             __out DWORD_PTR *pdwAdviseCookie) // where your cookie goes
{
    CheckPointer(pdwAdviseCookie, E_POINTER);
    *pdwAdviseCookie = 0;

    // Check that the event is not already set
    ASSERT(WAIT_TIMEOUT == WaitForSingleObject(HANDLE(hEvent), 0));

    HRESULT hr;

    const REFERENCE_TIME lRefTime = baseTime + streamTime;
    if (lRefTime <= 0 || lRefTime == MAX_TIME)
    {
        hr = E_INVALIDARG;
    }
    else
    {
        *pdwAdviseCookie = m_pSchedule->AddAdvisePacket(lRefTime, 0, HANDLE(hEvent), FALSE);
        hr = *pdwAdviseCookie ? NOERROR : E_OUTOFMEMORY;
    }
    return hr;
}

/* Ask for an asynchronous periodic notification that a time has elapsed */

STDMETHODIMP CBaseReferenceClock::AdvisePeriodic(REFERENCE_TIME StartTime,         // starting at this time
                                                 REFERENCE_TIME PeriodTime,        // time between notifications
                                                 HSEMAPHORE hSemaphore,            // advise via a semaphore
                                                 __out DWORD_PTR *pdwAdviseCookie) // where your cookie goes
{
    CheckPointer(pdwAdviseCookie, E_POINTER);
    *pdwAdviseCookie = 0;

    HRESULT hr;
    if (StartTime > 0 && PeriodTime > 0 && StartTime != MAX_TIME)
    {
        *pdwAdviseCookie = m_pSchedule->AddAdvisePacket(StartTime, PeriodTime, HANDLE(hSemaphore), TRUE);
        hr = *pdwAdviseCookie ? NOERROR : E_OUTOFMEMORY;
    }
    else
        hr = E_INVALIDARG;

    return hr;
}

STDMETHODIMP CBaseReferenceClock::Unadvise(DWORD_PTR dwAdviseCookie)
{
    return m_pSchedule->Unadvise(dwAdviseCookie);
}

REFERENCE_TIME CBaseReferenceClock::GetPrivateTime()
{
    CAutoLock cObjectLock(this);

    /* If the clock has wrapped then the current time will be less than
     * the last time we were notified so add on the extra milliseconds
     *
     * The time period is long enough so that the likelihood of
     * successive calls spanning the clock cycle is not considered.
     */

    DWORD dwTime = timeGetTime();
    {
        m_rtPrivateTime += Int32x32To64(UNITS / MILLISECONDS, (DWORD)(dwTime - m_dwPrevSystemTime));
        m_dwPrevSystemTime = dwTime;
    }

    return m_rtPrivateTime;
}

/* Adjust the current time by the input value.  This allows an
   external time source to work out some of the latency of the clock
   system and adjust the "current" time accordingly.  The intent is
   that the time returned to the user is synchronised to a clock
   source and allows drift to be catered for.

   For example: if the clock source detects a drift it can pass a delta
   to the current time rather than having to set an explicit time.
*/

STDMETHODIMP CBaseReferenceClock::SetTimeDelta(const REFERENCE_TIME &TimeDelta)
{
#ifdef DEBUG

    // Just break if passed an improper time delta value
    LONGLONG llDelta = TimeDelta > 0 ? TimeDelta : -TimeDelta;
    if (llDelta > UNITS * 1000)
    {
        DbgLog((LOG_TRACE, 0, TEXT("Bad Time Delta")));
        // DebugBreak();
    }

    // We're going to calculate a "severity" for the time change. Max -1
    // min 8.  We'll then use this as the debug logging level for a
    // debug log message.
    const LONG usDelta = LONG(TimeDelta / 10); // Delta in micro-secs

    DWORD delta = abs(usDelta); // varying delta
    // Severity == 8 - ceil(log<base 8>(abs( micro-secs delta)))
    int Severity = 8;
    while (delta > 0)
    {
        delta >>= 3; // div 8
        Severity--;
    }

    // Sev == 0 => > 2 second delta!
    DbgLog((LOG_TIMING, Severity < 0 ? 0 : Severity,
            TEXT("Sev %2i: CSystemClock::SetTimeDelta(%8ld us) %lu -> %lu ms."), Severity, usDelta,
            DWORD(ConvertToMilliseconds(m_rtPrivateTime)), DWORD(ConvertToMilliseconds(TimeDelta + m_rtPrivateTime))));

// Don't want the DbgBreak to fire when running stress on debug-builds.
#ifdef BREAK_ON_SEVERE_TIME_DELTA
    if (Severity < 0)
        DbgBreakPoint(TEXT("SetTimeDelta > 16 seconds!"), TEXT(__FILE__), __LINE__);
#endif

#endif

    CAutoLock cObjectLock(this);
    m_rtPrivateTime += TimeDelta;
    // If time goes forwards, and we have advises, then we need to
    // trigger the thread so that it can re-evaluate its wait time.
    // Since we don't want the cost of the thread switches if the change
    // is really small, only do it if clock goes forward by more than
    // 0.5 millisecond.  If the time goes backwards, the thread will
    // wake up "early" (relativly speaking) and will re-evaluate at
    // that time.
    if (TimeDelta > 5000 && m_pSchedule->GetAdviseCount() > 0)
        TriggerThread();
    return NOERROR;
}

// Thread stuff

DWORD __stdcall CBaseReferenceClock::AdviseThreadFunction(__in LPVOID p)
{
    return DWORD(reinterpret_cast<CBaseReferenceClock *>(p)->AdviseThread());
}

HRESULT CBaseReferenceClock::AdviseThread()
{
    DWORD dwWait = INFINITE;

    // The first thing we do is wait until something interesting happens
    // (meaning a first advise or shutdown).  This prevents us calling
    // GetPrivateTime immediately which is goodness as that is a virtual
    // routine and the derived class may not yet be constructed.  (This
    // thread is created in the base class constructor.)

    while (!m_bAbort)
    {
        // Wait for an interesting event to happen
        DbgLog((LOG_TIMING, 3, TEXT("CBaseRefClock::AdviseThread() Delay: %lu ms"), dwWait));
        WaitForSingleObject(m_pSchedule->GetEvent(), dwWait);
        if (m_bAbort)
            break;

        // There are several reasons why we need to work from the internal
        // time, mainly to do with what happens when time goes backwards.
        // Mainly, it stop us looping madly if an event is just about to
        // expire when the clock goes backward (i.e. GetTime stop for a
        // while).
        const REFERENCE_TIME rtNow = GetPrivateTime();

        DbgLog((LOG_TIMING, 3, TEXT("CBaseRefClock::AdviseThread() Woke at = %lu ms"), ConvertToMilliseconds(rtNow)));

        // We must add in a millisecond, since this is the resolution of our
        // WaitForSingleObject timer.  Failure to do so will cause us to loop
        // franticly for (approx) 1 a millisecond.
        m_rtNextAdvise = m_pSchedule->Advise(10000 + rtNow);
        LONGLONG llWait = m_rtNextAdvise - rtNow;

        ASSERT(llWait > 0);

        llWait = ConvertToMilliseconds(llWait);
        // DON'T replace this with a max!! (The type's of these things is VERY important)
        dwWait = (llWait > REFERENCE_TIME(UINT_MAX)) ? UINT_MAX : DWORD(llWait);
    };
    return NOERROR;
}

HRESULT CBaseReferenceClock::SetDefaultTimerResolution(REFERENCE_TIME timerResolution // in 100ns
)
{
    CAutoLock cObjectLock(this);
    if (0 == timerResolution)
    {
        if (m_TimerResolution)
        {
            timeEndPeriod(m_TimerResolution);
            m_TimerResolution = 0;
        }
    }
    else
    {
        TIMECAPS tc;
        DWORD dwMinResolution = (TIMERR_NOERROR == timeGetDevCaps(&tc, sizeof(tc))) ? tc.wPeriodMin : 1;
        DWORD dwResolution = max(dwMinResolution, DWORD(timerResolution / 10000));
        if (dwResolution != m_TimerResolution)
        {
            timeEndPeriod(m_TimerResolution);
            m_TimerResolution = dwResolution;
            timeBeginPeriod(m_TimerResolution);
        }
    }
    return S_OK;
}

HRESULT CBaseReferenceClock::GetDefaultTimerResolution(__out REFERENCE_TIME *pTimerResolution // in 100ns
)
{
    if (!pTimerResolution)
    {
        return E_POINTER;
    }
    CAutoLock cObjectLock(this);
    *pTimerResolution = m_TimerResolution * 10000;
    return S_OK;
}
