/*
 *      Copyright (C) 2010-2021 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 */

#pragma once

#include <list>
#include <string>
#include <DShow.h>

#define LCID_NOSUBTITLES -1

// SafeRelease Template, for type safety
template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}

#ifdef _DEBUG
#define DBG_TIMING(x, l, y)                                                      \
    {                                                                            \
        DWORD start = timeGetTime();                                             \
        y;                                                                       \
        DWORD end = timeGetTime();                                               \
        if (end - start > l)                                                     \
            DbgLog((LOG_CUSTOM5, 10, L"TIMING: %S took %u ms", x, end - start)); \
    }
extern void DbgSetLogFile(LPCTSTR szLogFile);
extern void DbgSetLogFileDesktop(LPCTSTR szLogFile);
extern void DbgCloseLogFile();
#else
#define DBG_TIMING(x, l, y) y;
#define DbgSetLogFile(sz)
#define DbgSetLogFileDesktop(sz)
#define DbgCloseLogFile()
#endif

// SAFE_ARRAY_DELETE macro.
// Deletes an array allocated with new [].

#ifndef SAFE_ARRAY_DELETE
#define SAFE_ARRAY_DELETE(x) \
    if (x)                   \
    {                        \
        delete[] x;          \
        x = nullptr;         \
    }
#endif

// some common macros
#define SAFE_DELETE(pPtr) \
    {                     \
        delete pPtr;      \
        pPtr = nullptr;   \
    }
#define SAFE_CO_FREE(pPtr)   \
    {                        \
        CoTaskMemFree(pPtr); \
        pPtr = nullptr;      \
    }
#define CHECK_HR(hr) \
    if (FAILED(hr))  \
    {                \
        goto done;   \
    }
#define QI(i) (riid == __uuidof(i)) ? GetInterface((i *)this, ppv):
#define QI2(i) (riid == IID_##i) ? GetInterface((i *)this, ppv):
#define countof(array) (sizeof(array) / sizeof(array[0]))

// Gennenric IUnknown creation function
template <class T> static CUnknown *WINAPI CreateInstance(LPUNKNOWN lpunk, HRESULT *phr)
{
    *phr = S_OK;
    CUnknown *punk = new T(lpunk, phr);
    if (punk == nullptr)
    {
        *phr = E_OUTOFMEMORY;
    }
    return punk;
}

extern void SetThreadName(DWORD dwThreadID, LPCSTR szThreadName);

void split(const std::string &text, const std::string &separators, std::list<std::string> &words);

// Filter Registration
extern void RegisterSourceFilter(const CLSID &clsid, const GUID &subtype2, LPCWSTR chkbytes, ...);
extern void RegisterSourceFilter(const CLSID &clsid, const GUID &subtype2, std::list<LPCWSTR> chkbytes, ...);
extern void UnRegisterSourceFilter(const GUID &subtype);

extern void RegisterProtocolSourceFilter(const CLSID &clsid, LPCWSTR protocol);
extern void UnRegisterProtocolSourceFilter(LPCWSTR protocol);

extern BOOL CheckApplicationBlackList(LPCTSTR subkey);

// Locale
extern std::string ISO6391ToLanguage(LPCSTR code);
extern std::string ISO6392ToLanguage(LPCSTR code);
extern std::string ProbeLangForLanguage(LPCSTR code);
extern LCID ISO6391ToLcid(LPCSTR code);
extern LCID ISO6392ToLcid(LPCSTR code);
extern LCID ProbeLangForLCID(LPCSTR code);
extern std::string ISO6391To6392(LPCSTR code);
extern std::string ISO6392To6391(LPCSTR code);
extern std::string ProbeForISO6392(LPCSTR lang);

// FilterGraphUtils
extern IBaseFilter *FindFilter(const GUID &clsid, IFilterGraph *pFG);
extern BOOL FilterInGraph(const GUID &clsid, IFilterGraph *pFG);
extern BOOL FilterInGraphWithInputSubtype(const GUID &clsid, IFilterGraph *pFG, const GUID &clsidSubtype);
extern IBaseFilter *GetFilterFromPin(IPin *pPin);
extern HRESULT NukeDownstream(IFilterGraph *pGraph, IPin *pPin);
extern HRESULT NukeDownstream(IFilterGraph *pGraph, IBaseFilter *pFilter);
extern HRESULT FindIntefaceInGraph(IPin *pPin, REFIID refiid, void **pUnknown);
extern HRESULT FindPinIntefaceInGraph(IPin *pPin, REFIID refiid, void **pUnknown);
extern HRESULT FindFilterSafe(IPin *pPin, const GUID &guid, IBaseFilter **ppFilter, BOOL bReverse = FALSE);
extern BOOL FilterInGraphSafe(IPin *pPin, const GUID &guid, BOOL bReverse = FALSE);
extern BOOL HasSourceWithType(IPin *pPin, const GUID &mediaType);
extern BOOL HasSourceWithTypeAdvanced(IPin *pPinInput, IPin *pPinOutput, const GUID &mediaType);

std::wstring WStringFromGUID(const GUID &guid);
BSTR ConvertCharToBSTR(const char *sz);

int SafeMultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr,
                            int cchWideChar);
int SafeWideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr,
                            int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar);
LPWSTR CoTaskGetWideCharFromMultiByte(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte);
LPSTR CoTaskGetMultiByteFromWideChar(UINT CodePage, DWORD dwFlags, LPCWSTR lpMultiByteStr, int cbMultiByte);

unsigned int lav_xiphlacing(unsigned char *s, unsigned int v);

void videoFormatTypeHandler(const AM_MEDIA_TYPE &mt, BITMAPINFOHEADER **pBMI = nullptr,
                            REFERENCE_TIME *prtAvgTime = nullptr, DWORD *pDwAspectX = nullptr,
                            DWORD *pDwAspectY = nullptr);
void videoFormatTypeHandler(const BYTE *format, const GUID *formattype, BITMAPINFOHEADER **pBMI = nullptr,
                            REFERENCE_TIME *prtAvgTime = nullptr, DWORD *pDwAspectX = nullptr,
                            DWORD *pDwAspectY = nullptr);
void audioFormatTypeHandler(const BYTE *format, const GUID *formattype, DWORD *pnSamples, WORD *pnChannels,
                            WORD *pnBitsPerSample, WORD *pnBlockAlign, DWORD *pnBytesPerSec, DWORD *pnChannelMask);
void getExtraData(const AM_MEDIA_TYPE &mt, BYTE *extra, size_t *extralen);
void getExtraData(const BYTE *format, const GUID *formattype, const size_t formatlen, BYTE *extra, size_t *extralen);

struct AVPacket;
struct MediaSideDataFFMpeg;
void CopyMediaSideDataFF(AVPacket *dst, const MediaSideDataFFMpeg **sd);

BOOL IsWindows7OrNewer();
BOOL IsWindows8OrNewer();
BOOL IsWindows10OrNewer();
BOOL IsWindows10BuildOrNewer(DWORD dwBuild);
void __cdecl debugprintf(LPCWSTR format, ...);
