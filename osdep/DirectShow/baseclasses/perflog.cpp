//------------------------------------------------------------------------------
// File: perflog.cpp
//
// Desc: Macros for DirectShow performance logging.
//
// Copyright (c) 1992-2001 Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#pragma warning(disable : 4201)

#include <streams.h>
#include <windows.h>
#include <tchar.h>
#include <winperf.h>
#include <wmistr.h>
#include <evntrace.h>
#include <strsafe.h>
#include "perflog.h"

//
// Local function prototypes.
//

ULONG
WINAPI
PerflogCallback(WMIDPREQUESTCODE RequestCode, __in PVOID Context, __out ULONG *BufferSize, __in PVOID Buffer);

//
// Event tracing function pointers.
// We have to do this to run on down-level platforms.
//

#ifdef UNICODE

ULONG(__stdcall *_RegisterTraceGuids)
(__in IN WMIDPREQUEST RequestAddress, __in IN PVOID RequestContext, IN LPCGUID ControlGuid, IN ULONG GuidCount,
 __in IN PTRACE_GUID_REGISTRATION TraceGuidReg, IN LPCWSTR MofImagePath, IN LPCWSTR MofResourceName,
 OUT PTRACEHANDLE RegistrationHandle);

#define REGISTERTRACEGUIDS_NAME "RegisterTraceGuidsW"

#else

ULONG(__stdcall *_RegisterTraceGuids)
(__in IN WMIDPREQUEST RequestAddress, __in IN PVOID RequestContext, IN LPCGUID ControlGuid, IN ULONG GuidCount,
 __in IN PTRACE_GUID_REGISTRATION TraceGuidReg, IN LPCSTR MofImagePath, IN LPCSTR MofResourceName,
 __out OUT PTRACEHANDLE RegistrationHandle);

#define REGISTERTRACEGUIDS_NAME "RegisterTraceGuidsA"

#endif

ULONG(__stdcall *_UnregisterTraceGuids)(TRACEHANDLE RegistrationHandle);

TRACEHANDLE(__stdcall *_GetTraceLoggerHandle)(__in PVOID Buffer);

UCHAR(__stdcall *_GetTraceEnableLevel)(TRACEHANDLE TraceHandle);

ULONG(__stdcall *_GetTraceEnableFlags)(TRACEHANDLE TraceHandle);

ULONG(__stdcall *_TraceEvent)(TRACEHANDLE TraceHandle, __in PEVENT_TRACE_HEADER EventTrace);

HINSTANCE _Advapi32;

//
// Global variables.
//

BOOL EventTracingAvailable = FALSE;
ULONG PerflogEnableFlags;
UCHAR PerflogEnableLevel;
ULONG PerflogModuleLevel = 0;
void (*OnStateChanged)(void);
TRACEHANDLE PerflogTraceHandle = NULL;
TRACEHANDLE PerflogRegHandle;

// The Win32 wsprintf() function writes a maximum of 1024 characters to it's output buffer.
// See the documentation for wsprintf()'s lpOut parameter for more information.
const INT iDEBUGINFO = 1024; // Used to format strings

//
// This routine initializes performance logging.
// It should be called from DllMain().
//

VOID PerflogReadModuleLevel(HINSTANCE hInstance)
{
    LONG lReturn;                 // Create key return value
    TCHAR szInfo[iDEBUGINFO];     // Constructs key names
    TCHAR szFullName[iDEBUGINFO]; // Load the full path and module name
    HKEY hModuleKey;              // Module key handle
    LPTSTR pName;                 // Searches from the end for a backslash
    DWORD dwKeySize, dwKeyType, dwKeyValue;

    DWORD dwSize = GetModuleFileName((hInstance ? hInstance : GetModuleHandle(NULL)), szFullName, iDEBUGINFO);

    if (0 == dwSize || iDEBUGINFO == dwSize)
    {
        return;
    }

    pName = _tcsrchr(szFullName, '\\');
    if (pName == NULL)
    {
        pName = szFullName;
    }
    else
    {
        pName++;
    }

    /* Construct the base key name */
    (void)StringCchPrintf(szInfo, NUMELMS(szInfo), TEXT("SOFTWARE\\Debug\\%s"), pName);

    /* Open the key for this module */
    lReturn = RegOpenKeyEx(HKEY_LOCAL_MACHINE, // Handle of an open key
                           szInfo,             // Address of subkey name
                           (DWORD)0,           // Reserved value
                           KEY_QUERY_VALUE,    // Desired security access
                           &hModuleKey);       // Opened handle buffer

    if (lReturn != ERROR_SUCCESS)
    {
        return;
    }

    dwKeySize = sizeof(DWORD);
    lReturn = RegQueryValueEx(hModuleKey, // Handle to an open key
                              TEXT("PERFLOG"),
                              NULL,                // Reserved field
                              &dwKeyType,          // Returns the field type
                              (LPBYTE)&dwKeyValue, // Returns the field's value
                              &dwKeySize);         // Number of bytes transferred

    if ((lReturn == ERROR_SUCCESS) && (dwKeyType == REG_DWORD))
    {
        PerflogModuleLevel = dwKeyValue;
    }

    RegCloseKey(hModuleKey);
}

BOOL PerflogInitIfEnabled(IN HINSTANCE hInstance, __in IN PPERFLOG_LOGGING_PARAMS LogParams)
{
    PerflogReadModuleLevel(hInstance);
    if (PerflogModuleLevel)
    {
        return PerflogInitialize(LogParams);
    }
    else
    {
        return FALSE;
    }
}

BOOL PerflogInitialize(__in IN PPERFLOG_LOGGING_PARAMS LogParams)
{
    ULONG status;

    //
    // If we're running on a recent-enough platform, this will get
    // pointers to the event tracing routines.
    //

    _Advapi32 = GetModuleHandle(_T("ADVAPI32.DLL"));
    if (_Advapi32 == NULL)
    {
        return FALSE;
    }

    *((FARPROC *)&_RegisterTraceGuids) = GetProcAddress(_Advapi32, REGISTERTRACEGUIDS_NAME);
    *((FARPROC *)&_UnregisterTraceGuids) = GetProcAddress(_Advapi32, "UnregisterTraceGuids");
    *((FARPROC *)&_GetTraceLoggerHandle) = GetProcAddress(_Advapi32, "GetTraceLoggerHandle");
    *((FARPROC *)&_GetTraceEnableLevel) = GetProcAddress(_Advapi32, "GetTraceEnableLevel");
    *((FARPROC *)&_GetTraceEnableFlags) = GetProcAddress(_Advapi32, "GetTraceEnableFlags");
    *((FARPROC *)&_TraceEvent) = GetProcAddress(_Advapi32, "TraceEvent");

    if (_RegisterTraceGuids == NULL || _UnregisterTraceGuids == NULL || _GetTraceEnableLevel == NULL ||
        _GetTraceEnableFlags == NULL || _TraceEvent == NULL)
    {

        return FALSE;
    }

    EventTracingAvailable = TRUE;

    OnStateChanged = LogParams->OnStateChanged;

    //
    // Register our GUIDs.
    //

    status = _RegisterTraceGuids(PerflogCallback, LogParams, &LogParams->ControlGuid, LogParams->NumberOfTraceGuids,
                                 LogParams->TraceGuids, NULL, NULL, &PerflogRegHandle);

    return (status == ERROR_SUCCESS);
}

//
// This routine shuts down performance logging.
//

VOID PerflogShutdown(VOID)
{
    if (!EventTracingAvailable)
    {
        return;
    }

    _UnregisterTraceGuids(PerflogRegHandle);
    PerflogRegHandle = NULL;
    PerflogTraceHandle = NULL;
}

//
// Event tracing callback routine.
// It's called when controllers call event tracing control functions.
//

ULONG
WINAPI
PerflogCallback(WMIDPREQUESTCODE RequestCode, __in PVOID Context, __out ULONG *BufferSize, __in PVOID Buffer)
{
    ULONG status;

    UNREFERENCED_PARAMETER(Context);

    ASSERT(EventTracingAvailable);

    status = ERROR_SUCCESS;

    switch (RequestCode)
    {

    case WMI_ENABLE_EVENTS:
        PerflogTraceHandle = _GetTraceLoggerHandle(Buffer);
        PerflogEnableFlags = _GetTraceEnableFlags(PerflogTraceHandle);
        PerflogEnableLevel = _GetTraceEnableLevel(PerflogTraceHandle);
        break;

    case WMI_DISABLE_EVENTS:
        PerflogTraceHandle = NULL;
        PerflogEnableFlags = 0;
        PerflogEnableLevel = 0;
        break;

    default: status = ERROR_INVALID_PARAMETER;
    }

    if (OnStateChanged != NULL)
    {
        OnStateChanged();
    }

    *BufferSize = 0;
    return status;
}

//
// Logging routine.
//

VOID PerflogTraceEvent(__in PEVENT_TRACE_HEADER Event)
{
    if (!EventTracingAvailable)
    {
        return;
    }

    _TraceEvent(PerflogTraceHandle, Event);
}

VOID PerflogTraceEventLevel(ULONG Level, __in PEVENT_TRACE_HEADER Event)
{
    if ((!EventTracingAvailable) || (Level <= PerflogModuleLevel))
    {
        return;
    }

    _TraceEvent(PerflogTraceHandle, Event);
}
