//------------------------------------------------------------------------------
// File: DXMPerf.h
//
// Desc: Macros for DirectShow performance logging.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#ifndef _DXMPERF_H_
#define _DXMPERF_H_

#include <perfstruct.h>
#include "perflog.h"

#ifdef _IA64_
extern "C" unsigned __int64 __getReg(int whichReg);
#pragma intrinsic(__getReg)
#endif // _IA64_

inline ULONGLONG _RDTSC(void)
{
#ifdef _X86_
    LARGE_INTEGER li;
    __asm {
        _emit   0x0F
        _emit   0x31
        mov li.LowPart,eax
        mov li.HighPart,edx
    }
    return li.QuadPart;

#if 0 // This isn't tested yet

#elif defined(_IA64_)

#define INL_REGID_APITC 3116
    return __getReg(INL_REGID_APITC);

#endif // 0

#else  // unsupported platform
    // not implemented on non x86/IA64 platforms
    return 0;
#endif // _X86_/_IA64_
}

#define DXMPERF_VIDEOREND 0x00000001
#define DXMPERF_AUDIOGLITCH 0x00000002
//#define GETTIME_BIT         0x00000001
//#define AUDIOREND_BIT       0x00000004
//#define FRAMEDROP_BIT       0x00000008
#define AUDIOBREAK_BIT 0x00000010
#define DXMPERF_AUDIORECV 0x00000020
#define DXMPERF_AUDIOSLAVE 0x00000040
#define DXMPERF_AUDIOBREAK 0x00000080

#define PERFLOG_CTOR(name, iface)
#define PERFLOG_DTOR(name, iface)
#define PERFLOG_DELIVER(name, source, dest, sample, pmt)
#define PERFLOG_RECEIVE(name, source, dest, sample, pmt)
#define PERFLOG_RUN(name, iface, time, oldstate)
#define PERFLOG_PAUSE(name, iface, oldstate)
#define PERFLOG_STOP(name, iface, oldstate)
#define PERFLOG_JOINGRAPH(name, iface, graph)
#define PERFLOG_GETBUFFER(allocator, sample)
#define PERFLOG_RELBUFFER(allocator, sample)
#define PERFLOG_CONNECT(connector, connectee, status, pmt)
#define PERFLOG_RXCONNECT(connector, connectee, status, pmt)
#define PERFLOG_DISCONNECT(disconnector, disconnectee, status)

#define PERFLOG_GETTIME(clock, time) /*{                                   \
PERFINFO_WMI_GETTIME    perfData;                                          \
if (NULL != g_pTraceEvent) {                                               \
   memset( &perfData, 0, sizeof( perfData ) );                             \
   perfData.header.Size  = sizeof( perfData );                             \
   perfData.header.Flags = WNODE_FLAG_TRACED_GUID;                         \
   perfData.header.Guid  = GUID_GETTIME;                                   \
   perfData.data.cycleCounter = _RDTSC();                                  \
   perfData.data.dshowClock   = (ULONGLONG) (time);                        \
   if (g_perfMasks[GETTIME_INDEX] & GETTIME_BIT)                           \
       (*g_pTraceEvent)( g_traceHandle, (PEVENT_TRACE_HEADER) &perfData ); \
   }                                                                       \
}*/

#define PERFLOG_AUDIOREND(clocktime, sampletime, psample, bytetime, cbytes) /*{ \
  PERFINFO_WMI_AVREND    perfData;                                              \
  if (NULL != g_pTraceEvent) {                                                  \
      memset( &perfData, 0, sizeof( perfData ) );                               \
      perfData.header.Size  = sizeof( perfData );                               \
      perfData.header.Flags = WNODE_FLAG_TRACED_GUID;                           \
      perfData.header.Guid  = GUID_AUDIOREND;                                   \
      perfData.data.cycleCounter = _RDTSC();                                    \
      perfData.data.dshowClock   = (clocktime);                                 \
      perfData.data.sampleTime   = (sampletime);                                \
      if (g_perfMasks[AUDIOREND_INDEX] & AUDIOREND_BIT)                         \
          (*g_pTraceEvent)( g_traceHandle, (PEVENT_TRACE_HEADER) &perfData );   \
      }                                                                         \
  }*/

#define PERFLOG_AUDIORECV(StreamTime, SampleStart, SampleStop, Discontinuity, Duration) \
    if (PerflogEnableFlags & DXMPERF_AUDIORECV)                                         \
    {                                                                                   \
        PERFINFO_WMI_AUDIORECV perfData;                                                \
        memset(&perfData, 0, sizeof(perfData));                                         \
        perfData.header.Size = sizeof(perfData);                                        \
        perfData.header.Flags = WNODE_FLAG_TRACED_GUID;                                 \
        perfData.header.Guid = GUID_AUDIORECV;                                          \
        perfData.data.streamTime = StreamTime;                                          \
        perfData.data.sampleStart = SampleStart;                                        \
        perfData.data.sampleStop = SampleStop;                                          \
        perfData.data.discontinuity = Discontinuity;                                    \
        perfData.data.hwduration = Duration;                                            \
        PerflogTraceEvent((PEVENT_TRACE_HEADER)&perfData);                              \
    }

#define PERFLOG_AUDIOSLAVE(MasterClock, SlaveClock, ErrorAccum, LastHighErrorSeen, LastLowErrorSeen) \
    if (PerflogEnableFlags & DXMPERF_AUDIOSLAVE)                                                     \
    {                                                                                                \
        PERFINFO_WMI_AUDIOSLAVE perfData;                                                            \
        memset(&perfData, 0, sizeof(perfData));                                                      \
        perfData.header.Size = sizeof(perfData);                                                     \
        perfData.header.Flags = WNODE_FLAG_TRACED_GUID;                                              \
        perfData.header.Guid = GUID_AUDIOSLAVE;                                                      \
        perfData.data.masterClock = MasterClock;                                                     \
        perfData.data.slaveClock = SlaveClock;                                                       \
        perfData.data.errorAccum = ErrorAccum;                                                       \
        perfData.data.lastHighErrorSeen = LastHighErrorSeen;                                         \
        perfData.data.lastLowErrorSeen = LastLowErrorSeen;                                           \
        PerflogTraceEvent((PEVENT_TRACE_HEADER)&perfData);                                           \
    }

#define PERFLOG_AUDIOADDBREAK(IterNextWrite, OffsetNextWrite, IterWrite, OffsetWrite) \
    if (PerflogEnableFlags & DXMPERF_AUDIOBREAK)                                      \
    {                                                                                 \
        PERFINFO_WMI_AUDIOADDBREAK perfData;                                          \
        memset(&perfData, 0, sizeof(perfData));                                       \
        perfData.header.Size = sizeof(perfData);                                      \
        perfData.header.Flags = WNODE_FLAG_TRACED_GUID;                               \
        perfData.header.Guid = GUID_AUDIOADDBREAK;                                    \
        perfData.data.iterNextWrite = IterNextWrite;                                  \
        perfData.data.offsetNextWrite = OffsetNextWrite;                              \
        perfData.data.iterWrite = IterWrite;                                          \
        perfData.data.offsetWrite = OffsetWrite;                                      \
        PerflogTraceEvent((PEVENT_TRACE_HEADER)&perfData);                            \
    }

#define PERFLOG_VIDEOREND(sampletime, clocktime, psample)  \
    if (PerflogEnableFlags & DXMPERF_VIDEOREND)            \
    {                                                      \
        PERFINFO_WMI_AVREND perfData;                      \
        memset(&perfData, 0, sizeof(perfData));            \
        perfData.header.Size = sizeof(perfData);           \
        perfData.header.Flags = WNODE_FLAG_TRACED_GUID;    \
        perfData.header.Guid = GUID_VIDEOREND;             \
        perfData.data.cycleCounter = _RDTSC();             \
        perfData.data.dshowClock = (clocktime);            \
        perfData.data.sampleTime = (sampletime);           \
        PerflogTraceEvent((PEVENT_TRACE_HEADER)&perfData); \
    }

#define PERFLOG_AUDIOGLITCH(instance, glitchtype, currenttime, previoustime) \
    if (PerflogEnableFlags & DXMPERF_AUDIOGLITCH)                            \
    {                                                                        \
        PERFINFO_WMI_AUDIOGLITCH perfData;                                   \
        memset(&perfData, 0, sizeof(perfData));                              \
        perfData.header.Size = sizeof(perfData);                             \
        perfData.header.Flags = WNODE_FLAG_TRACED_GUID;                      \
        perfData.header.Guid = GUID_DSOUNDGLITCH;                            \
        perfData.data.cycleCounter = _RDTSC();                               \
        perfData.data.glitchType = (glitchtype);                             \
        perfData.data.sampleTime = (currenttime);                            \
        perfData.data.previousTime = (previoustime);                         \
        perfData.data.instanceId = (instance);                               \
        PerflogTraceEvent((PEVENT_TRACE_HEADER)&perfData);                   \
    }

#define PERFLOG_FRAMEDROP(sampletime, clocktime, psample, renderer) /*{    \
PERFINFO_WMI_FRAMEDROP    perfData;                                        \
if (NULL != g_pTraceEvent) {                                               \
   memset( &perfData, 0, sizeof( perfData ) );                             \
   perfData.header.Size  = sizeof( perfData );                             \
   perfData.header.Flags = WNODE_FLAG_TRACED_GUID;                         \
   perfData.header.Guid  = GUID_FRAMEDROP;                                 \
   perfData.data.cycleCounter = _RDTSC();                                  \
   perfData.data.dshowClock   = (clocktime);                               \
   perfData.data.frameTime    = (sampletime);                              \
   if (g_perfMasks[FRAMEDROP_INDEX] & FRAMEDROP_BIT)                       \
       (*g_pTraceEvent)( g_traceHandle, (PEVENT_TRACE_HEADER) &perfData ); \
   }                                                                       \
}*/

/*
#define PERFLOG_AUDIOBREAK( nextwrite, writepos, msecs )    { \
    PERFINFO_WMI_AUDIOBREAK    perfData; \
    if (NULL != g_pTraceEvent) { \
        memset( &perfData, 0, sizeof( perfData ) ); \
        perfData.header.Size  = sizeof( perfData ); \
        perfData.header.Flags = WNODE_FLAG_TRACED_GUID; \
        perfData.header.Guid  = GUID_AUDIOBREAK; \
        perfData.data.cycleCounter   = _RDTSC(); \
        perfData.data.dshowClock     = (writepos); \
        perfData.data.sampleTime     = (nextwrite); \
        perfData.data.sampleDuration = (msecs); \
        if (g_perfMasks[AUDIOBREAK_INDEX] & AUDIOBREAK_BIT) \
            (*g_pTraceEvent)( g_traceHandle, (PEVENT_TRACE_HEADER) &perfData ); \
        } \
    }
*/

#define PERFLOG_AUDIOBREAK(nextwrite, writepos, msecs)     \
    if (PerflogEnableFlags & AUDIOBREAK_BIT)               \
    {                                                      \
        PERFINFO_WMI_AUDIOBREAK perfData;                  \
        memset(&perfData, 0, sizeof(perfData));            \
        perfData.header.Size = sizeof(perfData);           \
        perfData.header.Flags = WNODE_FLAG_TRACED_GUID;    \
        perfData.header.Guid = GUID_AUDIOBREAK;            \
        perfData.data.cycleCounter = _RDTSC();             \
        perfData.data.dshowClock = (writepos);             \
        perfData.data.sampleTime = (nextwrite);            \
        perfData.data.sampleDuration = (msecs);            \
        PerflogTraceEvent((PEVENT_TRACE_HEADER)&perfData); \
    }

inline VOID PERFLOG_STREAMTRACE(ULONG Level, ULONG Id, ULONGLONG DShowClock, ULONGLONG Data1, ULONGLONG Data2,
                                ULONGLONG Data3, ULONGLONG Data4)
{
    if (Level <= PerflogModuleLevel)
    {
        PERFINFO_WMI_STREAMTRACE perfData;
        memset(&perfData, 0, sizeof(perfData));
        perfData.header.Size = sizeof(perfData);
        perfData.header.Flags = WNODE_FLAG_TRACED_GUID;
        perfData.header.Guid = GUID_STREAMTRACE;
        perfData.data.dshowClock = DShowClock;
        perfData.data.id = Id;
        perfData.data.data[0] = Data1;
        perfData.data.data[1] = Data2;
        perfData.data.data[2] = Data3;
        perfData.data.data[3] = Data4;
        PerflogTraceEvent((PEVENT_TRACE_HEADER)&perfData);
    }
}

#endif // _DXMPERF_H_
