
#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "stream.h"

static int open_s(stream_t *stream,int mode, void* opts, int* file_format) {
  stream->type = STREAMTYPE_DUMMY;

  return 1;
}


stream_info_t stream_info_null = {
  "Null stream",
  "null",
  "Albeu",
  "",
  open_s,
  { "null", NULL },
  NULL,
  0 // Urls are an option string
};
