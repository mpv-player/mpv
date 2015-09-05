#include "common/common.h"

#include "hwdec.h"
#include "utils.h"
#include "video/d3d.h"
#include "video/hwdec.h"

// This does not provide real (zero-copy) interop - it merely exists for
// making sure the same D3D device is used for decoding and display, which
// may help with OpenGL fullscreen mode.

struct priv {
    struct mp_d3d_ctx ctx;
};

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    if (p->ctx.d3d9_device)
        IDirect3DDevice9_Release(p->ctx.d3d9_device);
}

static int create(struct gl_hwdec *hw)
{
    GL *gl = hw->gl;
    if (hw->hwctx || !gl->MPGetD3DInterface)
        return -1;

    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;

    p->ctx.d3d9_device = gl->MPGetD3DInterface("IDirect3DDevice9");
    if (!p->ctx.d3d9_device)
        return -1;

    p->ctx.hwctx.type = HWDEC_DXVA2_COPY;
    p->ctx.hwctx.d3d_ctx = &p->ctx;

    MP_VERBOSE(hw, "Using libmpv supplied device %p.\n", p->ctx.d3d9_device);

    hw->hwctx = &p->ctx.hwctx;
    hw->converted_imgfmt = 0;
    return 0;
}

static int reinit(struct gl_hwdec *hw, struct mp_image_params *params)
{
    return -1;
}

static int map_image(struct gl_hwdec *hw, struct mp_image *hw_image,
                     GLuint *out_textures)
{
    return -1;
}

const struct gl_hwdec_driver gl_hwdec_dxva2 = {
    .api_name = "dxva2",
    .imgfmt = -1,
    .create = create,
    .reinit = reinit,
    .map_image = map_image,
    .destroy = destroy,
};
