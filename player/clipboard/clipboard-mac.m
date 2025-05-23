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

#include "clipboard.h"
#include "osdep/mac/swift.h"

struct clipboard_mac_priv {
    Clipboard *clipboard;
};

static int init(struct clipboard_ctx *cl, struct clipboard_init_params *params)
{
    struct clipboard_mac_priv *p = cl->priv = talloc_zero(cl, struct clipboard_mac_priv);
    p->clipboard = [[Clipboard alloc] init];
    return CLIPBOARD_SUCCESS;
}

static bool data_changed(struct clipboard_ctx *cl)
{
    struct clipboard_mac_priv *p = cl->priv;
    return [p->clipboard changed];
}

static int get_data(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                    struct clipboard_data *out, void *talloc_ctx)
{
    struct clipboard_mac_priv *p = cl->priv;
    return [p->clipboard getWithParams:params out:out tallocCtx:talloc_ctx];
}

static int set_data(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                    struct clipboard_data *data)
{
    struct clipboard_mac_priv *p = cl->priv;
    return [p->clipboard setWithParams:params data:data];
}

const struct clipboard_backend clipboard_backend_mac = {
    .name = "mac",
    .desc = "macOS clipboard",
    .init = init,
    .data_changed = data_changed,
    .get_data = get_data,
    .set_data = set_data,
};
