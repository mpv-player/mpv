#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "af.h"

// Static list of filters
extern af_info_t af_info_dummy;
extern af_info_t af_info_delay;
extern af_info_t af_info_channels;
extern af_info_t af_info_format;
extern af_info_t af_info_resample;
extern af_info_t af_info_volume;
extern af_info_t af_info_equalizer;
extern af_info_t af_info_gate;
extern af_info_t af_info_comp;
extern af_info_t af_info_pan;
extern af_info_t af_info_surround;
extern af_info_t af_info_sub;
extern af_info_t af_info_export;
extern af_info_t af_info_volnorm;
extern af_info_t af_info_extrastereo;
extern af_info_t af_info_lavcresample;
extern af_info_t af_info_sweep;
extern af_info_t af_info_hrtf;
extern af_info_t af_info_ladspa;
extern af_info_t af_info_center;
extern af_info_t af_info_sinesuppress;
extern af_info_t af_info_karaoke;

static af_info_t* filter_list[]={ 
   &af_info_dummy,
   &af_info_delay,
   &af_info_channels,
   &af_info_format,
   &af_info_resample,
   &af_info_volume,
   &af_info_equalizer,
   &af_info_gate,
   &af_info_comp,
   &af_info_pan,
   &af_info_surround,
   &af_info_sub,
#ifdef HAVE_SYS_MMAN_H
   &af_info_export,
#endif
   &af_info_volnorm,
   &af_info_extrastereo,
#ifdef USE_LIBAVCODEC
   &af_info_lavcresample,
#endif
   &af_info_sweep,
   &af_info_hrtf,
#ifdef HAVE_LADSPA
   &af_info_ladspa,
#endif
   &af_info_center,
   &af_info_sinesuppress,
   &af_info_karaoke,
   NULL 
};

// Message printing
af_msg_cfg_t af_msg_cfg={0,NULL,NULL};

// CPU speed
int* af_cpu_speed = NULL;

/* Find a filter in the static list of filters using it's name. This
   function is used internally */
static af_info_t* af_find(char*name)
{
  int i=0;
  while(filter_list[i]){
    if(!strcmp(filter_list[i]->name,name))
      return filter_list[i];
    i++;
  }
  af_msg(AF_MSG_ERROR,"Couldn't find audio filter '%s'\n",name);
  return NULL;
} 

/* Find filter in the dynamic filter list using it's name This
   function is used for finding already initialized filters */
af_instance_t* af_get(af_stream_t* s, char* name)
{
  af_instance_t* af=s->first; 
  // Find the filter
  while(af != NULL){
    if(!strcmp(af->info->name,name))
      return af;
    af=af->next;
  }
  return NULL;
}

/*/ Function for creating a new filter of type name. The name may
  contain the commandline parameters for the filter */
static af_instance_t* af_create(af_stream_t* s, char* name)
{
  char* cmdline = name;

  // Allocate space for the new filter and reset all pointers
  af_instance_t* new=malloc(sizeof(af_instance_t));
  if(!new){
    af_msg(AF_MSG_ERROR,"[libaf] Could not allocate memory\n");
    goto err_out;
  }  
  memset(new,0,sizeof(af_instance_t));

  // Check for commandline parameters
  strsep(&cmdline, "=");

  // Find filter from name
  if(NULL == (new->info=af_find(name)))
    goto err_out;

  /* Make sure that the filter is not already in the list if it is
     non-reentrant */
  if(new->info->flags & AF_FLAGS_NOT_REENTRANT){
    if(af_get(s,name)){
      af_msg(AF_MSG_ERROR,"[libaf] There can only be one instance of" 
	     " the filter '%s' in each stream\n",name);  
      goto err_out;
    }
  }
  
  af_msg(AF_MSG_VERBOSE,"[libaf] Adding filter %s \n",name);
  
  // Initialize the new filter
  if(AF_OK == new->info->open(new) && 
     AF_ERROR < new->control(new,AF_CONTROL_POST_CREATE,&s->cfg)){
    if(cmdline){
      if(AF_ERROR<new->control(new,AF_CONTROL_COMMAND_LINE,cmdline))
	return new;
    }
    else
      return new; 
  }
  
err_out:
  free(new);
  af_msg(AF_MSG_ERROR,"[libaf] Couldn't create or open audio filter '%s'\n",
	 name);  
  return NULL;
}

/* Create and insert a new filter of type name before the filter in the
   argument. This function can be called during runtime, the return
   value is the new filter */
static af_instance_t* af_prepend(af_stream_t* s, af_instance_t* af, char* name)
{
  // Create the new filter and make sure it is OK
  af_instance_t* new=af_create(s,name);
  if(!new)
    return NULL;
  // Update pointers
  new->next=af;
  if(af){
    new->prev=af->prev;
    af->prev=new;
  }
  else
    s->last=new;
  if(new->prev)
    new->prev->next=new;
  else
    s->first=new;
  return new;
}

/* Create and insert a new filter of type name after the filter in the
   argument. This function can be called during runtime, the return
   value is the new filter */
static af_instance_t* af_append(af_stream_t* s, af_instance_t* af, char* name)
{
  // Create the new filter and make sure it is OK
  af_instance_t* new=af_create(s,name);
  if(!new)
    return NULL;
  // Update pointers
  new->prev=af;
  if(af){
    new->next=af->next;
    af->next=new;
  }
  else
    s->first=new;
  if(new->next)
    new->next->prev=new;
  else
    s->last=new;
  return new;
}

// Uninit and remove the filter "af"
void af_remove(af_stream_t* s, af_instance_t* af)
{
  if(!af) return;

  // Print friendly message 
  af_msg(AF_MSG_VERBOSE,"[libaf] Removing filter %s \n",af->info->name); 

  // Notify filter before changing anything
  af->control(af,AF_CONTROL_PRE_DESTROY,0);

  // Detach pointers
  if(af->prev)
    af->prev->next=af->next;
  else
    s->first=af->next;
  if(af->next)
    af->next->prev=af->prev;
  else
    s->last=af->prev;

  // Uninitialize af and free memory   
  af->uninit(af);
  free(af);
}

/* Reinitializes all filters downstream from the filter given in the
   argument the return value is AF_OK if success and AF_ERROR if
   failure */
static int af_reinit(af_stream_t* s, af_instance_t* af)
{
  do{
    af_data_t in; // Format of the input to current filter
    int rv=0; // Return value

    // Check if there are any filters left in the list
    if(NULL == af){
      if(!(af=af_append(s,s->first,"dummy"))) 
	return AF_UNKNOWN; 
      else
	return AF_ERROR;
    }

    // Check if this is the first filter 
    if(!af->prev) 
      memcpy(&in,&(s->input),sizeof(af_data_t));
    else
      memcpy(&in,af->prev->data,sizeof(af_data_t));
    // Reset just in case...
    in.audio=NULL;
    in.len=0;
    
    rv = af->control(af,AF_CONTROL_REINIT,&in);
    switch(rv){
    case AF_OK:
	af = af->next;
      break;
    case AF_FALSE:{ // Configuration filter is needed
      // Do auto insertion only if force is not specified
      if((AF_INIT_TYPE_MASK & s->cfg.force) != AF_INIT_FORCE){
	af_instance_t* new = NULL;
	// Insert channels filter
	if((af->prev?af->prev->data->nch:s->input.nch) != in.nch){
	  // Create channels filter
	  if(NULL == (new = af_prepend(s,af,"channels")))
	    return AF_ERROR;
	  // Set number of output channels
	  if(AF_OK != (rv = new->control(new,AF_CONTROL_CHANNELS,&in.nch)))
	    return rv;
	  // Initialize channels filter
	  if(!new->prev) 
	    memcpy(&in,&(s->input),sizeof(af_data_t));
	  else
	    memcpy(&in,new->prev->data,sizeof(af_data_t));
	  if(AF_OK != (rv = new->control(new,AF_CONTROL_REINIT,&in)))
	    return rv;
	}
	// Insert format filter
	if((af->prev?af->prev->data->format:s->input.format) != in.format){
	  // Create format filter
	  if(NULL == (new = af_prepend(s,af,"format")))
	    return AF_ERROR;
	  // Set output bits per sample
	  in.format |= af_bits2fmt(in.bps*8);
	  if(AF_OK != (rv = new->control(new,AF_CONTROL_FORMAT_FMT,&in.format)))
	    return rv;
	  // Initialize format filter
	  if(!new->prev) 
	    memcpy(&in,&(s->input),sizeof(af_data_t));
	  else
	    memcpy(&in,new->prev->data,sizeof(af_data_t));
	  if(AF_OK != (rv = new->control(new,AF_CONTROL_REINIT,&in)))
	    return rv;
	}
	if(!new){ // Should _never_ happen
	  af_msg(AF_MSG_ERROR,"[libaf] Unable to correct audio format. " 
		 "This error should never uccur, please send bugreport.\n");
	  return AF_ERROR;
	}
	af=new->next;
      }
      else {
        af_msg(AF_MSG_ERROR, "[libaf] Automatic filter insertion disabled "
               "but formats do not match. Giving up.\n");
        return AF_ERROR;
      }
      break;
    }
    case AF_DETACH:{ // Filter is redundant and wants to be unloaded
      // Do auto remove only if force is not specified
      if((AF_INIT_TYPE_MASK & s->cfg.force) != AF_INIT_FORCE){
	af_instance_t* aft=af->prev;
	af_remove(s,af);
	if(aft)
	  af=aft->next;
	else
	  af=s->first; // Restart configuration
      }
      break;
    }
    default:
      af_msg(AF_MSG_ERROR,"[libaf] Reinitialization did not work, audio" 
	     " filter '%s' returned error code %i\n",af->info->name,rv);
      return AF_ERROR;
    }
  }while(af);
  return AF_OK;
}

// Uninit and remove all filters
void af_uninit(af_stream_t* s)
{
  while(s->first)
    af_remove(s,s->first);
}

/* Initialize the stream "s". This function creates a new filter list
   if necessary according to the values set in input and output. Input
   and output should contain the format of the current movie and the
   formate of the preferred output respectively. The function is
   reentrant i.e. if called with an already initialized stream the
   stream will be reinitialized.
   If one of the prefered output parameters is 0 the one that needs
   no conversion is used (i.e. the output format in the last filter).
   The return value is 0 if success and -1 if failure */
int af_init(af_stream_t* s)
{
  int i=0;

  // Sanity check
  if(!s) return -1;

  // Precaution in case caller is misbehaving
  s->input.audio  = s->output.audio  = NULL;
  s->input.len    = s->output.len    = 0;

  // Figure out how fast the machine is
  if(AF_INIT_AUTO == (AF_INIT_TYPE_MASK & s->cfg.force))
    s->cfg.force = (s->cfg.force & ~AF_INIT_TYPE_MASK) | AF_INIT_TYPE;

  // Check if this is the first call
  if(!s->first){
    // Add all filters in the list (if there are any)
    if(!s->cfg.list){      // To make automatic format conversion work
      if(!af_append(s,s->first,"dummy")) 
	return -1; 
    }
    else{
      while(s->cfg.list[i]){
	if(!af_append(s,s->last,s->cfg.list[i++]))
	  return -1;
      }
    }
  }

  // Init filters 
  if(AF_OK != af_reinit(s,s->first))
    return -1;

  // make sure the chain is not empty and valid (e.g. because of AF_DETACH)
  if (!s->first)
    if (!af_append(s,s->first,"dummy") || AF_OK != af_reinit(s,s->first))
      return -1;

  // Check output format
  if((AF_INIT_TYPE_MASK & s->cfg.force) != AF_INIT_FORCE){
    af_instance_t* af = NULL; // New filter
    // Check output frequency if not OK fix with resample
    if(s->output.rate && s->last->data->rate!=s->output.rate){
      // try to find a filter that can change samplrate
      af = af_control_any_rev(s, AF_CONTROL_RESAMPLE_RATE | AF_CONTROL_SET,
               &(s->output.rate));
      if (!af) {
        char *resampler = "resample";
#ifdef USE_LIBAVCODEC
        if ((AF_INIT_TYPE_MASK & s->cfg.force) == AF_INIT_SLOW)
          resampler = "lavcresample";
#endif
	if((AF_INIT_TYPE_MASK & s->cfg.force) == AF_INIT_SLOW){
	  if(!strcmp(s->first->info->name,"format"))
	    af = af_append(s,s->first,resampler);
	  else
	    af = af_prepend(s,s->first,resampler);
	}		
	else{
	  if(!strcmp(s->last->info->name,"format"))
	    af = af_prepend(s,s->last,resampler);
	  else
	    af = af_append(s,s->last,resampler);
	}
      // Init the new filter
      if(!af || (AF_OK != af->control(af,AF_CONTROL_RESAMPLE_RATE | AF_CONTROL_SET,
				      &(s->output.rate))))
	return -1;
      // Use lin int if the user wants fast
      if ((AF_INIT_TYPE_MASK & s->cfg.force) == AF_INIT_FAST) {
        char args[32];
	sprintf(args, "%d", s->output.rate);
#ifdef USE_LIBAVCODEC
	if (strcmp(resampler, "lavcresample") == 0)
	  strcat(args, ":1");
	else
#endif
	strcat(args, ":0:0");
	af->control(af, AF_CONTROL_COMMAND_LINE, args);
      }
      }
      if(AF_OK != af_reinit(s,af))
      	return -1;
    }	
      
    // Check number of output channels fix if not OK
    // If needed always inserted last -> easy to screw up other filters
    if(s->output.nch && s->last->data->nch!=s->output.nch){
      if(!strcmp(s->last->info->name,"format"))
	af = af_prepend(s,s->last,"channels");
      else
	af = af_append(s,s->last,"channels");
      // Init the new filter
      if(!af || (AF_OK != af->control(af,AF_CONTROL_CHANNELS,&(s->output.nch))))
	return -1;
      if(AF_OK != af_reinit(s,af))
	return -1;
    }
    
    // Check output format fix if not OK
    if(s->output.format != AF_FORMAT_UNKNOWN &&
		s->last->data->format != s->output.format){
      if(strcmp(s->last->info->name,"format"))
	af = af_append(s,s->last,"format");
      else
	af = s->last;
      // Init the new filter
      s->output.format |= af_bits2fmt(s->output.bps*8);
      if(!af || (AF_OK != af->control(af,AF_CONTROL_FORMAT_FMT,&(s->output.format))))
	return -1;
      if(AF_OK != af_reinit(s,af))
	return -1;
    }

    // Re init again just in case
    if(AF_OK != af_reinit(s,s->first))
      return -1;

    if (s->output.format == AF_FORMAT_UNKNOWN)
	s->output.format = s->last->data->format;
    if (!s->output.nch) s->output.nch = s->last->data->nch;
    if (!s->output.rate) s->output.rate = s->last->data->rate;
    if((s->last->data->format != s->output.format) || 
       (s->last->data->nch    != s->output.nch)    || 
       (s->last->data->rate   != s->output.rate))  {
      // Something is stuffed audio out will not work 
      af_msg(AF_MSG_ERROR,"[libaf] Unable to setup filter system can not" 
	     " meet sound-card demands, please send bugreport. \n");
      af_uninit(s);
      return -1;
    }
  }
  return 0;
}

/* Add filter during execution. This function adds the filter "name"
   to the stream s. The filter will be inserted somewhere nice in the
   list of filters. The return value is a pointer to the new filter,
   If the filter couldn't be added the return value is NULL. */
af_instance_t* af_add(af_stream_t* s, char* name){
  af_instance_t* new;
  // Sanity check
  if(!s || !s->first || !name)
    return NULL;
  // Insert the filter somwhere nice
  if(!strcmp(s->first->info->name,"format"))
    new = af_append(s, s->first, name);
  else
    new = af_prepend(s, s->first, name);
  if(!new)
    return NULL;

  // Reinitalize the filter list
  if(AF_OK != af_reinit(s, s->first)){
    free(new);
    return NULL;
  }
  return new;
}

// Filter data chunk through the filters in the list
af_data_t* af_play(af_stream_t* s, af_data_t* data)
{
  af_instance_t* af=s->first; 
  // Iterate through all filters 
  do{
    if (data->len <= 0) break;
    data=af->play(af,data);
    af=af->next;
  }while(af);
  return data;
}

/* Helper function used to calculate the exact buffer length needed
   when buffers are resized. The returned length is >= than what is
   needed */
inline int af_lencalc(frac_t mul, af_data_t* d){
  register int t = d->bps*d->nch;
  return t*(((d->len/t)*mul.n)/mul.d + 1);
}

/* Calculate how long the output from the filters will be given the
   input length "len". The calculated length is >= the actual
   length. */
int af_outputlen(af_stream_t* s, int len)
{
  int t = s->input.bps*s->input.nch;
  af_instance_t* af=s->first; 
  frac_t mul = {1,1};
  // Iterate through all filters 
  do{
    af_frac_mul(&mul, &af->mul);
    af=af->next;
  }while(af);
  return t * (((len/t)*mul.n + 1)/mul.d);
}

/* Calculate how long the input to the filters should be to produce a
   certain output length, i.e. the return value of this function is
   the input length required to produce the output length "len". The
   calculated length is <= the actual length */
int af_inputlen(af_stream_t* s, int len)
{
  int t = s->input.bps*s->input.nch;
  af_instance_t* af=s->first; 
  frac_t mul = {1,1};
  // Iterate through all filters 
  do{
    af_frac_mul(&mul, &af->mul);
    af=af->next;
  }while(af);
  return t * (((len/t) * mul.d - 1)/mul.n);
}

/* Calculate how long the input IN to the filters should be to produce
   a certain output length OUT but with the following three constraints:
   1. IN <= max_insize, where max_insize is the maximum possible input
      block length
   2. OUT <= max_outsize, where max_outsize is the maximum possible
      output block length
   3. If possible OUT >= len. 
   Return -1 in case of error */ 
int af_calc_insize_constrained(af_stream_t* s, int len,
			       int max_outsize,int max_insize)
{
  int t   = s->input.bps*s->input.nch;
  int in  = 0;
  int out = 0;
  af_instance_t* af=s->first; 
  frac_t mul = {1,1};
  // Iterate through all filters and calculate total multiplication factor
  do{
    af_frac_mul(&mul, &af->mul);
    af=af->next;
  }while(af);
  // Sanity check 
  if(!mul.n || !mul.d) 
    return -1;

  in = t * (((len/t) * mul.d - 1)/mul.n);
  
  if(in>max_insize) in=t*(max_insize/t);

  // Try to meet constraint nr 3. 
  while((out=t * (((in/t+1)*mul.n - 1)/mul.d)) <= max_outsize && in<=max_insize){
    if( (t * (((in/t)*mul.n))/mul.d) >= len) return in;
    in+=t;
  }
  
  // Could no meet constraint nr 3.
  while(out > max_outsize || in > max_insize){
    in-=t;
    if(in<t) return -1; // Input parameters are probably incorrect
    out = t * (((in/t)*mul.n + 1)/mul.d);
  }
  return in;
}

/* Calculate the total delay [ms] caused by the filters */
double af_calc_delay(af_stream_t* s)
{
  af_instance_t* af=s->first; 
  register double delay = 0.0;
  // Iterate through all filters 
  while(af){
    delay += af->delay;
    af=af->next;
  }
  return delay;
}

/* Helper function called by the macro with the same name this
   function should not be called directly */
inline int af_resize_local_buffer(af_instance_t* af, af_data_t* data)
{
  // Calculate new length
  register int len = af_lencalc(af->mul,data);
  af_msg(AF_MSG_VERBOSE,"[libaf] Reallocating memory in module %s, " 
	 "old len = %i, new len = %i\n",af->info->name,af->data->len,len);
  // If there is a buffer free it
  if(af->data->audio) 
    free(af->data->audio);
  // Create new buffer and check that it is OK
  af->data->audio = malloc(len);
  if(!af->data->audio){
    af_msg(AF_MSG_FATAL,"[libaf] Could not allocate memory \n");
    return AF_ERROR;
  }
  af->data->len=len;
  return AF_OK;
}

// documentation in af.h
af_instance_t *af_control_any_rev (af_stream_t* s, int cmd, void* arg) {
  int res = AF_UNKNOWN;
  af_instance_t* filt = s->last;
  while (filt) {
    res = filt->control(filt, cmd, arg);
    if (res == AF_OK)
      return filt;
    filt = filt->prev;
  }
  return NULL;
}

/**
 * \brief calculate greatest common divisior of a and b.
 * \ingroup af_filter
 *
 *   If both are 0 the result is 1.
 */
int af_gcd(register int a, register int b) {
  while (b != 0) {
    a %= b;
    if (a == 0)
      break;
    b %= a;
  }
  // the result is either in a or b. As the other one is 0 just add them.
  a += b;
  if (!a)
    return 1;
  return a;
}

/**
 * \brief cancel down a fraction f
 * \param f fraction to cancel down
 * \ingroup af_filter
 */
void af_frac_cancel(frac_t *f) {
  int gcd = af_gcd(f->n, f->d);
  f->n /= gcd;
  f->d /= gcd;
}

/**
 * \brief multiply out by in and store result in out.
 * \param out [inout] fraction to multiply by in
 * \param in [in] fraction to multiply out by
 * \ingroup af_filter
 *
 *        the resulting fraction will be cancelled down
 *        if in and out were.
 */
void af_frac_mul(frac_t *out, const frac_t *in) {
  int gcd1 = af_gcd(out->n, in->d);
  int gcd2 = af_gcd(in->n, out->d);
  out->n = (out->n / gcd1) * (in->n / gcd2);
  out->d = (out->d / gcd2) * (in->d / gcd1);
}

void af_help (void) {
  int i = 0;
  af_msg(AF_MSG_INFO, "Available audio filters:\n");
  while (filter_list[i]) {
    if (filter_list[i]->comment && filter_list[i]->comment[0])
      af_msg(AF_MSG_INFO, "  %-15s: %s (%s)\n", filter_list[i]->name, filter_list[i]->info, filter_list[i]->comment);
    else
      af_msg(AF_MSG_INFO, "  %-15s: %s\n", filter_list[i]->name, filter_list[i]->info);
    i++;
  }
}

void af_fix_parameters(af_data_t *data)
{
    data->bps = af_fmt2bits(data->format)/8;
}
