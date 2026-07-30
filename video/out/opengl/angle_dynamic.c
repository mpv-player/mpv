#include <string.h>
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

// Some ANGLE builds export the EGL entry points under the EGL_-prefixed names used by Chromium
// (EGL_QueryString rather than eglQueryString). Applications that embed such a build inherit it.
// Without this fallback angle_load() fails, and every ANGLE-based hwdec is refused at its second
// gate, before any device, capability or extension is examined.
//
// One extra GetProcAddress() per entry point on builds that export the plain names; none on the
// builds that need it.
static void *mp_angle_get_proc(HANDLE dll, const char *name)
{
    void *fn = (void *)GetProcAddress(dll, name);
    if (fn)
        return fn;
    if (name[0] != 'e' || name[1] != 'g' || name[2] != 'l')
        return NULL;
    char buf[128] = "EGL_";
    size_t n = strlen(name + 3);
    if (n + 5 > sizeof(buf))
        return NULL;
    memcpy(buf + 4, name + 3, n + 1);
    return (void *)GetProcAddress(dll, buf);
}

static void angle_do_load(void)
{
    // Note: we let this handle "leak", as the functions remain valid forever.
    HANDLE angle_dll = LoadLibraryW(L"LIBEGL.DLL");
    if (!angle_dll)
        return;
#define ANGLE_LOAD_ENTRY(NAME, VAR) \
    NAME = (void *)mp_angle_get_proc(angle_dll, #NAME); \
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
