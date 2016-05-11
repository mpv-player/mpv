#include <pthread.h>
#include <windows.h>

#define ANGLE_NO_ALIASES
#include "angle_dynamic.h"

#include "common/common.h"

#define ANGLE_DECL(NAME, VAR) \
    VAR;
ANGLE_FNS(ANGLE_DECL)

static bool angle_loaded;
static pthread_once_t angle_load_once = PTHREAD_ONCE_INIT;

static void angle_do_load(void)
{
    // Note: we let this handle "leak", as the functions remain valid forever.
    HANDLE angle_dll = LoadLibraryW(L"LIBEGL.DLL");
    if (!angle_dll)
        return;
#define ANGLE_LOAD_ENTRY(NAME, VAR) \
    MP_CONCAT(PFN_, NAME) = (void *)GetProcAddress(angle_dll, #NAME); \
    if (!MP_CONCAT(PFN_, NAME)) return;
    ANGLE_FNS(ANGLE_LOAD_ENTRY)
    angle_loaded = true;
}

bool angle_load(void)
{
    pthread_once(&angle_load_once, angle_do_load);
    return angle_loaded;
}
