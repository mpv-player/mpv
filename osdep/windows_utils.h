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

#ifndef MP_WINDOWS_UTILS_H_
#define MP_WINDOWS_UTILS_H_

#include <windows.h>

char *mp_GUID_to_str_buf(char *buf, size_t buf_size, const GUID *guid);
#define mp_GUID_to_str(guid) mp_GUID_to_str_buf((char[40]){0}, 40, (guid))
char *mp_HRESULT_to_str_buf(char *buf, size_t buf_size, HRESULT hr);
#define mp_HRESULT_to_str(hr) mp_HRESULT_to_str_buf((char[256]){0}, 256, (hr))
#define mp_LastError_to_str() mp_HRESULT_to_str(HRESULT_FROM_WIN32(GetLastError()))

// copied from mingw-w64-x86_64-headers-git 5.0.0.4621.a7e64f9-1 in msys2
FORCEINLINE BOOL IsWindowsVersionOrGreater(WORD major, WORD minor, WORD servpack)
{
    OSVERSIONINFOEXW vi = {sizeof(vi),major,minor,0,0,{0},servpack};
    return VerifyVersionInfoW(&vi, VER_MAJORVERSION|VER_MINORVERSION|VER_SERVICEPACKMAJOR,
        VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
            VER_MAJORVERSION,VER_GREATER_EQUAL),
            VER_MINORVERSION,VER_GREATER_EQUAL),
            VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL));
}

FORCEINLINE BOOL IsWindows7OrGreater(void) {
    return IsWindowsVersionOrGreater(HIBYTE(_WIN32_WINNT_WIN7), LOBYTE(_WIN32_WINNT_WIN7), 0);
}

#endif
