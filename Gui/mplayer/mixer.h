
#ifndef _MIXER_H
#define _MIXER_H

extern float mixerGetVolume( void );
extern void  mixerSetVolume( float v );
extern void  mixerIncVolume( void );
extern void  mixerDecVolume( void );
extern void  mixerMute( void );
extern void  mixerSetBalance( float b );

#endif
