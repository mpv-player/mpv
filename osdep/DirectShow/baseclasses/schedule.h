//------------------------------------------------------------------------------
// File: Schedule.h
//
// Desc: DirectShow base classes.
//
// Copyright (c) 1996-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifndef __CAMSchedule__
#define __CAMSchedule__

class CAMSchedule : private CBaseObject
{
  public:
    virtual ~CAMSchedule();
    // ev is the event we should fire if the advise time needs re-evaluating
    CAMSchedule(HANDLE ev);

    DWORD GetAdviseCount();
    REFERENCE_TIME GetNextAdviseTime();

    // We need a method for derived classes to add advise packets, we return the cookie
    DWORD_PTR AddAdvisePacket(const REFERENCE_TIME &time1, const REFERENCE_TIME &time2, HANDLE h, BOOL periodic);
    // And a way to cancel
    HRESULT Unadvise(DWORD_PTR dwAdviseCookie);

    // Tell us the time please, and we'll dispatch the expired events.  We return the time of the next event.
    // NB: The time returned will be "useless" if you start adding extra Advises.  But that's the problem of
    // whoever is using this helper class (typically a clock).
    REFERENCE_TIME Advise(const REFERENCE_TIME &rtTime);

    // Get the event handle which will be set if advise time requires re-evaluation.
    HANDLE GetEvent() const { return m_ev; }

  private:
    // We define the nodes that will be used in our singly linked list
    // of advise packets.  The list is ordered by time, with the
    // elements that will expire first at the front.
    class CAdvisePacket
    {
      public:
        CAdvisePacket() {}

        CAdvisePacket *m_next;
        DWORD_PTR m_dwAdviseCookie;
        REFERENCE_TIME m_rtEventTime; // Time at which event should be set
        REFERENCE_TIME m_rtPeriod;    // Periodic time
        HANDLE m_hNotify;             // Handle to event or semephore
        BOOL m_bPeriodic;             // TRUE => Periodic event

        CAdvisePacket(__inout_opt CAdvisePacket *next, LONGLONG time)
            : m_next(next)
            , m_rtEventTime(time)
        {
        }

        void InsertAfter(__inout CAdvisePacket *p)
        {
            p->m_next = m_next;
            m_next = p;
        }

        int IsZ() const // That is, is it the node that represents the end of the list
        {
            return m_next == 0;
        }

        CAdvisePacket *RemoveNext()
        {
            CAdvisePacket *const next = m_next;
            CAdvisePacket *const new_next = next->m_next;
            m_next = new_next;
            return next;
        }

        void DeleteNext() { delete RemoveNext(); }

        CAdvisePacket *Next() const
        {
            CAdvisePacket *result = m_next;
            if (result->IsZ())
                result = 0;
            return result;
        }

        DWORD_PTR Cookie() const { return m_dwAdviseCookie; }
    };

    // Structure is:
    // head -> elmt1 -> elmt2 -> z -> null
    // So an empty list is:       head -> z -> null
    // Having head & z as links makes insertaion,
    // deletion and shunting much easier.
    CAdvisePacket head, z; // z is both a tail and a sentry

    volatile DWORD_PTR m_dwNextCookie; // Strictly increasing
    volatile DWORD m_dwAdviseCount;    // Number of elements on list

    CCritSec m_Serialize;

    // AddAdvisePacket: adds the packet, returns the cookie (0 if failed)
    DWORD_PTR AddAdvisePacket(__inout CAdvisePacket *pPacket);
    // Event that we should set if the packed added above will be the next to fire.
    const HANDLE m_ev;

    // A Shunt is where we have changed the first element in the
    // list and want it re-evaluating (i.e. repositioned) in
    // the list.
    void ShuntHead();

    // Rather than delete advise packets, we cache them for future use
    CAdvisePacket *m_pAdviseCache;
    DWORD m_dwCacheCount;
    enum
    {
        dwCacheMax = 5
    }; // Don't bother caching more than five

    void Delete(__inout CAdvisePacket *pLink); // This "Delete" will cache the Link

    // Attributes and methods for debugging
  public:
#ifdef DEBUG
    void DumpLinkedList();
#else
    void DumpLinkedList() {}
#endif
};

#endif // __CAMSchedule__
