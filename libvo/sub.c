
#include "sub.h"

//static int vo_font_loaded=-1;
font_desc_t* vo_font=NULL;

unsigned char* vo_osd_text="00:00:00";
int sub_unicode=0;
int sub_utf8=0;

inline static void vo_draw_text_osd(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
	unsigned char *cp=vo_osd_text;
	int c;
   	int font;
        int y=10;
        int x=20;

        while (*cp){
          c=*cp++;
          if ((font=vo_font->font[c])>=0)
            draw_alpha(x,y,
              vo_font->width[c],
              vo_font->pic_a[font]->h,
              vo_font->pic_b[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->w);
          x+=vo_font->width[c]+vo_font->charspace;
        }

}

int vo_osd_progbar_type=-1;
int vo_osd_progbar_value=100;   // 0..255

inline static void vo_draw_text_progbar(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
        int i;
        int y=(dys-vo_font->height)/2;
        int c,font;
        int delimw=vo_font->width[OSD_PB_START]
     		  +vo_font->width[OSD_PB_END]
     		  +vo_font->charspace;
        int width=(2*dxs-3*delimw)/3;
   	int charw=vo_font->width[OSD_PB_0]+vo_font->charspace;
        int elems=width/charw;
   	int x=(dxs-elems*charw-delimw)/2;
        int mark=(vo_osd_progbar_value*(elems+1))>>8;

//        printf("osd.progbar  width=%d  xpos=%d\n",width,x);

        c=vo_osd_progbar_type;
        if(vo_osd_progbar_type>0 && (font=vo_font->font[c])>=0) {
	    int xp=x-vo_font->width[c]-vo_font->spacewidth;
	   draw_alpha((xp<0?0:xp),y,
              vo_font->width[c],
              vo_font->pic_a[font]->h,
              vo_font->pic_b[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->w);
	}
   
        c=OSD_PB_START;
        if ((font=vo_font->font[c])>=0)
            draw_alpha(x,y,
              vo_font->width[c],
              vo_font->pic_a[font]->h,
              vo_font->pic_b[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->w);
        x+=vo_font->width[c]+vo_font->charspace;

   	c=OSD_PB_0;
   	if ((font=vo_font->font[c])>=0)
     	   for (i=mark;i--;){
	       draw_alpha(x,y,
			  vo_font->width[c],
			  vo_font->pic_a[font]->h,
			  vo_font->pic_b[font]->bmp+vo_font->start[c],
			  vo_font->pic_a[font]->bmp+vo_font->start[c],
			  vo_font->pic_a[font]->w);
	       x+=charw;
	   }

   	c=OSD_PB_1;
	if ((font=vo_font->font[c])>=0)
     	   for (i=elems-mark;i--;){
	       draw_alpha(x,y,
			  vo_font->width[c],
			  vo_font->pic_a[font]->h,
			  vo_font->pic_b[font]->bmp+vo_font->start[c],
			  vo_font->pic_a[font]->bmp+vo_font->start[c],
			  vo_font->pic_a[font]->w);
	       x+=charw;
	   }

        c=OSD_PB_END;
        if ((font=vo_font->font[c])>=0)
            draw_alpha(x,y,
              vo_font->width[c],
              vo_font->pic_a[font]->h,
              vo_font->pic_b[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->w);
//        x+=vo_font->width[c]+vo_font->charspace;


//        vo_osd_progbar_value=(vo_osd_progbar_value+1)&0xFF;

}

subtitle* vo_sub=NULL;

#define MAX_UCS 1600
#define MAX_UCSLINES 16

inline static void vo_draw_text_sub(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
   static int utbl[MAX_UCS+1];
   static int xtbl[MAX_UCSLINES];
   static int lines;
   static subtitle *memsub=NULL;
   static int memy;
   static int memdxs;
   static int memdys;
   unsigned char *t;
   int i;
   int j;
   int k;
   int l;
   int x;
   int y;

   int c;
   int len;
   int line;
   int font;
   int lastStripPosition;
   int xsize;
   int lastxsize;
   int lastk;
   
   if ((memsub!=vo_sub)||(memdxs!=dxs)||(memdys!=dys)){
      memsub=vo_sub;
      memdxs=dxs;
      memdys=dys;
      
      memy=dys;
      
      // too long lines divide into smaller ones
      i=k=lines=y=0; l=vo_sub->lines;
      while (l--){
	  t=vo_sub->text[i++];	  
	  len=strlen(t)-1;
	  xsize=lastxsize=-vo_font->charspace;
	  lastStripPosition=-1;

	  for (j=0;j<=len;j++){
	      if ((c=t[j])>=0x80){
		 if (sub_unicode) 
		    c = (c<<8) + t[++j]; 
		 else
		    if (sub_utf8){
		       if ((c & 0xe0) == 0xc0)    /* 2 bytes U+00080..U+0007FF*/
			  c = (c & 0x1f)<<6 | (t[++j] & 0x3f);
		       else if((c & 0xf0) == 0xe0)/* 3 bytes U+00800..U+00FFFF*/
			  c = ((c & 0x0f)<<6 |
			       (t[++j] & 0x3f))<<6 | (t[++j] & 0x3f);
		    }
	      }
	      if (k==MAX_UCS){
		 utbl[k]=l=0;
		 break;
	      } else
	         utbl[k++]=c;
	      if (c==' '){
		 lastk=k;
		 lastStripPosition=j;
		 lastxsize=xsize;
	      } else if (!l && ((font=vo_font->font[c])>=0)){
		  if (vo_font->pic_a[font]->h > y)
		     y=vo_font->pic_a[font]->h;
	      }
	      xsize+=vo_font->width[c]+vo_font->charspace;
	      if (dxs<xsize && lastStripPosition>0){
		 j=lastStripPosition;
		 k=lastk;
		 y=vo_font->height;
	      } else if (j==len){
		 lastxsize=xsize;
	      } else
	         continue;	       	       	       
	      utbl[k++]=0;
	      xtbl[lines++]=(dxs-lastxsize)/2;
	      if (lines==MAX_UCSLINES||k>MAX_UCS){
		 l=0;
		 break;
	      } else if(l || j<len){ // not last line or there is no eol
		 y=vo_font->height;
		 xsize=lastxsize=-vo_font->charspace;
	      } 
	      memy-=y;    // according to max of vo_font->pic_a[font]->h 
	  }		  // in last line
      }
   }
   
   y = memy;

   k=i=0; l=lines;
   while (l--){
      if (y>=0){
	 x= xtbl[i++]; 
	 while ((c=utbl[k++])){
	    if (x>=0 && x+vo_font->width[c]<=dxs)
	       if ((font=vo_font->font[c])>=0)
		  draw_alpha(x,y,
			     vo_font->width[c],
			     vo_font->pic_a[font]->h,
			     vo_font->pic_b[font]->bmp+vo_font->start[c],
			     vo_font->pic_a[font]->bmp+vo_font->start[c],
			     vo_font->pic_a[font]->w);
	    x+=vo_font->width[c]+vo_font->charspace;
	 }
      } else { 
	 while (utbl[k++]) ; // skip lines with negative y value
	 i++;		     // seldom case but who knows ;-)
      }
      y+=vo_font->height;
   }
}



static int draw_alpha_init_flag=0;

extern void vo_draw_alpha_init();

void vo_draw_text(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){

    if(!vo_font) return; // no font

    if(!draw_alpha_init_flag){
	draw_alpha_init_flag=1;
	vo_draw_alpha_init();
    }

    if(vo_osd_text){
        vo_draw_text_osd(dxs,dys,draw_alpha);
    }

    if(vo_sub){
        vo_draw_text_sub(dxs,dys,draw_alpha);
    }
    
    if(vo_osd_progbar_type>=0){
        vo_draw_text_progbar(dxs,dys,draw_alpha);
    }

}
