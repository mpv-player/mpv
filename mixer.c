
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "mixer.h"

#ifdef HAVE_DVB
#include <ost/audio.h>
audioMixer_t dvb_mixer={255,255};
extern int vo_mpegpes_fd;
extern int vo_mpegpes_fd2;
#endif

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

#ifdef HAVE_DVB
 if(vo_mpegpes_fd2>=0){
     // DVB card
     *l=dvb_mixer.volume_left/2.56;
     *r=dvb_mixer.volume_right/2.56;
     return;
 }
#endif 

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
 
#ifdef HAVE_DVB
 if(vo_mpegpes_fd2>=0){
     // DVB card
	 dvb_mixer.volume_left=l*2.56;
	 dvb_mixer.volume_right=r*2.56;
	 if(dvb_mixer.volume_left>255) dvb_mixer.volume_left=255;
	 if(dvb_mixer.volume_right>255) dvb_mixer.volume_right=255;
//	 printf("Setting DVB volume: %d ; %d  \n",dvb_mixer.volume_left,dvb_mixer.volume_right);
	if ( (ioctl(vo_mpegpes_fd2,AUDIO_SET_MIXER, &dvb_mixer) < 0)){
		perror("DVB AUDIO SET MIXER: ");
		return -1;
	}
	return;
 }
#endif

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

#define MIXER_CHANGE 3

void mixer_incvolume( void )
{
 float mixer_l, mixer_r;
 mixer_getvolume( &mixer_l,&mixer_r );
 mixer_l += MIXER_CHANGE;
 if ( mixer_l > 100 ) mixer_l = 100;
 mixer_r += MIXER_CHANGE;
 if ( mixer_r > 100 ) mixer_r = 100;
 mixer_setvolume( mixer_l,mixer_r );
}

void mixer_decvolume( void )
{
 float mixer_l, mixer_r;
 mixer_getvolume( &mixer_l,&mixer_r );
 mixer_l -= MIXER_CHANGE;
 if ( mixer_l < 0 ) mixer_l = 0;
 mixer_r -= MIXER_CHANGE;
 if ( mixer_r < 0 ) mixer_r = 0;
 mixer_setvolume( mixer_l,mixer_r );
}

float mixer_getbothvolume( void )
{
 float mixer_l, mixer_r;
 mixer_getvolume( &mixer_l,&mixer_r );
 return ( mixer_l + mixer_r ) / 2;
}
