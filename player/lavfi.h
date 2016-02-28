#ifndef MP_LAVFI
#define MP_LAVFI

struct mp_log;
struct lavfi;
struct lavfi_pad;
struct mp_image;
struct mp_audio;

enum lavfi_direction {
    LAVFI_IN = 1,
    LAVFI_OUT,
};

struct lavfi *lavfi_create(struct mp_log *log, char *graph_string);
void lavfi_destroy(struct lavfi *c);
struct lavfi_pad *lavfi_find_pad(struct lavfi *c, char *name);
enum lavfi_direction lavfi_pad_direction(struct lavfi_pad *pad);
enum stream_type lavfi_pad_type(struct lavfi_pad *pad);
void lavfi_set_connected(struct lavfi_pad *pad, bool connected);
bool lavfi_get_connected(struct lavfi_pad *pad);
bool lavfi_process(struct lavfi *c);
bool lavfi_has_failed(struct lavfi *c);
void lavfi_seek_reset(struct lavfi *c);
int lavfi_request_frame_a(struct lavfi_pad *pad, struct mp_audio **out_aframe);
int lavfi_request_frame_v(struct lavfi_pad *pad, struct mp_image **out_vframe);
bool lavfi_needs_input(struct lavfi_pad *pad);
void lavfi_send_status(struct lavfi_pad *pad, int status);
void lavfi_send_frame_a(struct lavfi_pad *pad, struct mp_audio *aframe);
void lavfi_send_frame_v(struct lavfi_pad *pad, struct mp_image *vframe);

#endif
