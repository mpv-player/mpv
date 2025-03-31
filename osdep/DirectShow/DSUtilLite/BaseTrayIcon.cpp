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
#include "BaseTrayIcon.h"
#include "BaseDSPropPage.h"

#include <time.h>
#include <process.h>
#include <Shlwapi.h>

#define TRAYICON 0x1f0
#define MSG_TRAYICON WM_USER + 1
#define MSG_QUIT WM_USER + 2

// The assumed size of the propery page
#define PROP_WIDTH_OFFSET 400
#define PROP_HEIGHT_OFFSET 250

static const WCHAR *noTrayProcesses[] = {L"dllhost.exe", L"explorer.exe", L"ReClockHelper.dll"};

BOOL CBaseTrayIcon::ProcessBlackList()
{
    WCHAR fileName[1024];
    GetModuleFileName(nullptr, fileName, 1024);
    WCHAR *processName = PathFindFileName(fileName);

    for (int i = 0; i < countof(noTrayProcesses); i++)
    {
        if (_wcsicmp(processName, noTrayProcesses[i]) == 0)
            return TRUE;
    }
    return FALSE;
}

CBaseTrayIcon::CBaseTrayIcon(IBaseFilter *pFilter, const WCHAR *wszName, int resIcon)
    : m_pFilter(pFilter)
    , m_wszName(wszName)
    , m_resIcon(resIcon)
{
    memset(&m_NotifyIconData, 0, sizeof(m_NotifyIconData));
    m_evSetupFinished.Reset();
    StartMessageThread();
}

CBaseTrayIcon::~CBaseTrayIcon(void)
{
    m_pFilter = nullptr;
    if (m_hWnd)
    {
        SendMessage(m_hWnd, MSG_QUIT, 0, 0);
        WaitForSingleObject(m_hThread, INFINITE);
    }
    CloseHandle(m_hThread);
}

HRESULT CBaseTrayIcon::StartMessageThread()
{
    m_hThread = (HANDLE)_beginthreadex(nullptr,           /* Security */
                                       0,                 /* Stack Size */
                                       InitialThreadProc, /* Thread process */
                                       (LPVOID)this,      /* Arguments */
                                       0,                 /* 0 = Start Immediately */
                                       nullptr            /* Thread Address */
    );
    m_evSetupFinished.Wait();
    return S_OK;
}

unsigned int WINAPI CBaseTrayIcon::InitialThreadProc(LPVOID pv)
{
    HRESULT hrCo = CoInitialize(nullptr);

    CBaseTrayIcon *pTrayIcon = (CBaseTrayIcon *)pv;
    unsigned int ret = pTrayIcon->TrayMessageThread();

    if (SUCCEEDED(hrCo))
        CoUninitialize();

    return ret;
}

DWORD CBaseTrayIcon::TrayMessageThread()
{
    HRESULT hr;
    MSG msg;

    // Create the Window Class if it doesn't exist yet
    hr = RegisterWindowClass();
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"CBaseTrayIcon::ThreadProc(): Failed to register window class"));
    }

    // And the Window we use for messages
    hr = CreateMessageWindow();
    if (FAILED(hr))
    {
        DbgLog((LOG_TRACE, 10, L"CBaseTrayIcon::ThreadProc(): Failed to create message window"));
        m_evSetupFinished.Set();
        UnregisterClass(m_wszClassName, g_hInst);
        return 1;
    }
    ASSERT(m_hWnd);

    CreateTrayIconData();
    Shell_NotifyIcon(NIM_ADD, &m_NotifyIconData);
    Shell_NotifyIcon(NIM_SETVERSION, &m_NotifyIconData);

    m_evSetupFinished.Set();

    // Message loop
    while (GetMessage(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Shell_NotifyIcon(NIM_DELETE, &m_NotifyIconData);
    // Free icon resources
    if (m_NotifyIconData.hIcon)
    {
        DestroyIcon(m_NotifyIconData.hIcon);
        m_NotifyIconData.hIcon = nullptr;
    }

    // Unregister the window class we used
    UnregisterClass(m_wszClassName, g_hInst);

    return 0;
}

HRESULT CBaseTrayIcon::RegisterWindowClass()
{
    swprintf_s(m_wszClassName, countof(m_wszClassName), L"LAVTrayIconClass%d", GetCurrentThreadId());

    WNDCLASSEX wx = {};
    wx.cbSize = sizeof(WNDCLASSEX);
    wx.lpfnWndProc = WindowProc;
    wx.hInstance = g_hInst;
    wx.lpszClassName = m_wszClassName;
    ATOM wndClass = RegisterClassEx(&wx);

    return !wndClass ? E_FAIL : S_OK;
}

HRESULT CBaseTrayIcon::CreateMessageWindow()
{
    m_hWnd = CreateWindowEx(0, m_wszClassName, L"LAV Tray Message Window", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                            nullptr, nullptr);
    SetWindowLongPtr(m_hWnd, GWLP_USERDATA, LONG_PTR(this));
    return m_hWnd == nullptr ? E_FAIL : S_OK;
}

HRESULT CBaseTrayIcon::CreateTrayIconData()
{
    memset(&m_NotifyIconData, 0, sizeof(m_NotifyIconData));
    m_NotifyIconData.cbSize = sizeof(m_NotifyIconData);
    m_NotifyIconData.hWnd = m_hWnd;
    m_NotifyIconData.uID = TRAYICON;
    m_NotifyIconData.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    m_NotifyIconData.hIcon = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(m_resIcon), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    m_NotifyIconData.uCallbackMessage = MSG_TRAYICON;
    m_NotifyIconData.uVersion = NOTIFYICON_VERSION;
    wcscpy_s(m_NotifyIconData.szTip, m_wszName);

    return S_OK;
}

HRESULT CBaseTrayIcon::OpenPropPage()
{
    CheckPointer(m_pFilter, E_UNEXPECTED);
    m_bPropPageOpen = TRUE;
    RECT desktopRect;
    GetWindowRect(GetDesktopWindow(), &desktopRect);
    SetWindowPos(m_hWnd, 0, (desktopRect.right / 2) - PROP_WIDTH_OFFSET, (desktopRect.bottom / 2) - PROP_HEIGHT_OFFSET,
                 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    CBaseDSPropPage::ShowPropPageDialog(m_pFilter, m_hWnd);
    m_bPropPageOpen = FALSE;
    return S_OK;
}

static BOOL CALLBACK enumWindowCallback(HWND hwnd, LPARAM lparam)
{
    HWND owner = (HWND)lparam;
    if (owner == GetWindow(hwnd, GW_OWNER))
    {
        SetForegroundWindow(hwnd);
        return FALSE;
    }
    return TRUE;
}

LRESULT CALLBACK CBaseTrayIcon::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CBaseTrayIcon *icon = (CBaseTrayIcon *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (uMsg)
    {
    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &icon->m_NotifyIconData);
        PostQuitMessage(0);
        break;
    case MSG_QUIT: DestroyWindow(hwnd); break;
    case MSG_TRAYICON: {
        UINT trayMsg = LOWORD(lParam);
        if (icon)
        {
            switch (trayMsg)
            {
            case WM_LBUTTONUP:
                if (!icon->m_bPropPageOpen)
                {
                    icon->OpenPropPage();
                }
                else
                {
                    EnumThreadWindows(GetCurrentThreadId(), enumWindowCallback, (LPARAM)icon->m_hWnd);
                }
                break;
            case WM_RBUTTONUP:
            case WM_CONTEXTMENU:
                if (icon->m_bPropPageOpen)
                {
                    break;
                }
                HMENU hMenu = icon->GetPopupMenu();
                if (hMenu)
                {
                    POINT p;
                    GetCursorPos(&p);
                    SetForegroundWindow(hwnd);
                    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD, p.x, p.y, 0, hwnd, nullptr);
                    PostMessage(hwnd, WM_NULL, 0, 0);
                    icon->ProcessMenuCommand(hMenu, cmd);
                    DestroyMenu(hMenu);
                }
                break;
            }
        }
    }
    break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
