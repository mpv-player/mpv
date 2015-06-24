/*
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <windows.h>
#include "osdep/w32_keyboard.h"
#include "input/keycodes.h"

struct keymap {
    int from;
    int to;
};

static const struct keymap vk_map_ext[] = {
    // cursor keys
    {VK_LEFT, MP_KEY_LEFT}, {VK_UP, MP_KEY_UP}, {VK_RIGHT, MP_KEY_RIGHT},
    {VK_DOWN, MP_KEY_DOWN},

    // navigation block
    {VK_INSERT, MP_KEY_INSERT}, {VK_DELETE, MP_KEY_DELETE},
    {VK_HOME, MP_KEY_HOME}, {VK_END, MP_KEY_END}, {VK_PRIOR, MP_KEY_PAGE_UP},
    {VK_NEXT, MP_KEY_PAGE_DOWN},

    // numpad independent of numlock
    {VK_RETURN, MP_KEY_KPENTER},

    {0, 0}
};

static const struct keymap vk_map[] = {
    // special keys
    {VK_ESCAPE, MP_KEY_ESC}, {VK_BACK, MP_KEY_BS}, {VK_TAB, MP_KEY_TAB},
    {VK_RETURN, MP_KEY_ENTER}, {VK_PAUSE, MP_KEY_PAUSE},
    {VK_SNAPSHOT, MP_KEY_PRINT}, {VK_APPS, MP_KEY_MENU},

    // F-keys
    {VK_F1, MP_KEY_F+1}, {VK_F2, MP_KEY_F+2}, {VK_F3, MP_KEY_F+3},
    {VK_F4, MP_KEY_F+4}, {VK_F5, MP_KEY_F+5}, {VK_F6, MP_KEY_F+6},
    {VK_F7, MP_KEY_F+7}, {VK_F8, MP_KEY_F+8}, {VK_F9, MP_KEY_F+9},
    {VK_F10, MP_KEY_F+10}, {VK_F11, MP_KEY_F+11}, {VK_F12, MP_KEY_F+12},

    // numpad with numlock
    {VK_NUMPAD0, MP_KEY_KP0}, {VK_NUMPAD1, MP_KEY_KP1},
    {VK_NUMPAD2, MP_KEY_KP2}, {VK_NUMPAD3, MP_KEY_KP3},
    {VK_NUMPAD4, MP_KEY_KP4}, {VK_NUMPAD5, MP_KEY_KP5},
    {VK_NUMPAD6, MP_KEY_KP6}, {VK_NUMPAD7, MP_KEY_KP7},
    {VK_NUMPAD8, MP_KEY_KP8}, {VK_NUMPAD9, MP_KEY_KP9},
    {VK_DECIMAL, MP_KEY_KPDEC},

    // numpad without numlock
    {VK_INSERT, MP_KEY_KPINS}, {VK_END, MP_KEY_KP1}, {VK_DOWN, MP_KEY_KP2},
    {VK_NEXT, MP_KEY_KP3}, {VK_LEFT, MP_KEY_KP4}, {VK_CLEAR, MP_KEY_KP5},
    {VK_RIGHT, MP_KEY_KP6}, {VK_HOME, MP_KEY_KP7}, {VK_UP, MP_KEY_KP8},
    {VK_PRIOR, MP_KEY_KP9}, {VK_DELETE, MP_KEY_KPDEL},

    {0, 0}
};

static int lookup_keymap(const struct keymap *map, int key)
{
    while (map->from && map->from != key) map++;
    return map->to;
}

int mp_w32_vkey_to_mpkey(UINT vkey, bool extended)
{
    // The extended flag is set for the navigation cluster and the arrow keys,
    // so it can be used to differentiate between them and the numpad. The
    // numpad enter key also has this flag set.
    int mpkey = lookup_keymap(extended ? vk_map_ext : vk_map, vkey);

    // If we got the extended flag for a key we don't recognize, search the
    // normal keymap before giving up
    if (extended && !mpkey)
        mpkey = lookup_keymap(vk_map, vkey);

    return mpkey;
}
