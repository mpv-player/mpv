// show QuickTime .mov file structure     (C) 2001. by A'rpi/ESP-team
// various hacks by alex@naxine.org

/*
  Blocks: 4bytes atom_size
          4bytes atom_type (name)
	  ...

  By older files, mdat is at the beginning, and moov follows it later,
  by newer files, moov is at the begininng.
  
  Fontosabb typeok:
  
  trak: track: ezeken belul van egy-egy stream (video/audio)
  tkhd: track header: fps (video esten picture size is itt van)
  vmhd: video media handler (video stream informaciok)
  smhd: sound media handler (audio stream informaciok)
*/

#include <stdio.h>
#include <stdlib.h>

#undef NO_SPECIAL

char *atom2human_type(int type)
{
switch (type)
{
  case 0x766F6F6D: return ("Information sections"); /* moov */
  case 0x6468766D: return ("Movie header"); /* mvhd */
  case 0x6169646D: return ("Media stream"); /* mdia */
  case 0x64686D76: return ("Video media header"); /* vmhd */
  case 0x64686D73: return ("Sound media header"); /* smhd */
  case 0x6468646D: return ("Media header"); /* mdhd */
  case 0x666E696D: return ("Media information"); /* minf */
  case 0x726C6468: return ("Handler reference"); /* hdlr */
  case 0x6B617274: return ("New track (stream)"); /* trak */
  case 0x75716D72: return ("rmqu");
  case 0x65657266: return ("free");
  case 0x64686B74: return ("Track header"); /* tkhd */
  case 0x61746475: return ("User data"); /* udta */
  case 0x7461646D: return ("Movie data"); /* mdat */
  case 0x6C627473: return ("Sample information table"); /* stbl */
  case 0x64737473: return ("Sample description"); /* stsd */
  case 0x6F637473: return ("Chunk offset table"); /* stco */
  case 0x73747473: return ("Sample time table"); /* stts */
  case 0x63737473: return ("Sample->Chunk mapping table"); /* stsc */
  case 0x7A737473: return ("Sample size table"); /* stsz */
}
    return("unknown");
}

#define S_NONE 0
#define S_AUDIO 1
#define S_VIDEO 2
int stream = S_NONE;
int v_stream = 0;
int a_stream = 0;

unsigned int read_dword(FILE *f){
 unsigned char atom_size_b[4];
 if(fread(&atom_size_b,4,1,f)<=0) return -1;
 return (atom_size_b[0]<<24)|(atom_size_b[1]<<16)|(atom_size_b[2]<<8)|atom_size_b[3];
}

void *video_stream_info(FILE *f, int len)
{
  int orig_pos = ftell(f);
  unsigned char data[len-8];
  int i;
  char codec[len-8];

  len -= 8;
  for (i=0; i<len; i++)
	fread(&data[i], 1, 1, f);

//  strncpy(codec, &data[43], len-43);
//  printf("  [codec: %s]\n", &codec);
  fseek(f,orig_pos,SEEK_SET);
}

void *audio_stream_info(FILE *f, int len)
{
  int orig_pos = ftell(f);
  unsigned char data[len-8];
  int i;

  len -= 8;
  for (i=0; i<len; i++)
	fread(&data[i], 1, 1, f);

  printf("  [%d bit", data[19]);
  if (data[17] == 1)
    printf(" mono");
  else
    printf(" %d channels", data[17]);
  printf("]\n");
  fseek(f,orig_pos,SEEK_SET);
}

void *userdata_info(FILE *f, int len, int pos, int level)
{
  int orig_pos = pos; /*ftell(f);*/
  unsigned char data[len-8];
  int i;
  unsigned int atom_size = 1;
  unsigned int atom_type;

//  printf("userdata @ %d:%d (%d)\n", pos, pos+len, len);

//  fseek(f, pos+3, SEEK_SET);

  while (atom_size != 0)
  {
    atom_size=read_dword(f);//  if(fread(&atom_size_b,4,1,f)<=0) break;
    if(fread(&atom_type,4,1,f)<=0) break;
  
    if(atom_size<8) break; // error

//    printf("%08X:  %*s %.4s (%08X) %05d (begin: %08X)\n",pos,level*2,"",
//	&atom_type,atom_type,atom_size,pos+8);

    switch(atom_type)
    {
      case 0x797063A9: /* cpy (copyright) */
        {
	  char *data = malloc(atom_size-8);
	  
	  fseek(f, pos+6, SEEK_SET);
	  fread(data, atom_size-8, 1, f);
	  printf(" Copyright: %s\n", data);
	  free(data);
	}
        break;
      case 0x666E69A9: /* inf (information) */
        {
	  char data[atom_size-8];
	  
	  fread(&data, 1, atom_size-8, f);
	  printf(" Owner: %s\n", &data);
	}
        break;
      case 0x6D616EA9: /* nam (name) */
        {
	  char data[atom_size-8];
	  
	  fread(&data, 1, atom_size-8, f);
	  printf(" Name: %s\n", &data);
	}
        break;
    }
  }
  fseek(f,orig_pos,SEEK_SET);
}

int time_scale = 0;

void lschunks(FILE *f,int level,unsigned int endpos){
 unsigned int atom_size;
 unsigned int atom_type;
 int pos;

 while(endpos==0 || ftell(f)<endpos){
  pos=ftell(f);
  atom_size=read_dword(f);//  if(fread(&atom_size_b,4,1,f)<=0) break;
  if(fread(&atom_type,4,1,f)<=0) break;
  
  if(atom_size<8) break; // error
  
  printf("%08X:  %*s %.4s (%08X) %05d [%s] (begin: %08X)\n",pos,level*2,"",&atom_type,atom_type,atom_size,
    atom2human_type(atom_type), pos+8); // 8: atom_size fields (4) + atom_type fields (4)

#ifndef NO_SPECIAL
//  if (atom_type == 0x61746475)
//    userdata_info(f, atom_size, pos, level);

  if (atom_type == 0x6468646D)
  {
    char data[4];
    
    fread(&data, 1, 1, f); // char
    printf("mdhd version %d\n", data[0]);
    fread(&data, 3, 1, f); // int24
    fread(&data, 4, 1, f); // int32
    fread(&data, 4, 1, f); // int32
    fread(&data, 4, 1, f); // int32
    time_scale = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    printf("timescale: %d\n", time_scale);
    fread(&data, 4, 1, f); // int32
    fread(&data, 2, 1, f); // int16
    fread(&data, 2, 1, f); // int16
  }

  if (atom_type == 0x64686D76)
  {
    stream = S_VIDEO;
    printf(" Found VIDEO Stream #%d\n", v_stream++);
  }
  
  if (atom_type == 0x64686D73)
  {
    stream = S_AUDIO;
    printf(" Found AUDIO Stream #%d\n", a_stream++);
  }

  if (atom_type == 0x64686B74) // tkhd - track header
  {
    int i;
    unsigned char data[atom_size];
    int x, y;

    for (i=0; i<atom_size; i++)
	fread(&data[i], 1, 1, f);

    x = data[77];
    y = data[81];
    printf(" Flags: %d\n", data[3]);
    printf(" Picture size: %dx%d\n", x, y);
    if (x == 0 && y == 0)
	printf(" Possible audio stream!\n");
  }
  
  if(atom_type==0x64737473) {  // stsd
    unsigned int tmp;
    unsigned int count;
    int i;
    fread(&tmp,4,1,f);
    count=read_dword(f);//    fread(&count,4,1,f);
    printf("desc count = %d\n",count);
    for(i=0;i<count;i++){
      unsigned int len;
      unsigned int format;
      len=read_dword(f); //      fread(&len,4,1,f);
      fread(&format,4,1,f);
      printf("  desc #%d: %.4s  (%d)\n",i+1,&format,len);
      if (stream == S_VIDEO)
        video_stream_info(f, len);
      if (stream == S_AUDIO)
        audio_stream_info(f, len);
      fseek(f,len-8,SEEK_CUR);
    }
  }
  
  if(atom_type==0x6F637473) {  // stco
    int len,i;
    read_dword(f);
    len=read_dword(f);
    printf("Chunk table size :%d\n",len);
    for(i=0;i<len;i++) printf("  chunk #%d: 0x%X\n",i+1,read_dword(f));
  }


  if(atom_type==0x73747473) {  // stts
    int len,i;
    read_dword(f);
    len=read_dword(f);
    printf("T->S table size :%d\n",len);
    for(i=0;i<len;i++){
      int num=read_dword(f);
      int dur=read_dword(f);
      printf("%5d samples: %d duration", num, dur);
      if (stream == S_AUDIO)
        printf("(rate: %f Hz)\n", (float)time_scale/dur);
      else
	printf("(fps: %f)\n", (float)time_scale/dur);
    }
  }

  if(atom_type==0x63737473) {  // stsc
    int len,i;
    read_dword(f);
    len=read_dword(f);
    printf("S->C table size :%d\n",len);
    for(i=0;i<len;i++){
      int first=read_dword(f);
      int spc=read_dword(f);
      int sdid=read_dword(f);
      printf("  chunk %d...  %d s/c   desc: %d\n",first,spc,sdid);
    }
  }

  if(atom_type==0x7A737473) {  // stsz
    int len,i,ss;
    read_dword(f);
    ss=read_dword(f);
    len=read_dword(f);
    printf("Sample size table len: %d\n",len);
    if(ss){
      printf("  common sample size: %d bytes\n",ss);
    } else {
      for(i=0;i<len;i++) printf("  sample #%d: %d bytes\n",i+1,read_dword(f));
    }
  }
#endif
  
#if 1
  switch(atom_type){
  case 0x7461646D: // mdat  Movie data
  case 0x75716D72: // rmqu
  case 0x65657266: // free  JUNK
  case 0x64686B74: // tkhd  Track header
  case 0x61746475: // udta  User data
  case 0x64737473: // stsd  Sample description
  case 0x6F637473: // stco  Chunk offset table
  case 0x73747473: // stts  Sample time table
  case 0x63737473: // stsc  Sample->Chunk mapping table
  case 0x7A737473: // stsz  Sample size table
  case 0x746f6e70: // pnot
  case 0x54434950: // PICT
  case 0x70797466:
      break;
  default: lschunks(f,level+1,pos+atom_size);
  }
#else
  switch(atom_type){
  case 0x766F6F6D: // moov
  case 0x61726D72: // rmra
  case 0x61646D72: // rmda
    lschunks(f,level+1,pos+atom_size);
  }
#endif
  fseek(f,pos+atom_size,SEEK_SET);
 }
}

int main(int argc,char* argv[])
{
    FILE *f;
    
    if ((f = fopen(argc>1?argv[1]:"Akira.mov","rb")) == NULL)
	return 1;

    printf("%.8s    %.4s (%.8s) %05s [%s]\n\n",
	"position", "atom", "atomtype", "len", "human readable atom name");

    lschunks(f, 0, 0);

    printf("\nSummary: streams: %d video/%d audio\n", v_stream, a_stream);
}
