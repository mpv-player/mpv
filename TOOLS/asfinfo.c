#define SAVE_STREAMS

// simple ASF header display program by A'rpi/ESP-team
// .asf fileformat docs from http://divx.euro.ru

#include <stdio.h>
#include <stdlib.h>

typedef struct __attribute__((packed)) 
{
    long        biSize; // sizeof(BITMAPINFOHEADER)
    long        biWidth;
    long        biHeight;
    short       biPlanes; // unused
    short       biBitCount;
    long        biCompression; // fourcc of image
    long        biSizeImage;   // size of image. For uncompressed images
                               // ( biCompression 0 or 3 ) can be zero.


    long        biXPelsPerMeter; // unused
    long        biYPelsPerMeter; // unused
    long        biClrUsed;     // valid only for palettized images.
                               // Number of colors in palette.
    long        biClrImportant;
} BITMAPINFOHEADER;

typedef struct
{
  short   wFormatTag; // value that identifies compression format
  short   nChannels;
  long  nSamplesPerSec;
  long  nAvgBytesPerSec;
  short   nBlockAlign; // size of a data sample
  short   wBitsPerSample;
  short   cbSize;    // size of format-specific data
} WAVEFORMATEX;

typedef struct __attribute__((packed)) {
  unsigned char guid[16];
  unsigned long long size;
} ASF_obj_header_t;

typedef struct __attribute__((packed)) {
  ASF_obj_header_t objh;
  unsigned int cno; // number of subchunks
  unsigned char v1; // unknown (0x01)
  unsigned char v2; // unknown (0x02)
} ASF_header_t;

typedef struct __attribute__((packed)) {
  unsigned char client[16]; // Client GUID
  unsigned long long file_size;
  unsigned long long creat_time; //File creation time FILETIME 8
  unsigned long long packets;    //Number of packets UINT64 8
  unsigned long long end_timestamp; //Timestamp of the end position UINT64 8
  unsigned long long duration;  //Duration of the playback UINT64 8
  unsigned long start_timestamp; //Timestamp of the start position UINT32 4
  unsigned long unk1; //Unknown, maybe reserved ( usually contains 0 ) UINT32 4
  unsigned long flags; //Unknown, maybe flags ( usually contains 2 ) UINT32 4
  unsigned long packetsize; //Size of packet, in bytes UINT32 4
  unsigned long packetsize2; //Size of packet ( confirm ) UINT32 4
  unsigned long frame_size; //Size of uncompressed video frame UINT32 4
} ASF_file_header_t;

typedef struct __attribute__((packed)) {
  unsigned char type[16]; // Stream type (audio/video) GUID 16
  unsigned char concealment[16]; // Audio error concealment type GUID 16
  unsigned long long unk1; // Unknown, maybe reserved ( usually contains 0 ) UINT64 8
  unsigned long type_size; //Total size of type-specific data UINT32 4
  unsigned long stream_size; //Size of stream-specific data UINT32 4
  unsigned short stream_no; //Stream number UINT16 2
  unsigned long unk2; //Unknown UINT32 4
} ASF_stream_header_t;

typedef struct __attribute__((packed)) {
  unsigned char streamno;
  unsigned char seq;
  unsigned long x;
  unsigned char flag;
} ASF_segmhdr_t;


ASF_header_t asfh;
ASF_obj_header_t objh;
ASF_file_header_t fileh;
ASF_stream_header_t streamh;
unsigned char buffer[8192];

int i;

char* chunk_type(unsigned char* guid){
  switch(*((unsigned int*)guid)){
    case 0xF8699E40: return "guid_audio_stream";
    case 0xBC19EFC0: return "guid_video_stream";
    case 0x49f1a440: return "guid_audio_conceal_none";
    case 0xbfc3cd50: return "guid_audio_conceal_interleave";
    case 0x75B22630: return "guid_header";
    case 0x75b22636: return "guid_data_chunk";
    case 0x33000890: return "guid_index_chunk";
    case 0xB7DC0791: return "guid_stream_header";
    case 0xD6E229D1: return "guid_header_2_0";
    case 0x8CABDCA1: return "guid_file_header";
  }
  return NULL;
}

void print_wave_header(WAVEFORMATEX *h){

  printf("======= WAVE Format =======\n");
  
  printf("Format Tag: %d (0x%X)\n",h->wFormatTag,h->wFormatTag);
  printf("Channels: %d\n",h->nChannels);
  printf("Samplerate: %d\n",h->nSamplesPerSec);
  printf("avg byte/sec: %d\n",h->nAvgBytesPerSec);
  printf("Block align: %d\n",h->nBlockAlign);
  printf("bits/sample: %d\n",h->wBitsPerSample);
  printf("cbSize: %d\n",h->cbSize);
  
  switch(h->wFormatTag){
      case 0x01:        printf("Audio in PCM format\n");break;
      case 0x50:        printf("Audio in MPEG Layer 1/2 format\n");break;
      case 0x55:        printf("Audio in MPEG Layer-3 format\n");break; // ACM
      case 0x02:        printf("Audio in MS ADPCM format\n");break;  // ACM
      case 0x11:        printf("Audio in IMA ADPCM format\n");break; // ACM
      case 0x31:
      case 0x32:        printf("Audio in MS GSM 6.10 format\n");break; // ACM
      case 0x160:
      case 0x161:       printf("Audio in DivX WMA format\n");break; // ACM
      default:          printf("Audio in UNKNOWN (id=0x%X) format\n",h->wFormatTag);
  }

  printf("===========================\n");
  

}

void print_video_header(BITMAPINFOHEADER *h){
  printf("======= VIDEO Format ======\n");
	printf("  biSize %d\n", h->biSize);
	printf("  biWidth %d\n", h->biWidth);
	printf("  biHeight %d\n", h->biHeight);
	printf("  biPlanes %d\n", h->biPlanes);
	printf("  biBitCount %d\n", h->biBitCount);
	printf("  biCompression %d='%.4s'\n", h->biCompression, &h->biCompression);
	printf("  biSizeImage %d\n", h->biSizeImage);
  printf("===========================\n");
}

FILE* streams[128];

int main(int argc,char* argv[]){
FILE *f=fopen(argc>1?argv[1]:"Alice Deejay - Back In My Life.asf","rb");

if(!f){ printf("file not found\n");exit(1);}

//printf("sizeof=%d\n",sizeof(objh));
//printf("sizeof=%d\n",sizeof(asfh));

fread(&asfh,sizeof(asfh),1,f); // header obj
//for(i=0;i<16;i++) printf("%02X ",asfh.objh.guid[i]);
printf("[%s] %d  (subchunks: %d)\n",chunk_type(asfh.objh.guid),(int) asfh.objh.size,asfh.cno);

while(fread(&objh,sizeof(objh),1,f)>0){
  int pos=ftell(f);
//  for(i=0;i<16;i++) printf("%02X ",objh.guid[i]);
  printf("0x%08X  [%s] %d\n",pos-sizeof(objh), chunk_type(objh.guid),(int) objh.size);
  switch(*((unsigned int*)&objh.guid)){
    case 0xB7DC0791: // guid_stream_header
      fread(&streamh,sizeof(streamh),1,f);
      printf("stream type: %s\n",chunk_type(streamh.type));
      printf("stream concealment: %s\n",chunk_type(streamh.concealment));
      printf("type: %d bytes,  stream: %d bytes  ID: %d\n",(int)streamh.type_size,(int)streamh.stream_size,(int)streamh.stream_no);
      printf("FILEPOS=0x%X\n",ftell(f));
      // type-specific data:
      fread(buffer,streamh.type_size,1,f);
      switch(*((unsigned int*)&streamh.type)){
      case 0xF8699E40:  // guid_audio_stream
        print_wave_header((WAVEFORMATEX*)buffer);
        break;
      case 0xBC19EFC0:  // guid_video_stream
        print_video_header((BITMAPINFOHEADER*)&buffer[4+4+1+2]);
        break;
      }
      // stream-specific data:
      fread(buffer,streamh.stream_size,1,f);
      break;
//    case 0xD6E229D1: return "guid_header_2_0";
    case 0x8CABDCA1: // guid_file_header
      fread(&fileh,sizeof(fileh),1,f);
      printf("packets: %d  flags: %d  pack_size: %d  frame_size: %d\n",(int)fileh.packets,(int)fileh.flags,(int)fileh.packetsize,(int)fileh.frame_size);
      break;
    case 0x75b22636: // guid_data_chunk
      { int endp=pos+objh.size-sizeof(objh);
        unsigned char* packet=malloc((int)fileh.packetsize);
        int fpos;
        fseek(f,26,SEEK_CUR);
        while((fpos=ftell(f))<endp){
          fread(packet,(int)fileh.packetsize,1,f);
          if(packet[0]==0x82){
            unsigned char flags=packet[3];
            unsigned char* p=&packet[5];
            unsigned long time;
            unsigned short duration;
            int segs=1;
            int seg;
            int padding=0;
            if(flags&8){
              padding=p[0];++p;
            } else
            if(flags&16){
              padding=p[0]|(p[1]<<8);p+=2;
            }
            time=*((unsigned long*)p);p+=4;
            duration=*((unsigned short*)p);p+=2;
            if(flags&1){
              segs=p[0]-0x80;++p;
            }
            printf("%08X:  flag=%02X  segs=%d  pad=%d  time=%d  dur=%d\n",
              fpos,flags,segs,padding,time,duration);
            for(seg=0;seg<segs;seg++){
              ASF_segmhdr_t* sh=(ASF_segmhdr_t*)p;
              int len=0;
              p+=sizeof(ASF_segmhdr_t);
              if(sh->flag&8) p+=8;// else
              if(sh->flag&1) ++p;
              if(flags&1){
                len=*((unsigned short*)p);p+=2;
              }
              printf("  seg #%d: streamno=%d  seq=%d  flag=%02X  len=%d\n",seg,sh->streamno&0x7F,sh->seq,sh->flag,len);
#ifdef SAVE_STREAMS
              if(!streams[sh->streamno&0x7F]){
                char name[256];
                snprintf(name,256,"stream%02X.dat",sh->streamno&0x7F);
                streams[sh->streamno&0x7F]=fopen(name,"wb");
              }
              fwrite(p,len,1,streams[sh->streamno&0x7F]);
#endif
              p+=len;
            }
          } else
            printf("%08X:  UNKNOWN  %02X %02X %02X %02X %02X...\n",fpos,packet[0],packet[1],packet[2],packet[3],packet[4]);
        }
      }
      break;

//    case 0x33000890: return "guid_index_chunk";

  }
  fseek(f,pos+objh.size-sizeof(objh),SEEK_SET);
}


}

