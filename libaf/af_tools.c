#include <math.h>
#include <string.h> 
#include <af.h>

/* Convert to gain value from dB. Returns AF_OK if of and AF_ERROR if
   fail */
inline int af_from_dB(int n, float* in, float* out, float k, float mi, float ma)
{
  int i = 0; 
  // Sanity check
  if(!in || !out) 
    return AF_ERROR;

  for(i=0;i<n;i++){
    if(in[i]<=-200)
      out[i]=0.0;
    else
      out[i]=pow(10.0,clamp(in[i],mi,ma)/k);
  }
  return AF_OK;
}

/* Convert from gain value to dB. Returns AF_OK if of and AF_ERROR if
   fail */
inline int af_to_dB(int n, float* in, float* out, float k)
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
inline int af_from_ms(int n, float* in, int* out, int rate, float mi, float ma)
{
  int i = 0; 
  // Sanity check
  if(!in || !out) 
    return AF_ERROR;

  for(i=0;i<n;i++)
    out[i]=(int)((float)rate * clamp(in[i],mi,ma)/1000.0);

  return AF_OK;
}

/* Convert from sample time to ms */
inline int af_to_ms(int n, int* in, float* out, int rate)
{
  int i = 0; 
  // Sanity check
  if(!in || !out || !rate) 
    return AF_ERROR;

  for(i=0;i<n;i++)
    out[i]=1000.0 * (float)in[i]/((float)rate);
  
  return AF_OK;
}

/* Helper function for testing the output format */
inline int af_test_output(struct af_instance_s* af, af_data_t* out)
{
  if((af->data->format != out->format) || 
     (af->data->bps    != out->bps)    ||
     (af->data->rate   != out->rate)   ||
     (af->data->nch    != out->nch)){
    memcpy(out,af->data,sizeof(af_data_t));
    return AF_FALSE;
  }
  return AF_OK;
}

/* Soft clipping, the sound of a dream, thanks to Jon Wattes
   post to Musicdsp.org */
inline float af_softclip(float a)
{
    if (a >= M_PI/2)
	return 1.0;
    else if (a <= -M_PI/2)
	return -1.0;
    else
	return sin(a);
}
