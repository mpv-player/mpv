
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font_load.h"

raw_file* load_raw(char *name){
    int bpp;
    raw_file* raw=malloc(sizeof(raw_file));
    unsigned char head[32];
    FILE *f=fopen(name,"rb");
    if(!f) return NULL;                        // can't open
    if(fread(head,32,1,f)<1) return NULL;        // too small
    if(memcmp(head,"mhwanh",6)) return NULL;        // not raw file
    raw->w=head[8]*256+head[9];
    raw->h=head[10]*256+head[11];
    raw->c=head[12]*256+head[13];
    if(raw->c>256) return NULL;                 // too many colors!?
    printf("RAW: %s  %d x %d, %d colors\n",name,raw->w,raw->h,raw->c);
    if(raw->c){
        raw->pal=malloc(raw->c*3);
        fread(raw->pal,3,raw->c,f);
        bpp=1;
    } else {
        raw->pal=NULL;
        bpp=3;
    }
    raw->bmp=malloc(raw->h*raw->w*bpp);
    fread(raw->bmp,raw->h*raw->w*bpp,1,f);
    fclose(f);
    return raw;
}

font_desc_t* read_font_desc(char* fname){
unsigned char sor[1024];
unsigned char sor2[1024];
font_desc_t *desc;
FILE *f;
char section[64];
int i,j;
int chardb=0;
int fontdb=-1;

desc=malloc(sizeof(font_desc_t));if(!desc) return NULL;
memset(desc,0,sizeof(font_desc_t));

f=fopen(fname,"rt");if(!f){ printf("font: can't open file: %s\n",fname); return NULL;}

// set up some defaults, and erase table
desc->charspace=2;
desc->spacewidth=12;
desc->height=0;
for(i=0;i<512;i++) desc->start[i]=desc->width[i]=desc->font[i]=-1;

section[0]=0;

while(fgets(sor,1020,f)){
  unsigned char* p[8];
  int pdb=0;
  unsigned char *s=sor;
  unsigned char *d=sor2;
  int ec=' ';
  int id=0;
  sor[1020]=0;
  p[0]=d;++pdb;
  while(1){
      int c=*s++;
      if(c==0 || c==13 || c==10) break;
      if(!id){
        if(c==39 || c==34){ id=c;continue;} // idezojel
        if(c==';' || c=='#') break;
        if(c==9) c=' ';
        if(c==' '){
          if(ec==' ') continue;
          *d=0; ++d;
          p[pdb]=d;++pdb;
          if(pdb>=8) break;
          continue;
        }
      } else {
        if(id==c){ id=0;continue;} // idezojel
          
      }
      *d=c;d++;
      ec=c;
  }
  if(d==sor2) continue; // skip empty lines
  *d=0;
  
//  printf("params=%d  sor=%s\n",pdb,sor);
//  for(i=0;i<pdb;i++) printf("  param %d = '%s'\n",i,p[i]);

  if(pdb==1 && p[0][0]=='['){
      int len=strlen(p[0]);
      if(len && len<63 && p[0][len-1]==']'){
        strcpy(section,p[0]);
        printf("font: Reading section: %s\n",section);
        if(strcmp(section,"[files]")==0){
            ++fontdb;
            if(fontdb>=16){ printf("font: Too many bitmaps defined!\n");return NULL;}
        }
        continue;
      }
  }
  
  if(strcmp(section,"[files]")==0){
      if(pdb==2 && strcmp(p[0],"alpha")==0){
          if(!((desc->pic_a[fontdb]=load_raw(p[1])))){
                printf("Can't load font bitmap: %s\n",p[1]);
                return NULL;
          }
          continue;
      }
      if(pdb==2 && strcmp(p[0],"bitmap")==0){
          if(!((desc->pic_b[fontdb]=load_raw(p[1])))){
                printf("Can't load font bitmap: %s\n",p[1]);
                return NULL;
          }
          continue;
      }
  } else

  if(strcmp(section,"[info]")==0){
      if(pdb==2 && strcmp(p[0],"spacewidth")==0){
          desc->spacewidth=atoi(p[1]);
          continue;
      }
      if(pdb==2 && strcmp(p[0],"charspace")==0){
          desc->charspace=atoi(p[1]);
          continue;
      }
      if(pdb==2 && strcmp(p[0],"height")==0){
          desc->height=atoi(p[1]);
          continue;
      }
  } else
  if(strcmp(section,"[characters]")==0){
      if(pdb==3 && strlen(p[0])==1){
          int chr=p[0][0];
          int start=atoi(p[1]);
          int end=atoi(p[2]);
          if(end<start) {
              printf("error in font desc: end<start for char '%c'\n",chr);
          } else {
              desc->start[chr]=start;
              desc->width[chr]=end-start+1;
              desc->font[chr]=fontdb;
//              printf("char %d '%c'  start=%d width=%d\n",chr,chr,desc->start[chr],desc->width[chr]);
              ++chardb;
          }
          continue;
      }
  }
  printf("Syntax error in font desc: %s\n",sor);

}
fclose(f);

//printf("font: pos of U = %d\n",desc->start[218]);

for(i=0;i<=fontdb;i++){
    if(!desc->pic_a[i] || !desc->pic_b[i]){
        printf("font: Missing bitmap(s) for sub-font #%d\n",i);
        return NULL;
    }
    if(!desc->height) desc->height=desc->pic_a[i]->h;
}

j='_';if(desc->font[j]<0) j='?';
for(i=0;i<512;i++)
  if(desc->font[i]<0){
      desc->start[i]=desc->start[j];
      desc->width[i]=desc->width[j];
      desc->font[i]=desc->font[j];
  }
desc->font[' ']=-1;
desc->width[' ']=desc->spacewidth;

printf("font: Font %s loaded successfully! (%d chars)\n",fname,chardb);

return desc;
}

#if 0
int main(){

read_font_desc("high_arpi.desc");

}
#endif
