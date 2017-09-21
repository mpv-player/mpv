#pragma once

#include <stdint.h>

struct mp_aframe;
struct AVFrame;
struct mp_chmap;

struct mp_aframe *mp_aframe_from_avframe(struct AVFrame *av_frame);
struct mp_aframe *mp_aframe_create(void);
struct mp_aframe *mp_aframe_new_ref(struct mp_aframe *frame);

void mp_aframe_reset(struct mp_aframe *frame);
void mp_aframe_unref_data(struct mp_aframe *frame);

struct AVFrame *mp_aframe_to_avframe(struct mp_aframe *frame);
struct AVFrame *mp_aframe_to_avframe_and_unref(struct mp_aframe *frame);
struct AVFrame *mp_aframe_get_raw_avframe(struct mp_aframe *frame);

bool mp_aframe_is_allocated(struct mp_aframe *frame);

void mp_aframe_config_copy(struct mp_aframe *dst, struct mp_aframe *src);
bool mp_aframe_config_equals(struct mp_aframe *a, struct mp_aframe *b);
bool mp_aframe_config_is_valid(struct mp_aframe *frame);

void mp_aframe_copy_attributes(struct mp_aframe *dst, struct mp_aframe *src);

uint8_t **mp_aframe_get_data_ro(struct mp_aframe *frame);
uint8_t **mp_aframe_get_data_rw(struct mp_aframe *frame);

int mp_aframe_get_format(struct mp_aframe *frame);
bool mp_aframe_get_chmap(struct mp_aframe *frame, struct mp_chmap *out);
int mp_aframe_get_channels(struct mp_aframe *frame);
int mp_aframe_get_rate(struct mp_aframe *frame);
int mp_aframe_get_size(struct mp_aframe *frame);
double mp_aframe_get_pts(struct mp_aframe *frame);

bool mp_aframe_set_format(struct mp_aframe *frame, int format);
bool mp_aframe_set_chmap(struct mp_aframe *frame, struct mp_chmap *in);
bool mp_aframe_set_rate(struct mp_aframe *frame, int rate);
bool mp_aframe_set_size(struct mp_aframe *frame, int samples);
void mp_aframe_set_pts(struct mp_aframe *frame, double pts);

int mp_aframe_get_planes(struct mp_aframe *frame);
int mp_aframe_get_total_plane_samples(struct mp_aframe *frame);
size_t mp_aframe_get_sstride(struct mp_aframe *frame);

void mp_aframe_skip_samples(struct mp_aframe *f, int samples);
double mp_aframe_end_pts(struct mp_aframe *f);
double mp_aframe_duration(struct mp_aframe *f);
void mp_aframe_clip_timestamps(struct mp_aframe *f, double start, double end);

struct mp_aframe_pool;
struct mp_aframe_pool *mp_aframe_pool_create(void *ta_parent);
int mp_aframe_pool_allocate(struct mp_aframe_pool *pool, struct mp_aframe *frame,
                            int samples);
