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
#include "MediaSampleSideData.h"

CMediaSampleSideData::CMediaSampleSideData(LPCTSTR pName, CBaseAllocator *pAllocator, HRESULT *phr, LPBYTE pBuffer,
                                           LONG length)
    : CMediaSample(pName, pAllocator, phr, pBuffer, length)
{
}

CMediaSampleSideData::~CMediaSampleSideData()
{
    ReleaseSideData();
}

STDMETHODIMP CMediaSampleSideData::QueryInterface(REFIID riid, __deref_out void **ppv)
{
    CheckPointer(ppv, E_POINTER);
    ValidateReadWritePtr(ppv, sizeof(PVOID));

    if (riid == __uuidof(IMediaSideData))
    {
        return GetInterface((IMediaSideData *)this, ppv);
    }
    else
    {
        return __super::QueryInterface(riid, ppv);
    }
}

STDMETHODIMP_(ULONG) CMediaSampleSideData::Release()
{
    /* Decrement our own private reference count */
    LONG lRef;
    if (m_cRef == 1)
    {
        lRef = 0;
        m_cRef = 0;
    }
    else
    {
        lRef = InterlockedDecrement(&m_cRef);
    }
    ASSERT(lRef >= 0);

    /* Did we release our final reference count */
    if (lRef == 0)
    {
        /* Free all resources */
        if (m_dwFlags & Sample_TypeChanged)
        {
            SetMediaType(NULL);
        }
        ASSERT(m_pMediaType == NULL);
        m_dwFlags = 0;
        m_dwTypeSpecificFlags = 0;
        m_dwStreamId = AM_STREAM_MEDIA;

        ReleaseSideData();

        /* This may cause us to be deleted */
        // Our refcount is reliably 0 thus no-one will mess with us
        m_pAllocator->ReleaseBuffer(this);
    }
    return (ULONG)lRef;
}

void CMediaSampleSideData::ReleaseSideData()
{
    CAutoLock Lock(&m_csSideData);

    for (auto it = m_SideData.begin(); it != m_SideData.end(); it++)
    {
        SideDataEntry *sd = &(it->second);
        _aligned_free(sd->pData);
    }

    m_SideData.clear();
}

// IMediaSideData
STDMETHODIMP CMediaSampleSideData::SetSideData(GUID guidType, const BYTE *pData, size_t size)
{
    if (!pData || !size)
        return E_POINTER;

    CAutoLock Lock(&m_csSideData);

    auto it = m_SideData.find(guidType);
    if (it != m_SideData.end())
    {
        SideDataEntry *sd = &(it->second);
        BYTE *newData = (BYTE *)_aligned_realloc(sd->pData, size, 16);
        if (newData)
        {
            sd->size = size;
            sd->pData = newData;
            memcpy(newData, pData, size);
        }
        else
        {
            return E_OUTOFMEMORY;
        }
    }
    else
    {
        SideDataEntry sd;
        sd.pData = (BYTE *)_aligned_malloc(size, 16);
        if (sd.pData)
        {
            sd.size = size;
            memcpy(sd.pData, pData, size);
        }
        else
        {
            return E_OUTOFMEMORY;
        }

        m_SideData[guidType] = sd;
    }

    return S_OK;
}

STDMETHODIMP CMediaSampleSideData::GetSideData(GUID guidType, const BYTE **pData, size_t *pSize)
{
    if (!pData || !pSize)
        return E_POINTER;

    CAutoLock Lock(&m_csSideData);

    auto it = m_SideData.find(guidType);
    if (it != m_SideData.end())
    {
        *pData = it->second.pData;
        *pSize = it->second.size;

        return S_OK;
    }

    return E_FAIL;
}
