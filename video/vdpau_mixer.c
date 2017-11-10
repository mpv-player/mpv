/*
 * This file is part of mpv.
 *
 * Parts of video mixer creation code:
 * Copyright (C) 2008 NVIDIA (Rajib Mahapatra <rmahapatra@nvidia.com>)
 * Copyright (C) 2009 Uoti Urpala
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

#include <assert.h>

#include "vdpau_mixer.h"

static void free_mixed_frame(void *arg)
{
    struct mp_vdpau_mixer_frame *frame = arg;
    talloc_free(frame);
}

// This creates an image of format IMGFMT_VDPAU with a mp_vdpau_mixer_frame
// struct. Use mp_vdpau_mixed_frame_get() to retrieve the struct and to
// initialize it.
// "base" is used only to set parameters, no image data is referenced.
struct mp_image *mp_vdpau_mixed_frame_create(struct mp_image *base)
{
    assert(base->imgfmt == IMGFMT_VDPAU);

    struct mp_vdpau_mixer_frame *frame =
        talloc_zero(NULL, struct mp_vdpau_mixer_frame);
    for (int n = 0; n < MP_VDP_HISTORY_FRAMES; n++)
        frame->past[n] = frame->future[n] = VDP_INVALID_HANDLE;
    frame->current = VDP_INVALID_HANDLE;
    frame->field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;

    struct mp_image *mpi = mp_image_new_custom_ref(base, frame, free_mixed_frame);
    if (mpi) {
        mpi->planes[2] = (void *)frame;
        mpi->planes[3] = (void *)(uintptr_t)VDP_INVALID_HANDLE;
    }
    return mpi;
}

struct mp_vdpau_mixer_frame *mp_vdpau_mixed_frame_get(struct mp_image *mpi)
{
    if (mpi->imgfmt != IMGFMT_VDPAU)
        return NULL;
    return (void *)mpi->planes[2];
}

struct mp_vdpau_mixer *mp_vdpau_mixer_create(struct mp_vdpau_ctx *vdp_ctx,
                                             struct mp_log *log)
{
    struct mp_vdpau_mixer *mixer = talloc_ptrtype(NULL, mixer);
    *mixer = (struct mp_vdpau_mixer){
        .ctx = vdp_ctx,
        .log = log,
        .video_mixer = VDP_INVALID_HANDLE,
    };
    mp_vdpau_handle_preemption(mixer->ctx, &mixer->preemption_counter);
    return mixer;
}

void mp_vdpau_mixer_destroy(struct mp_vdpau_mixer *mixer)
{
    struct vdp_functions *vdp = &mixer->ctx->vdp;
    VdpStatus vdp_st;
    if (mixer->video_mixer != VDP_INVALID_HANDLE) {
        vdp_st = vdp->video_mixer_destroy(mixer->video_mixer);
        CHECK_VDP_WARNING(mixer, "Error when calling vdp_video_mixer_destroy");
    }
    talloc_free(mixer);
}

static bool opts_equal(const struct mp_vdpau_mixer_opts *a,
                       const struct mp_vdpau_mixer_opts *b)
{
    return a->deint == b->deint && a->chroma_deint == b->chroma_deint &&
           a->pullup == b->pullup && a->hqscaling == b->hqscaling &&
           a->sharpen == b->sharpen && a->denoise == b->denoise;
}

static int set_video_attribute(struct mp_vdpau_mixer *mixer,
                               VdpVideoMixerAttribute attr,
                               const void *value, char *attr_name)
{
    struct vdp_functions *vdp = &mixer->ctx->vdp;
    VdpStatus vdp_st;

    vdp_st = vdp->video_mixer_set_attribute_values(mixer->video_mixer, 1,
                                                   &attr, &value);
    if (vdp_st != VDP_STATUS_OK) {
        MP_ERR(mixer, "Error setting video mixer attribute %s: %s\n", attr_name,
               vdp->get_error_string(vdp_st));
        return -1;
    }
    return 0;
}

#define SET_VIDEO_ATTR(attr_name, attr_type, value) set_video_attribute(mixer, \
                 VDP_VIDEO_MIXER_ATTRIBUTE_ ## attr_name, &(attr_type){value},\
                 # attr_name)
static int create_vdp_mixer(struct mp_vdpau_mixer *mixer,
                            VdpChromaType chroma_type, uint32_t w, uint32_t h)
{
    struct vdp_functions *vdp = &mixer->ctx->vdp;
    VdpDevice vdp_device = mixer->ctx->vdp_device;
    struct mp_vdpau_mixer_opts *opts = &mixer->opts;
#define VDP_NUM_MIXER_PARAMETER 3
#define MAX_NUM_FEATURES 6
    int i;
    VdpStatus vdp_st;

    MP_VERBOSE(mixer, "Recreating vdpau video mixer.\n");

    int feature_count = 0;
    VdpVideoMixerFeature features[MAX_NUM_FEATURES];
    VdpBool feature_enables[MAX_NUM_FEATURES];
    static const VdpVideoMixerParameter parameters[VDP_NUM_MIXER_PARAMETER] = {
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE,
    };
    const void *const parameter_values[VDP_NUM_MIXER_PARAMETER] = {
        &(uint32_t){w},
        &(uint32_t){h},
        &(VdpChromaType){chroma_type},
    };
    if (opts->deint >= 3)
        features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL;
    if (opts->deint == 4)
        features[feature_count++] =
            VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL;
    if (opts->pullup)
        features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_INVERSE_TELECINE;
    if (opts->denoise)
        features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION;
    if (opts->sharpen)
        features[feature_count++] = VDP_VIDEO_MIXER_FEATURE_SHARPNESS;
    if (opts->hqscaling) {
        VdpVideoMixerFeature hqscaling_feature =
            VDP_VIDEO_MIXER_FEATURE_HIGH_QUALITY_SCALING_L1 + opts->hqscaling - 1;
        VdpBool hqscaling_available;
        vdp_st = vdp->video_mixer_query_feature_support(vdp_device,
                                                        hqscaling_feature,
                                                        &hqscaling_available);
        CHECK_VDP_ERROR(mixer, "Error when calling video_mixer_query_feature_support");
        if (hqscaling_available) {
            features[feature_count++] = hqscaling_feature;
        } else {
            MP_ERR(mixer, "Your hardware or VDPAU library does not support "
                   "requested hqscaling.\n");
        }
    }

    vdp_st = vdp->video_mixer_create(vdp_device, feature_count, features,
                                     VDP_NUM_MIXER_PARAMETER,
                                     parameters, parameter_values,
                                     &mixer->video_mixer);
    if (vdp_st != VDP_STATUS_OK)
        mixer->video_mixer = VDP_INVALID_HANDLE;

    CHECK_VDP_ERROR(mixer, "Error when calling vdp_video_mixer_create");

    mixer->initialized = true;
    mixer->current_chroma_type = chroma_type;
    mixer->current_w = w;
    mixer->current_h = h;

    for (i = 0; i < feature_count; i++)
        feature_enables[i] = VDP_TRUE;
    if (feature_count) {
        vdp_st = vdp->video_mixer_set_feature_enables(mixer->video_mixer,
                                                      feature_count, features,
                                                      feature_enables);
        CHECK_VDP_WARNING(mixer, "Error calling vdp_video_mixer_set_feature_enables");
    }
    if (opts->denoise)
        SET_VIDEO_ATTR(NOISE_REDUCTION_LEVEL, float, opts->denoise);
    if (opts->sharpen)
        SET_VIDEO_ATTR(SHARPNESS_LEVEL, float, opts->sharpen);
    if (!opts->chroma_deint)
        SET_VIDEO_ATTR(SKIP_CHROMA_DEINTERLACE, uint8_t, 1);

    struct mp_cmat yuv2rgb;
    VdpCSCMatrix matrix;

    struct mp_csp_params cparams = MP_CSP_PARAMS_DEFAULTS;
    mp_csp_set_image_params(&cparams, &mixer->image_params);
    if (mixer->video_eq)
        mp_csp_equalizer_state_get(mixer->video_eq, &cparams);
    mp_get_csp_matrix(&cparams, &yuv2rgb);

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++)
            matrix[r][c] = yuv2rgb.m[r][c];
        matrix[r][3] = yuv2rgb.c[r];
    }

    set_video_attribute(mixer, VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX,
                        &matrix, "CSC matrix");

    return 0;
}

// If opts is NULL, use the opts as implied by the video image.
int mp_vdpau_mixer_render(struct mp_vdpau_mixer *mixer,
                          struct mp_vdpau_mixer_opts *opts,
                          VdpOutputSurface output, VdpRect *output_rect,
                          struct mp_image *video, VdpRect *video_rect)
{
    struct vdp_functions *vdp = &mixer->ctx->vdp;
    VdpStatus vdp_st;
    VdpRect fallback_rect = {0, 0, video->w, video->h};

    if (!video_rect)
        video_rect = &fallback_rect;

    int pe = mp_vdpau_handle_preemption(mixer->ctx, &mixer->preemption_counter);
    if (pe < 1) {
        mixer->video_mixer = VDP_INVALID_HANDLE;
        if (pe < 0)
            return -1;
    }

    if (video->imgfmt == IMGFMT_VDPAU_OUTPUT) {
        VdpOutputSurface surface = (uintptr_t)video->planes[3];
        int flags = VDP_OUTPUT_SURFACE_RENDER_ROTATE_0;
        vdp_st = vdp->output_surface_render_output_surface(output,
                                                           output_rect,
                                                           surface,
                                                           video_rect,
                                                           NULL, NULL, flags);
        CHECK_VDP_WARNING(mixer, "Error when calling "
                          "vdp_output_surface_render_output_surface");
        return 0;
    }

    if (video->imgfmt != IMGFMT_VDPAU)
        return -1;

    struct mp_vdpau_mixer_frame *frame = mp_vdpau_mixed_frame_get(video);
    struct mp_vdpau_mixer_frame fallback = {{0}};
    if (!frame) {
        frame = &fallback;
        frame->current = (uintptr_t)video->planes[3];
        for (int n = 0; n < MP_VDP_HISTORY_FRAMES; n++)
            frame->past[n] = frame->future[n] = VDP_INVALID_HANDLE;
        frame->field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;
    }

    if (!opts)
        opts = &frame->opts;

    if (mixer->video_mixer == VDP_INVALID_HANDLE)
        mixer->initialized = false;

    if (mixer->video_eq && mp_csp_equalizer_state_changed(mixer->video_eq))
        mixer->initialized = false;

    VdpChromaType s_chroma_type;
    uint32_t s_w, s_h;

    vdp_st = vdp->video_surface_get_parameters(frame->current, &s_chroma_type,
                                               &s_w, &s_h);
    CHECK_VDP_ERROR(mixer, "Error when calling vdp_video_surface_get_parameters");

    if (!mixer->initialized || !opts_equal(opts, &mixer->opts) ||
        !mp_image_params_equal(&video->params, &mixer->image_params) ||
        mixer->current_w != s_w || mixer->current_h != s_h ||
        mixer->current_chroma_type != s_chroma_type)
    {
        mixer->opts = *opts;
        mixer->image_params = video->params;
        if (mixer->video_mixer != VDP_INVALID_HANDLE) {
            vdp_st = vdp->video_mixer_destroy(mixer->video_mixer);
            CHECK_VDP_WARNING(mixer, "Error when calling vdp_video_mixer_destroy");
        }
        mixer->video_mixer = VDP_INVALID_HANDLE;
        mixer->initialized = false;
        if (create_vdp_mixer(mixer, s_chroma_type, s_w, s_h) < 0)
            return -1;
    }

    vdp_st = vdp->video_mixer_render(mixer->video_mixer, VDP_INVALID_HANDLE,
                                     0, frame->field,
                                     MP_VDP_HISTORY_FRAMES, frame->past,
                                     frame->current,
                                     MP_VDP_HISTORY_FRAMES, frame->future,
                                     video_rect,
                                     output, NULL, output_rect,
                                     0, NULL);
    CHECK_VDP_WARNING(mixer, "Error when calling vdp_video_mixer_render");
    return 0;
}
