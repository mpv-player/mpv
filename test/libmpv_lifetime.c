#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <libmpv/client.h>

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define __SANITIZE_ADDRESS__
#endif
#endif

#ifdef __SANITIZE_ADDRESS__
#include <sanitizer/lsan_interface.h>
#endif

// Check only the first iteration, before dlclose() happens. LSAN does not track
// unloaded modules, so reports are not very readable and require manual processing.
// Shared libraries often don't fully clean up after themselves. Ideally, these
// cases should be investigated at some point.
#define LSAN_IGNORE_DLCLOSE

#ifdef _WIN32
#include <windows.h>

#define LOAD_LIB() HMODULE lib = LoadLibraryW(L"libmpv-2.dll")
#define CLOSE_LIB() FreeLibrary(lib)
#define GET_SYM GetProcAddress
#else
#include <dlfcn.h>

#ifdef __APPLE__
#define LIB_NAME "libmpv.2.dylib"
#else
#define LIB_NAME "libmpv.so"
#endif
#define LOAD_LIB() void *lib = dlopen(LIB_NAME, RTLD_NOW | RTLD_LOCAL)
#define CLOSE_LIB() dlclose(lib)
#define GET_SYM dlsym
#endif

#define INIT_SYM(name) __typeof__(&mpv_##name) name = (void *) GET_SYM(lib, "mpv_" #name); \
                       if (!name) exit(1)

#define REPEAT 2

static void exit_log(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    exit(1);
}

#define check_error(status) check_error_(status, error_string)
static inline void check_error_(int status, __typeof__(&mpv_error_string) error_string)
{
    if (status < 0)
        exit_log("mpv API error: %s\n", error_string(status));
}

int main(void)
{
    // Skip this test when run through a wrapper like Wine. It is well-tested on
    // different configurations. Meson does not set PATH and WINEPATH when
    // libmpv is not directly linked, and doing it manually would be annoying.
    if (getenv("MESON_EXE_WRAPPER"))
        return 77;

    for (int i = 0; i < REPEAT; ++i) {
        LOAD_LIB();
        if (!lib)
            exit_log("Failed to load libmpv!\n");

        INIT_SYM(command);
        INIT_SYM(create);
        INIT_SYM(error_string);
        INIT_SYM(initialize);
        INIT_SYM(set_option_string);
        INIT_SYM(terminate_destroy);
        INIT_SYM(wait_event);

        for (int j = 0; j < REPEAT; ++j) {
            mpv_handle *ctx = create();
            if (!ctx)
                exit_log("Failed to create mpv context!\n");

            set_option_string(ctx, "msg-level", "all=trace");
            set_option_string(ctx, "terminal", "yes");

            check_error(initialize(ctx));

            for (int k = 0; k < REPEAT; ++k) {
                check_error(command(ctx, (const char *[]){"loadfile",
                                                          "av://lavfi:yuvtestsrc=d=0.1",
                                                          NULL}));
                bool loaded = false;
                while (true) {
                    mpv_event *event = wait_event(ctx, -1);
                    if (event->event_id == MPV_EVENT_START_FILE)
                        loaded = true;
                    if (loaded && event->event_id == MPV_EVENT_IDLE)
                        break;
                }
            }

            terminate_destroy(ctx);

#ifdef __SANITIZE_ADDRESS__
#ifdef LSAN_IGNORE_DLCLOSE
            __lsan_do_leak_check();
#else
            if (__lsan_do_recoverable_leak_check())
                exit_log("Detected memory leaks after terminate_destroy!\n");
#endif
#endif
        }

        CLOSE_LIB();

#if defined(__SANITIZE_ADDRESS__) && !defined(LSAN_IGNORE_DLCLOSE)
        if (__lsan_do_recoverable_leak_check())
            exit_log("Detected memory leaks after dlclose!\n");
#endif
    }

    return 0;
}
