
#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "stream.h"
#include "demuxer.h"

#ifdef USE_TV
extern char* tv_param_channel;
#endif


static int open_s(stream_t *stream,int mode, void* opts, int* file_format) {
  stream->type = STREAMTYPE_DUMMY;

  if(strncmp("mf://",stream->url,5) == 0) {
    *file_format =  DEMUXER_TYPE_MF;
  } 
#ifdef USE_TV
  else if (strncmp("tv://",stream->url,5) == 0) {
    *file_format =  DEMUXER_TYPE_TV;
    if(stream->url[5] != '\0')
      tv_param_channel = strdup(stream->url + 5);
  }
#endif
  return 1;
}


stream_info_t stream_info_null = {
  "Null stream",
  "null",
  "Albeu",
  "",
  open_s,
  { 
#ifdef USE_TV
"tv", 
#endif
"mf", "null", NULL },
  NULL,
  0 // Urls are an option string
};
