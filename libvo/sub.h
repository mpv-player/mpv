
#ifndef __MPLAYER_SUB_H
#define __MPLAYER_SUB_H

typedef struct mp_osd_bbox_s {
    int x1,y1,x2,y2;
} mp_osd_bbox_t;

#define OSDTYPE_OSD 1
#define OSDTYPE_SUBTITLE 2
#define OSDTYPE_PROGBAR 3
#define OSDTYPE_SPU 4
#define OSDTYPE_DVDNAV 5

#define OSDFLAG_VISIBLE 1
#define OSDFLAG_CHANGED 2
#define OSDFLAG_BBOX 4
#define OSDFLAG_OLD_BBOX 8
#define OSDFLAG_FORCE_UPDATE 16

#define MAX_UCS 1600
#define MAX_UCSLINES 16

typedef struct mp_osd_obj_s {
    struct mp_osd_obj_s* next;
    unsigned char type;
    unsigned char alignment; // 2 bits: x;y percentages, 2 bits: x;y relative to parent; 2 bits: alignment left/right/center
    unsigned short flags;
    int x,y;
    int dxs,dys;
    mp_osd_bbox_t bbox; // bounding box
    mp_osd_bbox_t old_bbox; // the renderer will save bbox here
    union {
	struct {
	    void* sub;			// value of vo_sub at last update
	    int utbl[MAX_UCS+1];	// subtitle text
	    int xtbl[MAX_UCSLINES];	// x positions
	    int lines;			// no. of lines
	} subtitle;
	struct {
	    int elems;
	} progbar;
    } params;
    int stride;

    int allocated;
    unsigned char *alpha_buffer;
    unsigned char *bitmap_buffer;
} mp_osd_obj_t;


#if 0

// disable subtitles:
static inline void vo_draw_text_osd(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)){
}

#else

#include "subreader.h"

extern sub_data* subdata; //currently used subtitles
extern subtitle* vo_sub;

extern unsigned char* vo_osd_text;

extern int vo_osd_progbar_type;
extern int vo_osd_progbar_value;   // 0..255

extern void* vo_spudec;
extern void* vo_vobsub;

#define OSD_PLAY 0x01
#define OSD_PAUSE 0x02
#define OSD_STOP 0x03
#define OSD_REW 0x04
#define OSD_FFW 0x05
#define OSD_CLOCK 0x06
#define OSD_CONTRAST 0x07
#define OSD_SATURATION 0x08
#define OSD_VOLUME 0x09
#define OSD_BRIGHTNESS 0x0A
#define OSD_HUE 0x0B
#define OSD_PANSCAN 0x50

#define OSD_PB_START 0x10
#define OSD_PB_0 0x11
#define OSD_PB_END 0x12
#define OSD_PB_1 0x13

/* now in textform */
extern char * __sub_osd_names[];
extern char * __sub_osd_names_short[];

extern int sub_unicode;
extern int sub_utf8;

#ifdef USE_ICONV
extern char *sub_cp;
#endif
extern int sub_pos;
extern int sub_width_p;
extern int sub_alignment;
extern int sub_visibility;
extern int suboverlap_enabled;
extern int sub_bg_color; /* subtitles background color */
extern int sub_bg_alpha;
extern int spu_alignment;
extern int spu_aamode;
extern float spu_gaussvar;

//extern void vo_draw_text_osd(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));
//extern void vo_draw_text_progbar(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));
//extern void vo_draw_text_sub(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));
extern void vo_draw_text(int dxs,int dys,void (*draw_alpha)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride));
extern void vo_remove_text(int dxs,int dys,void (*remove)(int x0,int y0, int w,int h));

void vo_init_osd(void);
int vo_update_osd(int dxs,int dys);
int vo_osd_changed(int new_value);
int vo_osd_check_range_update(int,int,int,int);
void free_osd_list(void);

extern int vo_osd_changed_flag;

unsigned utf8_get_char(const char **str);

#ifdef USE_DVDNAV
#include <inttypes.h>
void osd_set_nav_box (uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey);
#endif

#endif
#endif
