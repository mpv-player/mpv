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

// Class template: Re-sizable array.

// To grow or shrink the array, call SetSize().
// To pre-allocate the array, call Allocate().

// Notes:
// Copy constructor and assignment operator are private, to avoid throwing exceptions. (One could easily modify this.)
// It is the caller's responsibility to release the objects in the array. The array's destuctor does not release them.
// The array does not actually shrink when SetSize is called with a smaller size. Only the reported size changes.

#pragma once

#include <assert.h>
#include "DShowUtil.h"

template <class T> class GrowableArray
{
  public:
    GrowableArray() {}

    virtual ~GrowableArray() { free(m_pArray); }

    // Allocate: Reserves memory for the array, but does not increase the count.
    HRESULT Allocate(DWORD alloc)
    {
        HRESULT hr = S_OK;
        if (alloc > m_allocated || !m_pArray)
        {
            T *pNew = (T *)realloc(m_pArray, sizeof(T) * alloc);
            if (!pNew)
            {
                free(m_pArray);
                m_pArray = nullptr;
                m_allocated = 0;
                return E_OUTOFMEMORY;
            }
            m_pArray = pNew;
            ZeroMemory(m_pArray + m_allocated, (alloc - m_allocated) * sizeof(T));
            m_allocated = alloc;
        }
        return hr;
    }

    HRESULT Clear()
    {
        free(m_pArray);
        m_pArray = nullptr;
        m_count = m_allocated = 0;
        return S_OK;
    }

    // SetSize: Changes the count, and grows the array if needed.
    HRESULT SetSize(DWORD count)
    {
        HRESULT hr = S_OK;
        if (count > m_allocated)
        {
            hr = Allocate(count);
        }
        if (SUCCEEDED(hr))
        {
            m_count = count;
        }
        return hr;
    }

    HRESULT Append(GrowableArray<T> *other) { return Append(other->Ptr(), other->GetCount()); }

    HRESULT Append(const T *other, DWORD dwSize)
    {
        HRESULT hr = S_OK;
        DWORD old = GetCount();
        hr = SetSize(old + dwSize);
        if (SUCCEEDED(hr))
            memcpy(m_pArray + old, other, dwSize);

        return S_OK;
    }

    void Consume(DWORD dwSize)
    {
        ASSERT(dwSize <= m_count);

        if (dwSize == m_count)
            Clear();
        else
        {
            memmove(m_pArray, m_pArray + dwSize, m_count - dwSize);
            m_count -= dwSize;
        }
    }

    DWORD GetCount() const { return m_count; }
    DWORD GetAllocated() const { return m_allocated; }

    // Accessor.
    T &operator[](DWORD index)
    {
        assert(index < m_count);
        return m_pArray[index];
    }

    // Const accessor.
    const T &operator[](DWORD index) const
    {
        assert(index < m_count);
        return m_pArray[index];
    }

    // Return the underlying array.
    T *Ptr() { return m_pArray; }

  protected:
    GrowableArray &operator=(const GrowableArray &r);
    GrowableArray(const GrowableArray &r);

    T *m_pArray = nullptr;
    DWORD m_count = 0;     // Nominal count.
    DWORD m_allocated = 0; // Actual allocation size.
};
