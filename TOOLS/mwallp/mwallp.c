// based on x11_common.c/vo_x11.c from libvo

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "cpudetect.h"

char* mDisplayName=NULL;
Display* mDisplay=NULL;
Window   mRootWin=None;
int mScreen=0;
XImage  * mXImage = NULL;
GC         vo_gc = NULL;
Pixmap mPixmap=0;

int vo_depthonscreen=0;
int vo_screenwidth=0;
int vo_screenheight=0;

int verbose=0;
int namecnt=0;

int main(int argc,char* argv[]){
 unsigned int mask;
 int bpp,depth;

 mp_msg_init();
 mp_msg_set_level(5);

 GetCpuCaps(&gCpuCaps);
 
 mDisplayName = XDisplayName(mDisplayName);
 mDisplay=XOpenDisplay(mDisplayName);
 if ( !mDisplay )
  {
   printf("vo: couldn't open the X11 display (%s)!\n",mDisplayName );
   return 0;
  }
 mScreen=DefaultScreen( mDisplay );     // Screen ID.
 mRootWin=RootWindow( mDisplay,mScreen );// Root window ID.

 vo_screenwidth=DisplayWidth( mDisplay,mScreen );
 vo_screenheight=DisplayHeight( mDisplay,mScreen );

 // get color depth (from root window, or the best visual):
 {  XWindowAttributes attribs;
    XGetWindowAttributes(mDisplay, mRootWin, &attribs);
    depth=vo_depthonscreen=attribs.depth;
 }
 mXImage=XGetImage( mDisplay,mRootWin,0,0,vo_screenwidth,vo_screenheight,AllPlanes,ZPixmap );

 bpp=mXImage->bits_per_pixel;
 if((vo_depthonscreen+7)/8 != (bpp+7)/8) vo_depthonscreen=bpp; // by A'rpi
 mask=mXImage->red_mask|mXImage->green_mask|mXImage->blue_mask;

 if(((vo_depthonscreen+7)/8)==2){
   if(mask==0x7FFF) vo_depthonscreen=15; else
   if(mask==0xFFFF) vo_depthonscreen=16;
 }
 
 printf("X11 running at %d x %d, %d bpp \n",vo_screenwidth,vo_screenheight,vo_depthonscreen);

 mPixmap=XCreatePixmap(mDisplay,mRootWin,vo_screenwidth,vo_screenheight,depth);

 {  XGCValues xgcv;
    vo_gc=XCreateGC( mDisplay,mRootWin,0L,&xgcv );
 }

 if(argc<2){ printf("no filenames!\n");return;}

 srand(time(NULL));

while(1){
 FILE *f;
 char* data;
 int len;
 int ret;
 
 namecnt=1+(long long)rand()*(argc-1)/RAND_MAX;
 //++namecnt; if(namecnt>=argc) namecnt=1;
 if(namecnt<1 || namecnt>=argc) continue; // ???
 
 f=fopen(argv[namecnt],"rb");if(!f) continue;
 fseek(f,0,SEEK_END); len=ftell(f); fseek(f,0,SEEK_SET);
 data=malloc(len);
 len=fread(data,1,len,f);
 fclose(f);

 ret=decode_jpeg(data,len,mXImage->data,vo_screenwidth,vo_screenheight,
   ((vo_depthonscreen+7)/8)*vo_screenwidth,vo_depthonscreen);

 free(data);
 
 if(!ret) continue; // failed to load

 XPutImage( mDisplay,mPixmap,vo_gc,mXImage,
               0,0, 0,0, vo_screenwidth,vo_screenheight);
 XSetWindowBackgroundPixmap(mDisplay,mRootWin,mPixmap);
 XClearWindow(mDisplay,mRootWin);
 XSync(mDisplay, True);
 
 break; // DONE!
}

}
