
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "mp_msg.h"

#include "stream.h"
#include "demuxer.h"
#include "stheader.h"

//--------------------------

// audio stream skip/resync functions requires only for seeking.
// (they should be implemented in the audio codec layer)
void skip_audio_frame(sh_audio_t *sh_audio){
}
void resync_audio_stream(sh_audio_t *sh_audio){
}

int mp_input_check_interrupt(int time){
    if(time) usleep(time);
    return 0;
}

// for libmpdvdkit2:
#include "../get_path.c"

int verbose=5; // must be global!

int stream_cache_size=0;

// for demux_ogg:
void* vo_sub=NULL;
int vo_osd_changed(int new_value){return 0;}
int   subcc_enabled=0;

float sub_fps=0;
int sub_utf8=0;
int   suboverlap_enabled = 1;
float sub_delay=0;

//---------------

extern stream_t* open_stream(char* filename,int vcd_track,int* file_format);

int main(int argc,char* argv[]){

stream_t* stream=NULL;
demuxer_t* demuxer=NULL;
int file_format=DEMUXER_TYPE_UNKNOWN;

  mp_msg_init(verbose+MSGL_STATUS);

  if(argc>1)
    stream=open_stream(argv[1],0,&file_format);
  else
//  stream=open_stream("/3d/divx/405divx_sm_v2[1].avi",0,&file_format);
    stream=open_stream("/dev/cdrom",2,&file_format); // VCD track 2

  if(!stream){
	printf("Cannot open file/device\n");
	exit(1);
  }

  printf("success: format: %d  data: 0x%X - 0x%X\n",file_format, (int)(stream->start_pos),(int)(stream->end_pos));

  if(stream_cache_size)
      stream_enable_cache(stream,stream_cache_size,0,0);

  demuxer=demux_open(stream,file_format,-1,-1,-1,NULL);
  if(!demuxer){
	printf("Cannot open demuxer\n");
	exit(1);
  }

  if(demuxer->video->sh)
      video_read_properties(demuxer->video->sh);

  return 0;
}
