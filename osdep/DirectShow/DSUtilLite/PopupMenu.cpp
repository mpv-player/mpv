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
#include "PopupMenu.h"

CPopupMenu::CPopupMenu(void)
{
    m_hMenu = CreatePopupMenu();
}

CPopupMenu::~CPopupMenu(void)
{
    if (m_hMenu)
        DestroyMenu(m_hMenu);
}

HRESULT CPopupMenu::AddItem(UINT id, LPWSTR caption, BOOL checked, BOOL enabled)
{
    if (!m_hMenu)
        return E_UNEXPECTED;
    MENUITEMINFO mii;
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_ID | MIIM_STATE | MIIM_FTYPE | MIIM_STRING;
    mii.fType = MFT_STRING | MFT_RADIOCHECK;
    mii.wID = id;
    mii.fState = (checked ? MFS_CHECKED : 0) | (!enabled ? MFS_DISABLED : 0);
    mii.dwTypeData = caption;
    mii.cch = (UINT)wcslen(mii.dwTypeData);
    InsertMenuItem(m_hMenu, order++, TRUE, &mii);
    return S_OK;
}

HRESULT CPopupMenu::AddSeparator()
{
    if (!m_hMenu)
        return E_UNEXPECTED;
    MENUITEMINFO mii;
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_TYPE;
    mii.fType = MFT_SEPARATOR;
    InsertMenuItem(m_hMenu, order++, TRUE, &mii);
    return S_OK;
}

HRESULT CPopupMenu::AddSubmenu(HMENU hSubMenu, LPWSTR caption)
{
    if (!m_hMenu)
        return E_UNEXPECTED;
    MENUITEMINFO mii;
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_SUBMENU;
    mii.fType = MFT_STRING;
    mii.hSubMenu = hSubMenu;
    mii.dwTypeData = caption;
    mii.cch = (UINT)wcslen(mii.dwTypeData);
    InsertMenuItem(m_hMenu, order++, TRUE, &mii);
    return S_OK;
}

HMENU CPopupMenu::Finish()
{
    HMENU hMenu = m_hMenu;
    m_hMenu = nullptr;
    return hMenu;
}
