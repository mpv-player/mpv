#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "talloc.h"

#include "options/m_option.h"

#include "video/img_format.h"
#include "video/mp_image.h"

#include "vf.h"

#define MAX_Q 100

struct vf_priv_s {
    struct mp_image *queued[MAX_Q];
    int num_queued;
    int cfg_num;
};

static void flush(struct vf_instance *vf)
{
    for (int n = 0; n < vf->priv->num_queued; n++)
        mp_image_unrefp(&vf->priv->queued[n]);
    vf->priv->num_queued = 0;
}

static int config(struct vf_instance *vf,
                  int width, int height, int d_width, int d_height,
                  unsigned int flags, unsigned int outfmt)
{
    flush(vf);
    return vf_next_config(vf,width,height,d_width,d_height,flags,outfmt);
}

static int filter_ext(struct vf_instance *vf, struct mp_image *mpi)
{
    struct vf_priv_s *p = vf->priv;
    if (mpi) {
        if (p->num_queued == p->cfg_num) {
            vf_add_output_frame(vf, p->queued[p->num_queued - 1]);
            p->num_queued--;
        }
        p->num_queued++;
        for (int n = p->num_queued - 1; n > 0; n--)
            p->queued[n] = p->queued[n - 1];
        p->queued[0] = mpi;
    } else {
        // EOF
        while (p->num_queued) {
            vf_add_output_frame(vf, p->queued[p->num_queued - 1]);
            p->num_queued--;
        }
    }
    return 0;
}

static int control(vf_instance_t *vf, int request, void *data)
{
    if (request == VFCTRL_SEEK_RESET) {
        flush(vf);
        return CONTROL_OK;
    }
    return CONTROL_UNKNOWN;
}

static void uninit(vf_instance_t *vf)
{
    flush(vf);
}

static int vf_open(vf_instance_t *vf)
{
    vf->config = config;
    vf->filter_ext = filter_ext;
    vf->control = control;
    vf->uninit = uninit;
    return 1;
}

#define OPT_BASE_STRUCT struct vf_priv_s
const vf_info_t vf_info_buffer = {
    .description = "buffer a number of frames",
    .name = "buffer",
    .open = vf_open,
    .priv_size = sizeof(struct vf_priv_s),
    .options = (const struct m_option[]){
        OPT_INTRANGE("num", cfg_num, 0, 1, MAX_Q, OPTDEF_INT(2)),
        {0}
    },
};
