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
#include <sddl.h>

#include "config.h"

#include "osdep/io.h"
#include "osdep/threads.h"
#include "osdep/windows_utils.h"

#include "common/common.h"
#include "common/global.h"
#include "common/msg.h"
#include "input/input.h"
#include "libmpv/client.h"
#include "options/m_config.h"
#include "options/options.h"
#include "player/client.h"

struct mp_ipc_ctx {
    struct mp_log *log;
    struct mp_client_api *client_api;
    const wchar_t *path;

    pthread_t thread;
    HANDLE death_event;
};

struct client_arg {
    struct mp_log *log;
    struct mpv_handle *client;

    char *client_name;
    HANDLE client_h;
    bool writable;
    OVERLAPPED write_ol;
};

// Get a string SID representing the current user. Must be freed by LocalFree.
static char *get_user_sid(void)
{
    char *ssid = NULL;
    TOKEN_USER *info = NULL;
    HANDLE t;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t))
        goto done;

    DWORD info_len;
    if (!GetTokenInformation(t, TokenUser, NULL, 0, &info_len) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        goto done;

    info = talloc_size(NULL, info_len);
    if (!GetTokenInformation(t, TokenUser, info, info_len, &info_len))
        goto done;
    if (!info->User.Sid)
        goto done;

    ConvertSidToStringSidA(info->User.Sid, &ssid);
done:
    if (t)
        CloseHandle(t);
    talloc_free(info);
    return ssid;
}

// Get a string SID for the process integrity level. Must be freed by LocalFree.
static char *get_integrity_sid(void)
{
    char *ssid = NULL;
    TOKEN_MANDATORY_LABEL *info = NULL;
    HANDLE t;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t))
        goto done;

    DWORD info_len;
    if (!GetTokenInformation(t, TokenIntegrityLevel, NULL, 0, &info_len) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        goto done;

    info = talloc_size(NULL, info_len);
    if (!GetTokenInformation(t, TokenIntegrityLevel, info, info_len, &info_len))
        goto done;
    if (!info->Label.Sid)
        goto done;

    ConvertSidToStringSidA(info->Label.Sid, &ssid);
done:
    if (t)
        CloseHandle(t);
    talloc_free(info);
    return ssid;
}

// Create a security descriptor that only grants access to processes running
// under the current user at the current integrity level or higher
static PSECURITY_DESCRIPTOR create_restricted_sd(void)
{
    char *user_sid = get_user_sid();
    char *integrity_sid = get_integrity_sid();
    if (!user_sid || !integrity_sid)
        return NULL;

    char *sddl = talloc_asprintf(NULL,
        "O:%s"                 // Set the owner to user_sid
        "D:(A;;GRGW;;;%s)"     // Grant GENERIC_{READ,WRITE} access to user_sid
        "S:(ML;;NRNWNX;;;%s)", // Disallow read, write and execute permissions
                               // to integrity levels below integrity_sid
        user_sid, user_sid, integrity_sid);
    LocalFree(user_sid);
    LocalFree(integrity_sid);

    PSECURITY_DESCRIPTOR sd = NULL;
    ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl, SDDL_REVISION_1,
        &sd, NULL);
    talloc_free(sddl);

    return sd;
}

static void wakeup_cb(void *d)
{
    HANDLE event = d;
    SetEvent(event);
}

// Wrapper for ReadFile that treats ERROR_IO_PENDING as success
static DWORD async_read(HANDLE file, void *buf, unsigned size, OVERLAPPED* ol)
{
    DWORD err = ReadFile(file, buf, size, NULL, ol) ? 0 : GetLastError();
    return err == ERROR_IO_PENDING ? 0 : err;
}

// Wrapper for WriteFile that treats ERROR_IO_PENDING as success
static DWORD async_write(HANDLE file, const void *buf, unsigned size, OVERLAPPED* ol)
{
    DWORD err = WriteFile(file, buf, size, NULL, ol) ? 0 : GetLastError();
    return err == ERROR_IO_PENDING ? 0 : err;
}

static bool pipe_error_is_fatal(DWORD error)
{
    switch (error) {
    case 0:
    case ERROR_HANDLE_EOF:
    case ERROR_BROKEN_PIPE:
    case ERROR_PIPE_NOT_CONNECTED:
    case ERROR_NO_DATA:
        return false;
    }
    return true;
}

static DWORD ipc_write_str(struct client_arg *arg, const char *buf)
{
    DWORD error = 0;

    if ((error = async_write(arg->client_h, buf, strlen(buf), &arg->write_ol)))
        goto done;
    if (!GetOverlappedResult(arg->client_h, &arg->write_ol, &(DWORD){0}, TRUE)) {
        error = GetLastError();
        goto done;
    }

done:
    if (pipe_error_is_fatal(error)) {
        MP_VERBOSE(arg, "Error writing to pipe: %s\n",
            mp_HRESULT_to_str(HRESULT_FROM_WIN32(error)));
    }

    if (error)
        arg->writable = false;
    return error;
}

static void report_read_error(struct client_arg *arg, DWORD error)
{
    // Only report the error if it's not just due to the pipe closing
    if (pipe_error_is_fatal(error)) {
        MP_ERR(arg, "Error reading from pipe: %s\n",
            mp_HRESULT_to_str(HRESULT_FROM_WIN32(error)));
    } else {
        MP_VERBOSE(arg, "Client disconnected\n");
    }
}

static void *client_thread(void *p)
{
    pthread_detach(pthread_self());

    struct client_arg *arg = p;
    char buf[4096];
    HANDLE wakeup_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    OVERLAPPED ol = { .hEvent = CreateEventW(NULL, TRUE, TRUE, NULL) };
    bstr client_msg = { talloc_strdup(NULL, ""), 0 };
    DWORD ioerr = 0;
    DWORD r;

    mpthread_set_name(arg->client_name);

    arg->write_ol.hEvent = CreateEventW(NULL, TRUE, TRUE, NULL);
    if (!wakeup_event || !ol.hEvent || !arg->write_ol.hEvent) {
        MP_ERR(arg, "Couldn't create events\n");
        goto done;
    }

    MP_VERBOSE(arg, "Client connected\n");

    mpv_set_wakeup_callback(arg->client, wakeup_cb, wakeup_event);

    // Do the first read operation on the pipe
    if ((ioerr = async_read(arg->client_h, buf, 4096, &ol))) {
        report_read_error(arg, ioerr);
        goto done;
    }

    while (1) {
        HANDLE handles[] = { wakeup_event, ol.hEvent };
        int n = WaitForMultipleObjects(2, handles, FALSE, 0);
        if (n == WAIT_TIMEOUT)
            n = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        switch (n) {
        case WAIT_OBJECT_0: // wakeup_event
            ResetEvent(wakeup_event);

            while (1) {
                mpv_event *event = mpv_wait_event(arg->client, 0);

                if (event->event_id == MPV_EVENT_NONE)
                    break;

                if (event->event_id == MPV_EVENT_SHUTDOWN)
                    goto done;

                if (!arg->writable)
                    continue;

                char *event_msg = mp_json_encode_event(event);
                if (!event_msg) {
                    MP_ERR(arg, "Encoding error\n");
                    goto done;
                }

                ipc_write_str(arg, event_msg);
                talloc_free(event_msg);
            }

            break;
        case WAIT_OBJECT_0 + 1: // ol.hEvent
            // Complete the read operation on the pipe
            if (!GetOverlappedResult(arg->client_h, &ol, &r, TRUE)) {
                report_read_error(arg, GetLastError());
                goto done;
            }

            bstr_xappend(NULL, &client_msg, (bstr){buf, r});
            while (bstrchr(client_msg, '\n') != -1) {
                char *reply_msg = mp_ipc_consume_next_command(arg->client,
                    NULL, &client_msg);
                if (reply_msg && arg->writable)
                    ipc_write_str(arg, reply_msg);
                talloc_free(reply_msg);
            }

            // Begin the next read operation on the pipe
            if ((ioerr = async_read(arg->client_h, buf, 4096, &ol))) {
                report_read_error(arg, ioerr);
                goto done;
            }
            break;
        default:
            MP_ERR(arg, "WaitForMultipleObjects failed\n");
            goto done;
        }
    }

done:
    if (client_msg.len > 0)
        MP_WARN(arg, "Ignoring unterminated command on disconnect.\n");

    if (CancelIoEx(arg->client_h, &ol) || GetLastError() != ERROR_NOT_FOUND)
        GetOverlappedResult(arg->client_h, &ol, &(DWORD){0}, TRUE);
    if (wakeup_event)
        CloseHandle(wakeup_event);
    if (ol.hEvent)
        CloseHandle(ol.hEvent);
    if (arg->write_ol.hEvent)
        CloseHandle(arg->write_ol.hEvent);

    CloseHandle(arg->client_h);
    mpv_destroy(arg->client);
    talloc_free(arg);
    return NULL;
}

static void ipc_start_client(struct mp_ipc_ctx *ctx, struct client_arg *client)
{
    client->client = mp_new_client(ctx->client_api, client->client_name),
    client->log    = mp_client_get_log(client->client);

    pthread_t client_thr;
    if (pthread_create(&client_thr, NULL, client_thread, client)) {
        mpv_destroy(client->client);
        CloseHandle(client->client_h);
        talloc_free(client);
    }
}

static void ipc_start_client_json(struct mp_ipc_ctx *ctx, int id, HANDLE h)
{
    struct client_arg *client = talloc_ptrtype(NULL, client);
    *client = (struct client_arg){
        .client_name = talloc_asprintf(client, "ipc-%d", id),
        .client_h = h,
        .writable = true,
    };

    ipc_start_client(ctx, client);
}

static void *ipc_thread(void *p)
{
    // Use PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE so message framing is
    // maintained for message-mode clients, but byte-mode clients can still
    // connect, send and receive data. This is the most compatible mode.
    static const DWORD state =
        PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE | PIPE_WAIT |
        PIPE_REJECT_REMOTE_CLIENTS;
    static const DWORD mode =
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
    static const DWORD bufsiz = 4096;

    struct mp_ipc_ctx *arg = p;
    HANDLE server = INVALID_HANDLE_VALUE;
    HANDLE client = INVALID_HANDLE_VALUE;
    int client_num = 0;

    mpthread_set_name("ipc named pipe listener");
    MP_VERBOSE(arg, "Starting IPC master\n");

    SECURITY_ATTRIBUTES sa = {
        .nLength = sizeof sa,
        .lpSecurityDescriptor = create_restricted_sd(),
    };
    if (!sa.lpSecurityDescriptor) {
        MP_ERR(arg, "Couldn't create security descriptor");
        goto done;
    }

    OVERLAPPED ol = { .hEvent = CreateEventW(NULL, TRUE, TRUE, NULL) };
    if (!ol.hEvent) {
        MP_ERR(arg, "Couldn't create event");
        goto done;
    }

    server = CreateNamedPipeW(arg->path, mode | FILE_FLAG_FIRST_PIPE_INSTANCE,
        state, PIPE_UNLIMITED_INSTANCES, bufsiz, bufsiz, 0, &sa);
    if (server == INVALID_HANDLE_VALUE) {
        MP_ERR(arg, "Couldn't create first pipe instance: %s\n",
            mp_LastError_to_str());
        goto done;
    }

    MP_VERBOSE(arg, "Listening to IPC pipe.\n");

    while (1) {
        DWORD err = ConnectNamedPipe(server, &ol) ? 0 : GetLastError();

        if (err == ERROR_IO_PENDING) {
            int n = WaitForMultipleObjects(2, (HANDLE[]) {
                arg->death_event,
                ol.hEvent,
            }, FALSE, INFINITE) - WAIT_OBJECT_0;

            switch (n) {
            case 0:
                // Stop waiting for new clients
                CancelIo(server);
                GetOverlappedResult(server, &ol, &(DWORD){0}, TRUE);
                goto done;
            case 1:
                // Complete the ConnectNamedPipe request
                err = GetOverlappedResult(server, &ol, &(DWORD){0}, TRUE)
                    ? 0 : GetLastError();
                break;
            default:
                MP_ERR(arg, "WaitForMultipleObjects failed\n");
                goto done;
            }
        }

        // ERROR_PIPE_CONNECTED is returned if a client connects before
        // ConnectNamedPipe is called. ERROR_NO_DATA is returned if a client
        // connects, (possibly) writes data and exits before ConnectNamedPipe
        // is called. Both cases should be handled as normal connections.
        if (err == ERROR_PIPE_CONNECTED || err == ERROR_NO_DATA)
            err = 0;

        if (err) {
            MP_ERR(arg, "ConnectNamedPipe failed: %s\n",
                mp_HRESULT_to_str(HRESULT_FROM_WIN32(err)));
            goto done;
        }

        // Create the next pipe instance before the client thread to avoid the
        // theoretical race condition where the client thread immediately
        // closes the handle and there are no active instances of the pipe
        client = server;
        server = CreateNamedPipeW(arg->path, mode, state,
            PIPE_UNLIMITED_INSTANCES, bufsiz, bufsiz, 0, &sa);
        if (server == INVALID_HANDLE_VALUE) {
            MP_ERR(arg, "Couldn't create additional pipe instance: %s\n",
                mp_LastError_to_str());
            goto done;
        }

        ipc_start_client_json(arg, client_num++, client);
        client = NULL;
    }

done:
    if (sa.lpSecurityDescriptor)
        LocalFree(sa.lpSecurityDescriptor);
    if (client != INVALID_HANDLE_VALUE)
        CloseHandle(client);
    if (server != INVALID_HANDLE_VALUE)
        CloseHandle(server);
    if (ol.hEvent)
        CloseHandle(ol.hEvent);
    return NULL;
}

struct mp_ipc_ctx *mp_init_ipc(struct mp_client_api *client_api,
                               struct mpv_global *global)
{
    struct MPOpts *opts = mp_get_config_group(NULL, global, GLOBAL_CONFIG);

    struct mp_ipc_ctx *arg = talloc_ptrtype(NULL, arg);
    *arg = (struct mp_ipc_ctx){
        .log = mp_log_new(arg, global->log, "ipc"),
        .client_api = client_api,
    };

    if (!opts->ipc_path || !*opts->ipc_path)
        goto out;

    // Ensure the path is a legal Win32 pipe name by prepending \\.\pipe\ if
    // it's not already present. Qt's QLocalSocket uses the same logic, so
    // cross-platform programs that use paths like /tmp/mpv-socket should just
    // work. (Win32 converts this path to \Device\NamedPipe\tmp\mpv-socket)
    if (!strncmp(opts->ipc_path, "\\\\.\\pipe\\", 9)) {
        arg->path = mp_from_utf8(arg, opts->ipc_path);
    } else {
        char *path = talloc_asprintf(NULL, "\\\\.\\pipe\\%s", opts->ipc_path);
        arg->path = mp_from_utf8(arg, path);
        talloc_free(path);
    }

    if (!(arg->death_event = CreateEventW(NULL, TRUE, FALSE, NULL)))
        goto out;

    if (pthread_create(&arg->thread, NULL, ipc_thread, arg))
        goto out;

    talloc_free(opts);
    return arg;

out:
    if (arg->death_event)
        CloseHandle(arg->death_event);
    talloc_free(arg);
    talloc_free(opts);
    return NULL;
}

void mp_uninit_ipc(struct mp_ipc_ctx *arg)
{
    if (!arg)
        return;

    SetEvent(arg->death_event);
    pthread_join(arg->thread, NULL);

    CloseHandle(arg->death_event);
    talloc_free(arg);
}
