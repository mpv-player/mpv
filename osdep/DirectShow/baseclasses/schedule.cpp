//------------------------------------------------------------------------------
// File: Schedule.cpp
//
// Desc: DirectShow base classes.
//
// Copyright (c) 1996-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>

// DbgLog values (all on LOG_TIMING):
//
// 2 for schedulting, firing and shunting of events
// 3 for wait delays and wake-up times of event thread
// 4 for details of whats on the list when the thread awakes

/* Construct & destructors */

CAMSchedule::CAMSchedule(HANDLE ev)
    : CBaseObject(TEXT("CAMSchedule"))
    , head(&z, 0)
    , z(0, MAX_TIME)
    , m_dwNextCookie(0)
    , m_dwAdviseCount(0)
    , m_pAdviseCache(0)
    , m_dwCacheCount(0)
    , m_ev(ev)
{
    head.m_dwAdviseCookie = z.m_dwAdviseCookie = 0;
}

CAMSchedule::~CAMSchedule()
{
    m_Serialize.Lock();

    // Delete cache
    CAdvisePacket *p = m_pAdviseCache;
    while (p)
    {
        CAdvisePacket *const p_next = p->m_next;
        delete p;
        p = p_next;
    }

    ASSERT(m_dwAdviseCount == 0);
    // Better to be safe than sorry
    if (m_dwAdviseCount > 0)
    {
        DumpLinkedList();
        while (!head.m_next->IsZ())
        {
            head.DeleteNext();
            --m_dwAdviseCount;
        }
    }

    // If, in the debug version, we assert twice, it means, not only
    // did we have left over advises, but we have also let m_dwAdviseCount
    // get out of sync. with the number of advises actually on the list.
    ASSERT(m_dwAdviseCount == 0);

    m_Serialize.Unlock();
}

/* Public methods */

DWORD CAMSchedule::GetAdviseCount()
{
    // No need to lock, m_dwAdviseCount is 32bits & declared volatile
    return m_dwAdviseCount;
}

REFERENCE_TIME CAMSchedule::GetNextAdviseTime()
{
    CAutoLock lck(&m_Serialize); // Need to stop the linked list from changing
    return head.m_next->m_rtEventTime;
}

DWORD_PTR CAMSchedule::AddAdvisePacket(const REFERENCE_TIME &time1, const REFERENCE_TIME &time2, HANDLE h,
                                       BOOL periodic)
{
    // Since we use MAX_TIME as a sentry, we can't afford to
    // schedule a notification at MAX_TIME
    ASSERT(time1 < MAX_TIME);
    DWORD_PTR Result;
    CAdvisePacket *p;

    m_Serialize.Lock();

    if (m_pAdviseCache)
    {
        p = m_pAdviseCache;
        m_pAdviseCache = p->m_next;
        --m_dwCacheCount;
    }
    else
    {
        p = new CAdvisePacket();
    }
    if (p)
    {
        p->m_rtEventTime = time1;
        p->m_rtPeriod = time2;
        p->m_hNotify = h;
        p->m_bPeriodic = periodic;
        Result = AddAdvisePacket(p);
    }
    else
        Result = 0;

    m_Serialize.Unlock();

    return Result;
}

HRESULT CAMSchedule::Unadvise(DWORD_PTR dwAdviseCookie)
{
    HRESULT hr = S_FALSE;
    CAdvisePacket *p_prev = &head;
    CAdvisePacket *p_n;
    m_Serialize.Lock();
    while (p_n = p_prev->Next()) // The Next() method returns NULL when it hits z
    {
        if (p_n->m_dwAdviseCookie == dwAdviseCookie)
        {
            Delete(p_prev->RemoveNext());
            --m_dwAdviseCount;
            hr = S_OK;
            // Having found one cookie that matches, there should be no more
#ifdef DEBUG
            while (p_n = p_prev->Next())
            {
                ASSERT(p_n->m_dwAdviseCookie != dwAdviseCookie);
                p_prev = p_n;
            }
#endif
            break;
        }
        p_prev = p_n;
    };
    m_Serialize.Unlock();
    return hr;
}

REFERENCE_TIME CAMSchedule::Advise(const REFERENCE_TIME &rtTime)
{
    REFERENCE_TIME rtNextTime;
    CAdvisePacket *pAdvise;

    DbgLog((LOG_TIMING, 2, TEXT("CAMSchedule::Advise( %lu ms )"), ULONG(rtTime / (UNITS / MILLISECONDS))));

    CAutoLock lck(&m_Serialize);

#ifdef DEBUG
    if (DbgCheckModuleLevel(LOG_TIMING, 4))
        DumpLinkedList();
#endif

    //  Note - DON'T cache the difference, it might overflow
    while (rtTime >= (rtNextTime = (pAdvise = head.m_next)->m_rtEventTime) && !pAdvise->IsZ())
    {
        ASSERT(pAdvise->m_dwAdviseCookie); // If this is zero, its the head or the tail!!

        ASSERT(pAdvise->m_hNotify != INVALID_HANDLE_VALUE);

        if (pAdvise->m_bPeriodic == TRUE)
        {
            ReleaseSemaphore(pAdvise->m_hNotify, 1, NULL);
            pAdvise->m_rtEventTime += pAdvise->m_rtPeriod;
            ShuntHead();
        }
        else
        {
            ASSERT(pAdvise->m_bPeriodic == FALSE);
            EXECUTE_ASSERT(SetEvent(pAdvise->m_hNotify));
            --m_dwAdviseCount;
            Delete(head.RemoveNext());
        }
    }

    DbgLog((LOG_TIMING, 3, TEXT("CAMSchedule::Advise() Next time stamp: %lu ms, for advise %lu."),
            DWORD(rtNextTime / (UNITS / MILLISECONDS)), pAdvise->m_dwAdviseCookie));

    return rtNextTime;
}

/* Private methods */

DWORD_PTR CAMSchedule::AddAdvisePacket(__inout CAdvisePacket *pPacket)
{
    ASSERT(pPacket->m_rtEventTime >= 0 && pPacket->m_rtEventTime < MAX_TIME);
    ASSERT(CritCheckIn(&m_Serialize));

    CAdvisePacket *p_prev = &head;
    CAdvisePacket *p_n;

    const DWORD_PTR Result = pPacket->m_dwAdviseCookie = ++m_dwNextCookie;
    // This relies on the fact that z is a sentry with a maximal m_rtEventTime
    for (;; p_prev = p_n)
    {
        p_n = p_prev->m_next;
        if (p_n->m_rtEventTime >= pPacket->m_rtEventTime)
            break;
    }
    p_prev->InsertAfter(pPacket);
    ++m_dwAdviseCount;

    DbgLog((LOG_TIMING, 2, TEXT("Added advise %lu, for thread 0x%02X, scheduled at %lu"), pPacket->m_dwAdviseCookie,
            GetCurrentThreadId(), (pPacket->m_rtEventTime / (UNITS / MILLISECONDS))));

    // If packet added at the head, then clock needs to re-evaluate wait time.
    if (p_prev == &head)
        SetEvent(m_ev);

    return Result;
}

void CAMSchedule::Delete(__inout CAdvisePacket *pPacket)
{
    if (m_dwCacheCount >= dwCacheMax)
        delete pPacket;
    else
    {
        m_Serialize.Lock();
        pPacket->m_next = m_pAdviseCache;
        m_pAdviseCache = pPacket;
        ++m_dwCacheCount;
        m_Serialize.Unlock();
    }
}

// Takes the head of the list & repositions it
void CAMSchedule::ShuntHead()
{
    CAdvisePacket *p_prev = &head;
    CAdvisePacket *p_n;

    m_Serialize.Lock();
    CAdvisePacket *const pPacket = head.m_next;

    // This will catch both an empty list,
    // and if somehow a MAX_TIME time gets into the list
    // (which would also break this method).
    ASSERT(pPacket->m_rtEventTime < MAX_TIME);

    // This relies on the fact that z is a sentry with a maximal m_rtEventTime
    for (;; p_prev = p_n)
    {
        p_n = p_prev->m_next;
        if (p_n->m_rtEventTime > pPacket->m_rtEventTime)
            break;
    }
    // If p_prev == pPacket then we're already in the right place
    if (p_prev != pPacket)
    {
        head.m_next = pPacket->m_next;
        (p_prev->m_next = pPacket)->m_next = p_n;
    }
#ifdef DEBUG
    DbgLog((LOG_TIMING, 2, TEXT("Periodic advise %lu, shunted to %lu"), pPacket->m_dwAdviseCookie,
            (pPacket->m_rtEventTime / (UNITS / MILLISECONDS))));
#endif
    m_Serialize.Unlock();
}

#ifdef DEBUG
void CAMSchedule::DumpLinkedList()
{
    m_Serialize.Lock();
    int i = 0;
    DbgLog((LOG_TIMING, 1, TEXT("CAMSchedule::DumpLinkedList() this = 0x%p"), this));
    for (CAdvisePacket *p = &head; p; p = p->m_next, i++)
    {
        DbgLog((LOG_TIMING, 1, TEXT("Advise List # %lu, Cookie %d,  RefTime %lu"), i, p->m_dwAdviseCookie,
                p->m_rtEventTime / (UNITS / MILLISECONDS)));
    }
    m_Serialize.Unlock();
}
#endif
