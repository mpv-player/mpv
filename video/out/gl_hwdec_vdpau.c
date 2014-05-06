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

#include <stddef.h>
#include <assert.h>

#include "gl_common.h"
#include "video/vdpau.h"
#include "video/hwdec.h"

// This is a GL_NV_vdpau_interop specification bug, and headers (unfortunately)
// follow it. I'm not sure about the original nvidia headers.
#define BRAINDEATH(x) ((void *)(uintptr_t)(x))

static int reinit(struct gl_hwdec *hw, const struct mp_image_params *params);

struct priv {
    struct mp_log *log;
    struct mp_vdpau_ctx *ctx;
    uint64_t preemption_counter;
    struct mp_image_params image_params;
    GLuint gl_texture;
    GLvdpauSurfaceNV vdpgl_surface;
    VdpOutputSurface vdp_surface;
    VdpVideoMixer video_mixer;
};

static void mark_vdpau_objects_uninitialized(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;

    p->vdp_surface = VDP_INVALID_HANDLE;
    p->video_mixer = VDP_INVALID_HANDLE;
}

static int handle_preemption(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;

    if (!mp_vdpau_status_ok(p->ctx)) {
        mark_vdpau_objects_uninitialized(hw);
        return -1;
    }

    if (p->preemption_counter == p->ctx->preemption_counter)
        return 0;

    mark_vdpau_objects_uninitialized(hw);

    p->preemption_counter = p->ctx->preemption_counter;

    if (reinit(hw, &p->image_params) < 0)
        return -1;

    return 1;
}

static void destroy_objects(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    GL *gl = hw->mpgl->gl;
    struct vdp_functions *vdp = p->ctx->vdp;
    VdpStatus vdp_st;

    if (p->vdpgl_surface)
        gl->VDPAUUnregisterSurfaceNV(p->vdpgl_surface);
    p->vdpgl_surface = 0;

    glDeleteTextures(1, &p->gl_texture);
    p->gl_texture = 0;

    if (p->vdp_surface != VDP_INVALID_HANDLE) {
        vdp_st = vdp->output_surface_destroy(p->vdp_surface);
        CHECK_VDP_WARNING(p, "Error when calling vdp_output_surface_destroy");
    }

    if (p->video_mixer != VDP_INVALID_HANDLE) {
        vdp_st = vdp->video_mixer_destroy(p->video_mixer);
        CHECK_VDP_WARNING(p, "Error when calling vdp_video_mixer_destroy");
    }

    glCheckError(gl, hw->log, "Before uninitializing OpenGL interop");

    gl->VDPAUFiniNV();

    // If the GL/vdpau state is not initialized, above calls raises an error.
    while (1) {
        if (gl->GetError() == GL_NO_ERROR)
            break;
    }

    mark_vdpau_objects_uninitialized(hw);
}

static void destroy(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;

    destroy_objects(hw);
    mp_vdpau_destroy(p->ctx);
}

static int create(struct gl_hwdec *hw)
{
    GL *gl = hw->mpgl->gl;
    if (hw->info->vdpau_ctx)
        return -1;
    if (!hw->mpgl->vo->x11)
        return -1;
    if (!(gl->mpgl_caps & MPGL_CAP_VDPAU))
        return -1;
    struct priv *p = talloc_zero(hw, struct priv);
    hw->priv = p;
    p->log = hw->log;
    p->ctx = mp_vdpau_create_device_x11(hw->log, hw->mpgl->vo->x11);
    if (!p->ctx)
        return -1;
    p->preemption_counter = p->ctx->preemption_counter;
    mark_vdpau_objects_uninitialized(hw);
    hw->info->vdpau_ctx = p->ctx;
    hw->converted_imgfmt = IMGFMT_RGB0;
    return 0;
}

static int reinit(struct gl_hwdec *hw, const struct mp_image_params *params)
{
    struct priv *p = hw->priv;
    GL *gl = hw->mpgl->gl;
    struct vdp_functions *vdp = p->ctx->vdp;
    VdpStatus vdp_st;

    destroy_objects(hw);

    p->image_params = *params;

    if (!mp_vdpau_status_ok(p->ctx))
        return -1;

    gl->VDPAUInitNV(BRAINDEATH(p->ctx->vdp_device), p->ctx->get_proc_address);

#define VDP_NUM_MIXER_PARAMETER 3
    static const VdpVideoMixerParameter parameters[VDP_NUM_MIXER_PARAMETER] = {
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE,
    };

    const void *const parameter_values[VDP_NUM_MIXER_PARAMETER] = {
        &(uint32_t){params->w},
        &(uint32_t){params->h},
        &(VdpChromaType){VDP_CHROMA_TYPE_420},
    };
    vdp_st = vdp->video_mixer_create(p->ctx->vdp_device, 0, NULL,
                                     VDP_NUM_MIXER_PARAMETER,
                                     parameters, parameter_values,
                                     &p->video_mixer);
    CHECK_VDP_ERROR(p, "Error when calling vdp_video_mixer_create");

    struct mp_csp_params cparams = MP_CSP_PARAMS_DEFAULTS;
    cparams.colorspace.levels_in = params->colorlevels;
    cparams.colorspace.format = params->colorspace;
    // VdpCSCMatrix happens to be compatible with mpv's CSC matrix type
    // both are float[3][4]
    VdpCSCMatrix matrix;
    mp_get_yuv2rgb_coeffs(&cparams, matrix);
    VdpVideoMixerAttribute csc_attr = VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX;
    vdp_st = vdp->video_mixer_set_attribute_values(p->video_mixer, 1, &csc_attr,
                                                   &(const void *){matrix});
    CHECK_VDP_WARNING(p, "Error when setting vdpau colorspace conversion matrix");

    vdp_st = vdp->output_surface_create(p->ctx->vdp_device,
                                        VDP_RGBA_FORMAT_B8G8R8A8,
                                        params->w, params->h, &p->vdp_surface);
    CHECK_VDP_ERROR(p, "Error when calling vdp_output_surface_create");

    gl->GenTextures(1, &p->gl_texture);
    gl->BindTexture(GL_TEXTURE_2D, p->gl_texture);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->BindTexture(GL_TEXTURE_2D, 0);

    p->vdpgl_surface = gl->VDPAURegisterOutputSurfaceNV(BRAINDEATH(p->vdp_surface),
                                                        GL_TEXTURE_2D,
                                                        1, &p->gl_texture);
    if (!p->vdpgl_surface)
        return -1;

    gl->VDPAUSurfaceAccessNV(p->vdpgl_surface, GL_READ_ONLY);

    glCheckError(gl, hw->log, "After initializing vdpau OpenGL interop");

    return 0;
}

static int map_image(struct gl_hwdec *hw, struct mp_image *hw_image,
                     GLuint *out_textures)
{
    struct priv *p = hw->priv;
    GL *gl = hw->mpgl->gl;
    struct vdp_functions *vdp = p->ctx->vdp;
    VdpStatus vdp_st;

    assert(hw_image && hw_image->imgfmt == IMGFMT_VDPAU);
    VdpVideoSurface video_surface = (intptr_t)hw_image->planes[3];

    if (handle_preemption(hw) < 0)
        return -1;

    if (!p->vdpgl_surface)
        return -1;

    VdpRect *video_rect = NULL;
    vdp_st = vdp->video_mixer_render(p->video_mixer, VDP_INVALID_HANDLE,
                                     0, VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME,
                                     0, NULL, video_surface, 0, NULL,
                                     video_rect, p->vdp_surface,
                                     NULL, NULL, 0, NULL);
    CHECK_VDP_ERROR(p, "Error when calling vdp_video_mixer_render");

    gl->VDPAUMapSurfacesNV(1, &p->vdpgl_surface);
    out_textures[0] = p->gl_texture;
    return 0;
}

static void unmap_image(struct gl_hwdec *hw)
{
    struct priv *p = hw->priv;
    GL *gl = hw->mpgl->gl;

    gl->VDPAUUnmapSurfacesNV(1, &p->vdpgl_surface);
}

const struct gl_hwdec_driver gl_hwdec_vdpau = {
    .api_name = "vdpau",
    .imgfmt = IMGFMT_VDPAU,
    .create = create,
    .reinit = reinit,
    .map_image = map_image,
    .unmap_image = unmap_image,
    .destroy = destroy,
};
