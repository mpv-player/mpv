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
#include "FontInstaller.h"

CFontInstaller::CFontInstaller()
    : pAddFontMemResourceEx(nullptr)
    , pRemoveFontMemResourceEx(nullptr)
{
    if (HMODULE hGdi = GetModuleHandle(_T("gdi32.dll")))
    {
        pAddFontMemResourceEx =
            (HANDLE(WINAPI *)(PVOID, DWORD, PVOID, DWORD *))GetProcAddress(hGdi, "AddFontMemResourceEx");
        pRemoveFontMemResourceEx = (BOOL(WINAPI *)(HANDLE))GetProcAddress(hGdi, "RemoveFontMemResourceEx");
    }
}

CFontInstaller::~CFontInstaller()
{
    UninstallFonts();
}

bool CFontInstaller::InstallFont(const void *pData, UINT len)
{
    return InstallFontMemory(pData, len);
}

void CFontInstaller::UninstallFonts()
{
    if (pRemoveFontMemResourceEx)
    {
        std::vector<HANDLE>::iterator it;
        for (it = m_fonts.begin(); it != m_fonts.end(); ++it)
        {
            pRemoveFontMemResourceEx(*it);
        }
        m_fonts.clear();
    }
}

bool CFontInstaller::InstallFontMemory(const void *pData, UINT len)
{
    if (!pAddFontMemResourceEx)
    {
        return false;
    }

    DWORD nFonts = 0;
    HANDLE hFont = pAddFontMemResourceEx((PVOID)pData, len, nullptr, &nFonts);
    if (hFont && nFonts > 0)
    {
        m_fonts.push_back(hFont);
    }
    return hFont && nFonts > 0;
}
