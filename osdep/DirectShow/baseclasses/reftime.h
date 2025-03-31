//------------------------------------------------------------------------------
// File: RefTime.h
//
// Desc: DirectShow base classes - defines CRefTime, a class that manages
//       reference times.
//
// Copyright (c) 1992-2001 Microsoft Corporation. All rights reserved.
//------------------------------------------------------------------------------

//
// CRefTime
//
// Manage reference times.
// Shares same data layout as REFERENCE_TIME, but adds some (nonvirtual)
// functions providing simple comparison, conversion and arithmetic.
//
// A reference time (at the moment) is a unit of seconds represented in
// 100ns units as is used in the Win32 FILETIME structure. BUT the time
// a REFERENCE_TIME represents is NOT the time elapsed since 1/1/1601 it
// will either be stream time or reference time depending upon context
//
// This class provides simple arithmetic operations on reference times
//
// keep non-virtual otherwise the data layout will not be the same as
// REFERENCE_TIME

// -----
// note that you are safe to cast a CRefTime* to a REFERENCE_TIME*, but
// you will need to do so explicitly
// -----

#ifndef __REFTIME__
#define __REFTIME__

const LONGLONG MILLISECONDS = (1000);       // 10 ^ 3
const LONGLONG NANOSECONDS = (1000000000);  // 10 ^ 9
const LONGLONG UNITS = (NANOSECONDS / 100); // 10 ^ 7

/*  Unfortunately an inline function here generates a call to __allmul
    - even for constants!
*/
#define MILLISECONDS_TO_100NS_UNITS(lMs) Int32x32To64((lMs), (UNITS / MILLISECONDS))

class CRefTime
{
  public:
    // *MUST* be the only data member so that this class is exactly
    // equivalent to a REFERENCE_TIME.
    // Also, must be *no virtual functions*

    REFERENCE_TIME m_time;

    inline CRefTime()
    {
        // default to 0 time
        m_time = 0;
    };

    inline CRefTime(LONG msecs) { m_time = MILLISECONDS_TO_100NS_UNITS(msecs); };

    inline CRefTime(REFERENCE_TIME rt) { m_time = rt; };

    inline operator REFERENCE_TIME() const { return m_time; };

    inline CRefTime &operator=(const CRefTime &rt)
    {
        m_time = rt.m_time;
        return *this;
    };

    inline CRefTime &operator=(const LONGLONG ll)
    {
        m_time = ll;
        return *this;
    };

    inline CRefTime &operator+=(const CRefTime &rt) { return (*this = *this + rt); };

    inline CRefTime &operator-=(const CRefTime &rt) { return (*this = *this - rt); };

    inline LONG Millisecs(void) { return (LONG)(m_time / (UNITS / MILLISECONDS)); };

    inline LONGLONG GetUnits(void) { return m_time; };
};

const LONGLONG TimeZero = 0;

#endif /* __REFTIME__ */
