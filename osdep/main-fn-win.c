#include <windows.h>
#include <shellapi.h>

#ifndef BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE
#define BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE (0x0001)
#endif

#include "common/common.h"
#include "osdep/io.h"
#include "osdep/terminal.h"
#include "osdep/main-fn.h"

#ifndef HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION

#define HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION  1
enum { HeapOptimizeResources = 3 };

struct HEAP_OPTIMIZE_RESOURCES_INFORMATION {
    DWORD Version;
    DWORD Flags;
};

#endif

static void microsoft_nonsense(void)
{
    // stop Windows from showing all kinds of annoying error dialogs
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    // Enable heap corruption detection
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

    // Allow heap cache optimization and memory decommit
    struct HEAP_OPTIMIZE_RESOURCES_INFORMATION heap_info = {
        .Version = HEAP_OPTIMIZE_RESOURCES_CURRENT_VERSION
    };
    HeapSetInformation(NULL, HeapOptimizeResources, &heap_info,
                       sizeof(heap_info));

    // Always use safe search paths for DLLs and other files, ie. never use the
    // current directory
    SetDllDirectoryW(L"");
    SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE |
                      BASE_SEARCH_PATH_PERMANENT);
}

int main(int argc_, char **argv_)
{
    microsoft_nonsense();

    DWORD cproc_count = GetConsoleProcessList(&(DWORD){0}, 1);
    STARTUPINFOW si = { sizeof(si) };

    GetStartupInfoW(&si);
    bool use_stdhandles = si.dwFlags & STARTF_USESTDHANDLES;

    // Unless the standard IO handles have been inherited (MSYS2 console for
    // example), provide a UI when not attached to a console (see
    // osdep/win32-gui-wrapper.c), or when is the only process attached to the
    // console (e.g., started from Explorer or the Run dialog).
    bool gui = !use_stdhandles && cproc_count < 2;

    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    int argv_len = 0;
    char **argv_u8 = NULL;

    // Build mpv's UTF-8 argv, and add the pseudo-GUI profile if necessary
    if (argc > 0 && argv[0])
        MP_TARRAY_APPEND(NULL, argv_u8, argv_len, mp_to_utf8(argv_u8, argv[0]));
    if (gui) {
        MP_TARRAY_APPEND(NULL, argv_u8, argv_len, "--player-operation-mode=pseudo-gui");
        // Enable terminal output if attached to a console
        if (cproc_count)
            MP_TARRAY_APPEND(NULL, argv_u8, argv_len, "--terminal=yes");
    }
    for (int i = 1; i < argc; i++)
        MP_TARRAY_APPEND(NULL, argv_u8, argv_len, mp_to_utf8(argv_u8, argv[i]));
    MP_TARRAY_APPEND(NULL, argv_u8, argv_len, NULL);

    int ret = mpv_main(argv_len - 1, argv_u8);

    talloc_free(argv_u8);
    return ret;
}
