
#include "play.h"

float mixerGetVolume( void )
{
// ---
// ---
 return mplShMem->Volume;
}

void mixerSetVolume( float v )
{ // 0.0 ... 100.0
// ---
printf("%%%%%% mixerSetVolume(%5.3f)  \n",v);
// ---
 mplShMem->Volume=v;
}

void mixerIncVolume( void )
{
 mixerSetVolume(  mixerGetVolume() + 1.0f );
}

void mixerDecVolume( void )
{
 mixerSetVolume(  mixerGetVolume() - 1.0f );
}

void mixerMute( void )
{
}

void mixerSetBalance( float b )
{
// ---
// ---
printf("%%%%%% mixerSetBalance(%5.3f)  \n",b);
 mplShMem->Balance=b;
}
