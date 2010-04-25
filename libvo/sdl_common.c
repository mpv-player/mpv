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

#define shift_key (event->key.keysym.mod==(KMOD_LSHIFT||KMOD_RSHIFT))
int sdl_default_handle_event(SDL_Event *event)
{
    SDLKey keypressed = SDLK_UNKNOWN;
    switch (event->type) {
    case SDL_VIDEORESIZE:
        vo_dwidth  = event->resize.w;
        vo_dheight = event->resize.h;
        return VO_EVENT_RESIZE;
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
        keypressed = event->key.keysym.sym;
        mp_msg(MSGT_VO,MSGL_DBG2, "SDL: Key pressed: '%i'\n", keypressed);
        switch(keypressed) {
        case SDLK_RETURN: mplayer_put_key(KEY_ENTER);break;
        case SDLK_ESCAPE: mplayer_put_key(KEY_ESC);break;
        case SDLK_q: mplayer_put_key('q');break;
        case SDLK_F1: mplayer_put_key(KEY_F+1);break;
        case SDLK_F2: mplayer_put_key(KEY_F+2);break;
        case SDLK_F3: mplayer_put_key(KEY_F+3);break;
        case SDLK_F4: mplayer_put_key(KEY_F+4);break;
        case SDLK_F5: mplayer_put_key(KEY_F+5);break;
        case SDLK_F6: mplayer_put_key(KEY_F+6);break;
        case SDLK_F7: mplayer_put_key(KEY_F+7);break;
        case SDLK_F8: mplayer_put_key(KEY_F+8);break;
        case SDLK_F9: mplayer_put_key(KEY_F+9);break;
        case SDLK_F10: mplayer_put_key(KEY_F+10);break;
        case SDLK_F11: mplayer_put_key(KEY_F+11);break;
        case SDLK_F12: mplayer_put_key(KEY_F+12);break;
        /*case SDLK_o: mplayer_put_key('o');break;
        case SDLK_SPACE: mplayer_put_key(' ');break;
        case SDLK_p: mplayer_put_key('p');break;*/
        case SDLK_7: mplayer_put_key(shift_key?'/':'7');break;
        case SDLK_PLUS: mplayer_put_key(shift_key?'*':'+');break;
        case SDLK_KP_PLUS: mplayer_put_key('+');break;
        case SDLK_MINUS:
        case SDLK_KP_MINUS: mplayer_put_key('-');break;
        case SDLK_TAB: mplayer_put_key('\t');break;
        case SDLK_PAGEUP: mplayer_put_key(KEY_PAGE_UP);break;
        case SDLK_PAGEDOWN: mplayer_put_key(KEY_PAGE_DOWN);break;
        case SDLK_UP: mplayer_put_key(KEY_UP);break;
        case SDLK_DOWN: mplayer_put_key(KEY_DOWN);break;
        case SDLK_LEFT: mplayer_put_key(KEY_LEFT);break;
        case SDLK_RIGHT: mplayer_put_key(KEY_RIGHT);break;
        case SDLK_LESS: mplayer_put_key(shift_key?'>':'<'); break;
        case SDLK_GREATER: mplayer_put_key('>'); break;
        case SDLK_ASTERISK:
        case SDLK_KP_MULTIPLY: mplayer_put_key('*'); break;
        case SDLK_SLASH:
        case SDLK_KP_DIVIDE: mplayer_put_key('/'); break;
        case SDLK_KP0: mplayer_put_key(KEY_KP0); break;
        case SDLK_KP1: mplayer_put_key(KEY_KP1); break;
        case SDLK_KP2: mplayer_put_key(KEY_KP2); break;
        case SDLK_KP3: mplayer_put_key(KEY_KP3); break;
        case SDLK_KP4: mplayer_put_key(KEY_KP4); break;
        case SDLK_KP5: mplayer_put_key(KEY_KP5); break;
        case SDLK_KP6: mplayer_put_key(KEY_KP6); break;
        case SDLK_KP7: mplayer_put_key(KEY_KP7); break;
        case SDLK_KP8: mplayer_put_key(KEY_KP8); break;
        case SDLK_KP9: mplayer_put_key(KEY_KP9); break;
        case SDLK_KP_PERIOD: mplayer_put_key(KEY_KPDEC); break;
        case SDLK_KP_ENTER: mplayer_put_key(KEY_KPENTER); break;
        default:
            //printf("got scancode: %d keysym: %d mod: %d %d\n", event.key.keysym.scancode, keypressed, event.key.keysym.mod);
            mplayer_put_key(keypressed);
        }

        break;

    case SDL_QUIT: mplayer_put_key(KEY_CLOSE_WIN);break;
    }
    return 0;
}
