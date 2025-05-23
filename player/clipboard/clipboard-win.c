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
#include "common/common.h"
#include "clipboard.h"

struct clipboard_win_priv {
    DWORD sequence_number;
};

static int init(struct clipboard_ctx *cl, struct clipboard_init_params *params)
{
    cl->priv = talloc_zero(cl, struct clipboard_win_priv);
    return CLIPBOARD_SUCCESS;
}

static bool data_changed(struct clipboard_ctx *cl)
{
    struct clipboard_win_priv *priv = cl->priv;
    DWORD sequence_number = GetClipboardSequenceNumber();
    if (sequence_number != priv->sequence_number) {
        priv->sequence_number = sequence_number;
        return true;
    }
    return false;
}

static int get_data(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                    struct clipboard_data *out, void *talloc_ctx)
{
    if (params->type != CLIPBOARD_DATA_TEXT || !IsClipboardFormatAvailable(CF_UNICODETEXT))
        return CLIPBOARD_FAILED;
    if (!OpenClipboard(NULL))
        return CLIPBOARD_FAILED;

    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    wchar_t *wdata;
    if (!h || !(wdata = GlobalLock(h))) {
        CloseClipboard();
        return CLIPBOARD_FAILED;
    }

    int len = WideCharToMultiByte(CP_UTF8, 0, wdata, -1, NULL, 0, NULL, NULL);
    if (len <= 0)
        abort();
    char *data = talloc_array(talloc_ctx, char, len);
    WideCharToMultiByte(CP_UTF8, 0, wdata, -1, data, len, NULL, NULL);
    out->type = CLIPBOARD_DATA_TEXT;
    out->u.text = data;

    GlobalUnlock(h);
    CloseClipboard();
    return CLIPBOARD_SUCCESS;
}

static int set_data(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                    struct clipboard_data *data)
{
    if (params->type != CLIPBOARD_DATA_TEXT || data->type != CLIPBOARD_DATA_TEXT)
        return CLIPBOARD_FAILED;
    if (!OpenClipboard(NULL))
        return CLIPBOARD_FAILED;

    int len = MultiByteToWideChar(CP_UTF8, 0, data->u.text, -1, NULL, 0);
    if (len <= 0)
        abort();
    HANDLE h = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar_t));
    wchar_t *wdata;
    if (!h || !(wdata = GlobalLock(h))) {
        CloseClipboard();
        return CLIPBOARD_FAILED;
    }

    MultiByteToWideChar(CP_UTF8, 0, data->u.text, -1, wdata, len);
    GlobalUnlock(h);
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, h);
    CloseClipboard();
    return CLIPBOARD_SUCCESS;
}

const struct clipboard_backend clipboard_backend_win32 = {
    .name = "win32",
    .desc = "Windows clipboard",
    .init = init,
    .data_changed = data_changed,
    .get_data = get_data,
    .set_data = set_data,
};
