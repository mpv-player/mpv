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

#include <SDL.h>
#include <stdbool.h>
#include <pthread.h>
#include "common/common.h"
#include "common/msg.h"
#include "input.h"
#include "input/keycodes.h"

struct gamepad_priv {
    SDL_GameController *controller;
};

static Uint32 gamepad_cancel_wakeup;

static void initialize_events(void)
{
    gamepad_cancel_wakeup = SDL_RegisterEvents(1);
}

static pthread_once_t events_initialized = PTHREAD_ONCE_INIT;

#define INVALID_KEY -1

static const int button_map[][2] = {
    { SDL_CONTROLLER_BUTTON_A, MP_KEY_GAMEPAD_ACTION_DOWN },
    { SDL_CONTROLLER_BUTTON_B, MP_KEY_GAMEPAD_ACTION_RIGHT },
    { SDL_CONTROLLER_BUTTON_X, MP_KEY_GAMEPAD_ACTION_LEFT },
    { SDL_CONTROLLER_BUTTON_Y,  MP_KEY_GAMEPAD_ACTION_UP },
    { SDL_CONTROLLER_BUTTON_BACK, MP_KEY_GAMEPAD_BACK },
    { SDL_CONTROLLER_BUTTON_GUIDE, MP_KEY_GAMEPAD_MENU },
    { SDL_CONTROLLER_BUTTON_START, MP_KEY_GAMEPAD_START },
    { SDL_CONTROLLER_BUTTON_LEFTSTICK, MP_KEY_GAMEPAD_LEFT_STICK },
    { SDL_CONTROLLER_BUTTON_RIGHTSTICK, MP_KEY_GAMEPAD_RIGHT_STICK },
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, MP_KEY_GAMEPAD_LEFT_SHOULDER },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, MP_KEY_GAMEPAD_RIGHT_SHOULDER },
    { SDL_CONTROLLER_BUTTON_DPAD_UP, MP_KEY_GAMEPAD_DPAD_UP },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN, MP_KEY_GAMEPAD_DPAD_DOWN },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT, MP_KEY_GAMEPAD_DPAD_LEFT },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, MP_KEY_GAMEPAD_DPAD_RIGHT },
};

static const int analog_map[][5] = {
    // 0 -> sdl enum
    // 1 -> negative state
    // 2 -> neutral-negative state
    // 3 -> neutral-positive state
    // 4 -> positive state
    { SDL_CONTROLLER_AXIS_LEFTX,
        MP_KEY_GAMEPAD_LEFT_STICK_LEFT | MP_KEY_STATE_DOWN,
        MP_KEY_GAMEPAD_LEFT_STICK_LEFT | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_LEFT_STICK_RIGHT | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_LEFT_STICK_RIGHT | MP_KEY_STATE_DOWN },

    { SDL_CONTROLLER_AXIS_LEFTY,
        MP_KEY_GAMEPAD_LEFT_STICK_UP | MP_KEY_STATE_DOWN,
        MP_KEY_GAMEPAD_LEFT_STICK_UP | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_LEFT_STICK_DOWN | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_LEFT_STICK_DOWN | MP_KEY_STATE_DOWN },

    { SDL_CONTROLLER_AXIS_RIGHTX,
        MP_KEY_GAMEPAD_RIGHT_STICK_LEFT | MP_KEY_STATE_DOWN,
        MP_KEY_GAMEPAD_RIGHT_STICK_LEFT | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_RIGHT_STICK_RIGHT | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_RIGHT_STICK_RIGHT | MP_KEY_STATE_DOWN },

    { SDL_CONTROLLER_AXIS_RIGHTY,
        MP_KEY_GAMEPAD_RIGHT_STICK_UP | MP_KEY_STATE_DOWN,
        MP_KEY_GAMEPAD_RIGHT_STICK_UP | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_RIGHT_STICK_DOWN | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_RIGHT_STICK_DOWN | MP_KEY_STATE_DOWN },

    { SDL_CONTROLLER_AXIS_TRIGGERLEFT,
        INVALID_KEY,
        INVALID_KEY,
        MP_KEY_GAMEPAD_LEFT_TRIGGER | MP_KEY_STATE_UP,
        MP_KEY_GAMEPAD_LEFT_TRIGGER | MP_KEY_STATE_DOWN },

    { SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
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

#define GUID_LEN 33

static void add_gamepad(struct mp_input_src *src, int id)
{
    struct gamepad_priv *p = src->priv;

    if (p->controller) {
        MP_WARN(src, "can't add more than one controller\n");
        return;
    }

    if (SDL_IsGameController(id)) {
        SDL_GameController *controller = SDL_GameControllerOpen(id);

        if (controller) {
            const char *name = SDL_GameControllerName(controller);
            MP_INFO(src, "added controller: %s\n", name);
            p->controller = controller;
            return;
        }
    }
}

static void remove_gamepad(struct mp_input_src *src, int id)
{
    struct gamepad_priv *p = src->priv;
    SDL_GameController *controller = p->controller;
    SDL_Joystick* j = SDL_GameControllerGetJoystick(controller);
    SDL_JoystickID jid = SDL_JoystickInstanceID(j);

    if (controller && jid == id) {
        const char *name = SDL_GameControllerName(controller);
        MP_INFO(src, "removed controller: %s\n", name);
        SDL_GameControllerClose(controller);
        p->controller = NULL;
    }
}

static void read_gamepad_thread(struct mp_input_src *src, void *param)
{
    if (SDL_WasInit(SDL_INIT_EVENTS)) {
        MP_ERR(src, "Another component is using SDL already.\n");
        mp_input_src_init_done(src);
        return;
    }

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)) {
        MP_ERR(src, "SDL_Init failed\n");
        mp_input_src_init_done(src);
        return;
    }

    pthread_once(&events_initialized, initialize_events);

    if (gamepad_cancel_wakeup == (Uint32)-1) {
        MP_ERR(src, "Can't register SDL custom events\n");
        mp_input_src_init_done(src);
        return;
    }

    struct gamepad_priv *p =src->priv = talloc_zero(src, struct gamepad_priv);
    src->cancel = request_cancel;
    src->uninit = uninit;

    mp_input_src_init_done(src);

    SDL_Event ev;

    while (SDL_WaitEvent(&ev) != 0) {
        if (ev.type == gamepad_cancel_wakeup) {
            break;
        }

        switch (ev.type) {
            case SDL_CONTROLLERDEVICEADDED: {
                add_gamepad(src, ev.cdevice.which);
                continue;
            }
            case SDL_CONTROLLERDEVICEREMOVED: {
                remove_gamepad(src, ev.cdevice.which);
                continue;
            }
            case SDL_CONTROLLERBUTTONDOWN: {
                const int key = lookup_button_mp_key(ev.cbutton.button);
                if (key != INVALID_KEY) {
                    mp_input_put_key(src->input_ctx, key | MP_KEY_STATE_DOWN);
                }
                continue;
            }
            case SDL_CONTROLLERBUTTONUP: {
                const int key = lookup_button_mp_key(ev.cbutton.button);
                if (key != INVALID_KEY) {
                    mp_input_put_key(src->input_ctx, key | MP_KEY_STATE_UP);
                }
                continue;
            }
            case SDL_CONTROLLERAXISMOTION: {
                const int key =
                    lookup_analog_mp_key(ev.caxis.axis, ev.caxis.value);
                if (key != INVALID_KEY) {
                    mp_input_put_key(src->input_ctx, key);
                }
                continue;
            }

        }
    }

    if (p->controller) {
        SDL_Joystick* j = SDL_GameControllerGetJoystick(p->controller);
        SDL_JoystickID jid = SDL_JoystickInstanceID(j);
        remove_gamepad(src, jid);
    }

    // must be called on the same thread of SDL_InitSubSystem, so uninit
    // callback can't be used for this
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

void mp_input_sdl_gamepad_add(struct input_ctx *ictx)
{
    mp_input_add_thread_src(ictx, NULL, read_gamepad_thread);
}
