/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <windows.h>

int wmain(int argc, wchar_t **argv, wchar_t **envp);

static void cr_perror(const wchar_t *target)
{
    wchar_t *error;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_ARGUMENT_ARRAY,
                   NULL, GetLastError(), 0, (LPWSTR)&error, 0, (va_list*)&target);

    MessageBoxW(NULL, error, NULL, MB_ICONSTOP);
    LocalFree(error);
}

static int cr_runproc(wchar_t *name, wchar_t *cmdline)
{
    DWORD retval = 1;
    PROCESS_INFORMATION pi = { 0 };
    STARTUPINFOW si = { sizeof(si) };
    STARTUPINFOW our_si = { sizeof(our_si) };
    HANDLE hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hStdError = GetStdHandle(STD_ERROR_HANDLE);

    GetStartupInfoW(&our_si);

    // Copy the list of inherited CRT file descriptors to the new process
    si.lpReserved2 = our_si.lpReserved2;
    si.cbReserved2 = our_si.cbReserved2;

    // Copy any inherited stdio handles
    if (hStdInput != INVALID_HANDLE_VALUE)
        si.hStdInput = hStdInput;
    if (hStdOutput != INVALID_HANDLE_VALUE)
        si.hStdOutput = hStdOutput;
    if (hStdError != INVALID_HANDLE_VALUE)
        si.hStdError = hStdError;
    if (si.hStdInput || si.hStdOutput || si.hStdError)
        si.dwFlags |= STARTF_USESTDHANDLES;

    // If set, copy the window show state (minimized, maximized, etc.)
    if (our_si.wShowWindow && our_si.dwFlags & STARTF_USESHOWWINDOW) {
        si.wShowWindow = our_si.wShowWindow;
        si.dwFlags |= STARTF_USESHOWWINDOW;
    }

    if (!CreateProcessW(name, cmdline, NULL, NULL, TRUE, DETACHED_PROCESS,
                        NULL, NULL, &si, &pi)) {

        cr_perror(name);
    } else {
        BringWindowToTop(pi.hProcess);
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
    wchar_t com[MAX_PATH];

    cmd = GetCommandLineW();

    GetModuleFileNameW(NULL, com, MAX_PATH);
    wcscpy(wcsrchr(com, '.') + 1, L"com");

    return cr_runproc(com, cmd);
}
