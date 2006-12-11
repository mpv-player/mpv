
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef USE_DVDNAV
#include "stream/stream.h"
#include "stream/stream_dvdnav.h"
#define OSD_NAV_BOX_ALPHA 0x7f
#endif

#include "mp_msg.h"
#include "help_mp.h"
#include "video_out.h"
#include "font_load.h"
#include "sub.h"
#include "spudec.h"
#include "libavutil/common.h"

#define NEW_SPLITTING


// Structures needed for the new splitting algorithm.
// osd_text_t contains the single subtitle word.
// osd_text_p is used to mark the lines of subtitles
struct osd_text_t {
    int osd_kerning, //kerning with the previous word
	osd_length,  //orizontal length inside the bbox
	text_length, //number of characters
	*text;       //characters
    struct osd_text_t *prev,
                      *next;
};

struct osd_text_p {
    int  value;
    struct osd_text_t *ott;
    struct osd_text_p *prev,
                      *next;
};
//^

char * __sub_osd_names[]={
    MSGTR_VO_SUB_Seekbar,
    MSGTR_VO_SUB_Play,
    MSGTR_VO_SUB_Pause,
    MSGTR_VO_SUB_Stop,
    MSGTR_VO_SUB_Rewind,
    MSGTR_VO_SUB_Forward,
    MSGTR_VO_SUB_Clock,
    MSGTR_VO_SUB_Contrast,
    MSGTR_VO_SUB_Saturation,
    MSGTR_VO_SUB_Volume,
    MSGTR_VO_SUB_Brightness,
    MSGTR_VO_SUB_Hue
};
char * __sub_osd_names_short[] ={ "", "|>", "||", "[]", "<<" , ">>", "", "", "", "", "", ""};

//static int vo_font_loaded=-1;
font_desc_t* vo_font=NULL;

unsigned char* vo_osd_text=NULL;
int sub_unicode=0;
int sub_utf8=0;
int sub_pos=100;
int sub_width_p=100;
int sub_alignment=2; /* 0=top, 1=center, 2=bottom */
int sub_visibility=1;
int sub_bg_color=0; /* subtitles background color */
int sub_bg_alpha=0;
int sub_justify=0;
#ifdef USE_DVDNAV
static nav_highlight_t nav_hl;
#endif

// return the real height of a char:
static inline int get_height(int c,int h){
    int font;
    if ((font=vo_font->font[c])>=0)
	if(h<vo_font->pic_a[font]->h) h=vo_font->pic_a[font]->h;
    return h;
}

// renders char to a big per-object buffer where alpha and bitmap are separated
static void draw_alpha_buf(mp_osd_obj_t* obj, int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
    int dststride = obj->stride;
    int dstskip = obj->stride-w;
    int srcskip = stride-w;
    int i, j;
    unsigned char *b = obj->bitmap_buffer + (y0-obj->bbox.y1)*dststride + (x0-obj->bbox.x1);
    unsigned char *a = obj->alpha_buffer  + (y0-obj->bbox.y1)*dststride + (x0-obj->bbox.x1);
    unsigned char *bs = src;
    unsigned char *as = srca;

    if (x0 < obj->bbox.x1 || x0+w > obj->bbox.x2 || y0 < obj->bbox.y1 || y0+h > obj->bbox.y2) {
	fprintf(stderr, "osd text out of range: bbox [%d %d %d %d], txt [%d %d %d %d]\n",
		obj->bbox.x1, obj->bbox.x2, obj->bbox.y1, obj->bbox.y2,
		x0, x0+w, y0, y0+h);
	return;
    }
    
    for (i = 0; i < h; i++) {
	for (j = 0; j < w; j++, b++, a++, bs++, as++) {
	    if (*b < *bs) *b = *bs;
	    if (*as) {
		if (*a == 0 || *a > *as) *a = *as;
	    }
	}
	b+= dstskip;
	a+= dstskip;
	bs+= srcskip;
	as+= srcskip;
    }
}

// allocates/enlarges the alpha/bitmap buffer
static void alloc_buf(mp_osd_obj_t* obj)
{
    int len;
    if (obj->bbox.x2 < obj->bbox.x1) obj->bbox.x2 = obj->bbox.x1;
    if (obj->bbox.y2 < obj->bbox.y1) obj->bbox.y2 = obj->bbox.y1;
    obj->stride = ((obj->bbox.x2-obj->bbox.x1)+7)&(~7);
    len = obj->stride*(obj->bbox.y2-obj->bbox.y1);
    if (obj->allocated<len) {
	obj->allocated = len;
	free(obj->bitmap_buffer);
	free(obj->alpha_buffer);
	obj->bitmap_buffer = (unsigned char *)memalign(16, len);
	obj->alpha_buffer = (unsigned char *)memalign(16, len);
    }
    memset(obj->bitmap_buffer, sub_bg_color, len);
    memset(obj->alpha_buffer, sub_bg_alpha, len);
}

// renders the buffer
inline static void vo_draw_text_from_buffer(mp_osd_obj_t* obj,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
    if (obj->allocated > 0) {
	draw_alpha(obj->bbox.x1,obj->bbox.y1,
		   obj->bbox.x2-obj->bbox.x1,
		   obj->bbox.y2-obj->bbox.y1,
		   obj->bitmap_buffer,
		   obj->alpha_buffer,
		   obj->stride);
    }
}

unsigned utf8_get_char(const char **str) {
  const uint8_t *strp = (const uint8_t *)*str;
  unsigned c;
  GET_UTF8(c, *strp++, goto no_utf8;);
  *str = (const char *)strp;
  return c;

no_utf8:
  strp = (const uint8_t *)*str;
  c = *strp++;
  *str = (const char *)strp;
  return c;
}

inline static void vo_update_text_osd(mp_osd_obj_t* obj,int dxs,int dys){
	const char *cp=vo_osd_text;
	int x=20;
	int h=0;
	int font;

        obj->bbox.x1=obj->x=x;
        obj->bbox.y1=obj->y=10;

        while (*cp){
          uint16_t c=utf8_get_char(&cp);
	  render_one_glyph(vo_font, c);
	  x+=vo_font->width[c]+vo_font->charspace;
	  h=get_height(c,h);
        }
	
	obj->bbox.x2=x-vo_font->charspace;
	obj->bbox.y2=obj->bbox.y1+h;
	obj->flags|=OSDFLAG_BBOX;

	alloc_buf(obj);

	cp=vo_osd_text;
	x = obj->x;
        while (*cp){
          uint16_t c=utf8_get_char(&cp);
          if ((font=vo_font->font[c])>=0)
            draw_alpha_buf(obj,x,obj->y,
			   vo_font->width[c],
			   vo_font->pic_a[font]->h,
			   vo_font->pic_b[font]->bmp+vo_font->start[c],
			   vo_font->pic_a[font]->bmp+vo_font->start[c],
			   vo_font->pic_a[font]->w);
          x+=vo_font->width[c]+vo_font->charspace;
        }
}

#ifdef USE_DVDNAV
void osd_set_nav_box (uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey) {
  nav_hl.sx = sx;
  nav_hl.sy = sy;
  nav_hl.ex = ex;
  nav_hl.ey = ey;
}

inline static void vo_update_nav (mp_osd_obj_t *obj, int dxs, int dys) {
  int len;

  obj->bbox.x1 = obj->x = nav_hl.sx;
  obj->bbox.y1 = obj->y = nav_hl.sy;
  obj->bbox.x2 = nav_hl.ex;
  obj->bbox.y2 = nav_hl.ey;
  
  alloc_buf (obj);
  len = obj->stride * (obj->bbox.y2 - obj->bbox.y1);
  memset (obj->bitmap_buffer, OSD_NAV_BOX_ALPHA, len);
  memset (obj->alpha_buffer, OSD_NAV_BOX_ALPHA, len);
  obj->flags |= OSDFLAG_BBOX | OSDFLAG_CHANGED;
  if (obj->bbox.y2 > obj->bbox.y1 && obj->bbox.x2 > obj->bbox.x1)
    obj->flags |= OSDFLAG_VISIBLE;
}
#endif

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
    
    render_one_glyph(vo_font, OSD_PB_START);
    render_one_glyph(vo_font, OSD_PB_END);
    render_one_glyph(vo_font, OSD_PB_0);
    render_one_glyph(vo_font, OSD_PB_1);
    render_one_glyph(vo_font, vo_osd_progbar_type);

    // calculate bbox corners:    
    {	int h=0;
        int y=(dys-vo_font->height)/2;
        int delimw=vo_font->width[OSD_PB_START]
     		  +vo_font->width[OSD_PB_END]
     		  +vo_font->charspace;
        int width=(2*dxs-3*delimw)/3;
   	int charw=vo_font->width[OSD_PB_0]+vo_font->charspace;
        int elems=width/charw;
   	int x=(dxs-elems*charw-delimw)/2;
	int delta = 0;
	h=get_height(OSD_PB_START,h);
	h=get_height(OSD_PB_END,h);
	h=get_height(OSD_PB_0,h);
	h=get_height(OSD_PB_1,h);
	if (vo_osd_progbar_type>0 && vo_font->font[vo_osd_progbar_type]>=0){
	    delta = vo_font->width[vo_osd_progbar_type]+vo_font->spacewidth;
	    delta = (x-delta > 0) ? delta : x;
	    h=get_height(vo_osd_progbar_type,h);
	}
	obj->bbox.x1=obj->x=x;
	obj->bbox.y1=obj->y=y;
	obj->bbox.x2=x+width+delimw;
	obj->bbox.y2=y+h; //vo_font->height;
	obj->flags|=OSDFLAG_BBOX;
	obj->params.progbar.elems=elems;
	obj->bbox.x1-=delta; // space for an icon
    }

    alloc_buf(obj);
    
    {
	int minw = vo_font->width[OSD_PB_START]+vo_font->width[OSD_PB_END]+vo_font->width[OSD_PB_0];
	if (vo_osd_progbar_type>0 && vo_font->font[vo_osd_progbar_type]>=0){
	    minw += vo_font->width[vo_osd_progbar_type]+vo_font->charspace+vo_font->spacewidth;
	}
	if (obj->bbox.x2 - obj->bbox.x1 < minw) return; // space too small, don't render anything
    }
    
    // render it:
    {	unsigned char *s;
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
	   draw_alpha_buf(obj,(xp<0?0:xp),y,
              vo_font->width[c],
              vo_font->pic_a[font]->h,
              vo_font->pic_b[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->w);
	}
   
        c=OSD_PB_START;
        if ((font=vo_font->font[c])>=0)
            draw_alpha_buf(obj,x,y,
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
	       draw_alpha_buf(obj,x,y,w,h,s,sa,st);
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
	       draw_alpha_buf(obj,x,y,w,h,s,sa,st);
	       x+=charw;
	   } while(--i);
	}

        c=OSD_PB_END;
        if ((font=vo_font->font[c])>=0)
            draw_alpha_buf(obj,x,y,
              vo_font->width[c],
              vo_font->pic_a[font]->h,
              vo_font->pic_b[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->bmp+vo_font->start[c],
              vo_font->pic_a[font]->w);
//        x+=vo_font->width[c]+vo_font->charspace;

    }
//        vo_osd_progbar_value=(vo_osd_progbar_value+1)&0xFF;

}

subtitle* vo_sub=NULL;

// vo_draw_text_sub(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride))

inline static void vo_update_text_sub(mp_osd_obj_t* obj,int dxs,int dys){
   unsigned char *t;
   int c,i,j,l,x,y,font,prevc,counter;
   int k;
   int lastStripPosition;
   int xsize;
   int xmin=dxs,xmax=0;
   int h,lasth;
   int xtblc, utblc;
   
   obj->flags|=OSDFLAG_CHANGED|OSDFLAG_VISIBLE;

   if(!vo_sub || !vo_font || !sub_visibility || (vo_font->font[40]<0)){
       obj->flags&=~OSDFLAG_VISIBLE;
       return;
   }
   
   obj->bbox.y2=obj->y=dys;
   obj->params.subtitle.lines=0;

      // too long lines divide into a smaller ones
      i=k=lasth=0;
      h=vo_font->height;
      lastStripPosition=-1;
      l=vo_sub->lines;

    {
	struct osd_text_t *osl, *cp_ott, *tmp_ott, *tmp;
	struct osd_text_p *otp_sub = NULL, *otp_sub_tmp,	// these are used to store the whole sub text osd
	                  *otp, *tmp_otp, *pmt;	// these are used to manage sub text osd coming from a single sub line
	int *char_seq, char_position, xlimit = dxs * sub_width_p / 100, counter;

      while (l) {
	    xsize = -vo_font->charspace;
	  l--;
	  t=vo_sub->text[i++];	  
	    char_position = 0;
	    char_seq = calloc(strlen(t), sizeof(int));

	  prevc = -1;

	    otp = NULL;
	    osl = NULL;
	    x = 1;

	    // reading the subtitle words from vo_sub->text[]
          while (*t) {
            if (sub_utf8)
              c = utf8_get_char(&t);
            else if ((c = *t++) >= 0x80 && sub_unicode)
              c = (c<<8) + *t++;
	      if (k==MAX_UCS){
		 t += strlen(t); // end here
		 mp_msg(MSGT_OSD,MSGL_WARN,"\nMAX_UCS exceeded!\n");
	      }
	      if (!c) c++; // avoid UCS 0
	      render_one_glyph(vo_font, c);

		if (c == ' ') {
		    struct osd_text_t *tmp_ott = (struct osd_text_t *) calloc(1, sizeof(struct osd_text_t));

		    if (osl == NULL) {
			osl = cp_ott = tmp_ott;
		    } else {
			tmp_ott->prev = cp_ott;
			cp_ott->next = tmp_ott;
			tmp_ott->osd_kerning =
			    vo_font->charspace + vo_font->width[' '];
			cp_ott = tmp_ott;
		    }
		    tmp_ott->osd_length = xsize;
		    tmp_ott->text_length = char_position;
		    tmp_ott->text = (int *) malloc(char_position * sizeof(int));
		    for (counter = 0; counter < char_position; ++counter)
			tmp_ott->text[counter] = char_seq[counter];
		    char_position = 0;
		    xsize = 0;
		    prevc = c;
		} else {
		    int delta_xsize = vo_font->width[c] + vo_font->charspace + kerning(vo_font, prevc, c);
		    
		    if (xsize + delta_xsize <= dxs) {
			if (!x) x = 1;
			prevc = c;
			char_seq[char_position++] = c;
			xsize += delta_xsize;
			if ((!suboverlap_enabled) && ((font = vo_font->font[c]) >= 0)) {
			    if (vo_font->pic_a[font]->h > h) {
				h = vo_font->pic_a[font]->h;
			    }
			}
		    } else {
			if (x) {
			    mp_msg(MSGT_OSD, MSGL_WARN, "\nSubtitle word '%s' too long!\n", t);
			    x = 0;
			}
		    }
		}
	    }// for len (all words from subtitle line read)

	    // osl holds an ordered (as they appear in the lines) chain of the subtitle words
	    {
		struct osd_text_t *tmp_ott = (struct osd_text_t *) calloc(1, sizeof(struct osd_text_t));

		if (osl == NULL) {
		    osl = cp_ott = tmp_ott;
		} else {
		    tmp_ott->prev = cp_ott;
		    cp_ott->next = tmp_ott;
		    tmp_ott->osd_kerning =
			vo_font->charspace + vo_font->width[' '];
		    cp_ott = tmp_ott;
		}
		tmp_ott->osd_length = xsize;
		tmp_ott->text_length = char_position;
		tmp_ott->text = (int *) malloc(char_position * sizeof(int));
		for (counter = 0; counter < char_position; ++counter)
		    tmp_ott->text[counter] = char_seq[counter];
		char_position = 0;
		xsize = -vo_font->charspace;
	    }
	    free(char_seq);

	    if (osl != NULL) {
		int value = 0, exit = 0, minimum = 0;

		// otp will contain the chain of the osd subtitle lines coming from the single vo_sub line.
		otp = tmp_otp = (struct osd_text_p *) calloc(1, sizeof(struct osd_text_p));
		tmp_otp->ott = osl;
		for (tmp_ott = tmp_otp->ott; exit == 0; ) {
		    do {
			value += tmp_ott->osd_kerning + tmp_ott->osd_length;
			tmp_ott = tmp_ott->next;
		    } while ((tmp_ott != NULL) && (value + tmp_ott->osd_kerning + tmp_ott->osd_length <= xlimit));
		    if (tmp_ott != NULL) {
			struct osd_text_p *tmp = (struct osd_text_p *) calloc(1, sizeof(struct osd_text_p));

			tmp_otp->value = value;
			tmp_otp->next = tmp;
			tmp->prev = tmp_otp;
			tmp_otp = tmp;
			tmp_otp->ott = tmp_ott;
			value = -2 * vo_font->charspace - vo_font->width[' '];
		    } else {
			tmp_otp->value = value;
			exit = 1;
		    }
		}


#ifdef NEW_SPLITTING
		// minimum holds the 'sum of the differences in lenght among the lines',
		// a measure of the eveness of the lenghts of the lines
		for (tmp_otp = otp; tmp_otp->next != NULL; tmp_otp = tmp_otp->next) {
		    pmt = tmp_otp->next;
		    while (pmt != NULL) {
			minimum += abs(tmp_otp->value - pmt->value);
			pmt = pmt->next;
		    }
		}

		if (otp->next != NULL) {
		    int mem1, mem2;
		    struct osd_text_p *mem, *hold;

		    exit = 0;
		    // until the last word of a line can be moved to the beginning of following line
		    // reducing the 'sum of the differences in lenght among the lines', it is done
		    while (exit == 0) {
			hold = NULL;
			exit = 1;
			for (tmp_otp = otp; tmp_otp->next != NULL; tmp_otp = tmp_otp->next) {
			    pmt = tmp_otp->next;
			    for (tmp = tmp_otp->ott; tmp->next != pmt->ott; tmp = tmp->next);
			    if (pmt->value + tmp->osd_length + pmt->ott->osd_kerning <= xlimit) {
				mem1 = tmp_otp->value;
				mem2 = pmt->value;
				tmp_otp->value = mem1 - tmp->osd_length - tmp->osd_kerning;
				pmt->value = mem2 + tmp->osd_length + pmt->ott->osd_kerning;

				value = 0;
				for (mem = otp; mem->next != NULL; mem = mem->next) {
				    pmt = mem->next;
				    while (pmt != NULL) {
					value += abs(mem->value - pmt->value);
					pmt = pmt->next;
				    }
				}
				if (value < minimum) {
				    minimum = value;
				    hold = tmp_otp;
				    exit = 0;
				}
				tmp_otp->value = mem1;
				tmp_otp->next->value = mem2;
			    }
			}
			// merging
			if (exit == 0) {
			    tmp_otp = hold;
			    pmt = tmp_otp->next;
			    for (tmp = tmp_otp->ott; tmp->next != pmt->ott; tmp = tmp->next);
			    mem1 = tmp_otp->value;
			    mem2 = pmt->value;
			    tmp_otp->value = mem1 - tmp->osd_length - tmp->osd_kerning;
			    pmt->value = mem2 + tmp->osd_length + pmt->ott->osd_kerning;
			    pmt->ott = tmp;
			}//~merging
		    }//~while(exit == 0)
		}//~if(otp->next!=NULL)
#endif

		// adding otp (containing splitted lines) to otp chain
		if (otp_sub == NULL) {
		    otp_sub = otp;
		    for (otp_sub_tmp = otp_sub; otp_sub_tmp->next != NULL; otp_sub_tmp = otp_sub_tmp->next);
		} else {
		    //updating ott chain
		    tmp = otp_sub->ott;
		    while (tmp->next != NULL) tmp = tmp->next;
		    tmp->next = otp->ott;
		    otp->ott->prev = tmp;
		    //attaching new subtitle line at the end
		    otp_sub_tmp->next = otp;
		    otp->prev = otp_sub_tmp;
		    do
			otp_sub_tmp = otp_sub_tmp->next;
		    while (otp_sub_tmp->next != NULL);
		}
	    }//~ if(osl != NULL)
	} // while

	// write lines into utbl
	xtblc = 0;
	utblc = 0;
	obj->y = dys;
	obj->params.subtitle.lines = 0;
	for (tmp_otp = otp_sub; tmp_otp != NULL; tmp_otp = tmp_otp->next) {

	    if ((obj->params.subtitle.lines++) >= MAX_UCSLINES)
		break;

	    if (h > obj->y) {	// out of the screen so end parsing
		obj->y -= lasth - vo_font->height;	// correct the y position
		break;
	    }
	    xsize = tmp_otp->value;
	    obj->params.subtitle.xtbl[xtblc++] = (dxs - xsize) / 2;
	    if (xmin > (dxs - xsize) / 2)
		xmin = (dxs - xsize) / 2;
	    if (xmax < (dxs + xsize) / 2)
		xmax = (dxs + xsize) / 2;

	    tmp = (tmp_otp->next == NULL) ? NULL : tmp_otp->next->ott;
	    for (tmp_ott = tmp_otp->ott; tmp_ott != tmp; tmp_ott = tmp_ott->next) {
		for (counter = 0; counter < tmp_ott->text_length; ++counter) {
		    if (utblc > MAX_UCS) {
			break;
		    }
		    c = tmp_ott->text[counter];
		    render_one_glyph(vo_font, c);
		    obj->params.subtitle.utbl[utblc++] = c;
		    k++;
		}
		obj->params.subtitle.utbl[utblc++] = ' ';
	    }
	    obj->params.subtitle.utbl[utblc - 1] = 0;
	    obj->y -= vo_font->height;
	}
	if(obj->params.subtitle.lines)
	    obj->y = dys - ((obj->params.subtitle.lines - 1) * vo_font->height + vo_font->pic_a[vo_font->font[40]]->h);
	
	// free memory
	if (otp_sub != NULL) {
	    for (tmp = otp_sub->ott; tmp->next != NULL; free(tmp->prev)) {
		free(tmp->text);
		tmp = tmp->next;
	    }
	    free(tmp->text);
	    free(tmp);
	
	    for(pmt = otp_sub; pmt->next != NULL; free(pmt->prev)) {
		pmt = pmt->next;
	    }
	    free(pmt);
	}
	
    }
    /// vertical alignment
    h = dys - obj->y;
    if (sub_alignment == 2)
        obj->y = dys * sub_pos / 100 - h;
    else if (sub_alignment == 1)
        obj->y = dys * sub_pos / 100 - h / 2;
    else
        obj->y = dys * sub_pos / 100;

    if (obj->y < 0)
        obj->y = 0;
    if (obj->y > dys - h)
        obj->y = dys - h;

    obj->bbox.y2 = obj->y + h;

    // calculate bbox:
    if (sub_justify) xmin = 10;
    obj->bbox.x1=xmin;
    obj->bbox.x2=xmax;
    obj->bbox.y1=obj->y;
//    obj->bbox.y2=obj->y+obj->params.subtitle.lines*vo_font->height;
    obj->flags|=OSDFLAG_BBOX;

    alloc_buf(obj);

    y = obj->y;
    
    obj->alignment = 0;
    switch(vo_sub->alignment) {
       case SUB_ALIGNMENT_BOTTOMLEFT:
       case SUB_ALIGNMENT_MIDDLELEFT:
       case SUB_ALIGNMENT_TOPLEFT:
	    obj->alignment |= 0x1;
	    break;
       case SUB_ALIGNMENT_BOTTOMRIGHT:
       case SUB_ALIGNMENT_MIDDLERIGHT:
       case SUB_ALIGNMENT_TOPRIGHT:
	    obj->alignment |= 0x2;
	    break;
       case SUB_ALIGNMENT_BOTTOMCENTER:
       case SUB_ALIGNMENT_MIDDLECENTER:
       case SUB_ALIGNMENT_TOPCENTER:
	default:
	    obj->alignment |= 0x0;
    }

    i=j=0;
    if ((l = obj->params.subtitle.lines)) {
	for(counter = dxs; i < l; ++i)
	    if (obj->params.subtitle.xtbl[i] < counter) counter = obj->params.subtitle.xtbl[i];
	for (i = 0; i < l; ++i) {
	    switch (obj->alignment&0x3) {
		case 1:
		    // left
		    x = counter;
		    break;
		case 2:
		    // right
		    x = 2 * obj->params.subtitle.xtbl[i] - counter - ((obj->params.subtitle.xtbl[i] == counter) ? 0 : 1);
		    break;
		default:
		    //center
		    x = obj->params.subtitle.xtbl[i];
	    }
	 prevc = -1;
	 while ((c=obj->params.subtitle.utbl[j++])){
	       x += kerning(vo_font,prevc,c);
	       if ((font=vo_font->font[c])>=0)
		  draw_alpha_buf(obj,x,y,
			     vo_font->width[c],
			     vo_font->pic_a[font]->h+y<obj->dys ? vo_font->pic_a[font]->h : obj->dys-y,
			     vo_font->pic_b[font]->bmp+vo_font->start[c],
			     vo_font->pic_a[font]->bmp+vo_font->start[c],
			     vo_font->pic_a[font]->w);
	       x+=vo_font->width[c]+vo_font->charspace;
               prevc = c;
	    }
         y+=vo_font->height;
	}
    }
    
}

inline static void vo_update_spudec_sub(mp_osd_obj_t* obj, int dxs, int dys)
{
  unsigned int bbox[4];
  spudec_calc_bbox(vo_spudec, dxs, dys, bbox);
  obj->bbox.x1 = bbox[0];
  obj->bbox.x2 = bbox[1];
  obj->bbox.y1 = bbox[2];
  obj->bbox.y2 = bbox[3];
  obj->flags |= OSDFLAG_BBOX;
}

inline static void vo_draw_spudec_sub(mp_osd_obj_t* obj, void (*draw_alpha)(int x0, int y0, int w, int h, unsigned char* src, unsigned char* srca, int stride))
{
  spudec_draw_scaled(vo_spudec, obj->dxs, obj->dys, draw_alpha);
}

void *vo_spudec=NULL;
void *vo_vobsub=NULL;

static int draw_alpha_init_flag=0;

extern void vo_draw_alpha_init(void);

       mp_osd_obj_t* vo_osd_list=NULL;

static mp_osd_obj_t* new_osd_obj(int type){
    mp_osd_obj_t* osd=malloc(sizeof(mp_osd_obj_t));
    memset(osd,0,sizeof(mp_osd_obj_t));
    osd->next=vo_osd_list;
    vo_osd_list=osd;
    osd->type=type;
    osd->alpha_buffer = NULL;
    osd->bitmap_buffer = NULL;
    osd->allocated = -1;
    return osd;
}

void free_osd_list(void){
    mp_osd_obj_t* obj=vo_osd_list;
    while(obj){
	mp_osd_obj_t* next=obj->next;
	if (obj->alpha_buffer) free(obj->alpha_buffer);
	if (obj->bitmap_buffer) free(obj->bitmap_buffer);
	free(obj);
	obj=next;
    }
    vo_osd_list=NULL;
}

#define FONT_LOAD_DEFER 6

int vo_update_osd(int dxs,int dys){
    mp_osd_obj_t* obj=vo_osd_list;
    int chg=0;
#ifdef HAVE_FREETYPE    
    static int defer_counter = 0, prev_dxs = 0, prev_dys = 0;
#endif

#ifdef HAVE_FREETYPE    
    // here is the right place to get screen dimensions
    if (((dxs != vo_image_width)
	   && (subtitle_autoscale == 2 || subtitle_autoscale == 3))
	|| ((dys != vo_image_height)
	    && (subtitle_autoscale == 1 || subtitle_autoscale == 3)))
    {
	// screen dimensions changed
	// wait a while to avoid useless reloading of the font
	if (dxs == prev_dxs || dys == prev_dys) {
	    defer_counter++;
	} else {
	    prev_dxs = dxs;
	    prev_dys = dys;
	    defer_counter = 0;
	}
	if (defer_counter >= FONT_LOAD_DEFER) force_load_font = 1;
    }

    if (!vo_font || force_load_font) {
	force_load_font = 0;
	load_font_ft(dxs, dys);
	prev_dxs = dxs;
	prev_dys = dys;
	defer_counter = 0;
    }
#endif

    while(obj){
      if(dxs!=obj->dxs || dys!=obj->dys || obj->flags&OSDFLAG_FORCE_UPDATE){
        int vis=obj->flags&OSDFLAG_VISIBLE;
	obj->flags&=~OSDFLAG_BBOX;
	switch(obj->type){
#ifdef USE_DVDNAV
        case OSDTYPE_DVDNAV:
           vo_update_nav(obj,dxs,dys);
           break;
#endif
	case OSDTYPE_SUBTITLE:
	    vo_update_text_sub(obj,dxs,dys);
	    break;
	case OSDTYPE_PROGBAR:
	    vo_update_text_progbar(obj,dxs,dys);
	    break;
	case OSDTYPE_SPU:
	    if(sub_visibility && vo_spudec && spudec_visible(vo_spudec)){
	        vo_update_spudec_sub(obj, dxs, dys);
		obj->flags|=OSDFLAG_VISIBLE|OSDFLAG_CHANGED;
	    }
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

void vo_init_osd(void){
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
#ifdef USE_DVDNAV
    new_osd_obj(OSDTYPE_DVDNAV);
#endif
#ifdef HAVE_FREETYPE
    force_load_font = 1;
#endif
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
	    vo_draw_spudec_sub(obj, draw_alpha); // FIXME
	    break;
#ifdef USE_DVDNAV
        case OSDTYPE_DVDNAV:
#endif
	case OSDTYPE_OSD:
	case OSDTYPE_SUBTITLE:
	case OSDTYPE_PROGBAR:
	    vo_draw_text_from_buffer(obj,draw_alpha);
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
		(obj->bbox.y1<=y2 && obj->bbox.y2>=y1) &&
		obj->bbox.y2 > obj->bbox.y1 && obj->bbox.x2 > obj->bbox.x1
		) return 1;
	}
	obj=obj->next;
    }
    return 0;
}
