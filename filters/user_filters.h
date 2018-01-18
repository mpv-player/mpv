#pragma once

#include "options/m_option.h"

#include "f_output_chain.h"

// For creating filters from command line. Strictly for --vf/--af.
struct mp_user_filter_entry {
    // Name and sub-option description.
    struct m_obj_desc desc;
    // Create a filter. The option pointer is non-NULL if desc implies a priv
    // struct to be allocated; then options are parsed into it. The callee
    // must always free options (but can reparent it with talloc to keep it).
    struct mp_filter *(*create)(struct mp_filter *parent, void *options);
};

struct mp_filter *mp_create_user_filter(struct mp_filter *parent,
                                        enum mp_output_chain_type type,
                                        const char *name, char **args);

extern const struct mp_user_filter_entry af_lavfi;
extern const struct mp_user_filter_entry af_lavfi_bridge;
extern const struct mp_user_filter_entry af_scaletempo;
extern const struct mp_user_filter_entry af_format;
extern const struct mp_user_filter_entry af_rubberband;
extern const struct mp_user_filter_entry af_lavcac3enc;

extern const struct mp_user_filter_entry vf_lavfi;
extern const struct mp_user_filter_entry vf_lavfi_bridge;
extern const struct mp_user_filter_entry vf_sub;
extern const struct mp_user_filter_entry vf_vapoursynth;
extern const struct mp_user_filter_entry vf_vapoursynth_lazy;
extern const struct mp_user_filter_entry vf_format;
extern const struct mp_user_filter_entry vf_vdpaupp;
extern const struct mp_user_filter_entry vf_vavpp;
extern const struct mp_user_filter_entry vf_d3d11vpp;
