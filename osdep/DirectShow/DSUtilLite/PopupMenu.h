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

class CPopupMenu
{
  public:
    CPopupMenu(void);
    virtual ~CPopupMenu(void);

    HRESULT AddItem(UINT id, LPWSTR caption, BOOL checked = FALSE, BOOL enabled = TRUE);
    HRESULT AddSeparator();
    HRESULT AddSubmenu(HMENU hSubMenu, LPWSTR caption);

    HMENU Finish();

  private:
    HMENU m_hMenu = 0;
    int order = 0;
};
