#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct mp_egl_rpi {
    struct mp_log *log;
    struct GL *gl;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    // yep, the API keeps a pointer to it
    EGL_DISPMANX_WINDOW_T egl_window;
};

int mp_egl_rpi_init(struct mp_egl_rpi *p, DISPMANX_ELEMENT_HANDLE_T window,
                    int w, int h);
void mp_egl_rpi_destroy(struct mp_egl_rpi *p);
