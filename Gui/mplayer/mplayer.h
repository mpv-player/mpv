
#ifndef __MYMPLAYERHANDLER
#define __MYMPLAYERHANDLER

extern int             mplSubRender;
extern int             mplMainRender;

extern unsigned char * mplDrawBuffer;
extern unsigned char * mplMenuDrawBuffer;
extern int             mainVisible;

extern int             mplMainAutoPlay;
extern int             mplMiddleMenu;

extern void mplInit( void * disp );
extern void mplEventHandling( int msg,float param );

extern void mplMainDraw( void );
extern void mplEventHandling( int msg,float param );
extern void mplMainMouseHandle( int Button,int X,int Y,int RX,int RY );
extern void mplMainKeyHandle( int KeyCode,int Type,int Key );
extern void mplDandDHandler(int num,char** files);

extern void mplSubDraw( void );
extern void mplSubMouseHandle( int Button,int X,int Y,int RX,int RY );

extern void mplMenuInit( void );
extern void mplHideMenu( int mx,int my,int w );
extern void mplShowMenu( int mx,int my );
extern void mplMenuMouseHandle( int X,int Y,int RX,int RY );

extern void mplPBInit( void );
extern void mplPBShow( int x, int y );

#endif
