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

#include "common/common.h"
#include "clipboard.h"
#include "player/core.h"
#include "video/out/vo.h"

struct clipboard_vo_priv {
    struct MPContext *mpctx;
};

static int init(struct clipboard_ctx *cl, struct clipboard_init_params *params)
{
    struct clipboard_vo_priv *priv = talloc_ptrtype(cl, priv);
    priv->mpctx = params->mpctx;
    cl->priv = priv;
    return CLIPBOARD_SUCCESS;
}

static int get_data(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                    struct clipboard_data *out, void *talloc_ctx)
{
    struct clipboard_vo_priv *priv = cl->priv;
    struct vo *vo = priv->mpctx->video_out;
    struct voctrl_clipboard vc = {
        .data = *out,
        .params = *params,
        .talloc_ctx = talloc_ctx,
    };

    if (vo && vo_control(vo, VOCTRL_GET_CLIPBOARD, &vc) == VO_TRUE) {
        *out = vc.data;
        return CLIPBOARD_SUCCESS;
    } else {
        MP_WARN(cl, "VO is not initialized, or it does not support getting clipboard.\n");
        return CLIPBOARD_FAILED;
    }
}

static int set_data(struct clipboard_ctx *cl, struct clipboard_access_params *params,
                    struct clipboard_data *data)
{
    struct clipboard_vo_priv *priv = cl->priv;
    struct vo *vo = priv->mpctx->video_out;
    struct voctrl_clipboard vc = {
        .data = *data,
        .params = *params,
    };

    if (vo && vo_control(vo, VOCTRL_SET_CLIPBOARD, &vc) == VO_TRUE) {
        return CLIPBOARD_SUCCESS;
    } else {
        MP_WARN(cl, "VO is not initialized, or it does not support setting clipboard.\n");
        return CLIPBOARD_FAILED;
    }
}

const struct clipboard_backend clipboard_backend_vo = {
    .name = "vo",
    .desc = "VO clipboard",
    .init = init,
    .get_data = get_data,
    .set_data = set_data,
};
