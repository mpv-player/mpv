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
#include <string.h>

#include "osdep/subprocess.h"

#include "osdep/io.h"
#include "osdep/windows_utils.h"

#include "mpv_talloc.h"
#include "common/common.h"
#include "stream/stream.h"
#include "misc/bstr.h"
#include "misc/thread_tools.h"

// Internal CRT FD flags
#define FOPEN (0x01)
#define FPIPE (0x08)
#define FDEV  (0x40)

static void write_arg(bstr *cmdline, char *arg)
{
    // Empty args must be represented as an empty quoted string
    if (!arg[0]) {
        bstr_xappend(NULL, cmdline, bstr0("\"\""));
        return;
    }

    // If the string doesn't have characters that need to be escaped, it's best
    // to leave it alone for the sake of Windows programs that don't process
    // quoted args correctly.
    if (!strpbrk(arg, " \t\"")) {
        bstr_xappend(NULL, cmdline, bstr0(arg));
        return;
    }

    // If there are characters that need to be escaped, write a quoted string
    bstr_xappend(NULL, cmdline, bstr0("\""));

    // Escape the argument. To match the behavior of CommandLineToArgvW,
    // backslashes are only escaped if they appear before a quote or the end of
    // the string.
    int num_slashes = 0;
    for (int pos = 0; arg[pos]; pos++) {
        switch (arg[pos]) {
        case '\\':
            // Count consecutive backslashes
            num_slashes++;
            break;
        case '"':
            // Write the argument up to the point before the quote
            bstr_xappend(NULL, cmdline, (struct bstr){arg, pos});
            arg += pos;
            pos = 0;

            // Double backslashes preceding the quote
            for (int i = 0; i < num_slashes; i++)
                bstr_xappend(NULL, cmdline, bstr0("\\"));
            num_slashes = 0;

            // Escape the quote itself
            bstr_xappend(NULL, cmdline, bstr0("\\"));
            break;
        default:
            num_slashes = 0;
        }
    }

    // Write the rest of the argument
    bstr_xappend(NULL, cmdline, bstr0(arg));

    // Double backslashes at the end of the argument
    for (int i = 0; i < num_slashes; i++)
        bstr_xappend(NULL, cmdline, bstr0("\\"));

    bstr_xappend(NULL, cmdline, bstr0("\""));
}

// Convert an array of arguments to a properly escaped command-line string
static wchar_t *write_cmdline(void *ctx, char *argv0, char **args)
{
    // argv0 should always be quoted. Otherwise, arguments may be interpreted as
    // part of the program name. Also, it can't contain escape sequences.
    bstr cmdline = {0};
    bstr_xappend_asprintf(NULL, &cmdline, "\"%s\"", argv0);

    if (args) {
        for (int i = 0; args[i]; i++) {
            bstr_xappend(NULL, &cmdline, bstr0(" "));
            write_arg(&cmdline, args[i]);
        }
    }

    wchar_t *wcmdline = mp_from_utf8(ctx, cmdline.start);
    talloc_free(cmdline.start);
    return wcmdline;
}

static void delete_handle_list(void *p)
{
    LPPROC_THREAD_ATTRIBUTE_LIST list = p;
    DeleteProcThreadAttributeList(list);
}

// Create a PROC_THREAD_ATTRIBUTE_LIST that specifies exactly which handles are
// inherited by the subprocess
static LPPROC_THREAD_ATTRIBUTE_LIST create_handle_list(void *ctx,
                                                       HANDLE *handles, int num)
{
    // Get required attribute list size
    SIZE_T size = 0;
    if (!InitializeProcThreadAttributeList(NULL, 1, 0, &size)) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            return NULL;
    }

    // Allocate attribute list
    LPPROC_THREAD_ATTRIBUTE_LIST list = talloc_size(ctx, size);
    if (!InitializeProcThreadAttributeList(list, 1, 0, &size))
        goto error;
    talloc_set_destructor(list, delete_handle_list);

    if (!UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   handles, num * sizeof(HANDLE), NULL, NULL))
        goto error;

    return list;
error:
    talloc_free(list);
    return NULL;
}

// Helper method similar to sparse_poll, skips NULL handles
static int sparse_wait(HANDLE *handles, unsigned num_handles)
{
    unsigned w_num_handles = 0;
    HANDLE w_handles[MP_SUBPROCESS_MAX_FDS + 2];
    int map[MP_SUBPROCESS_MAX_FDS + 2];
    if (num_handles > MP_ARRAY_SIZE(w_handles))
        return -1;

    for (unsigned i = 0; i < num_handles; i++) {
        if (!handles[i])
            continue;

        w_handles[w_num_handles] = handles[i];
        map[w_num_handles] = i;
        w_num_handles++;
    }

    if (w_num_handles == 0)
        return -1;
    DWORD i = WaitForMultipleObjects(w_num_handles, w_handles, FALSE, INFINITE);
    i -= WAIT_OBJECT_0;

    if (i >= w_num_handles)
        return -1;
    return map[i];
}

// Wrapper for ReadFile that treats ERROR_IO_PENDING as success
static int async_read(HANDLE file, void *buf, unsigned size, OVERLAPPED* ol)
{
    if (!ReadFile(file, buf, size, NULL, ol))
        return (GetLastError() == ERROR_IO_PENDING) ? 0 : -1;
    return 0;
}

static bool is_valid_handle(HANDLE h)
{
    // _get_osfhandle can return -2 "when the file descriptor is not associated
    // with a stream"
    return h && h != INVALID_HANDLE_VALUE && (intptr_t)h != -2;
}

static wchar_t *convert_environ(void *ctx, char **env)
{
    // Environment size in wchar_ts, including the trailing NUL
    size_t env_size = 1;

    for (int i = 0; env[i]; i++) {
        int count = MultiByteToWideChar(CP_UTF8, 0, env[i], -1, NULL, 0);
        if (count <= 0)
            abort();
        env_size += count;
    }

    wchar_t *ret = talloc_array(ctx, wchar_t, env_size);
    size_t pos = 0;

    for (int i = 0; env[i]; i++) {
        int count = MultiByteToWideChar(CP_UTF8, 0, env[i], -1,
                                        ret + pos, env_size - pos);
        if (count <= 0)
            abort();
        pos += count;
    }

    return ret;
}

void mp_subprocess2(struct mp_subprocess_opts *opts,
                    struct mp_subprocess_result *res)
{
    wchar_t *tmp = talloc_new(NULL);
    DWORD r;

    HANDLE share_hndls[MP_SUBPROCESS_MAX_FDS] = {0};
    int share_hndl_count = 0;
    HANDLE wait_hndls[MP_SUBPROCESS_MAX_FDS + 2] = {0};
    int wait_hndl_count = 0;

    struct {
        HANDLE handle;
        bool handle_close;
        char crt_flags;

        HANDLE read;
        OVERLAPPED read_ol;
        char *read_buf;
    } fd_data[MP_SUBPROCESS_MAX_FDS] = {0};

    // The maximum target FD is limited because FDs have to fit in two sparse
    // arrays in STARTUPINFO.lpReserved2, which has a maximum size of 65535
    // bytes. The first four bytes are the handle count, followed by one byte
    // per handle for flags, and an intptr_t per handle for the HANDLE itself.
    static const int crt_fd_max = (65535 - sizeof(int)) / (1 + sizeof(intptr_t));
    int crt_fd_count = 0;

    // If the function exits before CreateProcess, there was an init error
    *res = (struct mp_subprocess_result){ .error = MP_SUBPROCESS_EINIT };

    STARTUPINFOEXW si = {
        .StartupInfo = {
            .cb = sizeof si,
            .dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK,
        },
    };

    for (int n = 0; n < opts->num_fds; n++) {
        if (opts->fds[n].fd >= crt_fd_max) {
            // Target FD is too big to fit in the CRT FD array
            res->error = MP_SUBPROCESS_EUNSUPPORTED;
            goto done;
        }

        if (opts->fds[n].fd >= crt_fd_count)
            crt_fd_count = opts->fds[n].fd + 1;

        if (opts->fds[n].src_fd >= 0) {
            HANDLE src_handle = (HANDLE)_get_osfhandle(opts->fds[n].src_fd);

            // Invalid handles are just ignored. This is because sometimes the
            // standard handles are invalid in Windows, like in GUI processes.
            // In this case mp_subprocess2 callers should still be able to
            // blindly forward the standard FDs.
            if (!is_valid_handle(src_handle))
                continue;

            DWORD type = GetFileType(src_handle);
            bool is_console_handle = false;
            switch (type & 0xff) {
            case FILE_TYPE_DISK:
                fd_data[n].crt_flags = FOPEN;
                break;
            case FILE_TYPE_CHAR:
                fd_data[n].crt_flags = FOPEN | FDEV;
                is_console_handle = GetConsoleMode(src_handle, &(DWORD){0});
                break;
            case FILE_TYPE_PIPE:
                fd_data[n].crt_flags = FOPEN | FPIPE;
                break;
            case FILE_TYPE_UNKNOWN:
                continue;
            }

            if (is_console_handle) {
                // Some Windows versions have bugs when duplicating console
                // handles, or when adding console handles to the CreateProcess
                // handle list, so just use the handle directly for now. Console
                // handles treat inheritance weirdly, so this should still work.
                fd_data[n].handle = src_handle;
            } else {
                // Instead of making the source handle inheritable, just
                // duplicate it to an inheritable handle
                if (!DuplicateHandle(GetCurrentProcess(), src_handle,
                                     GetCurrentProcess(), &fd_data[n].handle, 0,
                                     TRUE, DUPLICATE_SAME_ACCESS))
                    goto done;
                fd_data[n].handle_close = true;

                share_hndls[share_hndl_count++] = fd_data[n].handle;
            }

        } else if (opts->fds[n].on_read && !opts->detach) {
            fd_data[n].read_ol.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
            if (!fd_data[n].read_ol.hEvent)
                goto done;

            struct w32_create_anon_pipe_opts o = {
                .server_flags = PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                .client_inheritable = true,
            };
            if (!mp_w32_create_anon_pipe(&fd_data[n].read, &fd_data[n].handle, &o))
                goto done;
            fd_data[n].handle_close = true;

            wait_hndls[n] = fd_data[n].read_ol.hEvent;
            wait_hndl_count++;

            fd_data[n].crt_flags = FOPEN | FPIPE;
            fd_data[n].read_buf = talloc_size(tmp, 4096);

            share_hndls[share_hndl_count++] = fd_data[n].handle;

        } else {
            DWORD access;
            if (opts->fds[n].fd == 0) {
                access = FILE_GENERIC_READ;
            } else if (opts->fds[n].fd <= 2) {
                access = FILE_GENERIC_WRITE | FILE_READ_ATTRIBUTES;
            } else {
                access = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
            }

            SECURITY_ATTRIBUTES sa = {
                .nLength = sizeof sa,
                .bInheritHandle = TRUE,
            };
            fd_data[n].crt_flags = FOPEN | FDEV;
            fd_data[n].handle = CreateFileW(L"NUL", access,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            &sa, OPEN_EXISTING, 0, NULL);
            fd_data[n].handle_close = true;
        }

        switch (opts->fds[n].fd) {
        case 0:
            si.StartupInfo.hStdInput = fd_data[n].handle;
            break;
        case 1:
            si.StartupInfo.hStdOutput = fd_data[n].handle;
            break;
        case 2:
            si.StartupInfo.hStdError = fd_data[n].handle;
            break;
        }
    }

    // Convert the UTF-8 environment into a UTF-16 Windows environment block
    wchar_t *env = NULL;
    if (opts->env)
        env = convert_environ(tmp, opts->env);

    // Convert the args array to a UTF-16 Windows command-line string
    char **args = opts->args && opts->args[0] ? &opts->args[1] : 0;
    wchar_t *cmdline = write_cmdline(tmp, opts->exe, args);

    // Get pointers to the arrays in lpReserved2. This is an undocumented data
    // structure used by MSVCRT (and other frameworks and runtimes) to emulate
    // FD inheritance. The format is unofficially documented here:
    // https://www.catch22.net/tuts/undocumented-createprocess
    si.StartupInfo.cbReserved2 = sizeof(int) + crt_fd_count * (1 + sizeof(intptr_t));
    si.StartupInfo.lpReserved2 = talloc_size(tmp, si.StartupInfo.cbReserved2);
    char *crt_buf_flags = si.StartupInfo.lpReserved2 + sizeof(int);
    char *crt_buf_hndls = crt_buf_flags + crt_fd_count;

    memcpy(si.StartupInfo.lpReserved2, &crt_fd_count, sizeof(int));

    // Fill the handle array with INVALID_HANDLE_VALUE, for unassigned handles
    for (int n = 0; n < crt_fd_count; n++) {
        HANDLE h = INVALID_HANDLE_VALUE;
        memcpy(crt_buf_hndls + n * sizeof(intptr_t), &h, sizeof(intptr_t));
    }

    for (int n = 0; n < opts->num_fds; n++) {
        crt_buf_flags[opts->fds[n].fd] = fd_data[n].crt_flags;
        memcpy(crt_buf_hndls + opts->fds[n].fd * sizeof(intptr_t),
               &fd_data[n].handle, sizeof(intptr_t));
    }

    DWORD flags = CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT;
    PROCESS_INFORMATION pi = {0};

    // Specify which handles are inherited by the subprocess. If this isn't
    // specified, the subprocess inherits all inheritable handles, which could
    // include handles created by other threads. See:
    // http://blogs.msdn.com/b/oldnewthing/archive/2011/12/16/10248328.aspx
    si.lpAttributeList = create_handle_list(tmp, share_hndls, share_hndl_count);

    // If we have a console, the subprocess will automatically attach to it so
    // it can receive Ctrl+C events. If we don't have a console, prevent the
    // subprocess from creating its own console window by specifying
    // CREATE_NO_WINDOW. GetConsoleCP() can be used to reliably determine if we
    // have a console or not (Cygwin uses it too.)
    if (!GetConsoleCP())
        flags |= CREATE_NO_WINDOW;

    if (!CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, flags, env, NULL,
                        &si.StartupInfo, &pi))
        goto done;
    talloc_free(cmdline);
    talloc_free(env);
    talloc_free(si.StartupInfo.lpReserved2);
    talloc_free(si.lpAttributeList);
    CloseHandle(pi.hThread);

    for (int n = 0; n < opts->num_fds; n++) {
        if (fd_data[n].handle_close && is_valid_handle(fd_data[n].handle))
            CloseHandle(fd_data[n].handle);
        fd_data[n].handle = NULL;

        if (fd_data[n].read) {
            // Do the first read operation on each pipe
            if (async_read(fd_data[n].read, fd_data[n].read_buf, 4096,
                           &fd_data[n].read_ol))
            {
                CloseHandle(fd_data[n].read);
                wait_hndls[n] = fd_data[n].read = NULL;
                wait_hndl_count--;
            }
        }
    }

    if (opts->detach) {
        res->error = MP_SUBPROCESS_OK;
        goto done;
    }

    res->error = MP_SUBPROCESS_EGENERIC;

    wait_hndls[MP_SUBPROCESS_MAX_FDS] = pi.hProcess;
    wait_hndl_count++;

    if (opts->cancel)
        wait_hndls[MP_SUBPROCESS_MAX_FDS + 1] = mp_cancel_get_event(opts->cancel);

    DWORD exit_code;
    while (wait_hndl_count) {
        int n = sparse_wait(wait_hndls, MP_ARRAY_SIZE(wait_hndls));

        if (n >= 0 && n < MP_SUBPROCESS_MAX_FDS) {
            // Complete the read operation on the pipe
            if (!GetOverlappedResult(fd_data[n].read, &fd_data[n].read_ol, &r, TRUE)) {
                CloseHandle(fd_data[n].read);
                wait_hndls[n] = fd_data[n].read = NULL;
                wait_hndl_count--;
            } else {
                opts->fds[n].on_read(opts->fds[n].on_read_ctx,
                                     fd_data[n].read_buf, r);

                // Begin the next read operation on the pipe
                if (async_read(fd_data[n].read, fd_data[n].read_buf, 4096,
                               &fd_data[n].read_ol))
                {
                    CloseHandle(fd_data[n].read);
                    wait_hndls[n] = fd_data[n].read = NULL;
                    wait_hndl_count--;
                }
            }

        } else if (n == MP_SUBPROCESS_MAX_FDS) { // pi.hProcess
            GetExitCodeProcess(pi.hProcess, &exit_code);
            res->exit_status = exit_code;

            CloseHandle(pi.hProcess);
            wait_hndls[n] = pi.hProcess = NULL;
            wait_hndl_count--;

        } else if (n == MP_SUBPROCESS_MAX_FDS + 1) { // opts.cancel
            if (pi.hProcess) {
                TerminateProcess(pi.hProcess, 1);
                res->error = MP_SUBPROCESS_EKILLED_BY_US;
                goto done;
            }
        } else {
            goto done;
        }
    }

    res->error = MP_SUBPROCESS_OK;

done:
    for (int n = 0; n < opts->num_fds; n++) {
        if (is_valid_handle(fd_data[n].read)) {
            // Cancel any pending I/O (if the process was killed)
            CancelIo(fd_data[n].read);
            GetOverlappedResult(fd_data[n].read, &fd_data[n].read_ol, &r, TRUE);
            CloseHandle(fd_data[n].read);
        }
        if (fd_data[n].handle_close && is_valid_handle(fd_data[n].handle))
            CloseHandle(fd_data[n].handle);
        if (fd_data[n].read_ol.hEvent)
            CloseHandle(fd_data[n].read_ol.hEvent);
    }
    if (pi.hProcess)
        CloseHandle(pi.hProcess);
    talloc_free(tmp);
}
