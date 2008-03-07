#ifndef MPLAYER_GLOB_H
#define MPLAYER_GLOB_H

#include <sys/types.h>
#include "config.h"

typedef struct {
  size_t gl_pathc;
  char **gl_pathv;
  size_t gl_offs;
} glob_t;

void globfree(glob_t *pglob);

int  glob(const char *pattern, int flags, int (*errfunc)(const char *epath, int eerrno), glob_t *pglob);

#endif /* MPLAYER_GLOB_H */
