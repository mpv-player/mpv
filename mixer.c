
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "mixer.h"


#if	defined(USE_OSS_AUDIO)

/*
 * Mixer interface using OSS style soundcard commands.
 */

#include <sys/soundcard.h>


char * mixer_device=DEV_MIXER;
int    mixer_usemaster=0;

void mixer_getvolume( float *l,float *r )
{
 int fd,v,cmd,devs;

 fd=open( mixer_device,O_RDONLY );
 if ( fd != -1 )
  {
   ioctl( fd,SOUND_MIXER_READ_DEVMASK,&devs );
   if ( ( devs & SOUND_MASK_PCM ) && ( mixer_usemaster==0 ) ) cmd=SOUND_MIXER_READ_PCM;
     else
       if ( ( devs & SOUND_MASK_VOLUME ) && ( mixer_usemaster==1 ) ) cmd=SOUND_MIXER_READ_VOLUME;
         else
           {
            close( fd );
            return;
           }
   ioctl( fd,cmd,&v );
   *r=( v & 0xFF00 ) >> 8;
   *l=( v & 0x00FF );
   close( fd );
  }
}

void mixer_setvolume( float l,float r )
{
 int fd,v,cmd,devs;

 fd=open( mixer_device,O_RDONLY );
 if ( fd != -1 )
  {
   ioctl( fd,SOUND_MIXER_READ_DEVMASK,&devs );
   if ( ( devs & SOUND_MASK_PCM ) && ( mixer_usemaster==0 ) ) cmd=SOUND_MIXER_WRITE_PCM;
     else
       if ( ( devs & SOUND_MASK_VOLUME ) && ( mixer_usemaster==1 ) ) cmd=SOUND_MIXER_WRITE_VOLUME;
         else
           {
            close( fd );
            return;
           }
   v=( (int)r << 8 ) | (int)l;
   ioctl( fd,cmd,&v );
   close( fd );
  }
}

#elif	defined(USE_SUN_AUDIO)

/*
 * Mixer interface using Sun style soundcard commands.
 */

#include <sys/audioio.h>


char * mixer_device="/dev/audioctl";
int    mixer_usemaster=0;

void mixer_getvolume( float *l,float *r )
{
 int fd,v,cmd,devs;

 fd=open( mixer_device,O_RDONLY );
 if ( fd != -1 )
  {
   struct audio_info info;

   ioctl( fd,AUDIO_GETINFO,&info);
   *r=info.play.gain * 100. / AUDIO_MAX_GAIN;
   *l=info.play.gain * 100. / AUDIO_MAX_GAIN;
   close( fd );
  }
}

void mixer_setvolume( float l,float r )
{
 int fd,v,cmd,devs;

 fd=open( mixer_device,O_RDONLY );
 if ( fd != -1 )
  {
   struct audio_info info;
   AUDIO_INITINFO(&info);
   info.play.gain = (r+l) * AUDIO_MAX_GAIN / 100 / 2;
   ioctl( fd,AUDIO_SETINFO,&info );
   close( fd );
  }
}

#else

/*
 * No usable Mixer interface selected.
 * Just some stub routines.
 */

char * mixer_device=NULL;
int    mixer_usemaster=0;

void mixer_getvolume( float *l,float *r ){
 *l = *r = 50.0;
}
void mixer_setvolume( float l,float r ){
}

#endif


void mixer_incvolume( void )
{
 float mixer_l, mixer_r;
 mixer_getvolume( &mixer_l,&mixer_r );
 mixer_l++;
 if ( mixer_l > 100 ) mixer_l = 100;
 mixer_r++;
 if ( mixer_r > 100 ) mixer_r = 100;
 mixer_setvolume( mixer_l,mixer_r );
}

void mixer_decvolume( void )
{
 float mixer_l, mixer_r;
 mixer_getvolume( &mixer_l,&mixer_r );
 mixer_l--;
 if ( mixer_l < 0 ) mixer_l = 0;
 mixer_r--;
 if ( mixer_r < 0 ) mixer_r = 0;
 mixer_setvolume( mixer_l,mixer_r );
}

float mixer_getbothvolume( void )
{
 float mixer_l, mixer_r;
 mixer_getvolume( &mixer_l,&mixer_r );
 return ( mixer_l + mixer_r ) / 2;
}
