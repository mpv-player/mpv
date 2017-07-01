#ifndef MP_IMAGE_LOADER_H_
#define MP_IMAGE_LOADER_H_

#include <stddef.h>

struct mp_image;
struct mp_image *load_image_png_buf(void *buffer, size_t buffer_size, int imgfmt);

#endif
