
struct vf_instance_s;
struct vf_priv_s;

typedef struct vf_info_s {
    const char *info;
    const char *name;
    const char *author;
    const char *comment;
    int (*open)(struct vf_instance_s* vf,char* args);
} vf_info_t;

typedef struct vf_image_context_s {
    mp_image_t* static_images[2];
    mp_image_t* temp_images[1];
    mp_image_t* export_images[1];
    int static_idx;
} vf_image_context_t;

typedef struct vf_instance_s {
    vf_info_t* info;
    // funcs:
    int (*config)(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt);
    int (*control)(struct vf_instance_s* vf,
        int request, void* data);
    int (*query_format)(struct vf_instance_s* vf,
        unsigned int fmt);
    void (*get_image)(struct vf_instance_s* vf,
        mp_image_t *mpi);
    void (*put_image)(struct vf_instance_s* vf,
        mp_image_t *mpi);
    void (*draw_slice)(struct vf_instance_s* vf,
        unsigned char* src, int* stride, int w,int h, int x, int y);
    void (*uninit)(struct vf_instance_s* vf);
    // data:
    vf_image_context_t imgctx;
    struct vf_instance_s* next;
    struct vf_priv_s* priv;
} vf_instance_t;

// control codes:
#include "mpc_info.h"

#define VFCTRL_QUERY_MAX_PP_LEVEL 4 /* test for postprocessing support (max level) */
#define VFCTRL_SET_PP_LEVEL 5 /* set postprocessing level */
#define VFCTRL_SET_EQUALIZER 6 /* set color options (brightness,contrast etc) */

// VFCAP_* values: they are flags, returned by query_format():

// set, if the given colorspace is supported (with or without conversion)
#define VFCAP_CSP_SUPPORTED 0x1
// set, if the given colorspace is supported _without_ conversion
#define VFCAP_CSP_SUPPORTED_BY_HW 0x2
// set if the driver/filter can draw OSD
#define VFCAP_OSD 0x4
// set if the driver/filter can handle compressed SPU stream
#define VFCAP_SPU 0x8
// scaling up/down by hardware, or software:
#define VFCAP_HWSCALE_UP 0x10
#define VFCAP_HWSCALE_DOWN 0x20
#define VFCAP_SWSCALE 0x40
// driver/filter can do vertical flip (upside-down)
#define VFCAP_FLIP 0x80

// driver/hardware handles timing (blocking)
#define VFCAP_TIMER 0x100
// driver _always_ flip image upside-down (for ve_vfw)
#define VFCAP_FLIPPED 0x200
// driver accept stride: (put_image/draw_frame)
#define VFCAP_ACCEPT_STRIDE 0x400

// functions:
mp_image_t* vf_get_image(vf_instance_t* vf, unsigned int outfmt, int mp_imgtype, int mp_imgflag, int w, int h);

vf_instance_t* vf_open_plugin(vf_info_t** filter_list, vf_instance_t* next, char *name, char *args);
vf_instance_t* vf_open_filter(vf_instance_t* next, char *name, char *args);
vf_instance_t* vf_open_encoder(vf_instance_t* next, char *name, char *args);

// default wrappers:
int vf_next_config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt);
int vf_next_control(struct vf_instance_s* vf, int request, void* data);
int vf_next_query_format(struct vf_instance_s* vf, unsigned int fmt);
void vf_next_put_image(struct vf_instance_s* vf,mp_image_t *mpi);

vf_instance_t* append_filters(vf_instance_t* last);

