
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "wine/mmreg.h"
#include "wine/avifmt.h"
#include "wine/vfw.h"

extern char* encode_name;
extern char* encode_index_name;

void write_avi_chunk(FILE *f,unsigned int id,int len,void* data){

fwrite(&id,4,1,f);
fwrite(&len,4,1,f);
if(len>0){
  if(data){
    // DATA
    fwrite(data,len,1,f);
    if(len&1){  // padding
      unsigned char zerobyte=0;
      fwrite(&zerobyte,1,1,f);
    }
  } else {
    // JUNK
    char *avi_junk_data="[= MPlayer junk data! =]";
    if(len&1) ++len; // padding
    while(len>0){
      int l=strlen(avi_junk_data);
      if(l>len) l=len;
      fwrite(avi_junk_data,l,1,f);
      len-=l;
    }
  }
}

}


void write_avi_list(FILE *f,unsigned int id,int len){
  unsigned int list_id=FOURCC_LIST;
  len+=4; // list fix
  fwrite(&list_id,4,1,f);
  fwrite(&len,4,1,f);
  fwrite(&id,4,1,f);
}

struct {
  MainAVIHeader avih;
  AVIStreamHeader video;
  BITMAPINFOHEADER bih;
  unsigned int movi_start;
  unsigned int movi_end;
  unsigned int file_end;
} wah;

void write_avi_header(FILE *f){
  unsigned int riff[3];
  // RIFF header:
  riff[0]=mmioFOURCC('R','I','F','F');
  riff[1]=wah.file_end;  // filesize
  riff[2]=formtypeAVI; // 'AVI '
  fwrite(&riff,12,1,f);
  // AVI header:
  write_avi_list(f,listtypeAVIHEADER,sizeof(wah.avih)+8+12+sizeof(wah.video)+8+sizeof(wah.bih)+8);
  write_avi_chunk(f,ckidAVIMAINHDR,sizeof(wah.avih),&wah.avih);
  // stream header:
  write_avi_list(f,listtypeSTREAMHEADER,sizeof(wah.video)+8+sizeof(wah.bih)+8);
  write_avi_chunk(f,ckidSTREAMHEADER,sizeof(wah.video),&wah.video);
  write_avi_chunk(f,ckidSTREAMFORMAT,sizeof(wah.bih),&wah.bih);
  // JUNK:  
  write_avi_chunk(f,ckidAVIPADDING,2048-(ftell(f)&2047)-8,NULL);
  // 'movi' header:
  write_avi_list(f,listtypeAVIMOVIE,wah.movi_end-ftell(f)-12);
  wah.movi_start=ftell(f);
}

// called _before_ encoding:  (write placeholders and video info)
void write_avi_header_1(FILE *f,int fcc,float fps,int width,int height){
  int frames=8*3600*fps; // 8 hours
  
  wah.file_end=
  wah.movi_end=0x7f000000;

  wah.avih.dwMicroSecPerFrame=1000000.0f/fps;
  wah.avih.dwMaxBytesPerSec=fps*500000; // ?????
  wah.avih.dwPaddingGranularity=1; // padding
  wah.avih.dwFlags=AVIF_ISINTERLEAVED;
  wah.avih.dwTotalFrames=frames;
  wah.avih.dwInitialFrames=0;
  wah.avih.dwStreams=1;
  wah.avih.dwSuggestedBufferSize=0x10000; // 1MB
  wah.avih.dwWidth=width;
  wah.avih.dwHeight=height;
  wah.avih.dwReserved[0]=
  wah.avih.dwReserved[1]=
  wah.avih.dwReserved[2]=
  wah.avih.dwReserved[3]=0;
  
  wah.video.fccType=streamtypeVIDEO;
  wah.video.fccHandler=fcc;
  wah.video.dwFlags=0;
  wah.video.wPriority=0;
  wah.video.wLanguage=0;
  wah.video.dwInitialFrames=0;
  wah.video.dwScale=10000;
  wah.video.dwRate=fps*10000;
  wah.video.dwStart=0;
  wah.video.dwLength=frames;
  wah.video.dwSuggestedBufferSize=0x100000; // 1MB ????
  wah.video.dwQuality=10000;
  wah.video.dwSampleSize=width*height*3;
  
  wah.bih.biSize=sizeof(wah.bih); // 40 ?
  wah.bih.biWidth=width;
  wah.bih.biHeight=height;
  wah.bih.biPlanes=1;
  wah.bih.biBitCount=24;
  wah.bih.biCompression=fcc;
  wah.bih.biSizeImage=3*width*height;
  wah.bih.biXPelsPerMeter=
  wah.bih.biYPelsPerMeter=
  wah.bih.biClrUsed=
  wah.bih.biClrImportant=0;

  write_avi_header(f);  
}

void avi_fixate(){
  // append index and fix avi headers:
  FILE *f1=fopen(encode_name,"r+");
  FILE *f2;

  if(!f1) return; // error
  
  fseek(f1,0,SEEK_END);
  wah.file_end=wah.movi_end=ftell(f1);

  // index:
  if(encode_index_name && (f2=fopen(encode_index_name,"rb"))){
    AVIINDEXENTRY idx;
    unsigned int pos=0;
    int frames=0;
    write_avi_chunk(f1,ckidAVINEWINDEX,0,NULL);
    while(fread(&idx,sizeof(idx),1,f2)>0){
      idx.dwChunkOffset-=wah.movi_start-4;
      fwrite(&idx,sizeof(idx),1,f1);
      ++frames;
    }
    fclose(f2);
    unlink(encode_index_name);
    wah.file_end=ftell(f1);
    // re-write idx1 length:
    pos=wah.file_end-wah.movi_end-8;
    fseek(f1,wah.movi_end+4,SEEK_SET);
    fwrite(&pos,4,1,f1);
    // fixup frames:
    wah.avih.dwTotalFrames=frames;
    wah.video.dwLength=frames;
  }

  // re-write avi header:
  fseek(f1,0,SEEK_SET);
  write_avi_header(f1);

  fclose(f1);
  
}



