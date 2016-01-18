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
    default:
        return "<Unknown>";
    }
#undef E
}

char *mp_HRESULT_to_str_buf(char *buf, size_t buf_size, HRESULT hr)
{
    snprintf(buf, buf_size, "%s (0x%"PRIx32")",
             hresult_to_str(hr), (uint32_t) hr);
    return buf;
}
