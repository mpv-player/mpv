
#ifndef _MF_H
#define _MF_H

extern int    mf_support;
extern int    mf_w;
extern int    mf_h;
extern float  mf_fps;
extern char * mf_type;

typedef struct
{
 int     nr_of_files;
 char ** names;
} mf_t;

#endif
