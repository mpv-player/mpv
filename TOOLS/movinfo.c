// show QuickTime .mov file structure     (C) 2001. by A'rpi/ESP-team

#include <stdio.h>
#include <stdlib.h>

unsigned int read_dword(FILE *f){
 unsigned char atom_size_b[4];
 if(fread(&atom_size_b,4,1,f)<=0) return -1;
 return (atom_size_b[0]<<24)|(atom_size_b[1]<<16)|(atom_size_b[2]<<8)|atom_size_b[3];
}

void lschunks(FILE *f,int level,unsigned int endpos){
 unsigned int atom_size;
 unsigned int atom_type;
 int pos;
 while(endpos==0 || ftell(f)<endpos){
  pos=ftell(f);
  atom_size=read_dword(f);//  if(fread(&atom_size_b,4,1,f)<=0) break;
  if(fread(&atom_type,4,1,f)<=0) break;
  
  if(atom_size<8) break; // error
  
  printf("%08X:  %*s %.4s (%08X) %d\n",pos,level*2,"",&atom_type,atom_type,atom_size);
  
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
      printf("%5d samples: %d duration\n",num,dur);
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

  
#if 1
  switch(atom_type){
  case 0x75716D72: // rmqu
  case 0x65657266: // free  JUNK
  case 0x64686B74: // tkhd  Track header
  case 0x61746475: // udta  User data
  case 0x7461646D: // mdat  Movie data
  case 0x64737473: // stsd  Sample description
  case 0x6F637473: // stco  Chunk offset table
  case 0x73747473: // stts  Sample time table
  case 0x63737473: // stsc  Sample->Chunk mapping table
  case 0x7A737473: // stsz  Sample size table
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

int main(int argc,char* argv[]){
int pos;
FILE *f=fopen(argc>1?argv[1]:"Akira.mov","rb");
if(!f) return 1;

lschunks(f,0,0);

}
