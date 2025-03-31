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

#include <string>

bool CreateRegistryKey(HKEY hKeyRoot, LPCTSTR pszSubKey);

class CRegistry
{
  public:
    CRegistry();
    CRegistry(HKEY hkeyRoot, LPCTSTR pszSubKey, HRESULT &hr, BOOL bReadOnly = FALSE, BOOL b64Bit = TRUE);
    ~CRegistry();

    HRESULT Open(HKEY hkeyRoot, LPCTSTR pszSubKey, BOOL bReadOnly = FALSE, BOOL b64Bit = TRUE);

    std::wstring ReadString(LPCTSTR pszKey, HRESULT &hr);
    HRESULT WriteString(LPCTSTR pszKey, LPCTSTR pszValue);

    DWORD ReadDWORD(LPCTSTR pszKey, HRESULT &hr);
    HRESULT WriteDWORD(LPCTSTR pszKey, DWORD dwValue);

    BOOL ReadBOOL(LPCTSTR pszKey, HRESULT &hr);
    HRESULT WriteBOOL(LPCTSTR pszKey, BOOL bValue);

    BYTE *ReadBinary(LPCTSTR pszKey, DWORD &dwSize, HRESULT &hr);
    HRESULT WriteBinary(LPCTSTR pszKey, const BYTE *pbValue, int iLen);

    HRESULT DeleteKey(LPCTSTR pszKey);

  private:
    HKEY *m_key = nullptr;
};
