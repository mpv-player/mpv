/*
 * This file was ported to MPlayer from xine CVS sdpplin.c,v 1.1 2002/12/24 01:30:22
 */

/*
 * Copyright (C) 2002 the xine project
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 *
 * sdp/sdpplin parser.
 *
 */
 
#include "rmff.h"
#include "rtsp.h"
#include "sdpplin.h"
#include "xbuffer.h"

/*
#define LOG
*/

/*
 * Decodes base64 strings (based upon b64 package)
 */

static char *b64_decode(const char *in, char *out, int *size)
{
  char dtable[256];              /* Encode / decode table */
  int i,j,k;

  for (i = 0; i < 255; i++) {
    dtable[i] = 0x80;
  }
  for (i = 'A'; i <= 'Z'; i++) {
    dtable[i] = 0 + (i - 'A');
  }
  for (i = 'a'; i <= 'z'; i++) {
    dtable[i] = 26 + (i - 'a');
  }
  for (i = '0'; i <= '9'; i++) {
    dtable[i] = 52 + (i - '0');
  }
  dtable['+'] = 62;
  dtable['/'] = 63;
  dtable['='] = 0;

  k=0;
  
  /*CONSTANTCONDITION*/
  for (j=0; j<strlen(in); j+=4)
  {
    char a[4], b[4];

    for (i = 0; i < 4; i++) {
      int c = in[i+j];

      if (dtable[c] & 0x80) {
        printf("Illegal character '%c' in input.\n", c);
//        exit(1);
        return NULL;
      }
      a[i] = (char) c;
      b[i] = (char) dtable[c];
    }
    out = xbuffer_ensure_size(out, k+3);
    out[k++] = (b[0] << 2) | (b[1] >> 4);
    out[k++] = (b[1] << 4) | (b[2] >> 2);
    out[k++] = (b[2] << 6) | b[3];
    i = a[2] == '=' ? 1 : (a[3] == '=' ? 2 : 3);
    if (i < 3) {
      out[k]=0;
      *size=k;
      return out;
    }
  }
  out[k]=0;
  *size=k;
  return out;
}

static char *nl(char *data) {

  return strchr(data,'\n')+1;
}

static int filter(const char *in, const char *filter, char **out) {

  int flen=strlen(filter);
  int len=strchr(in,'\n')-in;

  if (!strncmp(in,filter,flen))
  {
    if(in[flen]=='"') flen++;
    if(in[len-1]==13) len--;
    if(in[len-1]=='"') len--;
    *out = xbuffer_copyin(*out, 0, in+flen, len-flen+1);
    (*out)[len-flen]=0;

    return len-flen;
  }
  
  return 0;
}
static sdpplin_stream_t *sdpplin_parse_stream(char **data) {

  sdpplin_stream_t *desc=calloc(1,sizeof(sdpplin_stream_t));
  char      *buf=xbuffer_init(32);
  char      *decoded=xbuffer_init(32);
  int       handled;
    
  if (filter(*data, "m=", &buf)) {
    desc->id = strdup(buf);
  } else
  {
    printf("sdpplin: no m= found.\n");
    free(desc);
    xbuffer_free(buf);
    return NULL;
  }
  *data=nl(*data);

  while (**data && *data[0]!='m') {

    handled=0;
    
    if(filter(*data,"a=control:streamid=",&buf)) {
      desc->stream_id=atoi(buf);
      handled=1;
      *data=nl(*data);
    }

    if(filter(*data,"a=MaxBitRate:integer;",&buf)) {
      desc->max_bit_rate=atoi(buf);
      if (!desc->avg_bit_rate)
        desc->avg_bit_rate=desc->max_bit_rate;
      handled=1;
      *data=nl(*data);
    }

    if(filter(*data,"a=MaxPacketSize:integer;",&buf)) {
      desc->max_packet_size=atoi(buf);
      if (!desc->avg_packet_size)
        desc->avg_packet_size=desc->max_packet_size;
      handled=1;
      *data=nl(*data);
    }
    
    if(filter(*data,"a=StartTime:integer;",&buf)) {
      desc->start_time=atoi(buf);
      handled=1;
      *data=nl(*data);
    }

    if(filter(*data,"a=Preroll:integer;",&buf)) {
      desc->preroll=atoi(buf);
      handled=1;
      *data=nl(*data);
    }

    if(filter(*data,"a=length:npt=",&buf)) {
      desc->duration=(uint32_t)(atof(buf)*1000);
      handled=1;
      *data=nl(*data);
    }

    if(filter(*data,"a=StreamName:string;",&buf)) {
      desc->stream_name=strdup(buf);
      desc->stream_name_size=strlen(desc->stream_name);
      handled=1;
      *data=nl(*data);
    }

    if(filter(*data,"a=mimetype:string;",&buf)) {
      desc->mime_type=strdup(buf);
      desc->mime_type_size=strlen(desc->mime_type);
      handled=1;
      *data=nl(*data);
    }

    if(filter(*data,"a=OpaqueData:buffer;",&buf)) {
      decoded = b64_decode(buf, decoded, &(desc->mlti_data_size));
      desc->mlti_data=malloc(sizeof(char)*desc->mlti_data_size);
      memcpy(desc->mlti_data, decoded, desc->mlti_data_size);
      handled=1;
      *data=nl(*data);
#ifdef LOG
      printf("mlti_data_size: %i\n", desc->mlti_data_size);
#endif
    }
    
    if(filter(*data,"a=ASMRuleBook:string;",&buf)) {
      desc->asm_rule_book=strdup(buf);
      handled=1;
      *data=nl(*data);
    }

    if(!handled) {
#ifdef LOG
      int len=strchr(*data,'\n')-(*data);
      buf = xbuffer_copyin(buf, 0, *data, len+1);
      buf[len]=0;
      printf("libreal: sdpplin: not handled: '%s'\n", buf);
#endif
      *data=nl(*data);
    }
  }

  xbuffer_free(buf);
  xbuffer_free(decoded);
  
  return desc;
}

sdpplin_t *sdpplin_parse(char *data) {

  sdpplin_t        *desc=calloc(1,sizeof(sdpplin_t));
  sdpplin_stream_t *stream;
  char             *buf=xbuffer_init(32);
  char             *decoded=xbuffer_init(32);
  int              handled;
  int              len;

  while (*data) {

    handled=0;
    
    if (filter(data, "m=", &buf)) {
      stream=sdpplin_parse_stream(&data);
#ifdef LOG
      printf("got data for stream id %u\n", stream->stream_id);
#endif
      desc->stream[stream->stream_id]=stream;
      continue;
    }

    if(filter(data,"a=Title:buffer;",&buf)) {
      decoded=b64_decode(buf, decoded, &len);
      desc->title=strdup(decoded);
      handled=1;
      data=nl(data);
    }
    
    if(filter(data,"a=Author:buffer;",&buf)) {
      decoded=b64_decode(buf, decoded, &len);
      desc->author=strdup(decoded);
      handled=1;
      data=nl(data);
    }
    
    if(filter(data,"a=Copyright:buffer;",&buf)) {
      decoded=b64_decode(buf, decoded, &len);
      desc->copyright=strdup(decoded);
      handled=1;
      data=nl(data);
    }
    
    if(filter(data,"a=Abstract:buffer;",&buf)) {
      decoded=b64_decode(buf, decoded, &len);
      desc->abstract=strdup(decoded);
      handled=1;
      data=nl(data);
    }
    
    if(filter(data,"a=StreamCount:integer;",&buf)) {
      desc->stream_count=atoi(buf);
      desc->stream=malloc(sizeof(sdpplin_stream_t*)*desc->stream_count);
      handled=1;
      data=nl(data);
    }

    if(filter(data,"a=Flags:integer;",&buf)) {
      desc->flags=atoi(buf);
      handled=1;
      data=nl(data);
    }

    if(!handled) {
#ifdef LOG
      int len=strchr(data,'\n')-data;
      buf = xbuffer_copyin(buf, 0, data, len+1);
      buf[len]=0;
      printf("libreal: sdpplin: not handled: '%s'\n", buf);
#endif
      data=nl(data);
    }
  }

  xbuffer_free(buf);
  xbuffer_free(decoded);
  
  return desc;
}

void sdpplin_free(sdpplin_t *description) {

  /* TODO: free strings */
  free(description);
}

