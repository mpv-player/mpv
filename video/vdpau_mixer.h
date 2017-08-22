#ifndef MP_VDPAU_MIXER_H_
#define MP_VDPAU_MIXER_H_

#include <stdbool.h>

#include "csputils.h"
#include "mp_image.h"
#include "vdpau.h"

struct mp_vdpau_mixer_opts {
    int deint;
    int chroma_deint;
    int pullup;
    float denoise;
    float sharpen;
    int hqscaling;
};

#define MP_VDP_HISTORY_FRAMES 2

struct mp_vdpau_mixer_frame {
    // settings
    struct mp_vdpau_mixer_opts opts;
    // video data
    VdpVideoMixerPictureStructure field;
    VdpVideoSurface past[MP_VDP_HISTORY_FRAMES];
    VdpVideoSurface current;
    VdpVideoSurface future[MP_VDP_HISTORY_FRAMES];
};

struct mp_vdpau_mixer {
    struct mp_log *log;
    struct mp_vdpau_ctx *ctx;
    uint64_t preemption_counter;
    bool initialized;

    struct mp_image_params image_params;
    struct mp_vdpau_mixer_opts opts;

    VdpChromaType current_chroma_type;
    int current_w, current_h;

    struct mp_csp_equalizer_state *video_eq;

    VdpVideoMixer video_mixer;
};

struct mp_image *mp_vdpau_mixed_frame_create(struct mp_image *base);

struct mp_vdpau_mixer_frame *mp_vdpau_mixed_frame_get(struct mp_image *mpi);

struct mp_vdpau_mixer *mp_vdpau_mixer_create(struct mp_vdpau_ctx *vdp_ctx,
                                             struct mp_log *log);
void mp_vdpau_mixer_destroy(struct mp_vdpau_mixer *mixer);

int mp_vdpau_mixer_render(struct mp_vdpau_mixer *mixer,
                          struct mp_vdpau_mixer_opts *opts,
                          VdpOutputSurface output, VdpRect *output_rect,
                          struct mp_image *video, VdpRect *video_rect);

#endif
