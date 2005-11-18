
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#ifdef __FreeBSD__
#include <sys/cdrio.h>
#endif

#include "m_option.h"
#include "stream.h"
#include "demuxer.h"
#include "mf.h"


/// We keep these 2 for the gui atm, but they will be removed.
int vcd_track=0;
char* cdrom_device=NULL;

// Open a new stream  (stdin/file/vcd/url)

stream_t* open_stream(char* filename,char** options, int* file_format){
  // Check if playlist or unknown 
  if (*file_format != DEMUXER_TYPE_PLAYLIST){
    *file_format=DEMUXER_TYPE_UNKNOWN;
  }

if(!filename) {
   mp_msg(MSGT_OPEN,MSGL_ERR,"NULL filename, report this bug\n");
   return NULL;
}

//============ Open STDIN or plain FILE ============

  return open_stream_full(filename,STREAM_READ,options,file_format);
}

