#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "font_load.h"
#include "mp_msg.h"

extern char *get_path ( char * );

raw_file* load_raw(char *name,int verbose){
    int bpp;
    raw_file* raw=malloc(sizeof(raw_file));
    unsigned char head[32];
    FILE *f=fopen(name,"rb");
    if(!f) goto err_out;                        // can't open
    if(fread(head,32,1,f)<1) goto err_out;        // too small
    if(memcmp(head,"mhwanh",6)) goto err_out;        // not raw file
    raw->w=head[8]*256+head[9];
    raw->h=head[10]*256+head[11];
    raw->c=head[12]*256+head[13];
    if(raw->w == 0) // 2 bytes were not enough for the width... read 4 bytes from the end of the header
    	raw->w = ((head[28]*0x100 + head[29])*0x100 + head[30])*0x100 + head[31];
    if(raw->c>256) goto err_out;                 // too many colors!?
    mp_msg(MSGT_OSD, MSGL_DBG2, "RAW: %s  %d x %d, %d colors\n",name,raw->w,raw->h,raw->c);
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

err_out:
    if (f)
      fclose(f);
    free(raw);
    return NULL;
}

extern int sub_unicode;

font_desc_t* read_font_desc(const char* fname,float factor,int verbose){
unsigned char sor[1024];
unsigned char sor2[1024];
font_desc_t *desc;
FILE *f = NULL;
char *dn;
struct stat fstate;
char section[64];
int i,j;
int chardb=0;
int fontdb=-1;
int version=0;
int first=1;

desc=malloc(sizeof(font_desc_t));if(!desc) goto fail_out;
memset(desc,0,sizeof(font_desc_t));

f=fopen(fname,"rt");if(!f){ mp_msg(MSGT_OSD, MSGL_V, "font: can't open file: %s\n",fname); goto fail_out;}

i = strlen (fname) - 9;
if ((dn = malloc(i+1))){
   strncpy (dn, fname, i);
   dn[i]='\0';
}

desc->fpath = dn; // search in the same dir as fonts.desc
	
// desc->fpath=get_path("font/");
// if (stat(desc->fpath, &fstate)!=0) desc->fpath=DATADIR"/font";

	
	
	
// set up some defaults, and erase table
desc->charspace=2;
desc->spacewidth=12;
desc->height=0;
for(i=0;i<65536;i++) desc->start[i]=desc->width[i]=desc->font[i]=-1;

section[0]=0;

while(fgets(sor,1020,f)){
  unsigned char* p[8];
  int pdb=0;
  unsigned char *s=sor;
  unsigned char *d=sor2;
  int ec=' ';
  int id=0;
  sor[1020]=0;

  /* skip files that look like: TTF (0x00, 0x01), PFM (0x00, 0x01), PFB
   * (0x80, 0x01), PCF (0x01, 0x66), fon ("MZ"), gzipped (0x1f, 0x8b) */
  
  if (first) {
    if (!sor[0] || sor[1] == 1 || (sor[0] == 'M' && sor[1] == 'Z') || (sor[0] == 0x1f && sor[1] == 0x8b) || (sor[0] == 1 && sor[1] == 0x66)) {
      mp_msg(MSGT_OSD, MSGL_ERR, "%s doesn't look like a bitmap font description, ignoring.\n", fname);
      goto fail_out;
    }
    first = 0;
  }
  
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
        mp_msg(MSGT_OSD, MSGL_DBG2, "font: Reading section: %s\n",section);
        if(strcmp(section,"[files]")==0){
            ++fontdb;
            if(fontdb>=16){ mp_msg(MSGT_OSD, MSGL_ERR, "font: Too many bitmaps defined.\n");goto fail_out;}
        }
        continue;
      }
  }
  
  if(strcmp(section,"[fpath]")==0){
      if(pdb==1){
	  if (desc->fpath)
	     free (desc->fpath); // release previously allocated memory
          desc->fpath=strdup(p[0]);
          continue;
      }
  } else    

#ifdef SYS_AMIGAOS4
#define FONT_PATH_SEP ""
#else
//! path seperator for font paths, may not be more than one character
#define FONT_PATH_SEP "/"
#endif

  if(strcmp(section,"[files]")==0){
      char *default_dir=MPLAYER_DATADIR FONT_PATH_SEP "font";
      if(pdb==2 && strcmp(p[0],"alpha")==0){
    	  char *cp;
	  if (!(cp=malloc(strlen(desc->fpath)+strlen(p[1])+2))) goto fail_out;

	  snprintf(cp,strlen(desc->fpath)+strlen(p[1])+2,"%s" FONT_PATH_SEP "%s",
		desc->fpath,p[1]);
          if(!((desc->pic_a[fontdb]=load_raw(cp,verbose)))){
		free(cp);
		if (!(cp=malloc(strlen(default_dir)+strlen(p[1])+2))) 
		   goto fail_out;
		snprintf(cp,strlen(default_dir)+strlen(p[1])+2,"%s" FONT_PATH_SEP "%s",
			 default_dir,p[1]);
		if (!((desc->pic_a[fontdb]=load_raw(cp,verbose)))){
		   mp_msg(MSGT_OSD, MSGL_ERR, "Can't load font bitmap: %s\n",p[1]);
		   free(cp);
		   goto fail_out;
		}
          }
	  free(cp);
          continue;
      }
      if(pdb==2 && strcmp(p[0],"bitmap")==0){
    	  char *cp;
	  if (!(cp=malloc(strlen(desc->fpath)+strlen(p[1])+2))) goto fail_out;

	  snprintf(cp,strlen(desc->fpath)+strlen(p[1])+2,"%s" FONT_PATH_SEP "%s",
		desc->fpath,p[1]);
          if(!((desc->pic_b[fontdb]=load_raw(cp,verbose)))){
		free(cp);
		if (!(cp=malloc(strlen(default_dir)+strlen(p[1])+2))) 
		   goto fail_out;
		snprintf(cp,strlen(default_dir)+strlen(p[1])+2,"%s" FONT_PATH_SEP "%s",
			 default_dir,p[1]);
		if (!((desc->pic_b[fontdb]=load_raw(cp,verbose)))){
		   mp_msg(MSGT_OSD, MSGL_ERR, "Can't load font bitmap: %s\n",p[1]);
		   free(cp);
		   goto fail_out;
		}
          }
	  free(cp);
          continue;
      }
  } else

  if(strcmp(section,"[info]")==0){
      if(pdb==2 && strcmp(p[0],"name")==0){
          desc->name=strdup(p[1]);
          continue;
      }
      if(pdb==2 && strcmp(p[0],"descversion")==0){
          version=atoi(p[1]);
          continue;
      }
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
      if(pdb==3){
          int chr=p[0][0];
          int start=atoi(p[1]);
          int end=atoi(p[2]);
          if(sub_unicode && (chr>=0x80)) chr=(chr<<8)+p[0][1];
          else if(strlen(p[0])!=1) chr=strtol(p[0],NULL,0);
          if(end<start) {
              mp_msg(MSGT_OSD, MSGL_WARN, "error in font desc: end<start for char '%c'\n",chr);
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
  mp_msg(MSGT_OSD, MSGL_ERR, "Syntax error in font desc: %s",sor);
  goto fail_out;

}
fclose(f);
f = NULL;

 if (first == 1) {
   mp_msg(MSGT_OSD, MSGL_ERR, "%s is empty or a directory, ignoring.\n", fname);
   goto fail_out;
 }

//printf("font: pos of U = %d\n",desc->start[218]);

for(i=0;i<=fontdb;i++){
    if(!desc->pic_a[i] || !desc->pic_b[i]){
        mp_msg(MSGT_OSD, MSGL_ERR, "font: Missing bitmap(s) for sub-font #%d\n",i);
        goto fail_out;
    }
    //if(factor!=1.0f)
    {
        // re-sample alpha
        int f=factor*256.0f;
        int size=desc->pic_a[i]->w*desc->pic_a[i]->h;
        int j;
        mp_msg(MSGT_OSD, MSGL_DBG2, "font: resampling alpha by factor %5.3f (%d) ",factor,f);fflush(stdout);
        for(j=0;j<size;j++){
            int x=desc->pic_a[i]->bmp[j];	// alpha
            int y=desc->pic_b[i]->bmp[j];	// bitmap

#ifdef FAST_OSD
	    x=(x<(255-f))?0:1;
#else

	    x=255-((x*f)>>8); // scale
	    //if(x<0) x=0; else if(x>255) x=255;
	    //x^=255; // invert

	    if(x+y>255) x=255-y; // to avoid overflows
	    
	    //x=0;            
            //x=((x*f*(255-y))>>16);
            //x=((x*f*(255-y))>>16)+y;
            //x=(x*f)>>8;if(x<y) x=y;

            if(x<1) x=1; else
            if(x>=252) x=0;
#endif

            desc->pic_a[i]->bmp[j]=x;
//            desc->pic_b[i]->bmp[j]=0; // hack
        }
        mp_msg(MSGT_OSD, MSGL_DBG2, "DONE!\n");
    }
    if(!desc->height) desc->height=desc->pic_a[i]->h;
}

j='_';if(desc->font[j]<0) j='?';
for(i=0;i<65536;i++)
  if(desc->font[i]<0){
      desc->start[i]=desc->start[j];
      desc->width[i]=desc->width[j];
      desc->font[i]=desc->font[j];
  }
desc->font[' ']=-1;
desc->width[' ']=desc->spacewidth;

mp_msg(MSGT_OSD, MSGL_V, "Bitmap font %s loaded successfully! (%d chars)\n",fname,chardb);

return desc;

fail_out:
  if (f)
    fclose(f);
  if (desc->fpath)
    free(desc->fpath);
  if (desc->name)
    free(desc->name);
  if (desc)
    free(desc);
  return NULL;
}

#if 0
int main(){

read_font_desc("high_arpi.desc",1);

}
#endif

