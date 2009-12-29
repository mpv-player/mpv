/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "stream/stream.h"
#include "demuxer.h"

/*
 * An autodetection based on the extension is not a good idea, but we don't care ;-)
 *
 * You should not add anything here where autodetection can be easily fixed except in
 * order to speed up auto-detection, in particular for formats that are often streamed.
 * In particular you should not normally add any DEMUXER_TYPE_LAVF, adding the
 * format to preferred_list in libmpdemux/demuxer_lavf.c will usually achieve
 * the same effect in a much more reliable way.
 */
static struct {
        const char *extension;
        int demuxer_type;
} extensions_table[] = {
//        { "mpeg", DEMUXER_TYPE_MPEG_PS },
//        { "mpg", DEMUXER_TYPE_MPEG_PS },
//        { "mpe", DEMUXER_TYPE_MPEG_PS },
        { "vob", DEMUXER_TYPE_MPEG_PS },
        { "m2v", DEMUXER_TYPE_MPEG_PS },
        { "avi", DEMUXER_TYPE_AVI },
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
        { "flac", DEMUXER_TYPE_AUDIO },
        { "fla", DEMUXER_TYPE_AUDIO },
        { "ogg", DEMUXER_TYPE_OGG },
        { "ogm", DEMUXER_TYPE_OGG },
//        { "pls", DEMUXER_TYPE_PLAYLIST },
//        { "m3u", DEMUXER_TYPE_PLAYLIST },
        { "xm", DEMUXER_TYPE_XMMS },
        { "mod", DEMUXER_TYPE_XMMS },
        { "s3m", DEMUXER_TYPE_XMMS },
        { "it", DEMUXER_TYPE_XMMS },
        { "mid", DEMUXER_TYPE_XMMS },
        { "midi", DEMUXER_TYPE_XMMS },
        { "nsv", DEMUXER_TYPE_NSV },
        { "nsa", DEMUXER_TYPE_NSV },
        { "mpc", DEMUXER_TYPE_MPC },
#ifdef CONFIG_WIN32DLL
        { "avs", DEMUXER_TYPE_AVS },
#endif
	{ "302", DEMUXER_TYPE_LAVF },
        { "264", DEMUXER_TYPE_H264_ES },
        { "26l", DEMUXER_TYPE_H264_ES },
	{ "ac3", DEMUXER_TYPE_LAVF },
        { "ape", DEMUXER_TYPE_LAVF },
        { "apl", DEMUXER_TYPE_LAVF },
        { "eac3",DEMUXER_TYPE_LAVF },
        { "mac", DEMUXER_TYPE_LAVF },
        { "str", DEMUXER_TYPE_LAVF },
        { "cdg", DEMUXER_TYPE_LAVF },

// At least the following are hacks against broken autodetection
// that should not be there

};

int demuxer_type_by_filename(char* filename){
  int i;
  char* extension=strrchr(filename,'.');
  mp_msg(MSGT_OPEN, MSGL_V, "Searching demuxer type for filename %s ext: %s\n",filename,extension);
  if(extension) {
    ++extension;
//    mp_msg(MSGT_CPLAYER,MSGL_DBG2,"Extension: %s\n", extension );
    // Look for the extension in the extensions table
    for( i=0 ; i<(sizeof(extensions_table)/sizeof(extensions_table[0])) ; i++ ) {
      if( !strcasecmp(extension, extensions_table[i].extension) ) {
        mp_msg(MSGT_OPEN, MSGL_V, "Trying demuxer %d based on filename extension\n",extensions_table[i].demuxer_type);
        return extensions_table[i].demuxer_type;
      }
    }
  }
  return DEMUXER_TYPE_UNKNOWN;
}
