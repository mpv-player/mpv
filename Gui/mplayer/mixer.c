
#include "play.h"
#include "../../mixer.h"

//extern void mixer_getvolume( float *l,float *r );
//extern void mixer_setvolume( float l,float r );
//extern void mixer_incvolume( void );
//extern void mixer_decvolume( void );
//extern float mixer_getbothvolume( void );

float mixerGetVolume( void )
{
 mplShMem->Volume=mixer_getbothvolume();
 return mplShMem->Volume;
}

void  mixerSetVolume( float v )
{
 mplShMem->Volume=v;
 mixer_setvolume( v,v );
}

void mixerIncVolume( void )
{
 mixer_incvolume();
 mixerGetVolume();
}

void mixerDecVolume( void )
{
 mixer_decvolume();
 mixerGetVolume();
}

void mixerMute( void )
{
}

void mixerSetBalance( float b )
{
//printf("%%%%%% mixerSetBalance(%5.3f)  \n",b);
 mplShMem->Balance=b;
}

float mixerGetBalance( void )
{
 return mplShMem->Balance;
}
