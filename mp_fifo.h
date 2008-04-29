#ifndef MPLAYER_MP_FIFO_H
#define MPLAYER_MP_FIFO_H

struct mp_fifo;
int mplayer_get_key(void *ctx, int fd);
void mplayer_put_key(struct mp_fifo *fifo, int code);
// Can be freed with talloc_free()
struct mp_fifo *mp_fifo_create(void);

#ifdef IS_OLD_VO
#define mplayer_put_key(key) mplayer_put_key(global_vo->key_fifo, key)
#endif

#endif /* MPLAYER_MP_FIFO_H */
