#ifndef MPLAYER_VOBSUB_H
#define MPLAYER_VOBSUB_H

extern void *vobsub_open(const char *subname, const char *const ifo, const int force, void** spu);
extern void vobsub_reset(void *vob);
extern int vobsub_parse_ifo(void* this, const char *const name, unsigned int *palette, unsigned int *width, unsigned int *height, int force, int sid, char *langid);
extern int vobsub_get_packet(void *vobhandle, float pts,void** data, int* timestamp);
extern int vobsub_get_next_packet(void *vobhandle, void** data, int* timestamp);
extern void vobsub_close(void *this);

extern void *vobsub_out_open(const char *basename, const unsigned int *palette, unsigned int orig_width, unsigned int orig_height, const char *id, unsigned int index);
extern void vobsub_out_output(void *me, const unsigned char *packet, int len, double pts);
extern void vobsub_out_close(void *me);

#endif /* MPLAYER_VOBSUB_H */

