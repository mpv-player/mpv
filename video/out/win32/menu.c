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

#include <windows.h>
#include <string.h>

#include "libmpv/client.h"
#include "osdep/io.h"
#include "mpv_talloc.h"

#include "menu.h"

struct menu_ctx {
    HWND  hwnd;
    bool  updated;
    HMENU menu;
    void *ta_data; // talloc context for MENUITEMINFOW.dwItemData
};

// append menu item to HMENU
static int append_menu(HMENU hmenu, UINT fMask, UINT fType, UINT fState,
                       wchar_t *title, HMENU submenu, void *data)
{
    static UINT id = WM_USER + 100;
    MENUITEMINFOW mii = {0};

    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_ID | fMask;
    mii.wID = id++;

    if (fMask & MIIM_FTYPE)
        mii.fType = fType;
    if (fMask & MIIM_STATE)
        mii.fState = fState;
    if (fMask & MIIM_STRING) {
        mii.dwTypeData = title;
        mii.cch = wcslen(title);
    }
    if (fMask & MIIM_SUBMENU)
        mii.hSubMenu = submenu;
    if (fMask & MIIM_DATA)
        mii.dwItemData = (ULONG_PTR)data;

    return InsertMenuItemW(hmenu, -1, TRUE, &mii) ? mii.wID : -1;
}

// build fState for menu item creation
static int build_state(mpv_node *node)
{
    int fState = 0;
    for (int i = 0; i < node->u.list->num; i++) {
        mpv_node *item = &node->u.list->values[i];
        if (item->format != MPV_FORMAT_STRING)
            continue;

        if (strcmp(item->u.string, "hidden") == 0) {
            return -1;
        } else if (strcmp(item->u.string, "checked") == 0) {
            fState |= MFS_CHECKED;
        } else if (strcmp(item->u.string, "disabled") == 0) {
            fState |= MFS_DISABLED;
        }
    }
    return fState;
}

// build dwTypeData for menu item creation
static wchar_t *build_title(void *talloc_ctx, char *title, char *shortcut)
{
    if (shortcut && shortcut[0]) {
        char *buf = talloc_asprintf(NULL, "%s\t%s", title, shortcut);
        wchar_t *wbuf = mp_from_utf8(talloc_ctx, buf);
        talloc_free(buf);
        return wbuf;
    }
    return mp_from_utf8(talloc_ctx, title);
}

// build HMENU from mpv node
//
// node structure:
//
// MPV_FORMAT_NODE_ARRAY
//   MPV_FORMAT_NODE_MAP (menu item)
//      "type"           MPV_FORMAT_STRING
//      "title"          MPV_FORMAT_STRING
//      "cmd"            MPV_FORMAT_STRING
//      "shortcut"       MPV_FORMAT_STRING
//      "state"          MPV_FORMAT_NODE_ARRAY[MPV_FORMAT_STRING]
//      "submenu"        MPV_FORMAT_NODE_ARRAY[menu item]
static void build_menu(void *talloc_ctx, HMENU hmenu, struct mpv_node *node)
{
    if (node->format != MPV_FORMAT_NODE_ARRAY)
        return;

    for (int i = 0; i < node->u.list->num; i++) {
        mpv_node *item = &node->u.list->values[i];
        if (item->format != MPV_FORMAT_NODE_MAP)
            continue;

        mpv_node_list *list = item->u.list;

        char *type = "";
        char *title = NULL;
        char *cmd = NULL;
        char *shortcut = NULL;
        int fState = 0;
        HMENU submenu = NULL;

        for (int j = 0; j < list->num; j++) {
            char *key = list->keys[j];
            mpv_node *value = &list->values[j];

            switch (value->format) {
            case MPV_FORMAT_STRING:
                if (strcmp(key, "title") == 0) {
                    title = value->u.string;
                } else if (strcmp(key, "cmd") == 0) {
                    cmd = value->u.string;
                } else if (strcmp(key, "type") == 0) {
                    type = value->u.string;
                } else if (strcmp(key, "shortcut") == 0) {
                    shortcut = value->u.string;
                }
                break;
            case MPV_FORMAT_NODE_ARRAY:
                if (strcmp(key, "state") == 0) {
                    fState = build_state(value);
                } else if (strcmp(key, "submenu") == 0) {
                    submenu = CreatePopupMenu();
                    build_menu(talloc_ctx, submenu, value);
                }
                break;
            default:
                break;
            }
        }

        if (fState == -1) // hidden
            continue;

        if (strcmp(type, "separator") == 0) {
            append_menu(hmenu, MIIM_FTYPE, MFT_SEPARATOR, 0, NULL, NULL, NULL);
        } else {
            if (title == NULL || title[0] == '\0')
                continue;

            UINT fMask = MIIM_STRING | MIIM_STATE;
            bool grayed = false;
            if (strcmp(type, "submenu") == 0) {
                if (submenu == NULL)
                    submenu = CreatePopupMenu();
                fMask |= MIIM_SUBMENU;
                grayed = GetMenuItemCount(submenu) == 0;
            } else {
                fMask |= MIIM_DATA;
                grayed = cmd == NULL || cmd[0] == '\0' || cmd[0] == '#' ||
                         strcmp(cmd, "ignore") == 0;
            }
            int id = append_menu(hmenu, fMask, 0, (UINT)fState,
                                 build_title(talloc_ctx, title, shortcut),
                                 submenu, talloc_strdup(talloc_ctx, cmd));
            if (id > 0 && grayed)
                EnableMenuItem(hmenu, id, MF_BYCOMMAND | MF_GRAYED);
        }
    }
}

struct menu_ctx *mp_win32_menu_init(HWND hwnd)
{
    struct menu_ctx *ctx = talloc_ptrtype(NULL, ctx);
    ctx->hwnd = hwnd;
    ctx->updated = false;
    ctx->menu = CreatePopupMenu();
    ctx->ta_data = talloc_new(ctx);
    return ctx;
}

void mp_win32_menu_uninit(struct menu_ctx *ctx)
{
    DestroyMenu(ctx->menu);
    talloc_free(ctx);
}

void mp_win32_menu_show(struct menu_ctx *ctx, HWND hwnd)
{
    POINT pt;
    RECT rc;

    if (!GetCursorPos(&pt))
        return;

    GetClientRect(hwnd, &rc);
    ScreenToClient(hwnd, &pt);

    if (!PtInRect(&rc, pt))
        return;

    ClientToScreen(hwnd, &pt);
    TrackPopupMenuEx(ctx->menu, TPM_LEFTALIGN | TPM_LEFTBUTTON, pt.x, pt.y,
                     hwnd, NULL);
}

void mp_win32_menu_update(struct menu_ctx *ctx, struct mpv_node *data)
{
    while (GetMenuItemCount(ctx->menu) > 0)
        RemoveMenu(ctx->menu, 0, MF_BYPOSITION);
    talloc_free_children(ctx->ta_data);

    build_menu(ctx->ta_data, ctx->menu, data);
    if (!ctx->updated) {
        append_menu(GetSystemMenu(ctx->hwnd, FALSE), MIIM_FTYPE, MFT_SEPARATOR, 0, NULL, NULL, NULL);
        append_menu(GetSystemMenu(ctx->hwnd, FALSE), MIIM_STRING | MIIM_SUBMENU, 0, 0,
                    build_title(ctx->ta_data, "mpv", NULL), ctx->menu, NULL);
        ctx->updated = true;
    } else if (data->format != MPV_FORMAT_NODE_ARRAY) {
        // recreate ctx->menu because it is destroyed here
        GetSystemMenu(ctx->hwnd, TRUE);
        ctx->menu = CreatePopupMenu();
        ctx->updated = false;
    }
}

const char* mp_win32_menu_get_cmd(struct menu_ctx *ctx, UINT id)
{
    MENUITEMINFOW mii = {0};
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_DATA;

    GetMenuItemInfoW(ctx->menu, id, FALSE, &mii);
    return (const char *)mii.dwItemData;
}
