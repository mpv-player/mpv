#ifndef MP_VF_LAVFI_H_
#define MP_VF_LAVFI_H_

#include "config.h"

#include "common/common.h"
#include "vf.h"

struct vf_lw_opts;

#if HAVE_LIBAVFILTER

extern const struct m_sub_options vf_lw_conf;

int vf_lw_set_graph(struct vf_instance *vf, struct vf_lw_opts *lavfi_opts,
                    char *filter, char *opts, ...) PRINTF_ATTRIBUTE(4,5);
void *vf_lw_old_priv(struct vf_instance *vf);
void vf_lw_update_graph(struct vf_instance *vf, char *filter, char *opts, ...)
                        PRINTF_ATTRIBUTE(3,4);
void vf_lw_set_recreate_cb(struct vf_instance *vf,
                           void (*recreate)(struct vf_instance *vf));
#else
static inline
int vf_lw_set_graph(struct vf_instance *vf, struct vf_lw_opts *lavfi_opts,
                    char *filter, char *opts, ...)
{
    return -1;
}
static void *vf_lw_old_priv(struct vf_instance *vf)
{
    return 0;
}
static void vf_lw_update_graph(struct vf_instance *vf, char *filter, char *opts, ...)
{
}
static void vf_lw_set_recreate_cb(struct vf_instance *vf,
                                  void (*recreate)(struct vf_instance *vf))
{
}
#include "options/m_option.h"
static const struct m_sub_options vf_lw_conf = {0};
#endif

#endif
