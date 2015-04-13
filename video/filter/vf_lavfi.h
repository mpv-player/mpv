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

#ifndef MP_VF_LAVFI_H_
#define MP_VF_LAVFI_H_

#include "common/common.h"
#include "vf.h"

struct vf_lw_opts;

extern const struct m_sub_options vf_lw_conf;

int vf_lw_set_graph(struct vf_instance *vf, struct vf_lw_opts *lavfi_opts,
                    char *filter, char *opts, ...) PRINTF_ATTRIBUTE(4,5);
void *vf_lw_old_priv(struct vf_instance *vf);
void vf_lw_update_graph(struct vf_instance *vf, char *filter, char *opts, ...)
                        PRINTF_ATTRIBUTE(3,4);
void vf_lw_set_reconfig_cb(struct vf_instance *vf,
                                int (*reconfig)(struct vf_instance *vf,
                                                struct mp_image_params *in,
                                                struct mp_image_params *out));

#endif
