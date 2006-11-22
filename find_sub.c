//**************************************************************************//
//             .SUB 
//**************************************************************************//

#include "config.h"

#include <stdio.h>

#include "libvo/video_out.h"
#include "libvo/sub.h"
#include "subreader.h"

#include "mp_msg.h"
#include "help_mp.h"

static int current_sub=0;

//static subtitle* subtitles=NULL;
static int nosub_range_start=-1;
static int nosub_range_end=-1;

extern float sub_delay;
extern float  sub_fps;

void step_sub(sub_data *subd, float pts, int movement) {
    subtitle *subs; 
    int key;

    if (subd == NULL) return;
    subs = subd->subtitles;
    key = (pts+sub_delay) * (subd->sub_uses_time ? 100 : sub_fps);

    /* Tell the OSD subsystem that the OSD contents will change soon */
    vo_osd_changed(OSDTYPE_SUBTITLE);

    /* If we are moving forward, don't count the next (current) subtitle
     * if we haven't displayed it yet. Same when moving other direction.
     */
    if (movement > 0 && key < subs[current_sub].start)
    	movement--;
    if (movement < 0 && key >= subs[current_sub].end)
    	movement++;

    /* Never move beyond first or last subtitle. */
    if (current_sub+movement < 0)
    	movement = 0-current_sub;
    if (current_sub+movement >= subd->sub_num)
    	movement = subd->sub_num - current_sub - 1;

    current_sub += movement;
    sub_delay = subs[current_sub].start / (subd->sub_uses_time ? 100 : sub_fps) - pts;
}

void find_sub(sub_data* subd,int key){
    subtitle *subs;
    int i,j;
    
    if ( !subd || subd->sub_num == 0) return;
    subs = subd->subtitles;
    
    if(vo_sub){
      if(key>=vo_sub->start && key<=vo_sub->end) return; // OK!
    } else {
      if(key>nosub_range_start && key<nosub_range_end) return; // OK!
    }
    // sub changed!

    /* Tell the OSD subsystem that the OSD contents will change soon */
    vo_osd_changed(OSDTYPE_SUBTITLE);

    if(key<=0){
      vo_sub=NULL; // no sub here
      return;
    }
    
//    printf("\r---- sub changed ----\n");
    
    // check next sub.
    if(current_sub>=0 && current_sub+1 < subd->sub_num){
      if(key>subs[current_sub].end && key<subs[current_sub+1].start){
          // no sub
          nosub_range_start=subs[current_sub].end;
          nosub_range_end=subs[current_sub+1].start;
          vo_sub=NULL;
          return;
      }
      // next sub?
      ++current_sub;
      vo_sub=&subs[current_sub];
      if(key>=vo_sub->start && key<=vo_sub->end) return; // OK!
    }

//    printf("\r---- sub log search... ----\n");
    
    // use logarithmic search:
    i=0; 
    j = subd->sub_num - 1;
//    printf("Searching %d in %d..%d\n",key,subs[i].start,subs[j].end);
    while(j>=i){
        current_sub=(i+j+1)/2;
        vo_sub=&subs[current_sub];
        if(key<vo_sub->start) j=current_sub-1;
        else if(key>vo_sub->end) i=current_sub+1;
        else return; // found!
    }
//    if(key>=vo_sub->start && key<=vo_sub->end) return; // OK!
    
    // check where are we...
    if(key<vo_sub->start){
      if(current_sub<=0){
          // before the first sub
          nosub_range_start=key-1; // tricky
          nosub_range_end=vo_sub->start;
//          printf("FIRST...  key=%d  end=%d  \n",key,vo_sub->start);
          vo_sub=NULL;
          return;
      }
      --current_sub;
      if(key>subs[current_sub].end && key<subs[current_sub+1].start){
          // no sub
          nosub_range_start=subs[current_sub].end;
          nosub_range_end=subs[current_sub+1].start;
//          printf("No sub... 1 \n");
          vo_sub=NULL;
          return;
      }
      printf("HEH????  ");
    } else {
      if(key<=vo_sub->end) printf("JAJJ!  "); else
      if(current_sub+1 >= subd->sub_num){
          // at the end?
          nosub_range_start=vo_sub->end;
          nosub_range_end=0x7FFFFFFF; // MAXINT
//          printf("END!?\n");
          vo_sub=NULL;
          return;
      } else
      if(key>subs[current_sub].end && key<subs[current_sub+1].start){
          // no sub
          nosub_range_start=subs[current_sub].end;
          nosub_range_end=subs[current_sub+1].start;
//          printf("No sub... 2 \n");
          vo_sub=NULL;
          return;
      }
    }
    
    mp_msg(MSGT_FIXME,MSGL_FIXME,"SUB ERROR:  %d  ?  %d --- %d  [%d]  \n",key,(int)vo_sub->start,(int)vo_sub->end,current_sub);

    vo_sub=NULL; // no sub here
}
