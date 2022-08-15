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

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <unistd.h>
#include "common/common.h"
#include "common/msg.h"
#include "input.h"
#include "input/keycodes.h"

struct gamepad {
    int fd;
    int left_x_min, left_x_max;
    int left_y_min, left_y_max;
    int right_x_min, right_x_max;
    int right_y_min, right_y_max;
    int left_trigger, right_trigger;
    char name[128];
};

#define MAX_GAMEPADS 4

static int event_fd = -1;

static const int button_map[][2] = {
    { BTN_SOUTH, MP_KEY_GAMEPAD_ACTION_DOWN },
    { BTN_EAST, MP_KEY_GAMEPAD_ACTION_RIGHT },
    { BTN_WEST, MP_KEY_GAMEPAD_ACTION_LEFT },
    { BTN_NORTH,  MP_KEY_GAMEPAD_ACTION_UP },
    { BTN_SELECT, MP_KEY_GAMEPAD_BACK },
    { BTN_MODE, MP_KEY_GAMEPAD_MENU },
    { BTN_START, MP_KEY_GAMEPAD_START },
    { BTN_THUMBL, MP_KEY_GAMEPAD_LEFT_STICK },
    { BTN_THUMBR, MP_KEY_GAMEPAD_RIGHT_STICK },
    { BTN_TL, MP_KEY_GAMEPAD_LEFT_SHOULDER },
    { BTN_TR, MP_KEY_GAMEPAD_RIGHT_SHOULDER },
    { BTN_TL2, MP_KEY_GAMEPAD_LEFT_TRIGGER },
    { BTN_TR2, MP_KEY_GAMEPAD_RIGHT_TRIGGER },
    { BTN_DPAD_UP, MP_KEY_GAMEPAD_DPAD_UP },
    { BTN_DPAD_DOWN, MP_KEY_GAMEPAD_DPAD_DOWN },
    { BTN_DPAD_LEFT, MP_KEY_GAMEPAD_DPAD_LEFT },
    { BTN_DPAD_RIGHT, MP_KEY_GAMEPAD_DPAD_RIGHT },
};

static int lookup_button_mp_key(int evdev_key)
{
    for (int i = 0; i < MP_ARRAY_SIZE(button_map); i++) {
        if (button_map[i][0] == evdev_key) {
            return button_map[i][1];
        }
    }
    return -1;
}

static struct prev {
    int left_x;
    int left_y;
    int right_x;
    int right_y;
} prev;

static void handle_abs(struct mp_input_src *src, int code, int value, struct gamepad *gamepad)
{
    int trigger;
    switch (code) {
        case ABS_HAT0X:
            if (value == -1) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_DPAD_LEFT | MP_KEY_STATE_DOWN);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_DPAD_RIGHT | MP_KEY_STATE_UP);
            } else if (value == 0) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_DPAD_LEFT | MP_KEY_STATE_UP);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_DPAD_RIGHT | MP_KEY_STATE_UP);
            } else {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_DPAD_LEFT | MP_KEY_STATE_UP);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_DPAD_RIGHT | MP_KEY_STATE_DOWN);
            }
            break;
        case ABS_HAT0Y:
            if (value == -1) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_DPAD_UP | MP_KEY_STATE_DOWN);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_DPAD_DOWN | MP_KEY_STATE_UP);
            } else if (value == 0) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_DPAD_UP | MP_KEY_STATE_UP);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_DPAD_DOWN | MP_KEY_STATE_UP);
            } else {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_DPAD_UP | MP_KEY_STATE_UP);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_DPAD_DOWN | MP_KEY_STATE_DOWN);
            }
            break;
        case ABS_X:
            if (value < gamepad->left_x_min && prev.left_x != -1) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_STICK_LEFT | MP_KEY_STATE_DOWN);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_STICK_RIGHT | MP_KEY_STATE_UP);
                prev.left_x = -1;
            } else if (value >= gamepad->left_x_min && value < gamepad->left_x_max && prev.left_x != 0) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_STICK_LEFT | MP_KEY_STATE_UP);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_STICK_RIGHT | MP_KEY_STATE_UP);
                prev.left_x = 0;
            } else if (value >= gamepad->left_x_max && prev.left_x != 1) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_STICK_LEFT | MP_KEY_STATE_UP);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_STICK_RIGHT | MP_KEY_STATE_DOWN);
                prev.left_x = 1;
            }
            break;
        case ABS_Y:
            if (value < gamepad->left_y_min && prev.left_y != -1) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_STICK_UP | MP_KEY_STATE_DOWN);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_STICK_DOWN | MP_KEY_STATE_UP);
                prev.left_y = -1;
            } else if (value >= gamepad->left_y_min && value < gamepad->left_y_max && prev.left_y != 0) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_STICK_UP | MP_KEY_STATE_UP);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_STICK_DOWN | MP_KEY_STATE_UP);
                prev.left_y = 0;
            } else if (value >= gamepad->left_y_max && prev.left_y != 1) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_STICK_UP | MP_KEY_STATE_UP);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_STICK_DOWN | MP_KEY_STATE_DOWN);
                prev.left_y = 1;
            }
            break;
        case ABS_RX:
            if (value < gamepad->right_x_min && prev.right_x != -1) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_STICK_LEFT | MP_KEY_STATE_DOWN);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_STICK_RIGHT | MP_KEY_STATE_UP);
                prev.right_x = -1;
            } else if (value >= gamepad->right_x_min && value < gamepad->right_x_max && prev.right_x != 0) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_STICK_LEFT | MP_KEY_STATE_UP);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_STICK_RIGHT | MP_KEY_STATE_UP);
                prev.right_x = 0;
            } else if (value >= gamepad->right_x_max && prev.right_x != 1) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_STICK_LEFT | MP_KEY_STATE_UP);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_STICK_RIGHT | MP_KEY_STATE_DOWN);
                prev.right_x = 1;
            }
            break;
        case ABS_RY:
            if (value < gamepad->right_y_min && prev.right_y != -1) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_STICK_UP | MP_KEY_STATE_DOWN);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_STICK_DOWN | MP_KEY_STATE_UP);
                prev.right_y = -1;
            } else if (value >= gamepad->right_y_min && value < gamepad->right_y_max && prev.right_y != 0) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_STICK_UP | MP_KEY_STATE_UP);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_STICK_DOWN | MP_KEY_STATE_UP);
                prev.right_y = 0;
            } else if (value >= gamepad->right_y_max && prev.right_y != 1) {
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_STICK_UP | MP_KEY_STATE_UP);
                mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_STICK_DOWN | MP_KEY_STATE_DOWN);
                prev.right_y = 1;
            }
            break;
        case ABS_Z:
            trigger = value >= gamepad->left_trigger ? MP_KEY_STATE_DOWN : MP_KEY_STATE_UP;
            mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_LEFT_TRIGGER | trigger);
            break;
        case ABS_RZ:
            trigger = value >= gamepad->right_trigger ? MP_KEY_STATE_DOWN : MP_KEY_STATE_UP;
            mp_input_put_key(src->input_ctx, MP_KEY_GAMEPAD_RIGHT_TRIGGER | trigger);
            break;
    }
}

static int test_device(struct mp_input_src *src, int dir_fd, char *name)
{
    int fd = openat(dir_fd, name, O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return -1;

#define BITS_PER_LONG           (sizeof(unsigned long) * 8)
#define NBITS(x)                ((((x)-1)/BITS_PER_LONG)+1)
#define EVDEV_OFF(x)            ((x)%BITS_PER_LONG)
#define EVDEV_LONG(x)           ((x)/BITS_PER_LONG)
#define test_bit(array, bit)    ((array[EVDEV_LONG(bit)] >> EVDEV_OFF(bit)) & 1)

    unsigned long keybit[NBITS(KEY_MAX)] = { 0 };
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) {
        close(fd);
        return -1;
    }

    bool is_joystick = test_bit(keybit, BTN_SOUTH) && test_bit(keybit, BTN_EAST);
    if (!is_joystick) {
        MP_VERBOSE(src, "Device %s doesn't look like a gamepad, skipping.\n", name);
        close(fd);
        return -1;
    }

    return fd;
}

static void fill_gamepad(struct gamepad *gamepad)
{
    /* If these axes are absent, there just won't be events for them so we
     * can safely ignore the errors emitted here */
    struct input_absinfo absinfo;
    int fd = gamepad->fd;
    if (ioctl(fd, EVIOCGABS(ABS_X), &absinfo) >= 0) {
        int center = (absinfo.maximum - absinfo.minimum) / 2;
        gamepad->left_x_min = absinfo.minimum + center / 2;
        gamepad->left_x_max = absinfo.maximum - center / 2;
    }
    if (ioctl(fd, EVIOCGABS(ABS_Y), &absinfo) >= 0) {
        int center = (absinfo.maximum - absinfo.minimum) / 2;
        gamepad->left_y_min = absinfo.minimum + center / 2;
        gamepad->left_y_max = absinfo.maximum - center / 2;
    }
    if (ioctl(fd, EVIOCGABS(ABS_Z), &absinfo) >= 0) {
        gamepad->left_trigger = (absinfo.maximum - absinfo.minimum) / 2;
    }
    if (ioctl(fd, EVIOCGABS(ABS_RX), &absinfo) >= 0) {
        int center = (absinfo.maximum - absinfo.minimum) / 2;
        gamepad->right_x_min = absinfo.minimum + center / 2;
        gamepad->right_x_max = absinfo.maximum - center / 2;
    }
    if (ioctl(fd, EVIOCGABS(ABS_RY), &absinfo) >= 0) {
        int center = (absinfo.maximum - absinfo.minimum) / 2;
        gamepad->right_y_min = absinfo.minimum + center / 2;
        gamepad->right_y_max = absinfo.maximum - center / 2;
    }
    if (ioctl(fd, EVIOCGABS(ABS_RZ), &absinfo) >= 0) {
        gamepad->right_trigger = (absinfo.maximum - absinfo.minimum) / 2;
    }

    if (ioctl(fd, EVIOCGNAME(sizeof(gamepad->name)), gamepad->name) < 0) {
        gamepad->name[0] = '\0';
    }
}

static int find_gamepads(struct mp_input_src *src, struct gamepad *gamepads, int max_gamepads)
{
    DIR *dir = opendir("/dev/input");
    if (!dir) {
        MP_ERR(src, "opendir(\"/dev/input\") failed\n");
        mp_input_src_init_done(src);
        return -1;
    }

    int fd, num = 0;
    struct dirent *dirent;
    int dir_fd = dirfd(dir);
    while ((dirent = readdir(dir))) {
        if (strncmp(dirent->d_name, "event", 5) != 0)
            continue;
        if ((fd = test_device(src, dir_fd, dirent->d_name)) < 0)
            continue;

        gamepads[num].fd = fd;
        fill_gamepad(&gamepads[num]);
        MP_INFO(src, "Added controller: %s\n", gamepads[num].name);

        num++;
        if (num >= max_gamepads)
            break;
    }

    return num;
}

static void request_cancel(struct mp_input_src *src)
{
    MP_VERBOSE(src, "exiting...\n");
    /* eventfd is basically a counter of how many events we wrote, we just need
     * one here. */
    eventfd_write(event_fd, 1);
}

static void uninit(struct mp_input_src *src)
{
    MP_VERBOSE(src, "exited.\n");
}

static void close_gamepads(struct mp_input_src *src, struct gamepad *gamepads, int num)
{
    while (num--) {
        MP_INFO(src, "Removed controller: %s\n", gamepads[num].name);
        close(gamepads[num].fd);
        gamepads[num].fd = -1;
    }
}

static void read_gamepad_thread(struct mp_input_src *src, void *param)
{
    struct gamepad gamepads[MAX_GAMEPADS];
    int num = find_gamepads(src, gamepads, MAX_GAMEPADS);
    if (num == 0) {
        MP_VERBOSE(src, "Couldn't find any gamepad.");
        mp_input_src_init_done(src);
        return;
    }

    event_fd = eventfd(0, EFD_CLOEXEC);
    if (event_fd < 0) {
        MP_ERR(src, "Couldn't create eventfd for gamepad.");
        close_gamepads(src, gamepads, num);
        mp_input_src_init_done(src);
        return;
    }

    src->cancel = request_cancel;
    src->uninit = uninit;

    mp_input_src_init_done(src);

    struct pollfd pfds[MAX_GAMEPADS + 1];
    pfds[0].fd = event_fd;
    pfds[0].events = POLLIN;
    for (int i = 0; i < num; ++i) {
        pfds[i + 1].fd = gamepads[i].fd;
        pfds[i + 1].events = POLLIN;
    }

    int ret;
    while ((ret = poll(pfds, num + 1, -1)) >= 0) {
        /* First check whether we have to exit this thread. */
        if (pfds[0].revents == POLLIN) {
            eventfd_t value;
            eventfd_read(event_fd, &value);
            break;
        }

	/* Then handle a single gamepad, we’ll loop again if more than one has
	 * events for us. */
        int i = 0, fd = -1;
        for (; i < num; ++i) {
            if (pfds[i + 1].revents == POLLIN) {
                fd = pfds[i + 1].fd;
                break;
            }
        }

        struct input_event ev;
        if (read(fd, &ev, sizeof(ev)) < 0) {
            MP_ERR(src, "read() failed\n");
            break;
        }

        if (ev.type == EV_KEY) {
            const int key = lookup_button_mp_key(ev.code);
            if (key != -1) {
                const int value = ev.value ? MP_KEY_STATE_DOWN : MP_KEY_STATE_UP;
                mp_input_put_key(src->input_ctx, key | value);
            }
        } else if (ev.type == EV_ABS) {
            handle_abs(src, ev.code, ev.value, &gamepads[i]);
        }
    }

    close_gamepads(src, gamepads, num);
}

void mp_input_evdev_gamepad_add(struct input_ctx *ictx)
{
    mp_input_add_thread_src(ictx, NULL, read_gamepad_thread);
}
