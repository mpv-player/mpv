#ifndef MPLAYER_VOBSUB_H
#define MPLAYER_VOBSUB_H

extern void *vobsub_open(const char *subname, const char *const ifo, const int force, void** spu);
extern void vobsub_reset(void *vob);
extern int vobsub_parse_ifo(void* this, const char *const name, unsigned int *palette, unsigned int *width, unsigned int *height, int force);
extern int vobsub_get_packet(void *vobhandle, float pts,void** data, int* timestamp);
extern void vobsub_close(void *this);

#endif /* MPLAYER_VOBSUB_H */
