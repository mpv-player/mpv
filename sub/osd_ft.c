#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "stream/stream.h"
#include "stream/stream_dvdnav.h"
#define OSD_NAV_BOX_ALPHA 0x7f

#include "libmpcodecs/dec_teletext.h"
#include "osdep/timer.h"

#include "talloc.h"
#include "mplayer.h"
#include "path.h"
#include "mp_msg.h"
#include "libvo/video_out.h"
#include "sub.h"
#include "sub/font_load.h"
#include "spudec.h"
#include "libavutil/common.h"

#define FONT_LOAD_DEFER 6
#define NEW_SPLITTING

//static int vo_font_loaded=-1;
font_desc_t* vo_font=NULL;

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


// return the real height of a char:
static inline int get_height(int c,int h){
    int font;
    if ((font=vo_font->font[c])>=0)
        if(h<vo_font->pic_a[font]->h) h=vo_font->pic_a[font]->h;
    return h;
}


void vo_update_text_osd(struct osd_state *osd, mp_osd_obj_t *obj)
{
        const char *cp = osd->osd_text;
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

        osd_alloc_buf(obj);

        cp = osd->osd_text;
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

static int vo_osd_teletext_scale=0;

// renders char to a big per-object buffer where alpha and bitmap are separated
static void tt_draw_alpha_buf(mp_osd_obj_t* obj, int x0,int y0, int w,int h, unsigned char* src, int stride,int fg,int bg,int alpha)
{
    int dststride = obj->stride;
    int dstskip = obj->stride-w;
    int srcskip = stride-w;
    int i, j;
    unsigned char *b = obj->bitmap_buffer + (y0-obj->bbox.y1)*dststride + (x0-obj->bbox.x1);
    unsigned char *a = obj->alpha_buffer  + (y0-obj->bbox.y1)*dststride + (x0-obj->bbox.x1);
    unsigned char *bs = src;
    if (x0 < obj->bbox.x1 || x0+w > obj->bbox.x2 || y0 < obj->bbox.y1 || y0+h > obj->bbox.y2) {
        mp_msg(MSGT_OSD,MSGL_ERR,"tt osd text out of range: bbox [%d %d %d %d], txt [%d %d %d %d]\n",
                obj->bbox.x1, obj->bbox.x2, obj->bbox.y1, obj->bbox.y2,
                x0, x0+w, y0, y0+h);
        return;
    }
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++, b++, a++, bs++) {
            *b=(fg-bg)*(*bs)/255+bg;
            *a=alpha;
        }
        b+= dstskip;
        a+= dstskip;
        bs+= srcskip;
    }
}
void vo_update_text_teletext(struct osd_state *osd, mp_osd_obj_t *obj)
{
    int h=0,w=0,i,j,font,flashon;
    int wm,hm;
    int color;
    int x,y,x0,y0;
    int cols,rows;
    int wm12;
    int hm13;
    int hm23;
    int start_row,max_rows;
    int b,ax[6],ay[6],aw[6],ah[6];
    tt_char tc;
    tt_char* tdp=vo_osd_teletext_page;
    static const uint8_t colors[8]={1,85,150,226,70,105,179,254};
    unsigned char* buf[9];
    int dxs = osd->w, dys = osd->h;

    obj->flags|=OSDFLAG_CHANGED|OSDFLAG_VISIBLE;
    if (!tdp || !vo_osd_teletext_mode) {
        obj->flags&=~OSDFLAG_VISIBLE;
        return;
    }
    flashon=(GetTimer()/1000000)%2;
    switch(vo_osd_teletext_half){
    case TT_ZOOM_TOP_HALF:
        start_row=0;
        max_rows=VBI_ROWS/2;
        break;
    case TT_ZOOM_BOTTOM_HALF:
        start_row=VBI_ROWS/2;
        max_rows=VBI_ROWS/2;
        break;
    default:
        start_row=0;
        max_rows=VBI_ROWS;
        break;
    }
    wm=0;
    for(i=start_row;i<max_rows;i++){
        for(j=0;j<VBI_COLUMNS;j++){
            tc=tdp[i*VBI_COLUMNS+j];
            if(!tc.ctl && !tc.gfx)
            {
                render_one_glyph(vo_font, tc.unicode);
                if (wm<vo_font->width[tc.unicode])
                    wm=vo_font->width[tc.unicode];
            }
        }
    }

    hm=vo_font->height+1;
    wm=dxs*hm*max_rows/(dys*VBI_COLUMNS);

#ifdef CONFIG_FREETYPE
    //very simple teletext font auto scaling
    if(!vo_osd_teletext_scale && hm*(max_rows+1)>dys){
        osd_font_scale_factor*=1.0*(dys)/((max_rows+1)*hm);
        force_load_font=1;
        vo_osd_teletext_scale=osd_font_scale_factor;
        obj->flags&=~OSDFLAG_VISIBLE;
        return;
    }
#endif

    cols=dxs/wm;
    rows=dys/hm;

    if(cols>VBI_COLUMNS)
        cols=VBI_COLUMNS;
    if(rows>max_rows)
        rows=max_rows;
    w=cols*wm-vo_font->charspace;
    h=rows*hm-vo_font->charspace;

    if(w<dxs)
        x0=(dxs-w)/2;
    else
        x0=0;
    if(h<dys)
        y0=(dys-h)/2;
    else
        y0=0;

    wm12=wm>>1;
    hm13=(hm+1)/3;
    hm23=hm13<<1;

    for(i=0;i<6;i+=2){
        ax[i+0]=0;
        aw[i+0]=wm12;

        ax[i+1]=wm12;
        aw[i+1]=wm-wm12;
    }

    for(i=0;i<2;i++){
        ay[i+0]=0;
        ah[i+0]=hm13;

        ay[i+2]=hm13;
        ah[i+2]=hm-hm23;

        ay[i+4]=hm-hm13;
        ah[i+4]=hm13;
    }

    obj->x = 0;
    obj->y = 0;
    obj->bbox.x1 = x0;
    obj->bbox.y1 = y0;
    obj->bbox.x2 = x0+w;
    obj->bbox.y2 = y0+h;
    obj->flags |= OSDFLAG_BBOX;
    osd_alloc_buf(obj);

    for(i=0;i<9;i++)
        buf[i]=malloc(wm*hm);

    //alpha
    if(vo_osd_teletext_format==TT_FORMAT_OPAQUE ||vo_osd_teletext_format==TT_FORMAT_OPAQUE_INV)
        color=1;
    else
        color=200;
    memset(buf[8],color,wm*hm);
    //colors
    if(vo_osd_teletext_format==TT_FORMAT_OPAQUE ||vo_osd_teletext_format==TT_FORMAT_TRANSPARENT){
        for(i=0;i<8;i++){
            memset(buf[i],(unsigned char)(1.0*(255-color)*colors[i]/255),wm*hm);
        }
    }else{
        for(i=0;i<8;i++)
            memset(buf[i],(unsigned char)(1.0*(255-color)*colors[7-i]/255),wm*hm);
    }

    y=y0;
    for(i=0;i<rows;i++){
        x=x0;
        for(j=0;j<cols;j++){
            tc=tdp[(i+start_row)*VBI_COLUMNS+j];
            if (tc.hidden) { x+=wm; continue;}
            if(!tc.gfx || (tc.flh && !flashon)){
                /* Rendering one text character */
                draw_alpha_buf(obj,x,y,wm,hm,buf[tc.bg],buf[8],wm);
                if(tc.unicode!=0x20 && tc.unicode!=0x00 && !tc.ctl &&
                    (!tc.flh || flashon) &&
                    (font=vo_font->font[tc.unicode])>=0 && y+hm<dys){
                        tt_draw_alpha_buf(obj,x,y,vo_font->width[tc.unicode],vo_font->height,
                            vo_font->pic_b[font]->bmp+vo_font->start[tc.unicode]-vo_font->charspace*vo_font->pic_a[font]->w,
                            vo_font->pic_b[font]->w,
                            buf[tc.fg][0],buf[tc.bg][0],buf[8][0]);
                }
            }else{
/*
Rendering one graphics character
TODO: support for separated graphics symbols (where six rectangles does not touch each other)

    +--+    +--+    87654321
    |01|    |12|    --------
    |10| <= |34| <= 00100110 <= 0x26
    |01|    |56|
    +--+    +--+

(0:wm/2)    (wm/2:wm-wm/2)

********** *********** (0:hm/3)
***   **** ****   ****
*** 1 **** **** 2 ****
***   **** ****   ****
********** ***********
********** ***********

********** *********** (hm/3:hm-2*hm/3)
********** ***********
***   **** ****   ****
*** 3 **** **** 4 ****
***   **** ****   ****
********** ***********
********** ***********
********** ***********

********** *********** (hm-hm/3:hm/3)
***   **** ****   ****
*** 5 **** **** 6 ****
***   **** ****   ****
********** ***********
********** ***********

*/
                if(tc.gfx>1){ //separated gfx
                    for(b=0;b<6;b++){
                        color=(tc.unicode>>b)&1?tc.fg:tc.bg;
                        draw_alpha_buf(obj,x+ax[b]+1,y+ay[b]+1,aw[b]-2,ah[b]-2,buf[color],buf[8],wm);
                    }
                    //separated gfx (background borders)
                    //vertical
                    draw_alpha_buf(obj,x        ,y,1,hm,buf[tc.bg],buf[8],wm);
                    draw_alpha_buf(obj,x+ax[1]-1,y,2,hm,buf[tc.bg],buf[8],wm);
                    draw_alpha_buf(obj,x+ax[1]+aw[1]-1,y,wm-ax[1]-aw[1]+1,hm,buf[tc.bg],buf[8],wm);
                    //horizontal
                    draw_alpha_buf(obj,x,y      ,wm,1,buf[tc.bg],buf[8],wm);
                    draw_alpha_buf(obj,x,y+ay[0]+ah[0]-1,wm,2,buf[tc.bg],buf[8],wm);
                    draw_alpha_buf(obj,x,y+ay[2]+ah[2]-1,wm,2,buf[tc.bg],buf[8],wm);
                    draw_alpha_buf(obj,x,y+ay[4]+ah[4]-1,wm,hm-ay[4]-ah[4]+1,buf[tc.bg],buf[8],wm);
                }else{
                    for(b=0;b<6;b++){
                        color=(tc.unicode>>b)&1?tc.fg:tc.bg;
                        draw_alpha_buf(obj,x+ax[b],y+ay[b],aw[b],ah[b],buf[color],buf[8],wm);
                    }
                }
            }
            x+=wm;
        }
        y+=hm;
    }
    for(i=0;i<9;i++)
        free(buf[i]);
}

// if we have n=256 bars then OSD progbar looks like below
//
// 0   1    2    3 ... 256  <= vo_osd_progbar_value
// |   |    |    |       |
// [ ===  ===  === ... === ]
//
//  the above schema is rescalled to n=elems bars

void vo_update_text_progbar(struct osd_state *osd, mp_osd_obj_t *obj){

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
    {   int h=0;
        int y=(osd->h-vo_font->height)/2;
        int delimw=vo_font->width[OSD_PB_START]
                  +vo_font->width[OSD_PB_END]
                  +vo_font->charspace;
        int width=(2*osd->w-3*delimw)/3;
        int charw=vo_font->width[OSD_PB_0]+vo_font->charspace;
        int elems=width/charw;
        int x=(osd->w-elems*charw-delimw)/2;
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

    osd_alloc_buf(obj);

    {
        int minw = vo_font->width[OSD_PB_START]+vo_font->width[OSD_PB_END]+vo_font->width[OSD_PB_0];
        if (vo_osd_progbar_type>0 && vo_font->font[vo_osd_progbar_type]>=0){
            minw += vo_font->width[vo_osd_progbar_type]+vo_font->charspace+vo_font->spacewidth;
        }
        if (obj->bbox.x2 - obj->bbox.x1 < minw) return; // space too small, don't render anything
    }

    // render it:
    {   unsigned char *s;
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

void vo_update_text_sub(struct osd_state *osd, mp_osd_obj_t *obj){
   unsigned char *t;
   int c,i,j,l,x,y,font,prevc,counter;
   int k;
   int xsize;
   int dxs = osd->w, dys = osd->h;
   int xmin=dxs,xmax=0;
   int h,lasth;
   int xtblc, utblc;
   struct font_desc *sub_font = osd->sub_font;

   obj->flags|=OSDFLAG_CHANGED|OSDFLAG_VISIBLE;

   if(!vo_sub || !osd->sub_font || !sub_visibility || (sub_font->font[40]<0)){
       obj->flags&=~OSDFLAG_VISIBLE;
       return;
   }

   obj->bbox.y2=obj->y=dys;
   obj->params.subtitle.lines=0;

      // too long lines divide into a smaller ones
      i=k=lasth=0;
      h=sub_font->height;
      l=vo_sub->lines;

    {
        struct osd_text_t *osl, *cp_ott, *tmp_ott, *tmp;
        struct osd_text_p *otp_sub = NULL, *otp_sub_tmp = NULL, // these are used to store the whole sub text osd
                          *otp, *tmp_otp, *pmt; // these are used to manage sub text osd coming from a single sub line
        int *char_seq, char_position, xlimit = dxs * sub_width_p / 100, counter;

      while (l) {
            xsize = -sub_font->charspace;
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
              c = utf8_get_char((const char **)&t);
            else if ((c = *t++) >= 0x80 && sub_unicode)
              c = (c<<8) + *t++;
              if (k==MAX_UCS){
                 t += strlen(t); // end here
                 mp_msg(MSGT_OSD,MSGL_WARN,"\nMAX_UCS exceeded!\n");
              }
              if (!c) c++; // avoid UCS 0
              render_one_glyph(sub_font, c);

                if (c == ' ') {
                    struct osd_text_t *tmp_ott = calloc(1, sizeof(struct osd_text_t));

                    if (osl == NULL) {
                        osl = cp_ott = tmp_ott;
                    } else {
                        tmp_ott->prev = cp_ott;
                        cp_ott->next = tmp_ott;
                        tmp_ott->osd_kerning =
                            sub_font->charspace + sub_font->width[' '];
                        cp_ott = tmp_ott;
                    }
                    tmp_ott->osd_length = xsize;
                    tmp_ott->text_length = char_position;
                    tmp_ott->text = malloc(char_position * sizeof(int));
                    for (counter = 0; counter < char_position; ++counter)
                        tmp_ott->text[counter] = char_seq[counter];
                    char_position = 0;
                    xsize = 0;
                    prevc = c;
                } else {
                    int delta_xsize = sub_font->width[c] + sub_font->charspace + kerning(sub_font, prevc, c);

                    if (xsize + delta_xsize <= dxs) {
                        if (!x) x = 1;
                        prevc = c;
                        char_seq[char_position++] = c;
                        xsize += delta_xsize;
                        if ((!suboverlap_enabled) && ((font = sub_font->font[c]) >= 0)) {
                            if (sub_font->pic_a[font]->h > h) {
                                h = sub_font->pic_a[font]->h;
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
                struct osd_text_t *tmp_ott = calloc(1, sizeof(struct osd_text_t));

                if (osl == NULL) {
                    osl = cp_ott = tmp_ott;
                } else {
                    tmp_ott->prev = cp_ott;
                    cp_ott->next = tmp_ott;
                    tmp_ott->osd_kerning =
                        sub_font->charspace + sub_font->width[' '];
                    cp_ott = tmp_ott;
                }
                tmp_ott->osd_length = xsize;
                tmp_ott->text_length = char_position;
                tmp_ott->text = malloc(char_position * sizeof(int));
                for (counter = 0; counter < char_position; ++counter)
                    tmp_ott->text[counter] = char_seq[counter];
                char_position = 0;
                xsize = -sub_font->charspace;
            }
            free(char_seq);

            if (osl != NULL) {
                int value = 0, exit = 0, minimum = 0;

                // otp will contain the chain of the osd subtitle lines coming from the single vo_sub line.
                otp = tmp_otp = calloc(1, sizeof(struct osd_text_p));
                tmp_otp->ott = osl;
                for (tmp_ott = tmp_otp->ott; exit == 0; ) {
                    do {
                        value += tmp_ott->osd_kerning + tmp_ott->osd_length;
                        tmp_ott = tmp_ott->next;
                    } while ((tmp_ott != NULL) && (value + tmp_ott->osd_kerning + tmp_ott->osd_length <= xlimit));
                    if (tmp_ott != NULL) {
                        struct osd_text_p *tmp = calloc(1, sizeof(struct osd_text_p));

                        tmp_otp->value = value;
                        tmp_otp->next = tmp;
                        tmp->prev = tmp_otp;
                        tmp_otp = tmp;
                        tmp_otp->ott = tmp_ott;
                        value = -2 * sub_font->charspace - sub_font->width[' '];
                    } else {
                        tmp_otp->value = value;
                        exit = 1;
                    }
                }


#ifdef NEW_SPLITTING
                // minimum holds the 'sum of the differences in length among the lines',
                // a measure of the evenness of the lengths of the lines
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
                    // reducing the 'sum of the differences in length among the lines', it is done
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

            if (h > obj->y) {   // out of the screen so end parsing
                obj->y -= lasth - sub_font->height;     // correct the y position
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
                    render_one_glyph(sub_font, c);
                    obj->params.subtitle.utbl[utblc++] = c;
                    k++;
                }
                obj->params.subtitle.utbl[utblc++] = ' ';
            }
            obj->params.subtitle.utbl[utblc - 1] = 0;
            obj->y -= sub_font->height;
        }
        if(obj->params.subtitle.lines)
            obj->y = dys - ((obj->params.subtitle.lines - 1) * sub_font->height + sub_font->pic_a[sub_font->font[40]]->h);

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
//    obj->bbox.y2=obj->y+obj->params.subtitle.lines*sub_font->height;
    obj->flags|=OSDFLAG_BBOX;

    osd_alloc_buf(obj);

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
               x += kerning(sub_font,prevc,c);
               if ((font=sub_font->font[c])>=0)
                  draw_alpha_buf(obj,x,y,
                             sub_font->width[c],
                             sub_font->pic_a[font]->h+y<obj->dys ? sub_font->pic_a[font]->h : obj->dys-y,
                             sub_font->pic_b[font]->bmp+sub_font->start[c],
                             sub_font->pic_a[font]->bmp+sub_font->start[c],
                             sub_font->pic_a[font]->w);
               x+=sub_font->width[c]+sub_font->charspace;
               prevc = c;
            }
         y+=sub_font->height;
        }
    }

}

void osd_init_backend(struct osd_state *osd)
{
    // check font
#ifdef CONFIG_FREETYPE
    init_freetype();
#endif
#ifdef CONFIG_FONTCONFIG
    if (font_fontconfig <= 0) {
#endif
#ifdef CONFIG_BITMAP_FONT
        if (font_name) {
            vo_font = read_font_desc(font_name, font_factor, verbose > 1);
            if (!vo_font)
                mp_tmsg(MSGT_CPLAYER, MSGL_ERR, "Cannot load bitmap font: %s\n",
                        filename_recode(font_name));
        } else {
            // try default:
            char *mem_ptr;
            vo_font = read_font_desc(mem_ptr = get_path("font/font.desc"),
                                    font_factor, verbose > 1);
            free(mem_ptr); // release the buffer created by get_path()
            if (!vo_font)
                vo_font = read_font_desc(MPLAYER_DATADIR "/font/font.desc",
                                        font_factor, verbose > 1);
        }
        if (sub_font_name)
            osd->sub_font = read_font_desc(sub_font_name, font_factor,
                                                verbose > 1);
        else
            osd->sub_font = vo_font;
#endif
#ifdef CONFIG_FONTCONFIG
    }
#endif
}

void osd_destroy_backend(struct osd_state *osd)
{
#ifdef CONFIG_FREETYPE
    current_module = "uninit_font";
    if (osd && osd->sub_font != vo_font)
        free_font_desc(osd->sub_font);
    free_font_desc(vo_font);
    vo_font = NULL;
    done_freetype();
#endif
}

void osd_font_invalidate(void)
{
#ifdef CONFIG_FREETYPE
    force_load_font = 1;
#endif
}

void osd_font_load(struct osd_state *osd)
{
#ifdef CONFIG_FREETYPE
    static int defer_counter = 0, prev_dxs = 0, prev_dys = 0;
#endif

#ifdef CONFIG_FREETYPE
    // here is the right place to get screen dimensions
    int dxs = osd->w, dys = osd->h;
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

    if (force_load_font) {
        force_load_font = 0;
        load_font_ft(dxs, dys, &vo_font, font_name, osd_font_scale_factor);
        if (sub_font_name)
            load_font_ft(dxs, dys, &osd->sub_font, sub_font_name, text_font_scale_factor);
        else
            load_font_ft(dxs, dys, &osd->sub_font, font_name, text_font_scale_factor);
        prev_dxs = dxs;
        prev_dys = dys;
        defer_counter = 0;
    } else {
       if (!vo_font)
           load_font_ft(dxs, dys, &vo_font, font_name, osd_font_scale_factor);
       if (!osd->sub_font) {
           if (sub_font_name)
               load_font_ft(dxs, dys, &osd->sub_font, sub_font_name, text_font_scale_factor);
           else
               load_font_ft(dxs, dys, &osd->sub_font, font_name, text_font_scale_factor);
       }
    }
#endif
}

void osd_get_function_sym(char *buffer, size_t buffer_size, int osd_function)
{
    snprintf(buffer, buffer_size, "%c", osd_function);
}
