#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mpv_talloc.h"
#include "osd_state.h"

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
                            int format, struct sub_bitmaps *out_imgs)
{
    *out_imgs = (struct sub_bitmaps) {0};
}

void osd_set_external(struct osd_state *osd, void *id, int res_x, int res_y,
                      char *text)
{
}

void osd_get_text_size(struct osd_state *osd, int *out_screen_h, int *out_font_h)
{
    *out_screen_h = 0;
    *out_font_h = 0;
}
