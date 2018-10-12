#ifndef MPV_CLIENT_API_IMAGE_H_
#define MPV_CLIENT_API_IMAGE_H_

#ifdef __cplusplus
extern "C" {
#endif
typedef struct image_s {
	int width, height, stride;
	unsigned char *buffer;      // RGB24
} image_t;

typedef struct mpv_image_cb_context mpv_image_cb_context;

typedef void(*mpv_image_cb_update_fn)(void* callback_ctx,image_t *cb_ctx);

void mpv_image_set_update_callback(mpv_image_cb_context *ctx,mpv_image_cb_update_fn callback,
	void *callback_ctx);
	#ifdef __cplusplus
}
#endif

#endif