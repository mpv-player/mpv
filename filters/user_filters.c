#include <libavutil/avutil.h>

#include "config.h"

#include "common/common.h"
#include "common/msg.h"
#include "options/m_config_frontend.h"

#include "f_lavfi.h"
#include "user_filters.h"

static bool get_desc_from(const struct mp_user_filter_entry **list, int num,
                          struct m_obj_desc *dst, int index)
{
    if (index >= num)
        return false;
    const struct mp_user_filter_entry *entry = list[index];
    *dst = entry->desc;
    dst->p = entry;
    return true;
}

static bool check_unknown_entry(const char *name, int media_type)
{
    // Generic lavfi bridge: skip the lavfi- prefix, if present.
    if (strncmp(name, "lavfi-", 6) == 0)
        name += 6;
    return mp_lavfi_is_usable(name, media_type);
}

// --af option

const struct mp_user_filter_entry *af_list[] = {
    &af_lavfi,
    &af_lavfi_bridge,
    &af_scaletempo,
    &af_scaletempo2,
    &af_format,
#if HAVE_RUBBERBAND
    &af_rubberband,
#endif
    &af_lavcac3enc,
    &af_drop,
};

static bool get_af_desc(struct m_obj_desc *dst, int index)
{
    return get_desc_from(af_list, MP_ARRAY_SIZE(af_list), dst, index);
}

static void print_af_help_list(struct mp_log *log)
{
    print_lavfi_help_list(log, AVMEDIA_TYPE_AUDIO);
}

static void print_af_lavfi_help(struct mp_log *log, const char *name)
{
    print_lavfi_help(log, name, AVMEDIA_TYPE_AUDIO);
}

static bool check_af_lavfi(const char *name)
{
    return check_unknown_entry(name, AVMEDIA_TYPE_AUDIO);
}

const struct m_obj_list af_obj_list = {
    .get_desc = get_af_desc,
    .description = "audio filters",
    .allow_disable_entries = true,
    .allow_unknown_entries = true,
    .check_unknown_entry = check_af_lavfi,
    .print_help_list = print_af_help_list,
    .print_unknown_entry_help = print_af_lavfi_help,
};

// --vf option

const struct mp_user_filter_entry *vf_list[] = {
    &vf_format,
    &vf_lavfi,
    &vf_lavfi_bridge,
    &vf_sub,
#if HAVE_ZIMG
    &vf_fingerprint,
#endif
#if HAVE_VAPOURSYNTH
    &vf_vapoursynth,
#endif
#if HAVE_VDPAU
    &vf_vdpaupp,
#endif
#if HAVE_VAAPI
    &vf_vavpp,
#endif
#if HAVE_D3D_HWACCEL
    &vf_d3d11vpp,
#endif
#if HAVE_EGL_HELPERS && HAVE_GL && HAVE_EGL
    &vf_gpu,
#endif
};

static bool get_vf_desc(struct m_obj_desc *dst, int index)
{
    return get_desc_from(vf_list, MP_ARRAY_SIZE(vf_list), dst, index);
}

static void print_vf_help_list(struct mp_log *log)
{
    print_lavfi_help_list(log, AVMEDIA_TYPE_VIDEO);
}

static void print_vf_lavfi_help(struct mp_log *log, const char *name)
{
    print_lavfi_help(log, name, AVMEDIA_TYPE_VIDEO);
}

static bool check_vf_lavfi(const char *name)
{
    return check_unknown_entry(name, AVMEDIA_TYPE_VIDEO);
}

const struct m_obj_list vf_obj_list = {
    .get_desc = get_vf_desc,
    .description = "video filters",
    .allow_disable_entries = true,
    .allow_unknown_entries = true,
    .check_unknown_entry = check_vf_lavfi,
    .print_help_list = print_vf_help_list,
    .print_unknown_entry_help = print_vf_lavfi_help,
};

// Create a bidir, single-media filter from command line arguments.
struct mp_filter *mp_create_user_filter(struct mp_filter *parent,
                                        enum mp_output_chain_type type,
                                        const char *name, char **args)
{
    const struct m_obj_list *obj_list = NULL;
    const char *defs_name = NULL;
    enum mp_frame_type frame_type = 0;
    if (type == MP_OUTPUT_CHAIN_VIDEO) {
        frame_type = MP_FRAME_VIDEO;
        obj_list = &vf_obj_list;
        defs_name = "vf-defaults";
    } else if (type == MP_OUTPUT_CHAIN_AUDIO) {
        frame_type = MP_FRAME_AUDIO;
        obj_list = &af_obj_list;
        defs_name = "af-defaults";
    }
    assert(frame_type && obj_list);

    struct mp_filter *f = NULL;

    struct m_obj_desc desc;
    if (!m_obj_list_find(&desc, obj_list, bstr0(name))) {
        // Generic lavfi bridge.
        if (strncmp(name, "lavfi-", 6) == 0)
            name += 6;
        struct mp_lavfi *l =
            mp_lavfi_create_filter(parent, frame_type, true, NULL, name, args);
        if (l)
            f = l->f;
        goto done;
    }

    void *options = NULL;
    if (desc.options) {
        struct m_obj_settings *defs = NULL;
        if (defs_name) {
            mp_read_option_raw(parent->global, defs_name,
                                &m_option_type_obj_settings_list, &defs);
        }

        struct m_config *config =
            m_config_from_obj_desc_and_args(NULL, parent->log, parent->global,
                                            &desc, name, defs, args);

        struct m_option dummy = {.type = &m_option_type_obj_settings_list};
        m_option_free(&dummy, &defs);

        if (!config)
            goto done;

        options = config->optstruct;
        // Free config when options is freed.
        ta_set_parent(options, NULL);
        ta_set_parent(config, options);
    }

    const struct mp_user_filter_entry *entry = desc.p;
    f = entry->create(parent, options);

done:
    if (!f) {
        MP_ERR(parent, "Creating filter '%s' failed.\n", name);
        return NULL;
    }
    return f;
}
