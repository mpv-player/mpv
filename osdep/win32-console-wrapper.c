/*
 * conredir, a hack to get working console IO with Windows GUI applications
 *
 * Copyright (c) 2013, Martin Herkt
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <windows.h>

int wmain(int argc, wchar_t **argv, wchar_t **envp);

static void cr_perror(const wchar_t *prefix)
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

static int cr_runproc(wchar_t *name, wchar_t *cmdline)
{
    STARTUPINFOW si;
    STARTUPINFOW our_si;
    PROCESS_INFORMATION pi;
    DWORD retval = 1;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    // Copy the list of inherited CRT file descriptors to the new process
    our_si.cb = sizeof(our_si);
    GetStartupInfoW(&our_si);
    si.lpReserved2 = our_si.lpReserved2;
    si.cbReserved2 = our_si.cbReserved2;

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
