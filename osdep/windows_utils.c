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

#include <stdio.h>
#include <inttypes.h>

#include <windows.h>
#include <errors.h>
#include <audioclient.h>
#include <d3d9.h>
#include <dxgi1_2.h>

#include "common/common.h"
#include "osdep/atomic.h"
#include "windows_utils.h"

char *mp_GUID_to_str_buf(char *buf, size_t buf_size, const GUID *guid)
{
    snprintf(buf, buf_size,
             "{%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x}",
             (unsigned) guid->Data1, guid->Data2, guid->Data3,
             guid->Data4[0], guid->Data4[1],
             guid->Data4[2], guid->Data4[3],
             guid->Data4[4], guid->Data4[5],
             guid->Data4[6], guid->Data4[7]);
    return buf;
}

static char *hresult_to_str(const HRESULT hr)
{
#define E(x) case x : return # x ;
    switch (hr) {
    E(S_OK)
    E(S_FALSE)
    E(E_FAIL)
    E(E_OUTOFMEMORY)
    E(E_POINTER)
    E(E_HANDLE)
    E(E_NOTIMPL)
    E(E_INVALIDARG)
    E(E_PROP_ID_UNSUPPORTED)
    E(E_NOINTERFACE)
    E(REGDB_E_IIDNOTREG)
    E(CO_E_NOTINITIALIZED)
    E(AUDCLNT_E_NOT_INITIALIZED)
    E(AUDCLNT_E_ALREADY_INITIALIZED)
    E(AUDCLNT_E_WRONG_ENDPOINT_TYPE)
    E(AUDCLNT_E_DEVICE_INVALIDATED)
    E(AUDCLNT_E_NOT_STOPPED)
    E(AUDCLNT_E_BUFFER_TOO_LARGE)
    E(AUDCLNT_E_OUT_OF_ORDER)
    E(AUDCLNT_E_UNSUPPORTED_FORMAT)
    E(AUDCLNT_E_INVALID_SIZE)
    E(AUDCLNT_E_DEVICE_IN_USE)
    E(AUDCLNT_E_BUFFER_OPERATION_PENDING)
    E(AUDCLNT_E_THREAD_NOT_REGISTERED)
    E(AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED)
    E(AUDCLNT_E_ENDPOINT_CREATE_FAILED)
    E(AUDCLNT_E_SERVICE_NOT_RUNNING)
    E(AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED)
    E(AUDCLNT_E_EXCLUSIVE_MODE_ONLY)
    E(AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL)
    E(AUDCLNT_E_EVENTHANDLE_NOT_SET)
    E(AUDCLNT_E_INCORRECT_BUFFER_SIZE)
    E(AUDCLNT_E_BUFFER_SIZE_ERROR)
    E(AUDCLNT_E_CPUUSAGE_EXCEEDED)
    E(AUDCLNT_E_BUFFER_ERROR)
    E(AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
    E(AUDCLNT_E_INVALID_DEVICE_PERIOD)
    E(AUDCLNT_E_INVALID_STREAM_FLAG)
    E(AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE)
    E(AUDCLNT_E_RESOURCES_INVALIDATED)
    E(AUDCLNT_S_BUFFER_EMPTY)
    E(AUDCLNT_S_THREAD_ALREADY_REGISTERED)
    E(AUDCLNT_S_POSITION_STALLED)
    E(D3DERR_WRONGTEXTUREFORMAT)
    E(D3DERR_UNSUPPORTEDCOLOROPERATION)
    E(D3DERR_UNSUPPORTEDCOLORARG)
    E(D3DERR_UNSUPPORTEDALPHAOPERATION)
    E(D3DERR_UNSUPPORTEDALPHAARG)
    E(D3DERR_TOOMANYOPERATIONS)
    E(D3DERR_CONFLICTINGTEXTUREFILTER)
    E(D3DERR_UNSUPPORTEDFACTORVALUE)
    E(D3DERR_CONFLICTINGRENDERSTATE)
    E(D3DERR_UNSUPPORTEDTEXTUREFILTER)
    E(D3DERR_CONFLICTINGTEXTUREPALETTE)
    E(D3DERR_DRIVERINTERNALERROR)
    E(D3DERR_NOTFOUND)
    E(D3DERR_MOREDATA)
    E(D3DERR_DEVICELOST)
    E(D3DERR_DEVICENOTRESET)
    E(D3DERR_NOTAVAILABLE)
    E(D3DERR_OUTOFVIDEOMEMORY)
    E(D3DERR_INVALIDDEVICE)
    E(D3DERR_INVALIDCALL)
    E(D3DERR_DRIVERINVALIDCALL)
    E(D3DERR_WASSTILLDRAWING)
    E(D3DOK_NOAUTOGEN)
    E(D3DERR_DEVICEREMOVED)
    E(D3DERR_DEVICEHUNG)
    E(S_NOT_RESIDENT)
    E(S_RESIDENT_IN_SHARED_MEMORY)
    E(S_PRESENT_MODE_CHANGED)
    E(S_PRESENT_OCCLUDED)
    E(D3DERR_UNSUPPORTEDOVERLAY)
    E(D3DERR_UNSUPPORTEDOVERLAYFORMAT)
    E(D3DERR_CANNOTPROTECTCONTENT)
    E(D3DERR_UNSUPPORTEDCRYPTO)
    E(D3DERR_PRESENT_STATISTICS_DISJOINT)
    E(DXGI_ERROR_DEVICE_HUNG)
    E(DXGI_ERROR_DEVICE_REMOVED)
    E(DXGI_ERROR_DEVICE_RESET)
    E(DXGI_ERROR_DRIVER_INTERNAL_ERROR)
    E(DXGI_ERROR_INVALID_CALL)
    E(DXGI_ERROR_WAS_STILL_DRAWING)
    E(DXGI_STATUS_OCCLUDED)
    default:
        return "<Unknown>";
    }
#undef E
}

static char *fmtmsg_buf(char *buf, size_t buf_size, DWORD errorID)
{
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             NULL, errorID, 0, buf, buf_size, NULL);
    if (!n && GetLastError() == ERROR_MORE_DATA) {
        snprintf(buf, buf_size,
                 "<Insufficient buffer size (%zd) for error message>",
                 buf_size);
    } else {
        if (n > 0 && buf[n-1] == '\n')
            buf[n-1] = '\0';
        if (n > 1 && buf[n-2] == '\r')
            buf[n-2] = '\0';
    }
    return buf;
}
#define fmtmsg(hr) fmtmsg_buf((char[243]){0}, 243, (hr))

char *mp_HRESULT_to_str_buf(char *buf, size_t buf_size, HRESULT hr)
{
    char* msg = fmtmsg(hr);
    msg = msg[0] ? msg : hresult_to_str(hr);
    snprintf(buf, buf_size, "%s (0x%"PRIx32")", msg, (uint32_t)hr);
    return buf;
}

bool mp_w32_create_anon_pipe(HANDLE *server, HANDLE *client,
                             struct w32_create_anon_pipe_opts *opts)
{
    static atomic_ulong counter = ATOMIC_VAR_INIT(0);

    // Generate pipe name
    unsigned long id = atomic_fetch_add(&counter, 1);
    unsigned pid = GetCurrentProcessId();
    wchar_t buf[36];
    swprintf(buf, MP_ARRAY_SIZE(buf), L"\\\\.\\pipe\\mpv-anon-%08x-%08lx",
             pid, id);

    DWORD client_access = 0;
    DWORD out_buffer = opts->out_buf_size;
    DWORD in_buffer = opts->in_buf_size;

    if (opts->server_flags & PIPE_ACCESS_INBOUND) {
        client_access |= FILE_GENERIC_WRITE | FILE_READ_ATTRIBUTES;
        if (!in_buffer)
            in_buffer = 4096;
    }
    if (opts->server_flags & PIPE_ACCESS_OUTBOUND) {
        client_access |= FILE_GENERIC_READ | FILE_WRITE_ATTRIBUTES;
        if (!out_buffer)
            out_buffer = 4096;
    }

    SECURITY_ATTRIBUTES inherit_sa = {
        .nLength = sizeof inherit_sa,
        .bInheritHandle = TRUE,
    };

    // The function for creating anonymous pipes (CreatePipe) can't create
    // overlapped pipes, so instead, use a named pipe with a unique name
    *server = CreateNamedPipeW(buf,
                               opts->server_flags | FILE_FLAG_FIRST_PIPE_INSTANCE,
                               opts->server_mode | PIPE_REJECT_REMOTE_CLIENTS,
                               1, out_buffer, in_buffer, 0,
                               opts->server_inheritable ? &inherit_sa : NULL);
    if (*server == INVALID_HANDLE_VALUE)
        goto error;

    // Open the write end of the pipe as a synchronous handle
    *client = CreateFileW(buf, client_access, 0,
                          opts->client_inheritable ? &inherit_sa : NULL,
                          OPEN_EXISTING,
                          opts->client_flags | SECURITY_SQOS_PRESENT |
                          SECURITY_ANONYMOUS, NULL);
    if (*client == INVALID_HANDLE_VALUE) {
        CloseHandle(*server);
        goto error;
    }

    if (opts->client_mode) {
        if (!SetNamedPipeHandleState(*client, &opts->client_mode, NULL, NULL)) {
            CloseHandle(*server);
            CloseHandle(*client);
            goto error;
        }
    }

    return true;
error:
    *server = *client = INVALID_HANDLE_VALUE;
    return false;
}
