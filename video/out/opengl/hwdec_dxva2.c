#include <d3d9.h>

#include "common/common.h"

#include "hwdec.h"
#include "utils.h"
#include "video/hwdec.h"

// This does not provide real (zero-copy) interop - it merely exists for
// making sure the same D3D device is used for decoding and display, which
// may help with OpenGL fullscreen mode.

struct priv {
    struct mp_hwdec_ctx hwctx;
};

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    hwdec_devices_remove(hw->devs, &p->hwctx);
    if (p->hwctx.ctx)
        IDirect3DDevice9_Release((IDirect3DDevice9 *)p->hwctx.ctx);
}

static int create(struct gl_hwdec *hw)
{
    GL *gl = hw->gl;
    if (!gl->MPGetNativeDisplay)
        return -1;

    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;

    IDirect3DDevice9 *d3d = gl->MPGetNativeDisplay("IDirect3DDevice9");
    if (!d3d)
        return -1;

    MP_VERBOSE(hw, "Using libmpv supplied device %p.\n", d3d);

    p->hwctx = (struct mp_hwdec_ctx){
        .type = HWDEC_DXVA2_COPY,
        .driver_name = hw->driver->name,
        .ctx = d3d,
    };
    hwdec_devices_add(hw->devs, &p->hwctx);
    return 0;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    return -1;
}

static int map_frame(struct gl_hwdec *hw, struct mp_image *hw_image,
                     struct gl_hwdec_frame *out_frame)
{
    return -1;
}

const struct gl_hwdec_driver gl_hwdec_dxva2 = {
    .name = "dxva2-dummy",
    .api = HWDEC_DXVA2_COPY,
    .imgfmt = -1,
    .create = create,
    .reinit = reinit,
    .map_frame = map_frame,
    .destroy = destroy,
};
