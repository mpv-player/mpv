
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp_msg.h"

#include "stream.h"
#include "demuxer.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"
#include "codec-cfg.h"
#include "stheader.h"

//--------------------------

// audio stream skip/resync functions requires only for seeking.
// (they should be implemented in the audio codec layer)
void skip_audio_frame(sh_audio_t *sh_audio){
}
void resync_audio_stream(sh_audio_t *sh_audio){
}

// some globals:
int verbose=1;

// AVI demuxer parameters:
int index_mode=-1;  // -1=untouched  0=don't use index  1=use (geneate) index
int force_ni=0;     // force non-interleaved AVI parsing
int pts_from_bps=1; // PTS:  0=interleaved  1=BPS-based

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

  demuxer=demux_open(stream,file_format,-1,-1,-1);
  if(!demuxer){
	printf("Cannot open demuxer\n");
	exit(1);
  }
  

}
