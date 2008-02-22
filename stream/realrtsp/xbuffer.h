/* 
 * xbuffer code
 *
 * Includes a minimalistic replacement for xine_buffer functions used in
 * Real streaming code. Only function needed by this code are implemented.
 *
 * Most code comes from xine_buffer.c Copyright (C) 2002 the xine project
 *
 * WARNING: do not mix original xine_buffer functions with this code!
 * xbuffers behave like xine_buffers, but are not byte-compatible with them.
 * You must take care of pointers returned by xbuffers functions (no macro to
 * do it automatically)
 *
 */

#ifndef MPLAYER_XBUFFER_H
#define MPLAYER_XBUFFER_H

void *xbuffer_init(int chunk_size);
void *xbuffer_free(void *buf);
void *xbuffer_copyin(void *buf, int index, const void *data, int len);
void *xbuffer_ensure_size(void *buf, int size);
void *xbuffer_strcat(void *buf, char *data);

#endif /* MPLAYER_XBUFFER_H */
