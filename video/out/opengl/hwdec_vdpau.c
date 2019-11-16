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

#include <stddef.h>
#include <assert.h>

#include "video/out/gpu/hwdec.h"
#include "ra_gl.h"
#include "video/vdpau.h"
#include "video/vdpau_mixer.h"

// This is a GL_NV_vdpau_interop specification bug, and headers (unfortunately)
// follow it. I'm not sure about the original nvidia headers.
#define BRAINDEATH(x) ((void *)(uintptr_t)(x))

struct priv_owner {
    struct mp_vdpau_ctx *ctx;
};

struct priv {
    struct mp_vdpau_ctx *ctx;
    GL *gl;
    uint64_t preemption_counter;
    GLuint gl_texture;
    bool vdpgl_initialized;
    GLvdpauSurfaceNV vdpgl_surface;
    VdpOutputSurface vdp_surface;
    struct mp_vdpau_mixer *mixer;
    struct ra_imgfmt_desc direct_desc;
    bool mapped;
};

static int init(struct ra_hwdec *hw)
{
    Display *x11disp = ra_get_native_resource(hw->ra, "x11");
    if (!x11disp || !ra_is_gl(hw->ra))
        return -1;
    GL *gl = ra_gl_get(hw->ra);
    if (!(gl->mpgl_caps & MPGL_CAP_VDPAU))
        return -1;
    struct priv_owner *p = hw->priv;
    p->ctx = mp_vdpau_create_device_x11(hw->log, x11disp, true);
    if (!p->ctx)
        return -1;
    if (mp_vdpau_handle_preemption(p->ctx, NULL) < 1)
        return -1;
    if (hw->probing && mp_vdpau_guess_if_emulated(p->ctx))
        return -1;
    p->ctx->hwctx.driver_name = hw->driver->name;
    hwdec_devices_add(hw->devs, &p->ctx->hwctx);
    return 0;
}

static void uninit(struct ra_hwdec *hw)
{
    struct priv_owner *p = hw->priv;

    if (p->ctx)
        hwdec_devices_remove(hw->devs, &p->ctx->hwctx);
    mp_vdpau_destroy(p->ctx);
}

static void mapper_unmap(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = p->gl;

    for (int n = 0; n < 4; n++)
        ra_tex_free(mapper->ra, &mapper->tex[n]);

    if (p->mapped) {
        gl->VDPAUUnmapSurfacesNV(1, &p->vdpgl_surface);
    }
    p->mapped = false;
}

static void mark_vdpau_objects_uninitialized(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;

    p->vdp_surface = VDP_INVALID_HANDLE;
    p->mapped = false;
}

static void mapper_uninit(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = p->gl;
    struct vdp_functions *vdp = &p->ctx->vdp;
    VdpStatus vdp_st;

    assert(!p->mapped);

    if (p->vdpgl_surface)
        gl->VDPAUUnregisterSurfaceNV(p->vdpgl_surface);
    p->vdpgl_surface = 0;

    gl->DeleteTextures(1, &p->gl_texture);

    if (p->vdp_surface != VDP_INVALID_HANDLE) {
        vdp_st = vdp->output_surface_destroy(p->vdp_surface);
        CHECK_VDP_WARNING(mapper, "Error when calling vdp_output_surface_destroy");
    }
    p->vdp_surface = VDP_INVALID_HANDLE;

    gl_check_error(gl, mapper->log, "Before uninitializing OpenGL interop");

    if (p->vdpgl_initialized)
        gl->VDPAUFiniNV();

    p->vdpgl_initialized = false;

    gl_check_error(gl, mapper->log, "After uninitializing OpenGL interop");

    mp_vdpau_mixer_destroy(p->mixer);
}

static int mapper_init(struct ra_hwdec_mapper *mapper)
{
    struct priv_owner *p_owner = mapper->owner->priv;
    struct priv *p = mapper->priv;

    p->gl = ra_gl_get(mapper->ra);
    p->ctx = p_owner->ctx;

    GL *gl = p->gl;
    struct vdp_functions *vdp = &p->ctx->vdp;
    VdpStatus vdp_st;

    p->vdp_surface = VDP_INVALID_HANDLE;
    p->mixer = mp_vdpau_mixer_create(p->ctx, mapper->log);
    if (!p->mixer)
        return -1;

    mapper->dst_params = mapper->src_params;

    if (mp_vdpau_handle_preemption(p->ctx, &p->preemption_counter) < 0)
        return -1;

    gl->VDPAUInitNV(BRAINDEATH(p->ctx->vdp_device), p->ctx->get_proc_address);

    p->vdpgl_initialized = true;

    gl->GenTextures(1, &p->gl_texture);

    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    vdp_st = vdp->output_surface_create(p->ctx->vdp_device,
                                        VDP_RGBA_FORMAT_B8G8R8A8,
                                        mapper->src_params.w,
                                        mapper->src_params.h,
                                        &p->vdp_surface);
    CHECK_VDP_ERROR(mapper, "Error when calling vdp_output_surface_create");

    p->vdpgl_surface = gl->VDPAURegisterOutputSurfaceNV(BRAINDEATH(p->vdp_surface),
                                                        GL_TEXTURE_2D,
                                                        1, &p->gl_texture);
    if (!p->vdpgl_surface)
        return -1;

    gl->VDPAUSurfaceAccessNV(p->vdpgl_surface, GL_READ_ONLY);

    mapper->dst_params.imgfmt = IMGFMT_RGB0;
    mapper->dst_params.hw_subfmt = 0;

    gl_check_error(gl, mapper->log, "After initializing vdpau OpenGL interop");

    return 0;
}

static int mapper_map(struct ra_hwdec_mapper *mapper)
{
    struct priv *p = mapper->priv;
    GL *gl = p->gl;

    int pe = mp_vdpau_handle_preemption(p->ctx, &p->preemption_counter);
    if (pe < 1) {
        mark_vdpau_objects_uninitialized(mapper);
        if (pe < 0)
            return -1;
        mapper_uninit(mapper);
        if (mapper_init(mapper) < 0)
            return -1;
    }

    if (!p->vdpgl_surface)
        return -1;

    mp_vdpau_mixer_render(p->mixer, NULL, p->vdp_surface, NULL, mapper->src,
                            NULL);

    gl->VDPAUMapSurfacesNV(1, &p->vdpgl_surface);

    p->mapped = true;

    struct ra_tex_params params = {
        .dimensions = 2,
        .w = mapper->src_params.w,
        .h = mapper->src_params.h,
        .d = 1,
        .format = ra_find_unorm_format(mapper->ra, 1, 4),
        .render_src = true,
        .src_linear = true,
    };

    if (!params.format)
        return -1;

    mapper->tex[0] =
        ra_create_wrapped_tex(mapper->ra, &params, p->gl_texture);
    if (!mapper->tex[0])
        return -1;

    return 0;
}

const struct ra_hwdec_driver ra_hwdec_vdpau = {
    .name = "vdpau-gl",
    .priv_size = sizeof(struct priv_owner),
    .imgfmts = {IMGFMT_VDPAU, 0},
    .init = init,
    .uninit = uninit,
    .mapper = &(const struct ra_hwdec_mapper_driver){
        .priv_size = sizeof(struct priv),
        .init = mapper_init,
        .uninit = mapper_uninit,
        .map = mapper_map,
        .unmap = mapper_unmap,
    },
};
