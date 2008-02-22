
#ifndef MPLAYER_MF_H
#define MPLAYER_MF_H

extern int    mf_w;
extern int    mf_h;
extern double mf_fps;
extern char * mf_type;

typedef struct
{
 int curr_frame;
 int nr_of_files;
 char ** names;
} mf_t;

mf_t* open_mf(char * filename);

#endif /* MPLAYER_MF_H */
