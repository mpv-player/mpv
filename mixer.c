
#include <string.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <stdio.h>

#include "mixer.h"

#define DEV_MIXER "/dev/mixer"

char * mixer_device=NULL;
char * devname=NULL;
int    mixer_usemaster=0;

void mixer_getvolume( int *l,int *r )
{
 int fd,v,cmd,devs;

 if ( !mixer_device ) devname=strdup( DEV_MIXER );
   else devname=strdup( mixer_device );
 fd=open( devname,O_RDONLY );
 free( devname );
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

void mixer_setvolume( int l,int r )
{
 int fd,v,cmd,devs;

 if ( !mixer_device ) devname=strdup( DEV_MIXER );
   else devname=strdup( mixer_device );
 fd=open( devname,O_RDONLY );
 free( devname );
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
   v=( r << 8 ) | l;
   ioctl( fd,cmd,&v );
   close( fd );
  }
}

int mixer_l=0; int mixer_r=0;

void mixer_incvolume( void )
{
 fprintf( stderr,"[mixer] inc.\n" );
 mixer_getvolume( &mixer_l,&mixer_r );
 if ( mixer_l < 100 ) mixer_l++;
 if ( mixer_r < 100 ) mixer_r++;
 mixer_setvolume( mixer_l,mixer_r );
}

void mixer_decvolume( void )
{
 fprintf( stderr,"[mixer] dec.\n" );
 mixer_getvolume( &mixer_l,&mixer_r );
 if ( mixer_l > 0 ) mixer_l--;
 if ( mixer_r > 0 ) mixer_r--;
 mixer_setvolume( mixer_l,mixer_r );
}

int mixer_getbothvolume( void )
{
 mixer_getvolume( &mixer_l,&mixer_r );
 return ( mixer_l + mixer_r ) / 2;
}
