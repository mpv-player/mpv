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

#include <stdbool.h>

#include <SDL3/SDL.h>

#include "common/common.h"
#include "common/msg.h"
#include "input.h"
#include "input/keycodes.h"
#include "osdep/threads.h"

struct gamepad_priv {
    SDL_Gamepad *gamepad;
};

static Uint32 gamepad_cancel_wakeup;

static void initialize_events(void)
{
    gamepad_cancel_wakeup = SDL_RegisterEvents(1);
}

static mp_once events_initialized = MP_STATIC_ONCE_INITIALIZER;

#define INVALID_KEY -1

static const int button_map[][2] = {
    { SDL_GAMEPAD_BUTTON_SOUTH, MP_KEY_GAMEPAD_ACTION_DOWN },
    { SDL_GAMEPAD_BUTTON_EAST, MP_KEY_GAMEPAD_ACTION_RIGHT },
    { SDL_GAMEPAD_BUTTON_WEST, MP_KEY_GAMEPAD_ACTION_LEFT },
    { SDL_GAMEPAD_BUTTON_NORTH,  MP_KEY_GAMEPAD_ACTION_UP },
    { SDL_GAMEPAD_BUTTON_BACK, MP_KEY_GAMEPAD_BACK },
    { SDL_GAMEPAD_BUTTON_GUIDE, MP_KEY_GAMEPAD_MENU },
    { SDL_GAMEPAD_BUTTON_START, MP_KEY_GAMEPAD_START },
    { SDL_GAMEPAD_BUTTON_LEFT_STICK, MP_KEY_GAMEPAD_LEFT_STICK },
    { SDL_GAMEPAD_BUTTON_RIGHT_STICK, MP_KEY_GAMEPAD_RIGHT_STICK },
    { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, MP_KEY_GAMEPAD_LEFT_SHOULDER },
    { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, MP_KEY_GAMEPAD_RIGHT_SHOULDER },
    { SDL_GAMEPAD_BUTTON_DPAD_UP, MP_KEY_GAMEPAD_DPAD_UP },
    { SDL_GAMEPAD_BUTTON_DPAD_DOWN, MP_KEY_GAMEPAD_DPAD_DOWN },
    { SDL_GAMEPAD_BUTTON_DPAD_LEFT, MP_KEY_GAMEPAD_DPAD_LEFT },
    { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, MP_KEY_GAMEPAD_DPAD_RIGHT },
};

static const int analog_map[][5] = {
    // 0 -> sdl enum
    // 1 -> negative state
    // 2 -> neutral-negative state
    // 3 -> neutral-positive state
    // 4 -> positive state
    { SDL_GAMEPAD_AXIS_LEFTX,
        MP_KEY_GAMEPAD_LEFT_STICK_LEFT | MP_KEY_STATE_DOWN,
        MP_KEY_GAMEPAD_LEFT_STICK_LEFT | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_LEFT_STICK_RIGHT | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_LEFT_STICK_RIGHT | MP_KEY_STATE_DOWN },

    { SDL_GAMEPAD_AXIS_LEFTY,
        MP_KEY_GAMEPAD_LEFT_STICK_UP | MP_KEY_STATE_DOWN,
        MP_KEY_GAMEPAD_LEFT_STICK_UP | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_LEFT_STICK_DOWN | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_LEFT_STICK_DOWN | MP_KEY_STATE_DOWN },

    { SDL_GAMEPAD_AXIS_RIGHTX,
        MP_KEY_GAMEPAD_RIGHT_STICK_LEFT | MP_KEY_STATE_DOWN,
        MP_KEY_GAMEPAD_RIGHT_STICK_LEFT | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_RIGHT_STICK_RIGHT | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_RIGHT_STICK_RIGHT | MP_KEY_STATE_DOWN },

    { SDL_GAMEPAD_AXIS_RIGHTY,
        MP_KEY_GAMEPAD_RIGHT_STICK_UP | MP_KEY_STATE_DOWN,
        MP_KEY_GAMEPAD_RIGHT_STICK_UP | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_RIGHT_STICK_DOWN | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_RIGHT_STICK_DOWN | MP_KEY_STATE_DOWN },

    { SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
        INVALID_KEY,
        INVALID_KEY,
        MP_KEY_GAMEPAD_LEFT_TRIGGER | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_LEFT_TRIGGER | MP_KEY_STATE_DOWN },

    { SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
        INVALID_KEY,
        INVALID_KEY,
        MP_KEY_GAMEPAD_RIGHT_TRIGGER | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_RIGHT_TRIGGER | MP_KEY_STATE_DOWN },
};

static int lookup_button_mp_key(int sdl_key)
{
    for (int i = 0; i < MP_ARRAY_SIZE(button_map); i++) {
        if (button_map[i][0] == sdl_key) {
            return button_map[i][1];
        }
    }
    return INVALID_KEY;
}

static int lookup_analog_mp_key(int sdl_key, int16_t value)
{
    const int sdl_axis_max = 32767;
    const int negative = 1;
    const int negative_neutral = 2;
    const int positive_neutral = 3;
    const int positive = 4;

    const float activation_threshold = sdl_axis_max * 0.33;
    const float noise_threshold = sdl_axis_max * 0.06;

    // sometimes SDL just keeps shitting out low values around 0 that mess
    // with key repeating code
    if (value < noise_threshold && value > -noise_threshold) {
        return INVALID_KEY;
    }

    int state = value > 0 ? positive_neutral : negative_neutral;

    if (value >= sdl_axis_max - activation_threshold) {
        state = positive;
    }

    if (value <= activation_threshold - sdl_axis_max) {
        state = negative;
    }

    for (int i = 0; i < MP_ARRAY_SIZE(analog_map); i++) {
        if (analog_map[i][0] == sdl_key) {
            return analog_map[i][state];
        }
    }

    return INVALID_KEY;
}


static void request_cancel(struct mp_input_src *src)
{
    MP_VERBOSE(src, "exiting...\n");
    SDL_Event event = { .type = gamepad_cancel_wakeup };
    SDL_PushEvent(&event);
}

static void uninit(struct mp_input_src *src)
{
    MP_VERBOSE(src, "exited.\n");
}

static void add_gamepad(struct mp_input_src *src, int id)
{
    struct gamepad_priv *p = src->priv;

    if (p->gamepad) {
        MP_WARN(src, "can't add more than one gamepad\n");
        return;
    }

    if (SDL_IsGamepad(id)) {
        SDL_Gamepad *gamepad = SDL_OpenGamepad(id);

        if (gamepad) {
            const char *name = SDL_GetGamepadName(gamepad);
            MP_INFO(src, "added gamepad: %s\n", name);
            p->gamepad = gamepad;
            return;
        }
    }
}

static void remove_gamepad(struct mp_input_src *src, int id)
{
    struct gamepad_priv *p = src->priv;
    SDL_Gamepad *gamepad = p->gamepad;
    if (!gamepad)
        return;
    SDL_Joystick* j = SDL_GetGamepadJoystick(gamepad);
    SDL_JoystickID jid = SDL_GetJoystickID(j);

    if (jid == id) {
        const char *name = SDL_GetGamepadName(gamepad);
        MP_INFO(src, "removed gamepad: %s\n", name);
        SDL_CloseGamepad(gamepad);
        p->gamepad = NULL;
    }
}

static void read_gamepad_thread(struct mp_input_src *src, void *param)
{
    if (SDL_WasInit(SDL_INIT_EVENTS)) {
        MP_ERR(src, "Another component is using SDL already.\n");
        mp_input_src_init_done(src);
        return;
    }

    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        MP_ERR(src, "SDL_Init failed: %s\n", SDL_GetError());
        mp_input_src_init_done(src);
        return;
    }

    mp_exec_once(&events_initialized, initialize_events);

    if (!gamepad_cancel_wakeup) {
        MP_ERR(src, "Can't register SDL custom events\n");
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        mp_input_src_init_done(src);
        return;
    }

    struct gamepad_priv *p = src->priv = talloc_zero(src, struct gamepad_priv);
    src->cancel = request_cancel;
    src->uninit = uninit;

    mp_input_src_init_done(src);

    SDL_Event ev;

    while (SDL_WaitEvent(&ev)) {
        if (ev.type == gamepad_cancel_wakeup) {
            break;
        }

        switch (ev.type) {
            case SDL_EVENT_GAMEPAD_ADDED: {
                add_gamepad(src, ev.gdevice.which);
                continue;
            }
            case SDL_EVENT_GAMEPAD_REMOVED: {
                remove_gamepad(src, ev.gdevice.which);
                continue;
            }
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
                const int key = lookup_button_mp_key(ev.gbutton.button);
                if (key != INVALID_KEY) {
                    mp_input_put_key(src->input_ctx, key | MP_KEY_STATE_DOWN);
                }
                continue;
            }
            case SDL_EVENT_GAMEPAD_BUTTON_UP: {
                const int key = lookup_button_mp_key(ev.gbutton.button);
                if (key != INVALID_KEY) {
                    mp_input_put_key(src->input_ctx, key | MP_KEY_STATE_UP);
                }
                continue;
            }
            case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
                const int key =
                    lookup_analog_mp_key(ev.gaxis.axis, ev.gaxis.value);
                if (key != INVALID_KEY) {
                    mp_input_put_key(src->input_ctx, key);
                }
                continue;
            }

        }
    }

    if (p->gamepad) {
        SDL_Joystick* j = SDL_GetGamepadJoystick(p->gamepad);
        SDL_JoystickID jid = SDL_GetJoystickID(j);
        remove_gamepad(src, jid);
    }

    // must be called on the same thread of SDL_InitSubSystem, so uninit
    // callback can't be used for this
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

void mp_input_sdl_gamepad_add(struct input_ctx *ictx)
{
    mp_input_add_thread_src(ictx, NULL, read_gamepad_thread);
}
