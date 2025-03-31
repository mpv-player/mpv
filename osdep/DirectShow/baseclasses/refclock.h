//------------------------------------------------------------------------------
// File: RefClock.h
//
// Desc: DirectShow base classes - defines the IReferenceClock interface.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifndef __BASEREFCLOCK__
#define __BASEREFCLOCK__

#include <Schedule.h>

const UINT RESOLUTION = 1;                    /* High resolution timer */
const INT ADVISE_CACHE = 4;                   /* Default cache size */
const LONGLONG MAX_TIME = 0x7FFFFFFFFFFFFFFF; /* Maximum LONGLONG value */

inline LONGLONG WINAPI ConvertToMilliseconds(const REFERENCE_TIME &RT)
{
    /* This converts an arbitrary value representing a reference time
       into a MILLISECONDS value for use in subsequent system calls */

    return (RT / (UNITS / MILLISECONDS));
}

/* This class hierarchy will support an IReferenceClock interface so
   that an audio card (or other externally driven clock) can update the
   system wide clock that everyone uses.

   The interface will be pretty thin with probably just one update method
   This interface has not yet been defined.
 */

/* This abstract base class implements the IReferenceClock
 * interface.  Classes that actually provide clock signals (from
 * whatever source) have to be derived from this class.
 *
 * The abstract class provides implementations for:
 *  CUnknown support
 *      locking support (CCritSec)
 *  client advise code (creates a thread)
 *
 * Question: what can we do about quality?  Change the timer
 * resolution to lower the system load?  Up the priority of the
 * timer thread to force more responsive signals?
 *
 * During class construction we create a worker thread that is destroyed during
 * destuction.  This thread executes a series of WaitForSingleObject calls,
 * waking up when a command is given to the thread or the next wake up point
 * is reached.  The wakeup points are determined by clients making Advise
 * calls.
 *
 * Each advise call defines a point in time when they wish to be notified.  A
 * periodic advise is a series of these such events.  We maintain a list of
 * advise links and calculate when the nearest event notification is due for.
 * We then call WaitForSingleObject with a timeout equal to this time.  The
 * handle we wait on is used by the class to signal that something has changed
 * and that we must reschedule the next event.  This typically happens when
 * someone comes in and asks for an advise link while we are waiting for an
 * event to timeout.
 *
 * While we are modifying the list of advise requests we
 * are protected from interference through a critical section.  Clients are NOT
 * advised through callbacks.  One shot clients have an event set, while
 * periodic clients have a semaphore released for each event notification.  A
 * semaphore allows a client to be kept up to date with the number of events
 * actually triggered and be assured that they can't miss multiple events being
 * set.
 *
 * Keeping track of advises is taken care of by the CAMSchedule class.
 */

class CBaseReferenceClock
    : public CUnknown
    , public IReferenceClock
    , public CCritSec
    , public IReferenceClockTimerControl
{
  protected:
    virtual ~CBaseReferenceClock(); // Don't let me be created on the stack!
  public:
    CBaseReferenceClock(__in_opt LPCTSTR pName, __inout_opt LPUNKNOWN pUnk, __inout HRESULT *phr,
                        __inout_opt CAMSchedule *pSched = 0);

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void **ppv);

    DECLARE_IUNKNOWN

    /* IReferenceClock methods */
    // Derived classes must implement GetPrivateTime().  All our GetTime
    // does is call GetPrivateTime and then check so that time does not
    // go backwards.  A return code of S_FALSE implies that the internal
    // clock has gone backwards and GetTime time has halted until internal
    // time has caught up. (Don't know if this will be much use to folk,
    // but it seems odd not to use the return code for something useful.)
    STDMETHODIMP GetTime(__out REFERENCE_TIME *pTime);
    // When this is called, it sets m_rtLastGotTime to the time it returns.

    /* Provide standard mechanisms for scheduling events */

    /* Ask for an async notification that a time has elapsed */
    STDMETHODIMP AdviseTime(REFERENCE_TIME baseTime,         // base reference time
                            REFERENCE_TIME streamTime,       // stream offset time
                            HEVENT hEvent,                   // advise via this event
                            __out DWORD_PTR *pdwAdviseCookie // where your cookie goes
    );

    /* Ask for an asynchronous periodic notification that a time has elapsed */
    STDMETHODIMP AdvisePeriodic(REFERENCE_TIME StartTime,        // starting at this time
                                REFERENCE_TIME PeriodTime,       // time between notifications
                                HSEMAPHORE hSemaphore,           // advise via a semaphore
                                __out DWORD_PTR *pdwAdviseCookie // where your cookie goes
    );

    /* Cancel a request for notification(s) - if the notification was
     * a one shot timer then this function doesn't need to be called
     * as the advise is automatically cancelled, however it does no
     * harm to explicitly cancel a one-shot advise.  It is REQUIRED that
     * clients call Unadvise to clear a Periodic advise setting.
     */

    STDMETHODIMP Unadvise(DWORD_PTR dwAdviseCookie);

    /* Methods for the benefit of derived classes or outer objects */

    // GetPrivateTime() is the REAL clock.  GetTime is just a cover for
    // it.  Derived classes will probably override this method but not
    // GetTime() itself.
    // The important point about GetPrivateTime() is it's allowed to go
    // backwards.  Our GetTime() will keep returning the LastGotTime
    // until GetPrivateTime() catches up.
    virtual REFERENCE_TIME GetPrivateTime();

    /* Provide a method for correcting drift */
    STDMETHODIMP SetTimeDelta(const REFERENCE_TIME &TimeDelta);

    CAMSchedule *GetSchedule() const { return m_pSchedule; }

    // IReferenceClockTimerControl methods
    //
    // Setting a default of 0 disables the default of 1ms
    STDMETHODIMP SetDefaultTimerResolution(REFERENCE_TIME timerResolution // in 100ns
    );
    STDMETHODIMP GetDefaultTimerResolution(__out REFERENCE_TIME *pTimerResolution // in 100ns
    );

  private:
    REFERENCE_TIME m_rtPrivateTime; // Current best estimate of time
    DWORD m_dwPrevSystemTime;       // Last vaule we got from timeGetTime
    REFERENCE_TIME m_rtLastGotTime; // Last time returned by GetTime
    REFERENCE_TIME m_rtNextAdvise;  // Time of next advise
    UINT m_TimerResolution;

#ifdef PERF
    int m_idGetSystemTime;
#endif

    // Thread stuff
  public:
    void TriggerThread() // Wakes thread up.  Need to do this if
    {                    // time to next advise needs reevaluating.
        EXECUTE_ASSERT(SetEvent(m_pSchedule->GetEvent()));
    }

  private:
    BOOL m_bAbort;    // Flag used for thread shutdown
    HANDLE m_hThread; // Thread handle

    HRESULT AdviseThread();                                   // Method in which the advise thread runs
    static DWORD __stdcall AdviseThreadFunction(__in LPVOID); // Function used to get there

  protected:
    CAMSchedule *m_pSchedule;

    void Restart(IN REFERENCE_TIME rtMinTime = 0I64);
};

#endif
