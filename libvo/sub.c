
#include "sub.h"

//static int vo_font_loaded=-1;
font_desc_t* vo_font=NULL;

unsigned char* vo_osd_text="00:00:00";
int sub_unicode=0;

static void vo_draw_text_osd(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
        int len=strlen(vo_osd_text);
        int j;
        int y=10;
        int x=20;

        for(j=0;j<len;j++){
          int c=vo_osd_text[j];
          int font=vo_font->font[c];
          if(font>=0)
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

static void vo_draw_text_progbar(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
        int i;
        int y=dys/2;
        int x;
        int c,font;
        int width=(dxs*2/3-vo_font->width[0x10]-vo_font->width[0x12]);
        int elems=width/vo_font->width[0x11];
        int mark=(vo_osd_progbar_value*(elems+1))>>8;
        x=(dxs-width)/2;
//        printf("osd.progbar  width=%d  xpos=%d\n",width,x);

        c=vo_osd_progbar_type;font=vo_font->font[c];
        if(vo_osd_progbar_type>0 && font>=0)
            draw_alpha(x-vo_font->width[c]-vo_font->spacewidth,y,
              vo_font->width[c],
              vo_font->pic_a[font]->h,
              vo_font->pic_b[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->w);

        c=OSD_PB_START;font=vo_font->font[c];
        if(font>=0)
            draw_alpha(x,y,
              vo_font->width[c],
              vo_font->pic_a[font]->h,
              vo_font->pic_b[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->w);
        x+=vo_font->width[c];

        for(i=0;i<elems;i++){
          c=(i<mark)?OSD_PB_0:OSD_PB_1;font=vo_font->font[c];
          if(font>=0)
            draw_alpha(x,y,
              vo_font->width[c],
              vo_font->pic_a[font]->h,
              vo_font->pic_b[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->w);
          x+=vo_font->width[c];
        }

        c=OSD_PB_END;font=vo_font->font[c];
        if(font>=0)
            draw_alpha(x,y,
              vo_font->width[c],
              vo_font->pic_a[font]->h,
              vo_font->pic_b[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->w);
//        x+=vo_font->width[c];


//        vo_osd_progbar_value=(vo_osd_progbar_value+1)&0xFF;

}

subtitle* vo_sub=NULL;

static void vo_draw_text_sub(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
    int i;
    int y;
    y=dys-(1+vo_sub->lines-1)*vo_font->height-10;

    // too long lines divide into smaller ones
    for(i=0;i<vo_sub->lines;i++){
        unsigned char* text=vo_sub->text[i];
        int len=strlen(text);
	int j;
        int xsize=-vo_font->charspace;
	int lastStripPosition=-1;
	int previousStrip=0;
	int lastxsize=0;

	for(j=0;j<len;j++){
          int c=text[j];
          int w;
          if (sub_unicode && (c>=0x80)) c=(c<<8)+text[++j];
          w = vo_font->width[c];
	  if (text[j]==' ' && dxs>xsize)
	  {
	    lastStripPosition=j;
	    lastxsize=xsize;
	  }
          xsize+=w+vo_font->charspace;
	  if (dxs<xsize && lastStripPosition>0)
	  {
	    xsize=lastxsize;
	    j=lastStripPosition;
            y-=vo_font->height;
	    previousStrip=lastStripPosition;
            xsize=-vo_font->charspace;
	  }
        }
    }


    for(i=0;i<vo_sub->lines;i++){
        unsigned char* text=vo_sub->text[i];//  "Hello World! HÛDEJÓ!";
        int len=strlen(text);
        int j,k;
        int xsize=-vo_font->charspace;
        int x=0;

	int lastStripPosition=-1;
	int previousStrip=0;
	int lastxsize=xsize;

	for(j=0;j<len;j++){
          int c=text[j];
          int w;
          if (sub_unicode && (c>=0x80)) c=(c<<8)+text[++j];
          w = vo_font->width[c];
          if (c==' ' && dxs>xsize)
	  {
	    lastStripPosition=j;
	    lastxsize=xsize;
	  }
          xsize+=w+vo_font->charspace;
	  if ((dxs<xsize && lastStripPosition>0) || j==len-1)
	  {
	    if (j==len-1) lastStripPosition=len;
	      else xsize=lastxsize;
	    j=lastStripPosition;

            x=dxs/2-xsize/2;

            for(k=previousStrip;k<lastStripPosition;k++){
              int c=text[k];
	      int font;
              if (sub_unicode && (c>=0x80)) c=(c<<8)+text[++k];
              font=vo_font->font[c];
              if(x>=0 && x+vo_font->width[c]<dxs)
                if(font>=0)
                  draw_alpha(x,y,
                    vo_font->width[c],
                    vo_font->pic_a[font]->h,
                    vo_font->pic_b[font]->bmp+vo_font->start[c],
                    vo_font->pic_a[font]->bmp+vo_font->start[c],
                    vo_font->pic_a[font]->w);
              x+=vo_font->width[c]+vo_font->charspace;
            }
            x=0;
            y+=vo_font->height;
	    previousStrip=lastStripPosition;
            xsize=lastxsize=-vo_font->charspace;
	  }
        }
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
