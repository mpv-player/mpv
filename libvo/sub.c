
//static int vo_font_loaded=-1;
font_desc_t* vo_font=NULL;

int vo_sub_lines=2;
unsigned char* vo_sub_text[8];

unsigned char* vo_osd_text="00:00:00";

void vo_draw_text(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
    int i;
    int y;

    if(!vo_font) return; // no font

    if(vo_osd_text){
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

#if 1

    if(vo_sub_lines<=0) return; // no text
    y=dys-(1+vo_sub_lines)*vo_font->height;
    
    for(i=0;i<vo_sub_lines;i++){
        unsigned char* text="Hello World! HÛDEJÓ!"; //vo_sub_text[i];
        int len=strlen(text);
        int j;
        int xsize=-vo_font->charspace;
        int x=0;

        for(j=0;j<len;j++){
          int w=vo_font->width[text[j]];
          if(w>100) printf("gazvan: %d (%d=%c)\n",w,text[j],text[j]);
          xsize+=w+vo_font->charspace;
        }
        //printf("text width = %d\n",xsize);
        
        if(xsize>dxs) printf("Warning! SUB too wide!!! (%d>%d)\n",xsize,dxs);
        
        x=dxs/2-xsize/2;
        
        for(j=0;j<len;j++){
          int c=text[j];
          int font=vo_font->font[c];
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

#endif

}



