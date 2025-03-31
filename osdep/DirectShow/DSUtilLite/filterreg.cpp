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

#include "stdafx.h"
#include "DShowUtil.h"

std::wstring CStringFromGUID(const GUID &guid)
{
    WCHAR null[128] = {0}, buff[128];
    StringFromGUID2(GUID_NULL, null, 127);
    return std::wstring(StringFromGUID2(guid, buff, 127) > 0 ? buff : null);
}

// filter registration helpers

bool DeleteRegKey(std::wstring szKey, std::wstring szSubkey)
{
    bool bOK = false;

    HKEY hKey;
    LONG ec = ::RegOpenKeyEx(HKEY_CLASSES_ROOT, szKey.c_str(), 0, KEY_ALL_ACCESS, &hKey);
    if (ec == ERROR_SUCCESS)
    {
        if (szSubkey.length() > 0)
            ec = ::RegDeleteKey(hKey, szSubkey.c_str());

        bOK = (ec == ERROR_SUCCESS);

        ::RegCloseKey(hKey);
    }

    return bOK;
}

bool SetRegKeyValue(std::wstring szKey, std::wstring szSubkey, std::wstring szValueName, std::wstring szValue)
{
    bool bOK = false;

    if (szSubkey.length() > 0)
        szKey += _T("\\") + szSubkey;

    HKEY hKey;
    LONG ec =
        ::RegCreateKeyEx(HKEY_CLASSES_ROOT, szKey.c_str(), 0, 0, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &hKey, 0);
    if (ec == ERROR_SUCCESS)
    {
        if (szValue.length() > 0)
        {
            ec = ::RegSetValueEx(hKey, szValueName.c_str(), 0, REG_SZ,
                                 reinterpret_cast<BYTE *>(const_cast<LPTSTR>(szValue.c_str())),
                                 (DWORD)(_tcslen(szValue.c_str()) + 1) * sizeof(TCHAR));
        }

        bOK = (ec == ERROR_SUCCESS);

        ::RegCloseKey(hKey);
    }

    return bOK;
}

bool SetRegKeyValue(std::wstring szKey, std::wstring szSubkey, std::wstring szValue)
{
    return SetRegKeyValue(szKey, szSubkey, _T(""), szValue);
}

void RegisterSourceFilter(const CLSID &clsid, const GUID &subtype2, LPCWSTR chkbytes, ...)
{
    std::wstring null = CStringFromGUID(GUID_NULL);
    std::wstring majortype = CStringFromGUID(MEDIATYPE_Stream);
    std::wstring subtype = CStringFromGUID(subtype2);

    SetRegKeyValue(_T("Media Type\\") + majortype, subtype, _T("0"), chkbytes);
    SetRegKeyValue(_T("Media Type\\") + majortype, subtype, _T("Source Filter"), CStringFromGUID(clsid));

    DeleteRegKey(_T("Media Type\\") + null, subtype);

    va_list extensions;
    va_start(extensions, chkbytes);
    LPCWSTR ext = nullptr;
    while (ext = va_arg(extensions, LPCWSTR))
    {
        DeleteRegKey(_T("Media Type\\Extensions"), ext);
    }
    va_end(extensions);
}

void RegisterProtocolSourceFilter(const CLSID &clsid, LPCWSTR protocol)
{
    SetRegKeyValue(protocol, _T(""), _T("Source Filter"), CStringFromGUID(clsid));
}

void UnRegisterProtocolSourceFilter(LPCWSTR protocol)
{
    DeleteRegKey(protocol, _T(""));
}

void RegisterSourceFilter(const CLSID &clsid, const GUID &subtype2, std::list<LPCWSTR> chkbytes, ...)
{
    std::wstring null = CStringFromGUID(GUID_NULL);
    std::wstring majortype = CStringFromGUID(MEDIATYPE_Stream);
    std::wstring subtype = CStringFromGUID(subtype2);

    int i = 0;
    std::list<LPCWSTR>::iterator it;
    for (it = chkbytes.begin(); it != chkbytes.end(); ++it)
    {
        WCHAR idx[10] = {0};
        swprintf_s(idx, _T("%d"), i);
        SetRegKeyValue(_T("Media Type\\") + majortype, subtype, idx, *it);
        i++;
    }

    SetRegKeyValue(_T("Media Type\\") + majortype, subtype, _T("Source Filter"), CStringFromGUID(clsid));

    DeleteRegKey(_T("Media Type\\") + null, subtype);

    va_list extensions;
    va_start(extensions, chkbytes);
    LPCWSTR ext = nullptr;
    while (ext = va_arg(extensions, LPCWSTR))
    {
        DeleteRegKey(_T("Media Type\\Extensions"), ext);
    }
    va_end(extensions);
}

void UnRegisterSourceFilter(const GUID &subtype)
{
    DeleteRegKey(_T("Media Type\\") + CStringFromGUID(MEDIATYPE_Stream), CStringFromGUID(subtype));
}
