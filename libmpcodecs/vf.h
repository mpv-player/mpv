
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
        unsigned char** src, int* stride, int w,int h, int x, int y);
    void (*uninit)(struct vf_instance_s* vf);
    // caps:
    unsigned int default_caps; // used by default query_format()
    unsigned int default_reqs; // used by default config()
    // data:
    vf_image_context_t imgctx;
    struct vf_instance_s* next;
    struct vf_priv_s* priv;
} vf_instance_t;

// control codes:
#include "mpc_info.h"

typedef struct vf_seteq_s 
{
    char *item;
    int value;
} vf_equalizer_t;

#define VFCTRL_QUERY_MAX_PP_LEVEL 4 /* test for postprocessing support (max level) */
#define VFCTRL_SET_PP_LEVEL 5 /* set postprocessing level */
#define VFCTRL_SET_EQUALIZER 6 /* set color options (brightness,contrast etc) */
#define VFCTRL_GET_EQUALIZER 8 /* gset color options (brightness,contrast etc) */
#define VFCTRL_DRAW_OSD 7
#define VFCTRL_CHANGE_RECTANGLE 9 /* Change the rectangle boundaries */

#include "vfcap.h"

// functions:
void vf_mpi_clear(mp_image_t* mpi,int x0,int y0,int w,int h);
mp_image_t* vf_get_image(vf_instance_t* vf, unsigned int outfmt, int mp_imgtype, int mp_imgflag, int w, int h);

vf_instance_t* vf_open_plugin(vf_info_t** filter_list, vf_instance_t* next, char *name, char *args);
vf_instance_t* vf_open_filter(vf_instance_t* next, char *name, char *args);
vf_instance_t* vf_open_encoder(vf_instance_t* next, char *name, char *args);

unsigned int vf_match_csp(vf_instance_t** vfp,unsigned int* list,unsigned int preferred);

// default wrappers:
int vf_next_config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt);
int vf_next_control(struct vf_instance_s* vf, int request, void* data);
int vf_next_query_format(struct vf_instance_s* vf, unsigned int fmt);
void vf_next_put_image(struct vf_instance_s* vf,mp_image_t *mpi);

vf_instance_t* append_filters(vf_instance_t* last);

void vf_list_plugins();

void vf_uninit_filter(vf_instance_t* vf);
void vf_uninit_filter_chain(vf_instance_t* vf);

