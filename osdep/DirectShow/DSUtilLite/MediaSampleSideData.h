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

#pragma once

#include <map>

#include "amfilter.h"
#include "IMediaSideData.h"

struct SideDataGUIDComparer
{
    bool operator()(const GUID &Left, const GUID &Right) const
    {
        // comparison logic goes here
        return memcmp(&Left, &Right, sizeof(Right)) < 0;
    }
};

class CMediaSampleSideData
    : public CMediaSample
    , public IMediaSideData
{
  public:
    CMediaSampleSideData(LPCTSTR pName, CBaseAllocator *pAllocator, HRESULT *phr, LPBYTE pBuffer, LONG length);
    virtual ~CMediaSampleSideData();

    STDMETHODIMP QueryInterface(REFIID riid, __deref_out void **ppv);
    STDMETHODIMP_(ULONG) Release();

    // IMediaSideData
    STDMETHODIMP SetSideData(GUID guidType, const BYTE *pData, size_t size);
    STDMETHODIMP GetSideData(GUID guidType, const BYTE **pData, size_t *pSize);

  private:
    void ReleaseSideData();

    struct SideDataEntry
    {
        BYTE *pData;
        size_t size;
    };

    CCritSec m_csSideData;
    std::map<GUID, SideDataEntry, SideDataGUIDComparer> m_SideData;
};
