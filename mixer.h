
#ifndef __MPLAYER_MIXER
#define __MPLAYER_MIXER

#define DEV_MIXER "/dev/mixer"

extern int    mixer_usemaster;
extern char * mixer_device;

extern void mixer_getvolume( float *l,float *r );
extern void mixer_setvolume( float l,float r );
extern void mixer_incvolume( void );
extern void mixer_decvolume( void );
extern float mixer_getbothvolume( void );

//extern void mixer_setbothvolume( int v );
#define mixer_setbothvolume( v ) mixer_setvolume( v,v )

#endif
