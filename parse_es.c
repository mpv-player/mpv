//=================== MPEG-ES VIDEO PARSER =========================

#include <stdio.h>
#include <stdlib.h>

extern int verbose; // defined in mplayer.c

#include "config.h"

#include "stream.h"
#include "demuxer.h"

#include "parse_es.h"

//static unsigned char videobuffer[MAX_VIDEO_PACKET_SIZE];
unsigned char* videobuffer=NULL;
int videobuf_len=0;
unsigned char videobuf_code[4];
int videobuf_code_len=0;

// sync video stream, and returns next packet code
int sync_video_packet(demux_stream_t *ds){
  int skipped=0;
  // we need enough bytes in the buffer:
  while(videobuf_code_len<4){
#if 0
    int c;
    c=demux_getc(ds);if(c<0){ return 0;} // EOF
    videobuf_code[videobuf_code_len++]=c;
#else
    videobuf_code[videobuf_code_len++]=demux_getc(ds);
#endif
  }
  // sync packet:
  while(1){
    int c;
    if(videobuf_code[0]==0 &&
       videobuf_code[1]==0 &&
       videobuf_code[2]==1) break; // synced
    // shift buffer, drop first byte
    ++skipped;
    videobuf_code[0]=videobuf_code[1];
    videobuf_code[1]=videobuf_code[2];
    videobuf_code[2]=videobuf_code[3];
    c=demux_getc(ds);if(c<0){ return 0;} // EOF
    videobuf_code[3]=c;
  }
  if(verbose>=2) if(skipped) printf("videobuf: %d bytes skipped  (next: 0x1%02X)\n",skipped,videobuf_code[3]);
  return 0x100|videobuf_code[3];
}

// return: packet length
int read_video_packet(demux_stream_t *ds){
int packet_start;
  
  // SYNC STREAM
//  if(!sync_video_packet(ds)) return 0; // cannot sync (EOF)

  // COPY STARTCODE:
  packet_start=videobuf_len;
  videobuffer[videobuf_len+0]=videobuf_code[0];
  videobuffer[videobuf_len+1]=videobuf_code[1];
  videobuffer[videobuf_len+2]=videobuf_code[2];
  videobuffer[videobuf_len+3]=videobuf_code[3];
  videobuf_len+=4;
  
  // READ PACKET:
  { unsigned int head=-1;
    while(videobuf_len<VIDEOBUFFER_SIZE){
      int c=demux_getc(ds);
      if(c<0) break; // EOF
      videobuffer[videobuf_len++]=c;
#if 1
      head<<=8;
      if(head==0x100) break; // synced
      head|=c;
#else
      if(videobuffer[videobuf_len-4]==0 &&
         videobuffer[videobuf_len-3]==0 &&
         videobuffer[videobuf_len-2]==1) break; // synced
#endif
    }
  }
  
  if(ds->eof){
    videobuf_code_len=0; // EOF, no next code
    return videobuf_len-packet_start;
  }
  
  videobuf_len-=4;

  if(verbose>=2) printf("videobuf: packet 0x1%02X  len=%d  (total=%d)\n",videobuffer[packet_start+3],videobuf_len-packet_start,videobuf_len);

  // Save next packet code:
  videobuf_code[0]=videobuffer[videobuf_len];
  videobuf_code[1]=videobuffer[videobuf_len+1];
  videobuf_code[2]=videobuffer[videobuf_len+2];
  videobuf_code[3]=videobuffer[videobuf_len+3];
  videobuf_code_len=4;

  return videobuf_len-packet_start;
}

// return: next packet code
int skip_video_packet(demux_stream_t *ds){

  // SYNC STREAM
//  if(!sync_video_packet(ds)) return 0; // cannot sync (EOF)
  
  videobuf_code_len=0; // force resync
  
  // SYNC AGAIN:
  return sync_video_packet(ds);
}
