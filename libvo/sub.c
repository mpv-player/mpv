
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "mp_msg.h"
#include "video_out.h"
#include "font_load.h"
#include "sub.h"

char * __sub_osd_names[]={
    "Seekbar",
    "Play",
    "Pause",
    "Stop",
    "Rewind",
    "Forward",
    "Clock",
    "Contrast",
    "Saturation",
    "Volume",
    "Brightness",
    "Hue"
};
char * __sub_osd_names_short[] ={ "", "|>", "||", "[]", "<<" , ">>", "", "", "", "", "", ""};

//static int vo_font_loaded=-1;
font_desc_t* vo_font=NULL;

unsigned char* vo_osd_text=NULL;
int sub_unicode=0;
int sub_utf8=0;
int sub_pos=100;

// return the real height of a char:
static inline int get_height(int c,int h){
    int font;
    if ((font=vo_font->font[c])>=0)
	if(h<vo_font->pic_a[font]->h) h=vo_font->pic_a[font]->h;
    return h;
}

inline static void vo_update_text_osd(mp_osd_obj_t* obj,int dxs,int dys){
	unsigned char *cp=vo_osd_text;
	int x=20;
	int h=0;

        obj->bbox.x1=obj->x=x;
        obj->bbox.y1=obj->y=10;

        while (*cp){
          int c=*cp++;
          x+=vo_font->width[c]+vo_font->charspace;
	  h=get_height(c,h);
        }
	
	obj->bbox.x2=x-vo_font->charspace;
	obj->bbox.y2=obj->bbox.y1+h;
	obj->flags|=OSDFLAG_BBOX;

}

inline static void vo_draw_text_osd(mp_osd_obj_t* obj,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
	unsigned char *cp=vo_osd_text;
   	int font;
        int x=obj->x;

        while (*cp){
          int c=*cp++;
          if ((font=vo_font->font[c])>=0)
            draw_alpha(x,obj->y,
              vo_font->width[c],
              vo_font->pic_a[font]->h,
              vo_font->pic_b[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->w);
          x+=vo_font->width[c]+vo_font->charspace;
        }
}

int vo_osd_progbar_type=-1;
int vo_osd_progbar_value=100;   // 0..256

// if we have n=256 bars then OSD progbar looks like below
// 
// 0   1    2    3 ... 256  <= vo_osd_progbar_value
// |   |    |    |       |
// [ ===  ===  === ... === ]
// 
//  the above schema is rescalled to n=elems bars

inline static void vo_update_text_progbar(mp_osd_obj_t* obj,int dxs,int dys){

    obj->flags|=OSDFLAG_CHANGED|OSDFLAG_VISIBLE;
    
    if(vo_osd_progbar_type<0 || !vo_font){
       obj->flags&=~OSDFLAG_VISIBLE;
       return;
    }
    
    {	int h=0;
        int y=(dys-vo_font->height)/2;
        int delimw=vo_font->width[OSD_PB_START]
     		  +vo_font->width[OSD_PB_END]
     		  +vo_font->charspace;
        int width=(2*dxs-3*delimw)/3;
   	int charw=vo_font->width[OSD_PB_0]+vo_font->charspace;
        int elems=width/charw;
   	int x=(dxs-elems*charw-delimw)/2;
	h=get_height(OSD_PB_START,h);
	h=get_height(OSD_PB_END,h);
	h=get_height(OSD_PB_0,h);
	h=get_height(OSD_PB_1,h);
	obj->bbox.x1=obj->x=x;
	obj->bbox.y1=obj->y=y;
	obj->bbox.x2=x+width+delimw;
	obj->bbox.y2=y+h; //vo_font->height;
	obj->flags|=OSDFLAG_BBOX;
	obj->params.progbar.elems=elems;
    }
    
}

inline static void vo_draw_text_progbar(mp_osd_obj_t* obj,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
   	unsigned char *s;
   	unsigned char *sa;
        int i,w,h,st,mark;
   	int x=obj->x;
        int y=obj->y;
        int c,font;
   	int charw=vo_font->width[OSD_PB_0]+vo_font->charspace;
        int elems=obj->params.progbar.elems;

	if (vo_osd_progbar_value<=0)
     	   mark=0;
	else {
	   int ev=vo_osd_progbar_value*elems;
	   mark=ev>>8;
	   if (ev & 0xFF)  mark++;
	   if (mark>elems) mark=elems;
	}
   

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
   	if ((font=vo_font->font[c])>=0){
	   w=vo_font->width[c];
	   h=vo_font->pic_a[font]->h;
	   s=vo_font->pic_b[font]->bmp+vo_font->start[c];
	   sa=vo_font->pic_a[font]->bmp+vo_font->start[c];
	   st=vo_font->pic_a[font]->w;
	   if ((i=mark)) do {
	       draw_alpha(x,y,w,h,s,sa,st);
	       x+=charw;
	   } while(--i);
	}

   	c=OSD_PB_1;
	if ((font=vo_font->font[c])>=0){
	   w=vo_font->width[c];
	   h=vo_font->pic_a[font]->h;
	   s =vo_font->pic_b[font]->bmp+vo_font->start[c];
	   sa=vo_font->pic_a[font]->bmp+vo_font->start[c];
	   st=vo_font->pic_a[font]->w;
	   if ((i=elems-mark)) do {
	       draw_alpha(x,y,w,h,s,sa,st);
	       x+=charw;
	   } while(--i);
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

// vo_draw_text_sub(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride))

inline static void vo_update_text_sub(mp_osd_obj_t* obj,int dxs,int dys){
   unsigned char *t;
   int c,i,j,l,x,y,font;
   int len;
   int k,lastk;
   int lastStripPosition;
   int xsize,lastxsize;
   int xmin=dxs,xmax=0;
   int h,lasth;
   
   obj->flags|=OSDFLAG_CHANGED|OSDFLAG_VISIBLE;
   
   if(!vo_sub || !vo_font){
       obj->flags&=~OSDFLAG_VISIBLE;
       return;
   }
   
   obj->bbox.y2=obj->y=dys;
   obj->params.subtitle.lines=0;

      // too long lines divide into a smaller ones
      i=k=lasth=0;
      h=vo_font->height;
      xsize=-vo_font->charspace;
      lastStripPosition=-1;
      l=vo_sub->lines;

      while (l) {
	  l--;
	  t=vo_sub->text[i++];	  
	  len=strlen(t)-1;
	  
//	  printf("sub(%d) '%s'\n",len,t);
//	  if(len<0) memy -=h; // according to max of vo_font->pic_a[font]->h 
//	  else
	  for (j=0;j<=len;j++){
	      if ((c=t[j])>=0x80){
		 if (sub_utf8){
		    if ((c & 0xe0) == 0xc0)    /* 2 bytes U+00080..U+0007FF*/
		       c = (c & 0x1f)<<6 | (t[++j] & 0x3f);
		    else if((c & 0xf0) == 0xe0)/* 3 bytes U+00800..U+00FFFF*/
		       c = ((c & 0x0f)<<6 |
			    (t[++j] & 0x3f))<<6 | (t[++j] & 0x3f);
		 } else if (sub_unicode) 
		       c = (c<<8) + t[++j]; 
	      }
	      if (k==MAX_UCS){
		 len=j; // end here
		 mp_msg(MSGT_OSD,MSGL_WARN,"\nMAX_UCS exceeded!\n");
	      }
	      if (!c) c++; // avoid UCS 0
	      if (c==' '){
		 lastk=k;
		 lastStripPosition=j;
		 lastxsize=xsize;
	      } else if ((font=vo_font->font[c])>=0){
		  if (vo_font->pic_a[font]->h > h){
		     h=vo_font->pic_a[font]->h;
		  }
	      }
	      obj->params.subtitle.utbl[k++]=c;
	      xsize+=vo_font->width[c]+vo_font->charspace;
	      if (dxs<xsize){
		 if (lastStripPosition>0){
		    j=lastStripPosition;
		    xsize=lastxsize;
		    k=lastk;
		 } else {
		    xsize -=vo_font->width[c]+vo_font->charspace; // go back
		    k--; // cut line here
		    while (t[j] && t[j]!=' ') j++; // jump to the nearest space
		 }
	      } else if (j<len)
		   continue;
	      if (h>obj->y){ // out of the screen so end parsing
		 obj->y -= lasth - vo_font->height; // correct the y position
		 l=0;
		 break;
	      }
	      obj->params.subtitle.utbl[k++]=0;
	      obj->params.subtitle.xtbl[obj->params.subtitle.lines++]=(dxs-xsize)/2;
	      if(xmin>(dxs-xsize)/2) xmin=(dxs-xsize)/2;
	      if(xmax<(dxs+xsize)/2) xmax=(dxs+xsize)/2;
	      if (obj->params.subtitle.lines==MAX_UCSLINES||k>MAX_UCS){
		 l=0; len=j; // end parsing
	      } else if(l || j<len){ // not the last line or not the last char
		 lastStripPosition=-1;
		 xsize=-vo_font->charspace;
		 lasth=h;
		 h=vo_font->height;
	      }
//	      printf("h: %d -> %d  \n",vo_font->height,h);
	      obj->y -=h; // according to max of vo_font->pic_a[font]->h 
	  }
      }

    if (obj->y >= (dys * sub_pos / 100)){
	int old=obj->y;
	obj->y = dys * sub_pos /100;
	obj->bbox.y2-=old-obj->y;
    }
    
    // calculate bbox:
    obj->bbox.x1=xmin;
    obj->bbox.x2=xmax;
    obj->bbox.y1=obj->y;
//    obj->bbox.y2=obj->y+obj->params.subtitle.lines*vo_font->height;
    obj->flags|=OSDFLAG_BBOX;
    
}

inline static void vo_draw_text_sub(mp_osd_obj_t* obj,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
   int i,j,c,x,l,font;
   int y=obj->y;

   i=j=0;
   if ((l=obj->params.subtitle.lines)) for (;;) {
 	 x=obj->params.subtitle.xtbl[i++]; 
	 while ((c=obj->params.subtitle.utbl[j++])){
	       if ((font=vo_font->font[c])>=0)
		  draw_alpha(x,y,
			     vo_font->width[c],
			     vo_font->pic_a[font]->h+y<obj->dys ? vo_font->pic_a[font]->h : obj->dys-y,
			     vo_font->pic_b[font]->bmp+vo_font->start[c],
			     vo_font->pic_a[font]->bmp+vo_font->start[c],
			     vo_font->pic_a[font]->w);
	          x+=vo_font->width[c]+vo_font->charspace;
	 }
         if (!--l) break;
         y+=vo_font->height;
   }
}

void *vo_spudec=NULL;
void *vo_vobsub=NULL;

static int draw_alpha_init_flag=0;

extern void vo_draw_alpha_init();

static mp_osd_obj_t* vo_osd_list=NULL;

mp_osd_obj_t* new_osd_obj(int type){
    mp_osd_obj_t* osd=malloc(sizeof(mp_osd_obj_t));
    memset(osd,0,sizeof(mp_osd_obj_t));
    osd->next=vo_osd_list;
    vo_osd_list=osd;
    osd->type=type;
    return osd;
}

void free_osd_list(){
    mp_osd_obj_t* obj=vo_osd_list;
    while(obj){
	mp_osd_obj_t* next=obj->next;
	free(obj);
	obj=next;
    }
    vo_osd_list=NULL;
}

int vo_update_osd(int dxs,int dys){
    mp_osd_obj_t* obj=vo_osd_list;
    int chg=0;
    while(obj){
      if(dxs!=obj->dxs || dys!=obj->dys || obj->flags&OSDFLAG_FORCE_UPDATE){
        int vis=obj->flags&OSDFLAG_VISIBLE;
	obj->flags&=~OSDFLAG_BBOX;
	switch(obj->type){
	case OSDTYPE_SUBTITLE:
	    vo_update_text_sub(obj,dxs,dys);
	    break;
	case OSDTYPE_PROGBAR:
	    vo_update_text_progbar(obj,dxs,dys);
	    break;
	case OSDTYPE_SPU:
	    if(vo_spudec && spudec_visible(vo_spudec))
		obj->flags|=OSDFLAG_VISIBLE|OSDFLAG_CHANGED;
	    else
		obj->flags&=~OSDFLAG_VISIBLE;
	    break;
	case OSDTYPE_OSD:
	    if(vo_font && vo_osd_text && vo_osd_text[0]){
		vo_update_text_osd(obj,dxs,dys); // update bbox
		obj->flags|=OSDFLAG_VISIBLE|OSDFLAG_CHANGED;
	    } else
		obj->flags&=~OSDFLAG_VISIBLE;
	    break;
	}
	// check bbox:
	if(!(obj->flags&OSDFLAG_BBOX)){
	    // we don't know, so assume the whole screen changed :(
	    obj->bbox.x1=obj->bbox.y1=0;
	    obj->bbox.x2=dxs;
	    obj->bbox.y2=dys;
	    obj->flags|=OSDFLAG_BBOX;
	} else {
	    // check bbox, reduce it if it's out of bounds (corners):
	    if(obj->bbox.x1<0) obj->bbox.x1=0;
	    if(obj->bbox.y1<0) obj->bbox.y1=0;
	    if(obj->bbox.x2>dxs) obj->bbox.x2=dxs;
	    if(obj->bbox.y2>dys) obj->bbox.y2=dys;
	    if(obj->flags&OSDFLAG_VISIBLE)
	    // debug:
	    mp_msg(MSGT_OSD,MSGL_DBG2,"OSD update: %d;%d %dx%d  \n",
		obj->bbox.x1,obj->bbox.y1,obj->bbox.x2-obj->bbox.x1,
		obj->bbox.y2-obj->bbox.y1);
	}
	// check if visibility changed:
	if(vis != (obj->flags&OSDFLAG_VISIBLE) ) obj->flags|=OSDFLAG_CHANGED;
	// remove the cause of automatic update:
	obj->dxs=dxs; obj->dys=dys;
	obj->flags&=~OSDFLAG_FORCE_UPDATE;
      }
      if(obj->flags&OSDFLAG_CHANGED){
        chg|=1<<obj->type;
	mp_msg(MSGT_OSD,MSGL_DBG2,"OSD chg: %d  V: %s  pb:%d  \n",obj->type,(obj->flags&OSDFLAG_VISIBLE)?"yes":"no",vo_osd_progbar_type);
      }
      obj=obj->next;
    }
    return chg;
}

void vo_init_osd(){
    if(!draw_alpha_init_flag){
	draw_alpha_init_flag=1;
	vo_draw_alpha_init();
    }
    if(vo_osd_list) free_osd_list();
    // temp hack, should be moved to mplayer/mencoder later
    new_osd_obj(OSDTYPE_OSD);
    new_osd_obj(OSDTYPE_SUBTITLE);
    new_osd_obj(OSDTYPE_PROGBAR);
    new_osd_obj(OSDTYPE_SPU);
}

int vo_osd_changed_flag=0;

void vo_remove_text(int dxs,int dys,void (*remove)(int x0,int y0, int w,int h)){
    mp_osd_obj_t* obj=vo_osd_list;
    vo_update_osd(dxs,dys);
    while(obj){
      if(((obj->flags&OSDFLAG_CHANGED) || (obj->flags&OSDFLAG_VISIBLE)) && 
         (obj->flags&OSDFLAG_OLD_BBOX)){
          int w=obj->old_bbox.x2-obj->old_bbox.x1;
	  int h=obj->old_bbox.y2-obj->old_bbox.y1;
	  if(w>0 && h>0){
	      vo_osd_changed_flag=obj->flags&OSDFLAG_CHANGED;	// temp hack
              remove(obj->old_bbox.x1,obj->old_bbox.y1,w,h);
	  }
//	  obj->flags&=~OSDFLAG_OLD_BBOX;
      }
      obj=obj->next;
    }
}

void vo_draw_text(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
    mp_osd_obj_t* obj=vo_osd_list;
    vo_update_osd(dxs,dys);
    while(obj){
      if(obj->flags&OSDFLAG_VISIBLE){
	vo_osd_changed_flag=obj->flags&OSDFLAG_CHANGED;	// temp hack
	switch(obj->type){
	case OSDTYPE_SPU:
	    spudec_draw_scaled(vo_spudec, dxs, dys, draw_alpha); // FIXME
	    break;
	case OSDTYPE_OSD:
	    vo_draw_text_osd(obj,draw_alpha);
	    break;
	case OSDTYPE_SUBTITLE:
	    vo_draw_text_sub(obj,draw_alpha);
	    break;
	case OSDTYPE_PROGBAR:
	    vo_draw_text_progbar(obj,draw_alpha);
	    break;
	}
	obj->old_bbox=obj->bbox;
	obj->flags|=OSDFLAG_OLD_BBOX;
      }
      obj->flags&=~OSDFLAG_CHANGED;
      obj=obj->next;
    }
}

static int vo_osd_changed_status = 0;

int vo_osd_changed(int new_value)
{
    mp_osd_obj_t* obj=vo_osd_list;
    int ret = vo_osd_changed_status;
    vo_osd_changed_status = new_value;

    while(obj){
	if(obj->type==new_value) obj->flags|=OSDFLAG_FORCE_UPDATE;
	obj=obj->next;
    }

    return ret;
}

//      BBBBBBBBBBBB   AAAAAAAAAAAAA  BBBBBBBBBBB
//              BBBBBBBBBBBB  BBBBBBBBBBBBB
//                        BBBBBBB

// return TRUE if we have osd in the specified rectangular area:
int vo_osd_check_range_update(int x1,int y1,int x2,int y2){
    mp_osd_obj_t* obj=vo_osd_list;
    while(obj){
	if(obj->flags&OSDFLAG_VISIBLE){
	    if(	(obj->bbox.x1<=x2 && obj->bbox.x2>=x1) &&
		(obj->bbox.y1<=y2 && obj->bbox.y2>=y1) ) return 1;
	}
	obj=obj->next;
    }
    return 0;
}
