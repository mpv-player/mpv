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
 */

#include "stdafx.h"
#include "DShowUtil.h"

#include <Shlwapi.h>

#include <dvdmedia.h>
#include "moreuuids.h"

#include "registry.h"

#include "IMediaSideDataFFmpeg.h"

//
// Usage: SetThreadName (-1, "MainThread");
//
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;     // must be 0x1000
    LPCSTR szName;    // pointer to name (in user addr space)
    DWORD dwThreadID; // thread ID (-1=caller thread)
    DWORD dwFlags;    // reserved for future use, must be zero
} THREADNAME_INFO;

const DWORD MS_VC_EXCEPTION = 0x406D1388;

void SetThreadName(DWORD dwThreadID, LPCSTR szThreadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = szThreadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;

    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR *)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

#ifdef DEBUG

#include <Shlobj.h>
#include <Shlwapi.h>

extern HANDLE m_hOutput;
volatile LONG hOutputCounter = 0;
extern HRESULT DbgUniqueProcessName(LPCTSTR inName, LPTSTR outName);
void DbgSetLogFile(LPCTSTR szFile)
{
    HANDLE hOutput =
        CreateFile(szFile, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (INVALID_HANDLE_VALUE == hOutput && GetLastError() == ERROR_SHARING_VIOLATION)
    {
        TCHAR uniqueName[MAX_PATH] = {0};
        if (SUCCEEDED(DbgUniqueProcessName(szFile, uniqueName)))
        {
            hOutput = CreateFile(uniqueName, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
        }
    }

    if (hOutput != INVALID_HANDLE_VALUE)
    {
        if (InterlockedCompareExchangePointer(&m_hOutput, hOutput, INVALID_HANDLE_VALUE) != INVALID_HANDLE_VALUE)
            CloseHandle(hOutput);
    }

    InterlockedIncrement(&hOutputCounter);
}

void DbgSetLogFileDesktop(LPCTSTR szFile)
{
    TCHAR szLogPath[512];
    SHGetFolderPath(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, szLogPath);
    PathAppend(szLogPath, szFile);
    DbgSetLogFile(szLogPath);
}

void DbgCloseLogFile()
{
    LONG count = InterlockedDecrement(&hOutputCounter);
    if (count == 0 && m_hOutput != INVALID_HANDLE_VALUE)
    {
        FlushFileBuffers(m_hOutput);
        CloseHandle(m_hOutput);
        m_hOutput = INVALID_HANDLE_VALUE;
    }
}
#endif

void split(const std::string &text, const std::string &separators, std::list<std::string> &words)
{
    size_t n = text.length();
    size_t start, stop;

    start = text.find_first_not_of(separators);
    while ((start >= 0) && (start < n))
    {
        stop = text.find_first_of(separators, start);
        if ((stop < 0) || (stop > n))
            stop = n;
        words.push_back(text.substr(start, stop - start));
        start = text.find_first_not_of(separators, stop + 1);
    }
}

IBaseFilter *FindFilter(const GUID &clsid, IFilterGraph *pFG)
{
    IBaseFilter *pFilter = nullptr;
    IEnumFilters *pEnumFilters = nullptr;
    if (pFG && SUCCEEDED(pFG->EnumFilters(&pEnumFilters)))
    {
        for (IBaseFilter *pBF = nullptr; S_OK == pEnumFilters->Next(1, &pBF, 0);)
        {
            GUID clsid2;
            if (SUCCEEDED(pBF->GetClassID(&clsid2)) && clsid == clsid2)
            {
                pFilter = pBF;
                break;
            }
            SafeRelease(&pBF);
        }
        SafeRelease(&pEnumFilters);
    }

    return pFilter;
}

BOOL FilterInGraph(const GUID &clsid, IFilterGraph *pFG)
{
    BOOL bFound = FALSE;
    IBaseFilter *pFilter = nullptr;

    pFilter = FindFilter(clsid, pFG);
    bFound = (pFilter != nullptr);
    SafeRelease(&pFilter);

    return bFound;
}

BOOL FilterInGraphWithInputSubtype(const GUID &clsid, IFilterGraph *pFG, const GUID &clsidSubtype)
{
    BOOL bFound = FALSE;
    IBaseFilter *pFilter = nullptr;

    pFilter = FindFilter(clsid, pFG);

    if (pFilter)
    {
        IEnumPins *pPinEnum = nullptr;
        pFilter->EnumPins(&pPinEnum);
        IPin *pPin = nullptr;
        while ((S_OK == pPinEnum->Next(1, &pPin, nullptr)) && pPin)
        {
            PIN_DIRECTION dir;
            pPin->QueryDirection(&dir);
            if (dir == PINDIR_INPUT)
            {
                AM_MEDIA_TYPE mt;
                pPin->ConnectionMediaType(&mt);

                if (mt.subtype == clsidSubtype)
                {
                    bFound = TRUE;
                }
                FreeMediaType(mt);
            }
            SafeRelease(&pPin);

            if (bFound)
                break;
        }

        SafeRelease(&pPinEnum);
        SafeRelease(&pFilter);
    }

    return bFound;
}

std::wstring WStringFromGUID(const GUID &guid)
{
    WCHAR null[128] = {0}, buff[128];
    StringFromGUID2(GUID_NULL, null, 127);
    return std::wstring(StringFromGUID2(guid, buff, 127) > 0 ? buff : null);
}

int SafeMultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr,
                            int cchWideChar)
{
    int len = MultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar);
    if (cchWideChar)
    {
        if (len == cchWideChar || (len == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        {
            lpWideCharStr[cchWideChar - 1] = 0;
        }
        else if (len == 0)
        {
            DWORD dwErr = GetLastError();
            if (dwErr == ERROR_NO_UNICODE_TRANSLATION && CodePage == CP_UTF8)
            {
                return SafeMultiByteToWideChar(CP_ACP, dwFlags, lpMultiByteStr, cbMultiByte, lpWideCharStr,
                                               cchWideChar);
            }
            else if (dwErr == ERROR_NO_UNICODE_TRANSLATION && (dwFlags & MB_ERR_INVALID_CHARS))
            {
                return SafeMultiByteToWideChar(CP_UTF8, (dwFlags & ~MB_ERR_INVALID_CHARS), lpMultiByteStr, cbMultiByte,
                                               lpWideCharStr, cchWideChar);
            }
            lpWideCharStr[0] = 0;
        }
    }
    return len;
}

int SafeWideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr,
                            int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
{
    int len = WideCharToMultiByte(CodePage, dwFlags, lpWideCharStr, cchWideChar, lpMultiByteStr, cbMultiByte,
                                  lpDefaultChar, lpUsedDefaultChar);
    if (cbMultiByte)
    {
        if (len == cbMultiByte || (len == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        {
            lpMultiByteStr[cbMultiByte - 1] = 0;
        }
        else if (len == 0)
        {
            lpMultiByteStr[0] = 0;
        }
    }
    return len;
}

LPWSTR CoTaskGetWideCharFromMultiByte(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte)
{
    int len = MultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, nullptr, 0);
    if (len)
    {
        LPWSTR pszWideString = (LPWSTR)CoTaskMemAlloc(len * sizeof(WCHAR));
        MultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, pszWideString, len);

        return pszWideString;
    }
    else
    {
        DWORD dwErr = GetLastError();
        if (dwErr == ERROR_NO_UNICODE_TRANSLATION && CodePage == CP_UTF8)
        {
            return CoTaskGetWideCharFromMultiByte(CP_ACP, dwFlags, lpMultiByteStr, cbMultiByte);
        }
        else if (dwErr == ERROR_NO_UNICODE_TRANSLATION && (dwFlags & MB_ERR_INVALID_CHARS))
        {
            return CoTaskGetWideCharFromMultiByte(CP_UTF8, (dwFlags & ~MB_ERR_INVALID_CHARS), lpMultiByteStr,
                                                  cbMultiByte);
        }
    }
    return NULL;
}

LPSTR CoTaskGetMultiByteFromWideChar(UINT CodePage, DWORD dwFlags, LPCWSTR lpMultiByteStr, int cbMultiByte)
{
    int len = WideCharToMultiByte(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, nullptr, 0, nullptr, nullptr);
    if (len)
    {
        LPSTR pszMBString = (LPSTR)CoTaskMemAlloc(len * sizeof(char));
        WideCharToMultiByte(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, pszMBString, len, nullptr, nullptr);

        return pszMBString;
    }
    return NULL;
}

BSTR ConvertCharToBSTR(const char *sz)
{
    bool acp = false;
    if (!sz || strlen(sz) == 0)
        return nullptr;

    WCHAR *wide = CoTaskGetWideCharFromMultiByte(CP_UTF8, MB_ERR_INVALID_CHARS, sz, -1);
    if (!wide)
        return nullptr;

    BSTR bstr = SysAllocString(wide);
    CoTaskMemFree(wide);

    return bstr;
}

IBaseFilter *GetFilterFromPin(IPin *pPin)
{
    CheckPointer(pPin, nullptr);

    PIN_INFO pi;
    if (pPin && SUCCEEDED(pPin->QueryPinInfo(&pi)))
    {
        return pi.pFilter;
    }

    return nullptr;
}

HRESULT NukeDownstream(IFilterGraph *pGraph, IPin *pPin)
{
    PIN_DIRECTION dir;
    if (pPin)
    {
        IPin *pPinTo = nullptr;
        if (FAILED(pPin->QueryDirection(&dir)))
            return E_FAIL;
        if (dir == PINDIR_OUTPUT)
        {
            if (SUCCEEDED(pPin->ConnectedTo(&pPinTo)) && pPinTo)
            {
                if (IBaseFilter *pFilter = GetFilterFromPin(pPinTo))
                {
                    NukeDownstream(pGraph, pFilter);
                    pGraph->Disconnect(pPinTo);
                    pGraph->Disconnect(pPin);
                    pGraph->RemoveFilter(pFilter);
                    SafeRelease(&pFilter);
                }
                SafeRelease(&pPinTo);
            }
        }
    }

    return S_OK;
}

HRESULT NukeDownstream(IFilterGraph *pGraph, IBaseFilter *pFilter)
{
    IEnumPins *pEnumPins = nullptr;
    if (pFilter && SUCCEEDED(pFilter->EnumPins(&pEnumPins)))
    {
        for (IPin *pPin = nullptr; S_OK == pEnumPins->Next(1, &pPin, 0); pPin = nullptr)
        {
            NukeDownstream(pGraph, pPin);
            SafeRelease(&pPin);
        }
        SafeRelease(&pEnumPins);
    }

    return S_OK;
}

// pPin - pin of our filter to start searching
// refiid - guid of the interface to find
// pUnknown - variable that'll receive the interface
HRESULT FindIntefaceInGraph(IPin *pPin, REFIID refiid, void **pUnknown)
{
    PIN_DIRECTION dir;
    pPin->QueryDirection(&dir);

    IPin *pOtherPin = nullptr;
    if (SUCCEEDED(pPin->ConnectedTo(&pOtherPin)) && pOtherPin)
    {
        IBaseFilter *pFilter = GetFilterFromPin(pOtherPin);
        SafeRelease(&pOtherPin);

        HRESULT hrFilter = pFilter->QueryInterface(refiid, pUnknown);
        if (FAILED(hrFilter))
        {
            IEnumPins *pPinEnum = nullptr;
            pFilter->EnumPins(&pPinEnum);

            HRESULT hrPin = E_FAIL;
            for (IPin *pOtherPin2 = nullptr; pPinEnum->Next(1, &pOtherPin2, 0) == S_OK; pOtherPin2 = nullptr)
            {
                PIN_DIRECTION pinDir;
                pOtherPin2->QueryDirection(&pinDir);
                if (dir == pinDir)
                {
                    hrPin = FindIntefaceInGraph(pOtherPin2, refiid, pUnknown);
                }
                SafeRelease(&pOtherPin2);
                if (SUCCEEDED(hrPin))
                    break;
            }
            hrFilter = hrPin;
            SafeRelease(&pPinEnum);
        }
        SafeRelease(&pFilter);

        if (SUCCEEDED(hrFilter))
        {
            return S_OK;
        }
    }
    return E_NOINTERFACE;
}

// pPin - pin of our filter to start searching
// refiid - guid of the interface to find
// pUnknown - variable that'll receive the interface
HRESULT FindPinIntefaceInGraph(IPin *pPin, REFIID refiid, void **pUnknown)
{
    PIN_DIRECTION dir;
    pPin->QueryDirection(&dir);

    IPin *pOtherPin = nullptr;
    if (SUCCEEDED(pPin->ConnectedTo(&pOtherPin)) && pOtherPin)
    {
        IBaseFilter *pFilter = nullptr;
        HRESULT hrFilter = pOtherPin->QueryInterface(refiid, pUnknown);

        if (FAILED(hrFilter))
        {
            pFilter = GetFilterFromPin(pOtherPin);

            IEnumPins *pPinEnum = nullptr;
            pFilter->EnumPins(&pPinEnum);

            HRESULT hrPin = E_FAIL;
            for (IPin *pOtherPin2 = nullptr; pPinEnum->Next(1, &pOtherPin2, 0) == S_OK; pOtherPin2 = nullptr)
            {
                PIN_DIRECTION pinDir;
                pOtherPin2->QueryDirection(&pinDir);
                if (dir == pinDir)
                {
                    hrPin = FindPinIntefaceInGraph(pOtherPin2, refiid, pUnknown);
                }
                SafeRelease(&pOtherPin2);
                if (SUCCEEDED(hrPin))
                    break;
            }
            hrFilter = hrPin;
            SafeRelease(&pPinEnum);
        }
        SafeRelease(&pFilter);
        SafeRelease(&pOtherPin);

        if (SUCCEEDED(hrFilter))
        {
            return S_OK;
        }
    }
    return E_NOINTERFACE;
}

// pPin - pin of our filter to start searching
// guid - guid of the filter to find
// ppFilter - variable that'll receive a AddRef'd reference to the filter
HRESULT FindFilterSafe(IPin *pPin, const GUID &guid, IBaseFilter **ppFilter, BOOL bReverse)
{
    CheckPointer(ppFilter, E_POINTER);
    CheckPointer(pPin, E_POINTER);
    HRESULT hr = S_OK;

    PIN_DIRECTION dir;
    pPin->QueryDirection(&dir);

    IPin *pOtherPin = nullptr;
    if (bReverse)
    {
        dir = (dir == PINDIR_INPUT) ? PINDIR_OUTPUT : PINDIR_INPUT;
        pOtherPin = pPin;
        pPin->AddRef();
        hr = S_OK;
    }
    else
    {
        hr = pPin->ConnectedTo(&pOtherPin);
    }
    if (SUCCEEDED(hr) && pOtherPin)
    {
        IBaseFilter *pFilter = GetFilterFromPin(pOtherPin);
        SafeRelease(&pOtherPin);

        HRESULT hrFilter = E_NOINTERFACE;
        CLSID filterGUID;
        if (SUCCEEDED(pFilter->GetClassID(&filterGUID)))
        {
            if (filterGUID == guid)
            {
                *ppFilter = pFilter;
                hrFilter = S_OK;
            }
            else
            {
                IEnumPins *pPinEnum = nullptr;
                pFilter->EnumPins(&pPinEnum);

                HRESULT hrPin = E_FAIL;
                for (IPin *pOtherPin2 = nullptr; pPinEnum->Next(1, &pOtherPin2, 0) == S_OK; pOtherPin2 = nullptr)
                {
                    PIN_DIRECTION pinDir;
                    pOtherPin2->QueryDirection(&pinDir);
                    if (dir == pinDir)
                    {
                        hrPin = FindFilterSafe(pOtherPin2, guid, ppFilter);
                    }
                    SafeRelease(&pOtherPin2);
                    if (SUCCEEDED(hrPin))
                        break;
                }
                hrFilter = hrPin;
                SafeRelease(&pPinEnum);
                SafeRelease(&pFilter);
            }
        }

        if (SUCCEEDED(hrFilter))
        {
            return S_OK;
        }
    }
    return E_NOINTERFACE;
}

// pPin - pin of our filter to start searching
// guid - guid of the filter to find
// ppFilter - variable that'll receive a AddRef'd reference to the filter
BOOL HasSourceWithType(IPin *pPin, const GUID &mediaType)
{
    CheckPointer(pPin, false);
    BOOL bFound = FALSE;

    PIN_DIRECTION dir;
    pPin->QueryDirection(&dir);

    IPin *pOtherPin = nullptr;
    if (SUCCEEDED(pPin->ConnectedTo(&pOtherPin)) && pOtherPin)
    {
        IBaseFilter *pFilter = GetFilterFromPin(pOtherPin);

        HRESULT hrFilter = E_NOINTERFACE;
        IEnumPins *pPinEnum = nullptr;
        pFilter->EnumPins(&pPinEnum);

        HRESULT hrPin = E_FAIL;
        for (IPin *pOtherPin2 = nullptr; !bFound && pPinEnum->Next(1, &pOtherPin2, 0) == S_OK; pOtherPin2 = nullptr)
        {
            if (pOtherPin2 != pOtherPin)
            {
                PIN_DIRECTION pinDir;
                pOtherPin2->QueryDirection(&pinDir);
                if (dir != pinDir)
                {
                    IEnumMediaTypes *pMediaTypeEnum = nullptr;
                    if (SUCCEEDED(pOtherPin2->EnumMediaTypes(&pMediaTypeEnum)))
                    {
                        for (AM_MEDIA_TYPE *mt = nullptr; pMediaTypeEnum->Next(1, &mt, 0) == S_OK; mt = nullptr)
                        {
                            if (mt->majortype == mediaType)
                            {
                                bFound = TRUE;
                            }
                            DeleteMediaType(mt);
                        }
                        SafeRelease(&pMediaTypeEnum);
                    }
                }
                else
                {
                    bFound = HasSourceWithType(pOtherPin2, mediaType);
                }
            }
            SafeRelease(&pOtherPin2);
        }
        SafeRelease(&pPinEnum);
        SafeRelease(&pFilter);
        SafeRelease(&pOtherPin);
    }
    return bFound;
}

// Similar to HasSourceWithType but also checks forward pins for future backwards joins
BOOL HasSourceWithTypeAdvanced(IPin *pPinInput, IPin *pPinOutput, const GUID &mediaType)
{
    // check the input pin backwards first
    if (pPinInput && HasSourceWithType(pPinInput, mediaType))
        return true;

    if (pPinOutput == NULL)
        return false;

    // and check the tree forwards
    BOOL bFound = FALSE;
    IPin *pOtherPin = nullptr;
    if (SUCCEEDED(pPinOutput->ConnectedTo(&pOtherPin)) && pOtherPin)
    {
        IBaseFilter *pFilter = GetFilterFromPin(pOtherPin);

        HRESULT hrFilter = E_NOINTERFACE;
        IEnumPins *pPinEnum = nullptr;
        pFilter->EnumPins(&pPinEnum);

        // Iterate over pins of the filter..
        HRESULT hrPin = E_FAIL;
        for (IPin *pOtherPin2 = nullptr; !bFound && pPinEnum->Next(1, &pOtherPin2, 0) == S_OK; pOtherPin2 = nullptr)
        {
            // ignore the pint we're connected to
            if (pOtherPin2 != pOtherPin)
            {
                PIN_DIRECTION pinDir;
                pOtherPin2->QueryDirection(&pinDir);

                // if its another input, go backwards there
                if (pinDir == PINDIR_INPUT)
                {
                    bFound = HasSourceWithType(pOtherPin2, mediaType);
                }
                // if its an output, go forwards
                else if (pinDir == PINDIR_OUTPUT)
                {
                    bFound = HasSourceWithTypeAdvanced(NULL, pOtherPin2, mediaType);
                }
            }
            SafeRelease(&pOtherPin2);
        }
        SafeRelease(&pPinEnum);
        SafeRelease(&pFilter);
        SafeRelease(&pOtherPin);
    }

    return bFound;
}

BOOL FilterInGraphSafe(IPin *pPin, const GUID &guid, BOOL bReverse)
{
    IBaseFilter *pFilter = nullptr;
    HRESULT hr = FindFilterSafe(pPin, guid, &pFilter, bReverse);
    if (SUCCEEDED(hr) && pFilter)
    {
        SafeRelease(&pFilter);
        return TRUE;
    }
    return FALSE;
}

unsigned int lav_xiphlacing(unsigned char *s, unsigned int v)
{
    unsigned int n = 0;

    while (v >= 0xff)
    {
        *s++ = 0xff;
        v -= 0xff;
        n++;
    }
    *s = v;
    n++;
    return n;
}

void videoFormatTypeHandler(const AM_MEDIA_TYPE &mt, BITMAPINFOHEADER **pBMI, REFERENCE_TIME *prtAvgTime,
                            DWORD *pDwAspectX, DWORD *pDwAspectY)
{
    videoFormatTypeHandler(mt.pbFormat, &mt.formattype, pBMI, prtAvgTime, pDwAspectX, pDwAspectY);
}

void videoFormatTypeHandler(const BYTE *format, const GUID *formattype, BITMAPINFOHEADER **pBMI,
                            REFERENCE_TIME *prtAvgTime, DWORD *pDwAspectX, DWORD *pDwAspectY)
{
    REFERENCE_TIME rtAvg = 0;
    BITMAPINFOHEADER *bmi = nullptr;
    DWORD dwAspectX = 0, dwAspectY = 0;

    if (!format)
        goto done;

    if (*formattype == FORMAT_VideoInfo)
    {
        VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)format;
        rtAvg = vih->AvgTimePerFrame;
        bmi = &vih->bmiHeader;
    }
    else if (*formattype == FORMAT_VideoInfo2)
    {
        VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)format;
        rtAvg = vih2->AvgTimePerFrame;
        bmi = &vih2->bmiHeader;
        dwAspectX = vih2->dwPictAspectRatioX;
        dwAspectY = vih2->dwPictAspectRatioY;
    }
    else if (*formattype == FORMAT_MPEGVideo)
    {
        MPEG1VIDEOINFO *mp1vi = (MPEG1VIDEOINFO *)format;
        rtAvg = mp1vi->hdr.AvgTimePerFrame;
        bmi = &mp1vi->hdr.bmiHeader;
    }
    else if (*formattype == FORMAT_MPEG2Video)
    {
        MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)format;
        rtAvg = mp2vi->hdr.AvgTimePerFrame;
        bmi = &mp2vi->hdr.bmiHeader;
        dwAspectX = mp2vi->hdr.dwPictAspectRatioX;
        dwAspectY = mp2vi->hdr.dwPictAspectRatioY;
    }
    else
    {
        ASSERT(FALSE);
    }

done:
    if (pBMI)
    {
        *pBMI = bmi;
    }
    if (prtAvgTime)
    {
        *prtAvgTime = rtAvg;
    }
    if (pDwAspectX && pDwAspectY)
    {
        *pDwAspectX = dwAspectX;
        *pDwAspectY = dwAspectY;
    }
}

void audioFormatTypeHandler(const BYTE *format, const GUID *formattype, DWORD *pnSamples, WORD *pnChannels,
                            WORD *pnBitsPerSample, WORD *pnBlockAlign, DWORD *pnBytesPerSec, DWORD *pnChannelMask)
{
    DWORD nSamples = 0;
    WORD nChannels = 0;
    WORD nBitsPerSample = 0;
    WORD nBlockAlign = 0;
    DWORD nBytesPerSec = 0;
    DWORD nChannelMask = 0;

    if (!format)
        goto done;

    if (*formattype == FORMAT_WaveFormatEx)
    {
        WAVEFORMATEX *wfex = (WAVEFORMATEX *)format;
        nSamples = wfex->nSamplesPerSec;
        nChannels = wfex->nChannels;
        nBitsPerSample = wfex->wBitsPerSample;
        nBlockAlign = wfex->nBlockAlign;
        nBytesPerSec = wfex->nAvgBytesPerSec;

        if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wfex->cbSize >= 22)
        {
            WAVEFORMATEXTENSIBLE *wfexs = (WAVEFORMATEXTENSIBLE *)wfex;
            nChannelMask = wfexs->dwChannelMask;
        }
    }
    else if (*formattype == FORMAT_VorbisFormat2)
    {
        VORBISFORMAT2 *vf2 = (VORBISFORMAT2 *)format;
        nSamples = vf2->SamplesPerSec;
        nChannels = (WORD)vf2->Channels;
        nBitsPerSample = (WORD)vf2->BitsPerSample;
    }

done:
    if (pnSamples)
        *pnSamples = nSamples;
    if (pnChannels)
        *pnChannels = nChannels;
    if (pnBitsPerSample)
        *pnBitsPerSample = nBitsPerSample;
    if (pnBlockAlign)
        *pnBlockAlign = nBlockAlign;
    if (pnBytesPerSec)
        *pnBytesPerSec = nBytesPerSec;
    if (pnChannelMask)
        *pnChannelMask = nChannelMask;
}

void getExtraData(const AM_MEDIA_TYPE &mt, BYTE *extra, size_t *extralen)
{
    return getExtraData(mt.pbFormat, &mt.formattype, mt.cbFormat, extra, extralen);
}

void getExtraData(const BYTE *format, const GUID *formattype, const size_t formatlen, BYTE *extra, size_t *extralen)
{
    const BYTE *extraposition = nullptr;
    size_t extralength = 0;
    if (*formattype == FORMAT_WaveFormatEx)
    {
        WAVEFORMATEX *wfex = (WAVEFORMATEX *)format;
        extraposition = format + sizeof(WAVEFORMATEX);
        // Protected against over-reads
        extralength = formatlen - sizeof(WAVEFORMATEX);
    }
    else if (*formattype == FORMAT_VorbisFormat2)
    {
        VORBISFORMAT2 *vf2 = (VORBISFORMAT2 *)format;
        BYTE *start = nullptr, *end = nullptr;
        unsigned offset = 1;
        if (extra)
        {
            *extra = 2;
            offset += lav_xiphlacing(extra + offset, vf2->HeaderSize[0]);
            offset += lav_xiphlacing(extra + offset, vf2->HeaderSize[1]);
            extra += offset;
        }
        else
        {
            BYTE dummy[100];
            offset += lav_xiphlacing(dummy, vf2->HeaderSize[0]);
            offset += lav_xiphlacing(dummy, vf2->HeaderSize[1]);
        }
        extralength = vf2->HeaderSize[0] + vf2->HeaderSize[1] + vf2->HeaderSize[2];
        extralength = min(extralength, formatlen - sizeof(VORBISFORMAT2));

        if (extra && extralength)
            memcpy(extra, format + sizeof(VORBISFORMAT2), extralength);
        if (extralen)
            *extralen = extralength + offset;

        return;
    }
    else if (*formattype == FORMAT_VideoInfo)
    {
        extraposition = format + sizeof(VIDEOINFOHEADER);
        extralength = formatlen - sizeof(VIDEOINFOHEADER);
    }
    else if (*formattype == FORMAT_VideoInfo2)
    {
        extraposition = format + sizeof(VIDEOINFOHEADER2);
        extralength = formatlen - sizeof(VIDEOINFOHEADER2);
    }
    else if (*formattype == FORMAT_MPEGVideo)
    {
        MPEG1VIDEOINFO *mp1vi = (MPEG1VIDEOINFO *)format;
        extraposition = (BYTE *)mp1vi->bSequenceHeader;
        extralength = min(mp1vi->cbSequenceHeader, formatlen - FIELD_OFFSET(MPEG1VIDEOINFO, bSequenceHeader[0]));
    }
    else if (*formattype == FORMAT_MPEG2Video)
    {
        MPEG2VIDEOINFO *mp2vi = (MPEG2VIDEOINFO *)format;
        extraposition = (BYTE *)mp2vi->dwSequenceHeader;
        extralength = min(mp2vi->cbSequenceHeader, formatlen - FIELD_OFFSET(MPEG2VIDEOINFO, dwSequenceHeader[0]));
    }
    else if (*formattype == FORMAT_SubtitleInfo)
    {
        SUBTITLEINFO *sub = (SUBTITLEINFO *)format;
        extraposition = format + sub->dwOffset;
        extralength = formatlen - sub->dwOffset;
    }

    if (extra && extralength)
        memcpy(extra, extraposition, extralength);
    if (extralen)
        *extralen = extralength;
}

void CopyMediaSideDataFF(AVPacket *dst, const MediaSideDataFFMpeg **sd)
{
    if (!dst)
        return;

    if (!sd || !*sd)
    {
        dst->side_data = nullptr;
        dst->side_data_elems = 0;
        return;
    }

    // add sidedata to the packet
    for (int i = 0; i < (*sd)->side_data_elems; i++)
    {
        uint8_t *ptr = av_packet_new_side_data(dst, (*sd)->side_data[i].type, (*sd)->side_data[i].size);
        memcpy(ptr, (*sd)->side_data[i].data, (*sd)->side_data[i].size);
    }

    *sd = nullptr;
}

BOOL IsWindows7OrNewer()
{
    return (g_osInfo.dwMajorVersion == 6 && g_osInfo.dwMinorVersion >= 1) || (g_osInfo.dwMajorVersion > 6);
}

BOOL IsWindows8OrNewer()
{
    return (g_osInfo.dwMajorVersion == 6 && g_osInfo.dwMinorVersion >= 2) || (g_osInfo.dwMajorVersion > 6);
}

BOOL IsWindows10OrNewer()
{
    return (g_osInfo.dwMajorVersion >= 10);
}

BOOL IsWindows10BuildOrNewer(DWORD dwBuild)
{
    return (g_osInfo.dwMajorVersion > 10 || (g_osInfo.dwMajorVersion == 10 && g_osInfo.dwBuildNumber >= dwBuild));
}

void __cdecl debugprintf(LPCWSTR format, ...)
{
    WCHAR buf[4096], *p = buf;
    va_list args;
    int n;

    va_start(args, format);
    n = _vsnwprintf_s(p, 4096, 4096 - 3, format, args); // buf-3 is room for CR/LF/NUL
    va_end(args);

    p += (n < 0) ? (4096 - 3) : n;

    while (p > buf && isspace(p[-1]))
        *--p = L'\0';

    *p++ = L'\r';
    *p++ = L'\n';
    *p = L'\0';

    OutputDebugString(buf);
}

BOOL CheckApplicationBlackList(LPCTSTR subkey)
{
    HRESULT hr;
    DWORD dwVal;
    WCHAR fileName[1024];
    GetModuleFileName(NULL, fileName, 1024);
    WCHAR *processName = PathFindFileName(fileName);

    // Check local machine path
    CRegistry regLM = CRegistry(HKEY_LOCAL_MACHINE, subkey, hr, TRUE);
    if (SUCCEEDED(hr))
    {
        dwVal = regLM.ReadDWORD(processName, hr);
        return SUCCEEDED(hr) && dwVal;
    }

    // Check current user path
    CRegistry regCU = CRegistry(HKEY_CURRENT_USER, subkey, hr, TRUE);
    if (SUCCEEDED(hr))
    {
        dwVal = regCU.ReadDWORD(processName, hr);
        return SUCCEEDED(hr) && dwVal;
    }

    return FALSE;
}
