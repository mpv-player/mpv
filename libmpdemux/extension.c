
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "stream.h"
#include "demuxer.h"

/*
 * An autodetection based on the extension is not a good idea, but we don't care ;-)
 */
static struct {
        char *extension;
        int demuxer_type;
} extensions_table[] = {
        { "mpeg", DEMUXER_TYPE_MPEG_PS },
        { "mpg", DEMUXER_TYPE_MPEG_PS },
        { "mpe", DEMUXER_TYPE_MPEG_PS },
        { "vob", DEMUXER_TYPE_MPEG_PS },
        { "m2v", DEMUXER_TYPE_MPEG_PS },
        { "avi", DEMUXER_TYPE_AVI },
        { "mov", DEMUXER_TYPE_MOV },
        { "qt", DEMUXER_TYPE_MOV },
        { "asx", DEMUXER_TYPE_ASF },
        { "asf", DEMUXER_TYPE_ASF },
        { "wmv", DEMUXER_TYPE_ASF },
        { "wma", DEMUXER_TYPE_ASF },
        { "viv", DEMUXER_TYPE_VIVO },
        { "vivo", DEMUXER_TYPE_VIVO },
        { "rm", DEMUXER_TYPE_REAL },
        { "rmvb", DEMUXER_TYPE_REAL },
        { "ra", DEMUXER_TYPE_REAL },
        { "y4m", DEMUXER_TYPE_Y4M },
        { "mp3", DEMUXER_TYPE_AUDIO },
        { "wav", DEMUXER_TYPE_AUDIO },
        { "ogg", DEMUXER_TYPE_OGG },
        { "ogm", DEMUXER_TYPE_OGG },
        { "pls", DEMUXER_TYPE_PLAYLIST },
        { "m3u", DEMUXER_TYPE_PLAYLIST },
        { "xm", DEMUXER_TYPE_XMMS },
        { "mod", DEMUXER_TYPE_XMMS },
        { "s3m", DEMUXER_TYPE_XMMS },
        { "it", DEMUXER_TYPE_XMMS },
        { "mid", DEMUXER_TYPE_XMMS },
        { "midi", DEMUXER_TYPE_XMMS },
        { "vqf", DEMUXER_TYPE_XMMS }
};

int demuxer_type_by_filename(char* filename){
  int i;
  char* extension=strrchr(filename,'.');
  printf("Searching demuxer type for filename %s ext: %s\n",filename,extension);
  if(extension) {
    ++extension;
//    mp_msg(MSGT_CPLAYER,MSGL_DBG2,"Extension: %s\n", extension );
    // Look for the extension in the extensions table
    for( i=0 ; i<(sizeof(extensions_table)/sizeof(extensions_table[0])) ; i++ ) {
      if( !strcasecmp(extension, extensions_table[i].extension) ) {
        printf("\n!!! trying demuxer %d based on filename extension\n",extensions_table[i].demuxer_type);
        return extensions_table[i].demuxer_type;
      }
    }
  }
  return DEMUXER_TYPE_UNKNOWN;
}

