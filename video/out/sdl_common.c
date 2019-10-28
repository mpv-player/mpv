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
#include "sdl_common.h"

#include "vo.h"
#include "options/m_config.h"
#include "win_state.h"

#include "config.h"
#include "misc/bstr.h"
#include "options/options.h"
#include "options/m_config.h"
#include "common/common.h"
#include "common/msg.h"
#include "mpv_talloc.h"

#include "osdep/io.h"
#include "osdep/timer.h"
#include "osdep/subprocess.h"

#include "input/event.h"
#include "input/input.h"
#include "input/keycodes.h"

struct keymap_entry {
    SDL_Keycode sdl;
    int mpv;
};
const struct keymap_entry keys[] = {
    {SDLK_RETURN, MP_KEY_ENTER},
    {SDLK_ESCAPE, MP_KEY_ESC},
    {SDLK_BACKSPACE, MP_KEY_BACKSPACE},
    {SDLK_TAB, MP_KEY_TAB},
    {SDLK_PRINTSCREEN, MP_KEY_PRINT},
    {SDLK_PAUSE, MP_KEY_PAUSE},
    {SDLK_INSERT, MP_KEY_INSERT},
    {SDLK_HOME, MP_KEY_HOME},
    {SDLK_PAGEUP, MP_KEY_PAGE_UP},
    {SDLK_DELETE, MP_KEY_DELETE},
    {SDLK_END, MP_KEY_END},
    {SDLK_PAGEDOWN, MP_KEY_PAGE_DOWN},
    {SDLK_RIGHT, MP_KEY_RIGHT},
    {SDLK_LEFT, MP_KEY_LEFT},
    {SDLK_DOWN, MP_KEY_DOWN},
    {SDLK_UP, MP_KEY_UP},
    {SDLK_KP_ENTER, MP_KEY_KPENTER},
    {SDLK_KP_1, MP_KEY_KP1},
    {SDLK_KP_2, MP_KEY_KP2},
    {SDLK_KP_3, MP_KEY_KP3},
    {SDLK_KP_4, MP_KEY_KP4},
    {SDLK_KP_5, MP_KEY_KP5},
    {SDLK_KP_6, MP_KEY_KP6},
    {SDLK_KP_7, MP_KEY_KP7},
    {SDLK_KP_8, MP_KEY_KP8},
    {SDLK_KP_9, MP_KEY_KP9},
    {SDLK_KP_0, MP_KEY_KP0},
    {SDLK_KP_PERIOD, MP_KEY_KPDEC},
    {SDLK_POWER, MP_KEY_POWER},
    {SDLK_MENU, MP_KEY_MENU},
    {SDLK_STOP, MP_KEY_STOP},
    {SDLK_MUTE, MP_KEY_MUTE},
    {SDLK_VOLUMEUP, MP_KEY_VOLUME_UP},
    {SDLK_VOLUMEDOWN, MP_KEY_VOLUME_DOWN},
    {SDLK_KP_COMMA, MP_KEY_KPDEC},
    {SDLK_AUDIONEXT, MP_KEY_NEXT},
    {SDLK_AUDIOPREV, MP_KEY_PREV},
    {SDLK_AUDIOSTOP, MP_KEY_STOP},
    {SDLK_AUDIOPLAY, MP_KEY_PLAY},
    {SDLK_AUDIOMUTE, MP_KEY_MUTE},
    {SDLK_F1, MP_KEY_F + 1},
    {SDLK_F2, MP_KEY_F + 2},
    {SDLK_F3, MP_KEY_F + 3},
    {SDLK_F4, MP_KEY_F + 4},
    {SDLK_F5, MP_KEY_F + 5},
    {SDLK_F6, MP_KEY_F + 6},
    {SDLK_F7, MP_KEY_F + 7},
    {SDLK_F8, MP_KEY_F + 8},
    {SDLK_F9, MP_KEY_F + 9},
    {SDLK_F10, MP_KEY_F + 10},
    {SDLK_F11, MP_KEY_F + 11},
    {SDLK_F12, MP_KEY_F + 12},
    {SDLK_F13, MP_KEY_F + 13},
    {SDLK_F14, MP_KEY_F + 14},
    {SDLK_F15, MP_KEY_F + 15},
    {SDLK_F16, MP_KEY_F + 16},
    {SDLK_F17, MP_KEY_F + 17},
    {SDLK_F18, MP_KEY_F + 18},
    {SDLK_F19, MP_KEY_F + 19},
    {SDLK_F20, MP_KEY_F + 20},
    {SDLK_F21, MP_KEY_F + 21},
    {SDLK_F22, MP_KEY_F + 22},
    {SDLK_F23, MP_KEY_F + 23},
    {SDLK_F24, MP_KEY_F + 24}
};

struct mousemap_entry {
    Uint8 sdl;
    int mpv;
};
const struct mousemap_entry mousebtns[] = {
    {SDL_BUTTON_LEFT, MP_MBTN_LEFT},
    {SDL_BUTTON_MIDDLE, MP_MBTN_MID},
    {SDL_BUTTON_RIGHT, MP_MBTN_RIGHT},
    {SDL_BUTTON_X1, MP_MBTN_BACK},
    {SDL_BUTTON_X2, MP_MBTN_FORWARD},
};

int vo_sdl_init(struct vo *vo, int flags)
{
    assert(!vo->sdl);

    struct vo_sdl_state *sdl = talloc_ptrtype(NULL, sdl);
    *sdl = (struct vo_sdl_state){
        .screensaver_enabled = false,
    };
    vo->sdl = sdl;

    if (SDL_WasInit(SDL_INIT_EVENTS)) {
        MP_ERR(vo, "Another component is using SDL already.\n");
        return -1;
    }

    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        MP_ERR(vo, "already initialized\n");
        return false;
    }

    // predefine SDL defaults (SDL env vars shall override)
    SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1",
                            SDL_HINT_DEFAULT);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0",
                            SDL_HINT_DEFAULT);

    if (SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        MP_ERR(vo, "SDL_Init failed\n");
        return false;
    }

    // then actually try
    sdl->window = SDL_CreateWindow("MPV", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                  640, 480, flags | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    if (!sdl->window) {
        MP_ERR(vo, "SDL_CreateWindow failed\n");
        return false;
    }

    sdl->wakeup_event = SDL_RegisterEvents(1);
    if (sdl->wakeup_event == (Uint32)-1)
        MP_ERR(vo, "SDL_RegisterEvents() failed.\n");

    return true;
}

void vo_sdl_uninit(struct vo *vo)
{
    struct vo_sdl_state *sdl = vo->sdl;
    if (!sdl)
        return;

    SDL_DestroyWindow(sdl->window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    talloc_free(sdl);
    vo->sdl = NULL;
}

static inline void set_screensaver(bool enabled)
{
    if (!!enabled == !!SDL_IsScreenSaverEnabled())
        return;

    if (enabled)
        SDL_EnableScreenSaver();
    else
        SDL_DisableScreenSaver();
}

static void set_fullscreen(struct vo *vo)
{
    struct vo_sdl_state *sdl = vo->sdl;
    if (!sdl)
        return;

    int fs = vo->opts->fullscreen;
    SDL_bool prev_screensaver_state = SDL_IsScreenSaverEnabled();

    Uint32 fs_flag;
    if (sdl->switch_mode)
        fs_flag = SDL_WINDOW_FULLSCREEN;
    else
        fs_flag = SDL_WINDOW_FULLSCREEN_DESKTOP;

    Uint32 old_flags = SDL_GetWindowFlags(sdl->window);
    int prev_fs = !!(old_flags & fs_flag);
    if (fs == prev_fs)
        return;

    Uint32 flags = 0;
    if (fs)
        flags |= fs_flag;

    if (SDL_SetWindowFullscreen(sdl->window, flags)) {
        MP_ERR(vo, "SDL_SetWindowFullscreen failed\n");
        return;
    }

    // toggling fullscreen might recreate the window, so better guard for this
    set_screensaver(prev_screensaver_state);

    sdl->pending_vo_events |= VO_EVENT_RESIZE;
}

static void update_screeninfo(struct vo *vo, struct mp_rect *screenrc)
{
    struct vo_sdl_state *sdl = vo->sdl;
    if (!sdl)
        return;

    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(sdl->window),
                                  &mode)) {
        MP_ERR(vo, "SDL_GetCurrentDisplayMode failed\n");
        return;
    }
    *screenrc = (struct mp_rect){0, 0, mode.w, mode.h};
}

int vo_sdl_config(struct vo *vo)
{
    struct vo_sdl_state *sdl = vo->sdl;
    if (!sdl)
        return -1;

    struct vo_win_geometry geo;
    struct mp_rect screenrc;

    update_screeninfo(vo, &screenrc);
    vo_calc_window_geometry(vo, &screenrc, &geo);
    vo_apply_window_geometry(vo, &geo);

    int win_w = vo->dwidth;
    int win_h = vo->dheight;

    SDL_SetWindowSize(sdl->window, win_w, win_h);
    if (geo.flags & VO_WIN_FORCE_POS)
        SDL_SetWindowPosition(sdl->window, geo.win.x0, geo.win.y0);

    set_screensaver(sdl->screensaver_enabled);
    set_fullscreen(vo);

    SDL_ShowWindow(sdl->window);

    sdl->pending_vo_events |= VO_EVENT_RESIZE;

    return 0;
}

void vo_sdl_wakeup(struct vo *vo)
{
    struct vo_sdl_state *sdl = vo->sdl;
    SDL_Event event = {.type = sdl->wakeup_event};
    // Note that there is no context - SDL is a singleton.
    SDL_PushEvent(&event);
}

void vo_sdl_wait_events(struct vo *vo, int64_t until_time_us)
{
    struct vo_sdl_state *sdl = vo->sdl;
    int64_t wait_us = until_time_us - mp_time_us();
    int timeout_ms = MPCLAMP((wait_us + 500) / 1000, 0, 10000);
    SDL_Event ev;

    while (SDL_WaitEventTimeout(&ev, timeout_ms)) {
        timeout_ms = 0;
        switch (ev.type) {
        case SDL_WINDOWEVENT:
            switch (ev.window.event) {
            case SDL_WINDOWEVENT_EXPOSED:
                vo->want_redraw = true;
                break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                sdl->pending_vo_events |= VO_EVENT_RESIZE;
                break;
            case SDL_WINDOWEVENT_ENTER:
                mp_input_put_key(vo->input_ctx, MP_KEY_MOUSE_ENTER);
                break;
            case SDL_WINDOWEVENT_LEAVE:
                mp_input_put_key(vo->input_ctx, MP_KEY_MOUSE_LEAVE);
                break;
            }
            break;
        case SDL_QUIT:
            mp_input_put_key(vo->input_ctx, MP_KEY_CLOSE_WIN);
            break;
        case SDL_TEXTINPUT: {
            int sdl_mod = SDL_GetModState();
            int mpv_mod = 0;
            // we ignore KMOD_LSHIFT, KMOD_RSHIFT and KMOD_RALT (if
            // mp_input_use_alt_gr() is true) because these are already
            // factored into ev.text.text
            if (sdl_mod & (KMOD_LCTRL | KMOD_RCTRL))
                mpv_mod |= MP_KEY_MODIFIER_CTRL;
            if ((sdl_mod & KMOD_LALT) ||
                ((sdl_mod & KMOD_RALT) && !mp_input_use_alt_gr(vo->input_ctx)))
                mpv_mod |= MP_KEY_MODIFIER_ALT;
            if (sdl_mod & (KMOD_LGUI | KMOD_RGUI))
                mpv_mod |= MP_KEY_MODIFIER_META;
            struct bstr t = {
                ev.text.text, strlen(ev.text.text)
            };
            mp_input_put_key_utf8(vo->input_ctx, mpv_mod, t);
            break;
        }
        case SDL_KEYDOWN: {
            // Issue: we don't know in advance whether this keydown event
            // will ALSO cause a SDL_TEXTINPUT event
            // So we're conservative, and only map non printable keycodes
            // (e.g. function keys, arrow keys, etc.)
            // However, this does lose some keypresses at least on X11
            // (e.g. Ctrl-A generates SDL_KEYDOWN only, but the key is
            // 'a'... and 'a' is normally also handled by SDL_TEXTINPUT).
            // The default config does not use Ctrl, so this is fine...
            int keycode = 0;
            int i;
            for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
                if (keys[i].sdl == ev.key.keysym.sym) {
                    keycode = keys[i].mpv;
                    break;
                }
            if (keycode) {
                if (ev.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
                    keycode |= MP_KEY_MODIFIER_SHIFT;
                if (ev.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL))
                    keycode |= MP_KEY_MODIFIER_CTRL;
                if (ev.key.keysym.mod & (KMOD_LALT | KMOD_RALT))
                    keycode |= MP_KEY_MODIFIER_ALT;
                if (ev.key.keysym.mod & (KMOD_LGUI | KMOD_RGUI))
                    keycode |= MP_KEY_MODIFIER_META;
                mp_input_put_key(vo->input_ctx, keycode);
            }
            break;
        }
        case SDL_MOUSEMOTION:
            mp_input_set_mouse_pos(vo->input_ctx, ev.motion.x, ev.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN: {
            int i;
            for (i = 0; i < sizeof(mousebtns) / sizeof(mousebtns[0]); ++i)
                if (mousebtns[i].sdl == ev.button.button) {
                    mp_input_put_key(vo->input_ctx, mousebtns[i].mpv | MP_KEY_STATE_DOWN);
                    break;
                }
            break;
        }
        case SDL_MOUSEBUTTONUP: {
            int i;
            for (i = 0; i < sizeof(mousebtns) / sizeof(mousebtns[0]); ++i)
                if (mousebtns[i].sdl == ev.button.button) {
                    mp_input_put_key(vo->input_ctx, mousebtns[i].mpv | MP_KEY_STATE_UP);
                    break;
                }
            break;
        }
        case SDL_MOUSEWHEEL: {
#if SDL_VERSION_ATLEAST(2, 0, 4)
            double multiplier = ev.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -0.1 : 0.1;
#else
            double multiplier = 0.1;
#endif
            int y_code = ev.wheel.y > 0 ? MP_WHEEL_UP : MP_WHEEL_DOWN;
            mp_input_put_wheel(vo->input_ctx, y_code, abs(ev.wheel.y) * multiplier);
            int x_code = ev.wheel.x > 0 ? MP_WHEEL_RIGHT : MP_WHEEL_LEFT;
            mp_input_put_wheel(vo->input_ctx, x_code, abs(ev.wheel.x) * multiplier);
            break;
        }
        }
    }
}

int vo_sdl_control(struct vo *vo, int *events, uint32_t request, void *data)
{
    struct vo_sdl_state *sdl = vo->sdl;
    switch (request) {
    case VOCTRL_CHECK_EVENTS:
        *events |= sdl->pending_vo_events;
        sdl->pending_vo_events = 0;
        return VO_TRUE;
    case VOCTRL_FULLSCREEN:
        set_fullscreen(vo);
        return VO_TRUE;
    case VOCTRL_SET_CURSOR_VISIBILITY:
        SDL_ShowCursor(*(bool *)data);
        return VO_TRUE;
    case VOCTRL_KILL_SCREENSAVER:
        sdl->screensaver_enabled = false;
        set_screensaver(sdl->screensaver_enabled);
        return VO_TRUE;
    case VOCTRL_RESTORE_SCREENSAVER:
        sdl->screensaver_enabled = true;
        set_screensaver(sdl->screensaver_enabled);
        return VO_TRUE;
    case VOCTRL_UPDATE_WINDOW_TITLE:
        SDL_SetWindowTitle(sdl->window, (char *)data);
        return VO_TRUE;
    }
    return VO_NOTIMPL;
}
