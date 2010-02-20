/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string.h>
#ifndef __MINGW32__
#include <sys/ioctl.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "libao2/audio_out.h"
#include "libaf/af.h"
#include "mixer.h"

#include "help_mp.h"

char * mixer_device=NULL;
char * mixer_channel=NULL;
int soft_vol = 0;
float soft_vol_max = 110.0;

void mixer_getvolume(mixer_t *mixer, float *l, float *r)
{
  ao_control_vol_t vol;
  *l=0; *r=0;
  if(mixer->audio_out){
    if(soft_vol ||
        CONTROL_OK != mixer->audio_out->control(AOCONTROL_GET_VOLUME,&vol)) {
      if (!mixer->afilter)
        return;
      else {
        float db_vals[AF_NCH];
        if (!af_control_any_rev(mixer->afilter,
               AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_GET, db_vals))
          db_vals[0] = db_vals[1] = 1.0;
        else
        af_from_dB (2, db_vals, db_vals, 20.0, -200.0, 60.0);
        vol.left = (db_vals[0] / (soft_vol_max / 100.0)) * 100.0;
        vol.right = (db_vals[1] / (soft_vol_max / 100.0)) * 100.0;
      }
    }
    *r=vol.right;
    *l=vol.left;
  }
}

void mixer_setvolume(mixer_t *mixer, float l, float r)
{
  ao_control_vol_t vol;
  vol.right=r; vol.left=l;
  if(mixer->audio_out){
    if(soft_vol ||
        CONTROL_OK != mixer->audio_out->control(AOCONTROL_SET_VOLUME,&vol)) {
      if (!mixer->afilter)
        return;
      else {
        // af_volume uses values in dB
        float db_vals[AF_NCH];
        int i;
        db_vals[0] = (l / 100.0) * (soft_vol_max / 100.0);
        db_vals[1] = (r / 100.0) * (soft_vol_max / 100.0);
        for (i = 2; i < AF_NCH; i++) {
          db_vals[i] = ((l + r) / 100.0) * (soft_vol_max / 100.0) / 2.0;
        }
        af_to_dB (AF_NCH, db_vals, db_vals, 20.0);
        if (!af_control_any_rev(mixer->afilter,
               AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET, db_vals)) {
          mp_msg(MSGT_GLOBAL, MSGL_INFO, MSGTR_InsertingAfVolume);
          if (af_add(mixer->afilter, "volume")) {
            if (!af_control_any_rev(mixer->afilter,
                   AF_CONTROL_VOLUME_LEVEL | AF_CONTROL_SET, db_vals)) {
              mp_msg(MSGT_GLOBAL, MSGL_ERR, MSGTR_NoVolume);
              return;
            }
          }
	}
      }
    }
  }
 mixer->muted=0;
}

void mixer_incvolume(mixer_t *mixer)
{
 float mixer_l, mixer_r;
 mixer_getvolume(mixer, &mixer_l, &mixer_r);
 mixer_l += mixer->volstep;
 if ( mixer_l > 100 ) mixer_l = 100;
 mixer_r += mixer->volstep;
 if ( mixer_r > 100 ) mixer_r = 100;
 mixer_setvolume(mixer, mixer_l, mixer_r);
}

void mixer_decvolume(mixer_t *mixer)
{
 float mixer_l, mixer_r;
 mixer_getvolume(mixer, &mixer_l, &mixer_r);
 mixer_l -= mixer->volstep;
 if ( mixer_l < 0 ) mixer_l = 0;
 mixer_r -= mixer->volstep;
 if ( mixer_r < 0 ) mixer_r = 0;
 mixer_setvolume(mixer, mixer_l, mixer_r);
}

void mixer_getbothvolume(mixer_t *mixer, float *b)
{
 float mixer_l, mixer_r;
 mixer_getvolume(mixer, &mixer_l, &mixer_r);
 *b = ( mixer_l + mixer_r ) / 2;
}

void mixer_mute(mixer_t *mixer)
{
 if (mixer->muted) mixer_setvolume(mixer, mixer->last_l, mixer->last_r);
  else
   {
    mixer_getvolume(mixer, &mixer->last_l, &mixer->last_r);
    mixer_setvolume(mixer, 0, 0);
    mixer->muted=1;
   }
}

void mixer_getbalance(mixer_t *mixer, float *val)
{
  *val = 0.f;
  if(!mixer->afilter)
    return;
  af_control_any_rev(mixer->afilter,
      AF_CONTROL_PAN_BALANCE | AF_CONTROL_GET, val);
}

void mixer_setbalance(mixer_t *mixer, float val)
{
  float level[AF_NCH];
  int i;
  af_control_ext_t arg_ext = { .arg = level };
  af_instance_t* af_pan_balance;

  if(!mixer->afilter)
    return;
  if (af_control_any_rev(mixer->afilter,
	AF_CONTROL_PAN_BALANCE | AF_CONTROL_SET, &val))
    return;

  if (!(af_pan_balance = af_add(mixer->afilter, "pan"))) {
    mp_msg(MSGT_GLOBAL, MSGL_ERR, MSGTR_NoBalance);
    return;
  }

  af_init(mixer->afilter);
  /* make all other channels pass thru since by default pan blocks all */
  memset(level, 0, sizeof(level));
  for (i = 2; i < AF_NCH; i++) {
    arg_ext.ch = i;
    level[i] = 1.f;
    af_pan_balance->control(af_pan_balance,
	AF_CONTROL_PAN_LEVEL | AF_CONTROL_SET, &arg_ext);
    level[i] = 0.f;
  }

  af_pan_balance->control(af_pan_balance,
      AF_CONTROL_PAN_BALANCE | AF_CONTROL_SET, &val);
}
