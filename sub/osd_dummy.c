#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "talloc.h"
#include "osd.h"

const char *const osd_ass_0 = "";
const char *const osd_ass_1 = "";

void osd_init_backend(struct osd_state *osd)
{
}

void osd_destroy_backend(struct osd_state *osd)
{
}

void osd_get_function_sym(char *buffer, size_t buffer_size, int osd_function)
{
}

void osd_object_get_bitmaps(struct osd_state *osd, struct osd_object *obj,
                            struct sub_bitmaps *out_imgs)
{
    *out_imgs = (struct sub_bitmaps) {0};
}

void osd_object_get_resolution(struct osd_state *osd, int obj,
                               int *out_w, int *out_h)
{
    *out_w = 0;
    *out_h = 0;
}
