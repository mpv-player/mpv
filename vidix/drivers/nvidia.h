#include <inttypes.h>

#define RIVA_FIFO_FREE(hwptr, cnt) \
{ \
    while (nv_fifo_space < (cnt)) { \
	nv_fifo_space = hwptr->fifo_free >> 2; \
    } \
    nv_fifo_space -= (cnt); \
}

typedef struct {
    uint32_t	reserved00[4];
    uint16_t	fifo_free;
    uint16_t	nop[1];
    uint32_t	reserved01[0x03b];
    
    uint32_t	no_operation;
    uint32_t	notify;
    uint32_t	reserved02[0x01e];
    uint32_t	set_context_dma_notifies;
    uint32_t	set_context_dma_image;
    uint32_t	set_context_pattern;
    uint32_t	set_context_rop;
    uint32_t	set_context_beta1;
    uint32_t	set_context_surface;
    uint32_t	reserved03[0x05a];
    uint32_t	set_color_format;
    uint32_t	set_operation;
    int16_t	clip_x;
    int16_t	clip_y;
    uint16_t	clip_height;
    uint16_t	clip_width;
    int16_t	image_out_x;
    int16_t	image_out_y;
    uint16_t	image_out_height;
    uint16_t	image_out_width;
    uint32_t	du_dx;
    uint32_t	du_dy;
    uint32_t	reserved04[0x38];
    uint16_t	image_in_height;
    uint16_t	image_in_width;
    uint32_t	image_in_format;
    uint32_t	image_in_offset;
    uint32_t	image_in_point;
    uint32_t	reserved05[0x6fc];
} RivaScaledImage;

#define dump_scaledimage(x) { \
    printf("clip: pos: %dx%d, size: %dx%d\n", \
	x->clip_x, x->clip_y, x->clip_height, x->clip_width); \
    printf("image_out: pos: %dx%d, size: %dx%d\n", \
	x->image_out_x, x->image_out_y, x->image_out_height, x->image_out_width); \
    printf("image_in: size: %dx%d format: %x offset: %x\n", \
	x->image_in_height, x->image_in_width, x->image_in_format, x->image_in_offset); \
}
