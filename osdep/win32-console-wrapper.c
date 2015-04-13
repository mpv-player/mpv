/*
 * conredir, a hack to get working console IO with Windows GUI applications
 *
 * Copyright (c) 2013, Martin Herkt
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <windows.h>

void cr_perror(const wchar_t *prefix)
{
    wchar_t *error;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&error, 0, NULL);

    fwprintf(stderr, L"%s: %s", prefix, error);
    LocalFree(error);
}

int cr_runproc(wchar_t *name, wchar_t *cmdline)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    DWORD retval = 1;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(name, cmdline, NULL, NULL, TRUE, 0,
                        NULL, NULL, &si, &pi)) {

        cr_perror(L"CreateProcess");
    } else {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &retval);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return (int)retval;
}

int wmain(int argc, wchar_t **argv, wchar_t **envp)
{
    wchar_t *cmd;
    wchar_t exe[MAX_PATH];

    cmd = GetCommandLineW();
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    wcscpy(wcsrchr(exe, '.') + 1, L"exe");

    // Set an environment variable so the child process can tell whether it
    // was started from this wrapper and attach to the console accordingly
    SetEnvironmentVariableW(L"_started_from_console", L"yes");

    return cr_runproc(exe, cmd);
}
