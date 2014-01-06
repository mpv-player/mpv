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

void cr_runproc(wchar_t *name, wchar_t *cmdline)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

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
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

const wchar_t *cr_getargs(const wchar_t *cmd)
{
    wchar_t *args;

    if (cmd[0] == '"') {
        /* If the first argument starts with a quote, it always ends at the
           next quote */
        args = wcschr(cmd + 1, '"');

        if (!args)
            return L"";
        args ++;
    } else {
        /* If the first argument is unquoted, it always ends at the next space.
           Unlike the other arguments, it can't contain embedded quotes. */
        args = wcspbrk(cmd, L" \t");
        if (!args)
            return L"";
    }

    /* Skip whitespace */
    while (*args == ' ' || *args == '\t')
        args ++;

    return args;
}

int wmain(int argc, wchar_t **argv, wchar_t **envp)
{
    const wchar_t *args;
    wchar_t *cmd, *eargs;
    wchar_t exe[MAX_PATH];
    size_t len;

    cmd = GetCommandLineW();
    args = cr_getargs(cmd);

    GetModuleFileNameW(NULL, exe, MAX_PATH);
    wcscpy(wcsrchr(exe, '.') + 1, L"exe");

    len = wcslen(exe) + wcslen(args) + 4;
    eargs = malloc(len * sizeof(wchar_t));
    swprintf(eargs, len, L"\"%s\" %s", exe, args);

    cr_runproc(exe, eargs);

    free(eargs);
    return 0;
}
