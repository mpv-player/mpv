// AVI Parser tool   v0.1   (C) 2000. by A'rpi/ESP-team

#include <stdio.h>
#include <stdlib.h>

#include <signal.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/cdrom.h>

#include "config.h"

#include "loader.h"
#include "wine/avifmt.h"
//#include "libvo/video_out.h"

#include "linux/timer.h"
#include "linux/shmem.h"

#include "help_avp.h"

#define DEBUG if(0)

//static int show_packets=0;

typedef struct {
  // file:
  MainAVIHeader avih;
  unsigned int movi_start;
  unsigned int movi_end;
  // index:
  AVIINDEXENTRY* idx;
  int idx_size;
  int idx_pos;
//  int a_idx;
//  int v_idx;
  // video:
  AVIStreamHeader video;
  char *video_codec;
  BITMAPINFOHEADER bih;   // in format
  BITMAPINFOHEADER o_bih; // out format
  HIC hic;
  void *our_out_buffer;
  char yuv_supported;   // 1 if codec support YUY2 output format
  char yuv_hack_needed; // requires for divx & mpeg4
  // audio:
  AVIStreamHeader audio;
  char *audio_codec;
  char wf_ext[64];     // in format
  WAVEFORMATEX wf;     // out format
  HACMSTREAM srcstream;
  int audio_minsize;
} avi_header_t;

avi_header_t avi_header;

#include "aviprint.c"
//#include "codecs.c"

//**************************************************************************//
#include "stream.c"
//#include "demuxer.c"
//#include "demux_avi.c"

static stream_t* stream=NULL;

//**************************************************************************//

extern int errno;
static int play_in_bg=0;

void exit_player(){
//  int tmp;
  // restore terminal:
  getch2_disable();
  printf("\n\n");
  if(play_in_bg) system("xsetroot -solid \\#000000");
  exit(1);
}

void exit_sighandler(int x){
  printf("\nmpgplay2 interrupted by signal %d\n",x);
  exit_player();
}


int main(int argc,char* argv[]){
char* filename=NULL; //"MI2-Trailer.avi";
int i;
//int seek_to_sec=0;
int seek_to_byte=0;
int f; // filedes
int has_audio=1;
//int audio_format=0;
//int alsa=0;
//int audio_buffer_size=-1;
int audio_id=-1;
//int video_id=-1;
//float default_max_pts_correction=0.01f;
//int delay_corrected=0;
//float force_fps=0;
//float default_fps=25;
//float audio_delay=0;
int stream_type;
//int elementary_stream=0;
int vcd_track=0;
#ifdef VCD_CACHE
int vcd_cache_size=128;
#endif
//char* video_driver="mga"; // default
//int out_fmt=0;
int idx_filepos=0;
FILE *audiofile=NULL;
FILE *videofile=NULL;
char *audiofile_name=NULL;
char *videofile_name=NULL;

  printf("%s",banner_text);

for(i=1;i<argc;i++){
  if(strcmp(argv[i],"-afile")==0)  audiofile_name=argv[++i]; else
  if(strcmp(argv[i],"-vfile")==0)  videofile_name=argv[++i]; else
//  if(strcmp(argv[i],"-sb")==0) seek_to_byte=strtol(argv[++i],NULL,0); else
  if(strcmp(argv[i],"-aid")==0) audio_id=strtol(argv[++i],NULL,0); else
//  if(strcmp(argv[i],"-vid")==0) video_id=strtol(argv[++i],NULL,0); else
//  if(strcmp(argv[i],"-afm")==0) audio_format=strtol(argv[++i],NULL,0); else
//  if(strcmp(argv[i],"-vcd")==0) vcd_track=strtol(argv[++i],NULL,0); else
  if(strcmp(argv[i],"-h")==0) break; else
  if(strcmp(argv[i],"--help")==0) break; else
  { if(filename){ printf("invalid option: %s\n",filename);exit(1);}
    filename=argv[i];
  }
}

if(!filename){
  if(vcd_track) filename="/dev/cdrom"; 
//  else
//  filename="/4/Film/Joan of Arc [Hun DivX]/Joan of Arc - CD2.avi";
  { printf("%s",help_text); exit(0);}
}


if(vcd_track){
//============ Open VideoCD track ==============
  f=open(filename,O_RDONLY);
  if(f<0){ printf("Device not found!\n");return 1; }
  vcd_read_toc(f);
  if(!vcd_seek_to_track(f,vcd_track)){ printf("Error selecting VCD track!\n");return 1;}
  seek_to_byte+=VCD_SECTOR_DATA*vcd_get_msf();
  stream_type=STREAMTYPE_VCD;
#ifdef VCD_CACHE
  vcd_cache_init(vcd_cache_size);
#endif
} else {
//============ Open plain FILE ============
  f=open(filename,O_RDONLY);
  if(f<0){ printf("File not found!\n");return 1; }
  stream_type=STREAMTYPE_FILE;
}

//============ Open & Sync stream and detect file format ===============

stream=new_stream(f,stream_type);
//=============== Read AVI header:
{ //---- RIFF header:
  int id=stream_read_dword_le(stream); // "RIFF"
  if(id!=mmioFOURCC('R','I','F','F')){ printf("Not RIFF format file!\n");return 1; }
  stream_read_dword_le(stream); //filesize
  id=stream_read_dword_le(stream); // "AVI "
  if(id!=formtypeAVI){ printf("Not AVI file!\n");return 1; }
}
//---- AVI header:
avi_header.idx_size=0;
while(1){
  int id=stream_read_dword_le(stream);
  int chunksize,size2;
  static int last_fccType=0;
  //
  if(stream_eof(stream)) break;
  //
  if(id==mmioFOURCC('L','I','S','T')){
    int len=stream_read_dword_le(stream)-4; // list size
    id=stream_read_dword_le(stream);        // list type
    printf("LIST %.4s  len=%d\n",&id,len);
    if(id==listtypeAVIMOVIE){
      // found MOVI header
      avi_header.movi_start=stream_tell(stream);
      avi_header.movi_end=avi_header.movi_start+len;
//      printf("Found movie at 0x%X - 0x%X\n",avi_header.movi_start,avi_header.movi_end);
      len=(len+1)&(~1);
      stream_skip(stream,len);
    }
    continue;
  }
  size2=stream_read_dword_le(stream);
  printf("CHUNK %.4s  len=%d\n",&id,size2);
  chunksize=(size2+1)&(~1);
  switch(id){
    case ckidAVIMAINHDR:          // read 'avih'
      stream_read(stream,(char*) &avi_header.avih,sizeof(avi_header.avih));
      chunksize-=sizeof(avi_header.avih);
      print_avih(&avi_header.avih);
      break;
    case ckidSTREAMHEADER: {      // read 'strh'
      AVIStreamHeader h;
      stream_read(stream,(char*) &h,sizeof(h));
      chunksize-=sizeof(h);
      if(h.fccType==streamtypeVIDEO) memcpy(&avi_header.video,&h,sizeof(h));else
      if(h.fccType==streamtypeAUDIO) memcpy(&avi_header.audio,&h,sizeof(h));
      last_fccType=h.fccType;
      print_strh(&h);
      break; }
    case ckidSTREAMFORMAT: {      // read 'strf'
      if(last_fccType==streamtypeVIDEO){
        stream_read(stream,(char*) &avi_header.bih,sizeof(avi_header.bih));
        chunksize-=sizeof(avi_header.bih);
//        init_video_codec();
//        init_video_out();
      } else
      if(last_fccType==streamtypeAUDIO){
        int z=(chunksize<64)?chunksize:64;
        printf("found 'wf', %d bytes of %d\n",chunksize,sizeof(WAVEFORMATEX));
        stream_read(stream,(char*) &avi_header.wf_ext,z);
        chunksize-=z;
        print_wave_header((WAVEFORMATEX*)&avi_header.wf_ext);
//        init_audio_codec();
//        init_audio_out();
      }
      break;
    }
    case ckidAVINEWINDEX: {
      avi_header.idx_size=size2>>4;
//      printf("Reading INDEX block, %d chunks for %d frames\n",
//        avi_header.idx_size,avi_header.avih.dwTotalFrames);
      avi_header.idx=malloc(avi_header.idx_size<<4);
      idx_filepos=stream_tell(stream);
      stream_read(stream,(char*)avi_header.idx,avi_header.idx_size<<4);
      chunksize-=avi_header.idx_size<<4;
      print_index();
      break;
    }
  }
  if(chunksize>0) stream_skip(stream,chunksize); else
  if(chunksize<0) printf("WARNING!!! chunksize=%d  (id=%.4s)\n",chunksize,&id);
  
}

printf("----------------------------------------------------------------------\n");
printf("Found movie at 0x%X - 0x%X\n",avi_header.movi_start,avi_header.movi_end);
if(avi_header.idx_size<=0){ printf("No index block found!\n");return 0;}
printf("Index block at 0x%X, %d entries for %d frames\n",idx_filepos,
  avi_header.idx_size,avi_header.avih.dwTotalFrames  );

stream_reset(stream);
stream_seek(stream,avi_header.movi_start);
avi_header.idx_pos=0;

if(audiofile_name) audiofile=fopen(audiofile_name,"wb");
if(videofile_name) videofile=fopen(videofile_name,"wb");

for(i=0;i<avi_header.idx_size;i++){
#if 0
    printf("%.4s  %4X  %08X  %d  ",
      &avi_header.idx[i].ckid,
      avi_header.idx[i].dwFlags,
      avi_header.idx[i].dwChunkOffset,
      avi_header.idx[i].dwChunkLength
    );fflush(stdout);
#endif
    if(avi_header.idx[i].ckid&AVIIF_LIST){
//      printf("LIST\n");
    } else {
      int id,size;
      stream_seek(stream,avi_header.movi_start+avi_header.idx[i].dwChunkOffset-4);
      id=stream_read_dword_le(stream);
      size=stream_read_dword_le(stream);
      if(id!=avi_header.idx[i].ckid){
        printf("ChunkID mismatch! raw=%.4s (0x%X) idx=%.4s (0x%X)\n",
          &id,avi_header.movi_start+avi_header.idx[i].dwChunkOffset-4,
          &avi_header.idx[i].ckid,idx_filepos+16*i
        );
        continue;
      }
      if(size!=avi_header.idx[i].dwChunkLength){
        printf("ChunkSize mismatch! raw=%d (0x%X) idx=%d (0x%X)\n",
          size,avi_header.movi_start+avi_header.idx[i].dwChunkOffset-4,
          avi_header.idx[i].dwChunkLength,idx_filepos+16*i
        );
        continue;
      }

      if(id!=mmioFOURCC('J','U','N','K'))
      if(TWOCCFromFOURCC(id)==cktypeWAVEbytes){
          // audio
          int aid=StreamFromFOURCC(id);
          if(audio_id==-1) audio_id=aid;
          if(audio_id==aid){
            if(audiofile){
              void* mem=malloc(size);
              stream_read(stream,mem,size);
              fwrite(mem,size,1,audiofile);
              free(mem);
            }
          } else {
            printf("Invalid audio stream id: %d (%.4s)\n",aid,&id);
          }
      } else 
      if(LOWORD(id)==aviTWOCC('0','0')){
         // video
            if(videofile){
              void* mem=malloc(size);
              stream_read(stream,mem,size);
              fwrite(&size,4,1,videofile);
              fwrite(mem,size,1,videofile);
              free(mem);
            }
      } else {
         // unknown
         printf("Unknown chunk: %.4s (%X)\n",&id,id);
      }

    } // LIST or CHUNK

}

return 0;
}

