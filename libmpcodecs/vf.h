
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

// functions:
mp_image_t* vf_get_image(vf_instance_t* vf, unsigned int outfmt, int mp_imgtype, int mp_imgflag, int w, int h);
vf_instance_t* vf_open_filter(vf_instance_t* next, char *name, char *args);

// default wrappers:
int vf_next_config(struct vf_instance_s* vf,
        int width, int height, int d_width, int d_height,
	unsigned int flags, unsigned int outfmt);
int vf_next_control(struct vf_instance_s* vf, int request, void* data);
int vf_next_query_format(struct vf_instance_s* vf, unsigned int fmt);
void vf_next_put_image(struct vf_instance_s* vf,mp_image_t *mpi);
void vf_next_uninit(struct vf_instance_s* vf);
vf_instance_t* append_filters(vf_instance_t* last);


