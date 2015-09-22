// Build with: gcc -o main main.c `pkg-config --libs --cflags mpv sdl2`

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>

#include <mpv/client.h>
#include <mpv/opengl_cb.h>

static Uint32 wakeup_on_mpv_redraw, wakeup_on_mpv_events;

static void die(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static void *get_proc_address_mpv(void *fn_ctx, const char *name)
{
    return SDL_GL_GetProcAddress(name);
}

static void on_mpv_events(void *ctx)
{
    SDL_Event event = {.type = wakeup_on_mpv_events};
    SDL_PushEvent(&event);
}

static void on_mpv_redraw(void *ctx)
{
    SDL_Event event = {.type = wakeup_on_mpv_redraw};
    SDL_PushEvent(&event);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
        die("pass a single media file as argument");

    mpv_handle *mpv = mpv_create();
    if (!mpv)
        die("context init failed");

    // Some minor options can only be set before mpv_initialize().
    if (mpv_initialize(mpv) < 0)
        die("mpv init failed");

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        die("SDL init failed");

    SDL_Window *window =
        SDL_CreateWindow("hi", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1000, 500, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN |
                                    SDL_WINDOW_RESIZABLE);
    if (!window)
        die("failed to create SDL window");

    // The OpenGL API is somewhat separate from the normal mpv API. This only
    // returns NULL if no OpenGL support is compiled.
    mpv_opengl_cb_context *mpv_gl = mpv_get_sub_api(mpv, MPV_SUB_API_OPENGL_CB);
    if (!mpv_gl)
        die("failed to create mpv GL API handle");

    SDL_GLContext glcontext = SDL_GL_CreateContext(window);
    if (!glcontext)
        die("failed to create SDL GL context");

    // This makes mpv use the currently set GL context. It will use the callback
    // to resolve GL builtin functions, as well as extensions.
    if (mpv_opengl_cb_init_gl(mpv_gl, NULL, get_proc_address_mpv, NULL) < 0)
        die("failed to initialize mpv GL context");

    // Actually using the opengl_cb state has to be explicitly requested.
    // Otherwise, mpv will create a separate platform window.
    if (mpv_set_option_string(mpv, "vo", "opengl-cb") < 0)
        die("failed to set VO");

    // We use events for thread-safe notification of the SDL main loop.
    // Generally, the wakeup callbacks (set further below) should do as least
    // work as possible, and merely wake up another thread to do actual work.
    // On SDL, waking up the mainloop is the ideal course of action. SDL's
    // SDL_PushEvent() is thread-safe, so we use that.
    wakeup_on_mpv_redraw = SDL_RegisterEvents(1);
    wakeup_on_mpv_events = SDL_RegisterEvents(1);
    if (wakeup_on_mpv_redraw == (Uint32)-1 || wakeup_on_mpv_events == (Uint32)-1)
        die("could not register events");

    // When normal mpv events are available.
    mpv_set_wakeup_callback(mpv, on_mpv_events, NULL);

    // When a new frame should be drawn with mpv_opengl_cb_draw().
    // (Separate from the normal event handling mechanism for the sake of
    //  users which run OpenGL on a different thread.)
    mpv_opengl_cb_set_update_callback(mpv_gl, on_mpv_redraw, NULL);

    // Play this file. Note that this starts playback asynchronously.
    const char *cmd[] = {"loadfile", argv[1], NULL};
    mpv_command(mpv, cmd);

    while (1) {
        SDL_Event event;
        if (SDL_WaitEvent(&event) != 1)
            die("event loop error");
        int redraw = 0;
        switch (event.type) {
        case SDL_QUIT:
            goto done;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_EXPOSED)
                redraw = 1;
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_SPACE)
                mpv_command_string(mpv, "cycle pause");
            break;
        default:
            // Happens when a new video frame should be rendered, or if the
            // current frame has to be redrawn e.g. due to OSD changes.
            if (event.type == wakeup_on_mpv_redraw)
                redraw = 1;
            // Happens when at least 1 new event is in the mpv event queue.
            if (event.type == wakeup_on_mpv_events) {
                // Handle all remaining mpv events.
                while (1) {
                    mpv_event *mp_event = mpv_wait_event(mpv, 0);
                    if (mp_event->event_id == MPV_EVENT_NONE)
                        break;
                    printf("event: %s\n", mpv_event_name(mp_event->event_id));
                }
            }
        }
        if (redraw) {
            int w, h;
            SDL_GetWindowSize(window, &w, &h);
            // Note:
            // - The 0 is the FBO to use; 0 is the default framebuffer (i.e.
            //   render to the window directly.
            // - The negative height tells mpv to flip the coordinate system.
            // - If you do not want the video to cover the whole screen, or want
            //   to apply any form of fancy transformation, you will have to
            //   render to a FBO.
            // - See opengl_cb.h on what OpenGL environment mpv expects, and
            //   other API details.
            mpv_opengl_cb_draw(mpv_gl, 0, w, -h);
            SDL_GL_SwapWindow(window);
        }
    }
done:

    // Destroy the GL renderer and all of the GL objects it allocated. If video
    // is still running, the video track will be deselected.
    mpv_opengl_cb_uninit_gl(mpv_gl);

    mpv_terminate_destroy(mpv);
    return 0;
}
