
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "mixer.h"
#include "libao2/audio_out.h"

extern ao_functions_t *audio_out;

char * mixer_device=NULL;

void mixer_getvolume( float *l,float *r )
{
  ao_control_vol_t vol;
  *l=0; *r=0;
  if(audio_out){
    if(CONTROL_OK != audio_out->control(AOCONTROL_GET_VOLUME,(int)&vol))
      return;
    *r=vol.right;
    *l=vol.left;
  }
}

void mixer_setvolume( float l,float r )
{
  ao_control_vol_t vol;
  vol.right=r; vol.left=l;
  if(audio_out){
    if(CONTROL_OK != audio_out->control(AOCONTROL_SET_VOLUME,(int)&vol))
      return;
  }
}

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





