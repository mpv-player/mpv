#ifndef MPLAYER_VOBSUB_H
#define MPLAYER_VOBSUB_H

void *vobsub_open(const char *subname, const char *const ifo, const int force, void** spu);
void vobsub_reset(void *vob);
int vobsub_parse_ifo(void* this, const char *const name, unsigned int *palette, unsigned int *width, unsigned int *height, int force, int sid, char *langid);
int vobsub_get_packet(void *vobhandle, float pts,void** data, int* timestamp);
int vobsub_get_next_packet(void *vobhandle, void** data, int* timestamp);
void vobsub_close(void *this);
unsigned int vobsub_get_indexes_count(void * /* vobhandle */);
char *vobsub_get_id(void * /* vobhandle */, unsigned int /* index */);

/// Get vobsub id by its index in the valid streams.
int vobsub_get_id_by_index(void *vobhandle, unsigned int index);
/// Get index in the valid streams by vobsub id.
int vobsub_get_index_by_id(void *vobhandle, int id);

/// Convert palette value in idx file to yuv.
unsigned int vobsub_palette_to_yuv(unsigned int pal);
/// Convert rgb value to yuv.
unsigned int vobsub_rgb_to_yuv(unsigned int rgb);

void *vobsub_out_open(const char *basename, const unsigned int *palette, unsigned int orig_width, unsigned int orig_height, const char *id, unsigned int index);
void vobsub_out_output(void *me, const unsigned char *packet, int len, double pts);
void vobsub_out_close(void *me);
int vobsub_set_from_lang(void *vobhandle, unsigned char * lang);
void vobsub_seek(void * vobhandle, float pts);

#endif /* MPLAYER_VOBSUB_H */

