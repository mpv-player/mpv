#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#include <sys/time.h>

static LARGE_INTEGER qwTimerFrequency;
static LARGE_INTEGER qwTimerStart;
static LARGE_INTEGER m_lStartTime;
static float m_fuSecsPerTick;
static unsigned long RelativeTime = 0;

int usec_sleep(int usec_delay)
{
    LARGE_INTEGER qwStartTicks, qwCurrTicks;
    double dResult;
    long lTick;
    double fuSecDelay = ((float) usec_delay) / 1000000.0;

    QueryPerformanceCounter(&qwStartTicks);
    do {
	QueryPerformanceCounter(&qwCurrTicks);
	dResult =
	    ((double) (qwCurrTicks.QuadPart - qwStartTicks.QuadPart)) /
	    ((double) (qwTimerFrequency.QuadPart));
    } while (dResult < fuSecDelay);
}

// Returns current time in microseconds
unsigned long GetTimer()
{
    LARGE_INTEGER qwTime;
    FLOAT fTime;
    UINT64 uiQuadPart;

    QueryPerformanceCounter(&qwTime);
    qwTime.QuadPart -= m_lStartTime.QuadPart;
    uiQuadPart = (UINT64) qwTime.QuadPart;
    uiQuadPart /= ((UINT64) 10);	// prevent overflow after 4294.1 secs, now overflows after 42941 secs
    fTime = ((FLOAT) (uiQuadPart)) / m_fuSecsPerTick;
    return (unsigned long) fTime;
}

// Returns current time in microseconds
float GetRelativeTime()
{
    unsigned long t, r;

    t = GetTimer();
    r = t - RelativeTime;
    RelativeTime = t;
    return (float) r *0.000001F;
}

// Returns current time in milliseconds
unsigned int GetTimerMS()
{
    return GetTimer() / 1000;
}

void InitTimer()
{
    FLOAT t;

    QueryPerformanceFrequency(&qwTimerFrequency);	// ticks/sec
    m_fuSecsPerTick = (FLOAT) (((FLOAT) (qwTimerFrequency.QuadPart)) / 1000.0);	// tics/msec
    m_fuSecsPerTick = (FLOAT) (m_fuSecsPerTick / 1000.0);	// ticks/usec
    m_fuSecsPerTick /= 10.0;
    QueryPerformanceCounter(&m_lStartTime);
    t = GetRelativeTime();
}
