#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//extern int verbose; // defined in mplayer.c

#include "config.h"

#include "stream.h"
#include "demuxer.h"
#include "parse_es.h"

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

#include "codec-cfg.h"
#include "stheader.h"

//extern void resync_audio_stream(sh_audio_t *sh_audio);
//extern void skip_audio_frame(sh_audio_t *sh_audio);

//extern int asf_packetsize; // for seeking

//extern float avi_audio_pts;
//extern float avi_video_pts;
//extern float avi_video_ftime;
//extern int skip_video_frames;
//extern int seek_to_byte;
//extern char* current_module; // for debugging

// flags:
//   0x1 - absolute/relative
//   0x2 - keyframe/hard

int demux_seek_avi(demuxer_t *demuxer,float rel_seek_secs,int flags);
int demux_seek_asf(demuxer_t *demuxer,float rel_seek_secs,int flags);
int demux_seek_mpg(demuxer_t *demuxer,float rel_seek_secs,int flags);

int demux_seek(demuxer_t *demuxer,float rel_seek_secs,int flags){
    demux_stream_t *d_audio=demuxer->audio;
    demux_stream_t *d_video=demuxer->video;
    sh_audio_t *sh_audio=d_audio->sh;
    sh_video_t *sh_video=d_video->sh;
//    float skip_audio_secs=0;

if(demuxer->file_format==DEMUXER_TYPE_AVI && demuxer->idx_size<=0){
    printf("Can't seek in raw .AVI streams! (index required, try with the -idx switch!)  \n");
    return 0;
}

//    current_module="seek";

    // clear demux buffers:
    if(sh_audio){ ds_free_packs(d_audio);sh_audio->a_buffer_len=0;}
    ds_free_packs(d_video);
    
    demuxer->stream->eof=0; // clear eof flag

      if(sh_audio) sh_audio->timer=0;
      sh_video->timer=0; // !!!!!!
    
//    printf("sh_audio->a_buffer_len=%d  \n",sh_audio->a_buffer_len);
    

switch(demuxer->file_format){

  case DEMUXER_TYPE_AVI:
      demux_seek_avi(demuxer,rel_seek_secs,flags);  break;

  case DEMUXER_TYPE_ASF:
      demux_seek_asf(demuxer,rel_seek_secs,flags);  break;
  
  case DEMUXER_TYPE_MPEG_ES:
  case DEMUXER_TYPE_MPEG_PS:
      demux_seek_mpg(demuxer,rel_seek_secs,flags);  break;

} // switch(demuxer->file_format)

return 1;
}
