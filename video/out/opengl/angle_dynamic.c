#include <windows.h>

#include "angle_dynamic.h"

#include "common/common.h"
#include "osdep/threads.h"

#if HAVE_EGL_ANGLE_LIB
bool angle_load(void)
{
    return true;
}
#else
#define ANGLE_DECL(NAME, VAR) \
    VAR;
ANGLE_FNS(ANGLE_DECL)

static bool angle_loaded;
static mp_once angle_load_once = MP_STATIC_ONCE_INITIALIZER;

static void angle_do_load(void)
{
    // Note: we let this handle "leak", as the functions remain valid forever.
    HANDLE angle_dll = LoadLibraryW(L"LIBEGL.DLL");
    if (!angle_dll)
        return;
#define ANGLE_LOAD_ENTRY(NAME, VAR) \
    NAME = (void *)GetProcAddress(angle_dll, #NAME); \
    if (!NAME) return;
    ANGLE_FNS(ANGLE_LOAD_ENTRY)
    angle_loaded = true;
}

bool angle_load(void)
{
    mp_exec_once(&angle_load_once, angle_do_load);
    return angle_loaded;
}
#endif
