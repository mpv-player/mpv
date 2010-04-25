/*
 * common SDL routines
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "sdl_common.h"
#include "mp_msg.h"
#include "mp_fifo.h"
#include "osdep/keycodes.h"
#include "input/input.h"
#include "input/mouse.h"
#include "video_out.h"

static int old_w;
static int old_h;
static int mode_flags;

int vo_sdl_init(void)
{
    if (!SDL_WasInit(SDL_INIT_VIDEO) &&
        SDL_Init(SDL_INIT_VIDEO|SDL_INIT_NOPARACHUTE) < 0)
        return 0;

    // Setup Keyrepeats (500/30 are defaults)
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, 100 /*SDL_DEFAULT_REPEAT_INTERVAL*/);

    // Easiest way to get uppercase characters
    SDL_EnableUNICODE(1);

    // We don't want those in our event queue.
    SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

    return 1;
}

void vo_sdl_uninit(void)
{
    if (SDL_WasInit(SDL_INIT_VIDEO))
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void vo_sdl_fullscreen(void)
{
    if (vo_fs) {
        vo_dwidth  = old_w;
        vo_dheight = old_h;
    } else {
        old_w = vo_dwidth;
        old_h = vo_dheight;
        vo_dwidth  = vo_screenwidth;
        vo_dheight = vo_screenheight;
    }
    vo_fs = !vo_fs;
    sdl_set_mode(0, mode_flags);
}

int sdl_set_mode(int bpp, uint32_t flags)
{
    SDL_Surface *s;
    mode_flags = flags;
    if (vo_fs) flags |= SDL_FULLSCREEN;
    // doublebuf with opengl creates flickering
    if (vo_doublebuffering && !(flags & SDL_OPENGL))
        flags |= SDL_DOUBLEBUF;
    s = SDL_SetVideoMode(vo_dwidth, vo_dheight, bpp, flags);
    if (!s) {
      mp_msg(MSGT_VO, MSGL_FATAL, "SDL SetVideoMode failed: %s\n", SDL_GetError());
      return -1;
    }
    vo_dwidth  = s->w;
    vo_dheight = s->h;
    return 0;
}

static const struct mp_keymap keysym_map[] = {
    {SDLK_RETURN, KEY_ENTER}, {SDLK_ESCAPE, KEY_ESC},
    {SDLK_F1, KEY_F+1}, {SDLK_F2, KEY_F+2}, {SDLK_F3, KEY_F+3},
    {SDLK_F4, KEY_F+4}, {SDLK_F5, KEY_F+5}, {SDLK_F6, KEY_F+6},
    {SDLK_F7, KEY_F+7}, {SDLK_F8, KEY_F+8}, {SDLK_F9, KEY_F+9},
    {SDLK_F10, KEY_F+10}, {SDLK_F11, KEY_F+11}, {SDLK_F12, KEY_F+12},
    {SDLK_KP_PLUS, '+'}, {SDLK_KP_MINUS, '-'}, {SDLK_TAB, KEY_TAB},
    {SDLK_PAGEUP, KEY_PAGE_UP}, {SDLK_PAGEDOWN, KEY_PAGE_DOWN},
    {SDLK_UP, KEY_UP}, {SDLK_DOWN, KEY_DOWN},
    {SDLK_LEFT, KEY_LEFT}, {SDLK_RIGHT, KEY_RIGHT},
    {SDLK_KP_MULTIPLY, '*'}, {SDLK_KP_DIVIDE, '/'},
    {SDLK_KP0, KEY_KP0}, {SDLK_KP1, KEY_KP1}, {SDLK_KP2, KEY_KP2},
    {SDLK_KP3, KEY_KP3}, {SDLK_KP4, KEY_KP4}, {SDLK_KP5, KEY_KP5},
    {SDLK_KP6, KEY_KP6}, {SDLK_KP7, KEY_KP7}, {SDLK_KP8, KEY_KP8},
    {SDLK_KP9, KEY_KP9},
    {SDLK_KP_PERIOD, KEY_KPDEC}, {SDLK_KP_ENTER, KEY_KPENTER},
};

int sdl_default_handle_event(SDL_Event *event)
{
    int mpkey;
    switch (event->type) {
    case SDL_VIDEORESIZE:
        vo_dwidth  = event->resize.w;
        vo_dheight = event->resize.h;
        return VO_EVENT_RESIZE;
    case SDL_VIDEOEXPOSE:
        return VO_EVENT_EXPOSE;
    case SDL_MOUSEBUTTONDOWN:
        if(vo_nomouse_input)
            break;
        mplayer_put_key((MOUSE_BTN0 + event->button.button - 1) | MP_KEY_DOWN);
        break;

    case SDL_MOUSEBUTTONUP:
        if(vo_nomouse_input)
            break;
        mplayer_put_key(MOUSE_BTN0 + event->button.button - 1);
        break;

    case SDL_KEYDOWN:
        mpkey = lookup_keymap_table(keysym_map, event->key.keysym.sym);
        if (!mpkey &&
            event->key.keysym.unicode > 0 &&
            event->key.keysym.unicode < 128)
            mpkey = event->key.keysym.unicode;
        if (mpkey)
            mplayer_put_key(mpkey);
        break;

    case SDL_QUIT: mplayer_put_key(KEY_CLOSE_WIN);break;
    }
    return 0;
}
