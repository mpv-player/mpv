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

#include <math.h>
#include <string.h>

#include "common/common.h"
#include "af.h"

/* Convert to gain value from dB. Returns AF_OK if of and AF_ERROR if
 * fail. input <= -200dB will become 0 gain. */
int af_from_dB(int n, float* in, float* out, float k, float mi, float ma)
{
  int i = 0;
  // Sanity check
  if(!in || !out)
    return AF_ERROR;

  for(i=0;i<n;i++){
    if(in[i]<=-200)
      out[i]=0.0;
    else
      out[i]=pow(10.0,MPCLAMP(in[i],mi,ma)/k);
  }
  return AF_OK;
}

/* Convert from gain value to dB. Returns AF_OK if of and AF_ERROR if
 * fail. gain=0 will become -200 dB. k is just a multiplier. */
int af_to_dB(int n, float* in, float* out, float k)
{
  int i = 0;
  // Sanity check
  if(!in || !out)
    return AF_ERROR;

  for(i=0;i<n;i++){
    if(in[i] == 0.0)
      out[i]=-200.0;
    else
      out[i]=k*log10(in[i]);
  }
  return AF_OK;
}

/* Convert from ms to sample time */
int af_from_ms(int n, float* in, int* out, int rate, float mi, float ma)
{
  int i = 0;
  // Sanity check
  if(!in || !out)
    return AF_ERROR;

  for(i=0;i<n;i++)
    out[i]=(int)((float)rate * MPCLAMP(in[i],mi,ma)/1000.0);

  return AF_OK;
}

/* Convert from sample time to ms */
int af_to_ms(int n, int* in, float* out, int rate)
{
  int i = 0;
  // Sanity check
  if(!in || !out || !rate)
    return AF_ERROR;

  for(i=0;i<n;i++)
    out[i]=1000.0 * (float)in[i]/((float)rate);

  return AF_OK;
}

/*
 * test if output format matches
 *  af: audio filter
 *  out: needed format, will be overwritten by available
 *       format if they do not match
 *  returns: AF_FALSE if formats do not match, AF_OK if they match
 *
 * compares the format, rate and nch values of af->data with out
 * Note: logically, *out=*af->data always happens, because out contains the
 *       format only, no actual audio data or memory allocations. *out always
 *       contains the parameters from af->data after the function returns.
 */
int af_test_output(struct af_instance* af, struct mp_audio* out)
{
  if((af->data->format != out->format) ||
     (af->data->bps    != out->bps)    ||
     (af->data->rate   != out->rate)   ||
     !mp_chmap_equals(&af->data->channels, &out->channels)){
    *out = *af->data;
    return AF_FALSE;
  }
  return AF_OK;
}

/* Soft clipping, the sound of a dream, thanks to Jon Wattes
   post to Musicdsp.org */
float af_softclip(float a)
{
    if (a >= M_PI/2)
        return 1.0;
    else if (a <= -M_PI/2)
        return -1.0;
    else
        return sin(a);
}
