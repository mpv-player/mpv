
#include "sub.h"

//static int vo_font_loaded=-1;
font_desc_t* vo_font=NULL;

unsigned char* vo_osd_text="00:00:00";

void vo_draw_text_osd(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
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

void vo_draw_text_progbar(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
        int len=strlen(vo_osd_text);
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

void vo_draw_text_sub(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
    int i;
    int y;

    y=dys-(1+vo_sub->lines)*vo_font->height;
    
    for(i=0;i<vo_sub->lines;i++){
        unsigned char* text=vo_sub->text[i];//  "Hello World! HÛDEJÓ!";
        int len=strlen(text);
        int j;
        int xsize=-vo_font->charspace;
        int x=0;

        for(j=0;j<len;j++){
          int c=text[j];
          int w = vo_font->width[(c<0x80)?c:(c<<8)+text[++j]];
          if(w>100) printf("gazvan: %d (%d=%c)\n",w,c,c);
          xsize+=w+vo_font->charspace;
        }
        //printf("text width = %d\n",xsize);
        
        //if(xsize>dxs) printf("Warning! SUB too wide!!! (%d>%d)\n",xsize,dxs);
        
        x=dxs/2-xsize/2;
        
        for(j=0;j<len;j++){
          int c=text[j];
          int font;
          if (c>=0x80) c=(c<<8)+text[++j];
          font = vo_font->font[c];
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

        y+=vo_font->height;
    }

}


void vo_draw_text(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){

    if(!vo_font) return; // no font

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

