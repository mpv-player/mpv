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
#include <winternl.h>
#include <pthread.h>

#include "exclusive_hack.h"

// Missing NT API definitions
typedef enum _MP_MUTANT_INFORMATION_CLASS {
    MpMutantBasicInformation
} MP_MUTANT_INFORMATION_CLASS;
#define MUTANT_INFORMATION_CLASS MP_MUTANT_INFORMATION_CLASS
#define MutantBasicInformation MpMutantBasicInformation

typedef struct _MP_MUTANT_BASIC_INFORMATION {
    LONG CurrentCount;
    BOOLEAN OwnedByCaller;
    BOOLEAN AbandonedState;
} MP_MUTANT_BASIC_INFORMATION;
#define MUTANT_BASIC_INFORMATION MP_MUTANT_BASIC_INFORMATION

static pthread_once_t internal_api_load_ran = PTHREAD_ONCE_INIT;
static bool internal_api_loaded = false;

static HANDLE excl_mode_mutex;
static NTSTATUS (NTAPI *pNtQueryMutant)(HANDLE MutantHandle,
    MUTANT_INFORMATION_CLASS MutantInformationClass, PVOID MutantInformation,
    ULONG MutantInformationLength, PULONG ReturnLength);

static void internal_api_load(void)
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
        return;
    pNtQueryMutant = (void*)GetProcAddress(ntdll, "NtQueryMutant");
    if (!pNtQueryMutant)
        return;
    excl_mode_mutex = OpenMutexW(MUTANT_QUERY_STATE, FALSE,
        L"Local\\__DDrawExclMode__");
    if (!excl_mode_mutex)
        return;

    internal_api_loaded = true;
}

bool mp_w32_is_in_exclusive_mode(void)
{
    pthread_once(&internal_api_load_ran, internal_api_load);
    if (!internal_api_loaded)
        return false;

    // As far as we can tell, there is no way to know if a specific OpenGL
    // program is being redirected by the DWM. It is possible, however, to
    // know if some program on the computer is unredirected by the DWM, that
    // is, whether some program is in exclusive fullscreen mode. Exclusive
    // fullscreen programs acquire an undocumented mutex: __DDrawExclMode__. If
    // this is acquired, it's probably by mpv. Even if it isn't, the penalty
    // for incorrectly guessing true (dropped frames) is better than the
    // penalty for incorrectly guessing false (tearing.)

    // Testing this mutex is another problem. There is no public function for
    // testing a mutex without attempting to acquire it, but that method won't
    // work because if mpv is in fullscreen, the mutex will already be acquired
    // by this thread (in ddraw.dll) and Windows will happily let it be
    // acquired again. Instead, use the undocumented NtQueryMutant function to
    // test the mutex.

    // Note: SHQueryUserNotificationState uses this mutex internally, but it is
    // probably not suitable because it sends a message to the shell instead of
    // testing the mutex directly. mpv will check for exclusive mode once per
    // frame, so if the shell is not running or not responding, it may cause
    // performance issues.

    MUTANT_BASIC_INFORMATION mbi;
    NTSTATUS s = pNtQueryMutant(excl_mode_mutex, MutantBasicInformation, &mbi,
        sizeof mbi, NULL);
    if (!NT_SUCCESS(s))
        return false;

    return !mbi.CurrentCount;
}
