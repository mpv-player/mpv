#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "../config.h"
#include "../mp_msg.h"

#include "af.h"

// Static list of filters
extern af_info_t af_info_dummy;
extern af_info_t af_info_delay;
extern af_info_t af_info_channels;
extern af_info_t af_info_format;
extern af_info_t af_info_resample;

static af_info_t* filter_list[]={ \
   &af_info_dummy,\
   &af_info_delay,\
   &af_info_channels,\
   &af_info_format,\
   &af_info_resample,\
   NULL \
};

// Command line config switches
af_cfg_t af_cfg={\
   0,\
   0,\
   0,\
   0,\
   NULL,\
};


// Initialization types
#define SLOW  1
#define FAST  2
#define FORCE 3

// The first and last filter in the list
static af_instance_t* first=NULL;
static af_instance_t* last=NULL;
// Storage for input and output data formats (set by init)
static af_data_t input;
static af_data_t output;

/* Find a filter in the static list of filters using it's name. This
   function is used internally */
af_info_t* af_find(char*name)
{
  int i=0;
  while(filter_list[i]){
    if(!strcmp(filter_list[i]->name,name))
      return filter_list[i];
    i++;
  }
  mp_msg(MSGT_AFILTER,MSGL_ERR,"Couldn't find audio filter '%s'\n",name);
  return NULL;
} 

// Function for creating a new filter of type name
af_instance_t* af_create(char* name)
{
  // Allocate space for the new filter and reset all pointers
  af_instance_t* new=malloc(sizeof(af_instance_t));
  if(!new){
    mp_msg(MSGT_AFILTER,MSGL_ERR,"Could not allocate memory\n");
    return NULL;
  }  
  memset(new,0,sizeof(af_instance_t));

  // Find filter from name
  new->info=af_find(name);
    
  // Initialize the new filter
  if(new->info && (AF_OK==new->info->open(new))) 
    return new;

  free(new);
  mp_msg(MSGT_AFILTER,MSGL_ERR,"Couldn't create audio filter '%s'\n",name);  
  return NULL;
}

/* Create and insert a new filter of type name before the filter in the
   argument. This function can be called during runtime, the return
   value is the new filter */
af_instance_t* af_prepend(af_instance_t* af, char* name)
{
  // Create the new filter and make sure it is ok
  af_instance_t* new=af_create(name);
  if(!new)
    return NULL;
  // Update pointers
  new->next=af;
  if(af){
    new->prev=af->prev;
    af->prev=new;
  }
  else
    last=new;
  if(new->prev)
    new->prev->next=new;
  else
    first=new;
  return new;
}

/* Create and insert a new filter of type name after the filter in the
   argument. This function can be called during runtime, the return
   value is the new filter */
af_instance_t* af_append(af_instance_t* af, char* name)
{
  // Create the new filter and make sure it is OK
  af_instance_t* new=af_create(name);
  if(!new)
    return NULL;
  // Update pointers
  new->prev=af;
  if(af){
    new->next=af->next;
    af->next=new;
  }
  else
    first=new;
  if(new->next)
    new->next->prev=new;
  else
    last=new;
  return new;
}

// Uninit and remove the filter "af"
void af_remove(af_instance_t* af)
{
  if(!af) return;

  // Detach pointers
  if(af->prev)
    af->prev->next=af->next;
  else
    first=af->next;
  if(af->next)
    af->next->prev=af->prev;
  else
    last=af->prev;

  // Uninitialize af and free memory   
  af->uninit(af);
  free(af);
}

/* Reinitializes all filters downstream from the filter given in the argument */
int af_reinit(af_instance_t* af)
{
  if(!af)
    return AF_ERROR;

  do{
    af_data_t in; // Format of the input to current filter
    int rv=0; // Return value

    // Check if this is the first filter 
    if(!af->prev) 
      memcpy(&in,&input,sizeof(af_data_t));
    else
      memcpy(&in,af->prev->data,sizeof(af_data_t));
    // Reset just in case...
    in.audio=NULL;
    in.len=0;
    
    rv = af->control(af,AF_CONTROL_REINIT,&in);
    switch(rv){
    case AF_OK:
      break;
    case AF_FALSE:{ // Configuration filter is needed
      af_instance_t* new = NULL;
      // Insert channels filter
      if((af->prev?af->prev->data->nch:input.nch) != in.nch){
	// Create channels filter
	if(NULL == (new = af_prepend(af,"channels")))
	  return AF_ERROR;
	// Set number of output channels
	if(AF_OK != (rv = new->control(new,AF_CONTROL_CHANNELS,&in.nch)))
	  return rv;
	// Initialize channels filter
	if(!new->prev) 
	  memcpy(&in,&input,sizeof(af_data_t));
	else
	  memcpy(&in,new->prev->data,sizeof(af_data_t));
	if(AF_OK != (rv = new->control(new,AF_CONTROL_REINIT,&in)))
	  return rv;
      }
      // Insert format filter
      if(((af->prev?af->prev->data->format:input.format) != in.format) || 
	 ((af->prev?af->prev->data->bps:input.bps) != in.bps)){
	// Create format filter
	if(NULL == (new = af_prepend(af,"format")))
	  return AF_ERROR;
	// Set output format
	if(AF_OK != (rv = new->control(new,AF_CONTROL_FORMAT,&in)))
	  return rv;
	// Initialize format filter
	if(!new->prev) 
	  memcpy(&in,&input,sizeof(af_data_t));
	else
	  memcpy(&in,new->prev->data,sizeof(af_data_t));
	if(AF_OK != (rv = new->control(new,AF_CONTROL_REINIT,&in)))
	  return rv;
      }
      if(!new) // Should _never_ happen
	return AF_ERROR;
      af=new;
      break;
    }
    case AF_DETACH:{ // Filter is redundant and wants to be unloaded
      af_instance_t* aft=af->prev;
      af_remove(af);
      if(aft)
	af=aft;
      else
	af=first; // Restart configuration
      break;
    }
    default:
      mp_msg(MSGT_AFILTER,MSGL_ERR,"Reinit did not work, audio filter '%s' returned error code %i\n",af->info->name,rv);
      return AF_ERROR;
    }
    af=af->next;
  }while(af);
  return AF_OK;
}

/* Find filter in the dynamic filter list using it's name This
   function is used for finding already initialized filters */
af_instance_t* af_get(char* name)
{
  af_instance_t* af=first; 
  while(af->next != NULL){
    if(!strcmp(af->info->name,name))
      return af;
    af=af->next;
  }
  return NULL;
}

// Uninit and remove all filters
void af_uninit()
{
  while(first)
    af_remove(first);
}

/* Init read configuration and create filter list accordingly. In and
   out contains the format of the current movie and the formate of the
   preferred output respectively */
int af_init(af_data_t* in, af_data_t* out)
{
  int cfg=SLOW;  // configuration type
  int i=0;

  // Precaution in case caller is misbehaving
  in->audio  = out->audio  = NULL;
  in->len    = out->len    = 0;

  // Figure out how fast the machine is
  if(af_cfg.force)
    cfg=af_cfg.force;
  else{
#    if defined(HAVE_SSE) || defined(HAVE_3DNOWEX)
      cfg=FAST;
#    else
      cfg=SLOW;
#    endif
  }
  
  // Input and output configuration 
  memcpy(&input,in,sizeof(af_data_t));
  memcpy(&output,out,sizeof(af_data_t));

  // Check if this is the first call
  if(!first){
    // Add all filters in the list (if there are any)
    if(!af_cfg.list){
      if(!af_append(first,"dummy")) // To make automatic format conversion work
	return -1; 
    }
    else{
      while(af_cfg.list[i]){
	if(!af_append(last,af_cfg.list[i++]))
	  return -1;
      }
    }
  }
  
  // Init filters 
  if(AF_OK != af_reinit(first))
    return -1;

  // Check output format
  if(cfg!=FORCE){
    af_instance_t* af = NULL; // New filter
    // Check output frequency if not OK fix with resample
    if(last->data->rate!=output.rate){
      if(NULL==(af=af_get("resample"))){
	if(cfg==SLOW){
	  if(!strcmp(first->info->name,"format"))
	    af = af_append(first,"resample");
	  else
	    af = af_prepend(first,"resample");
	}		
	else{
	  if(!strcmp(last->info->name,"format"))
	    af = af_prepend(last,"resample");
	  else
	    af = af_append(last,"resample");
	}
      }
      // Init the new filter
      if(!af || (AF_OK != af->control(af,AF_CONTROL_RESAMPLE,&output.rate)))
	return -1;
      if(AF_OK != af_reinit(af))
      	return -1;
    }	
      
    // Check number of output channels fix if not OK
    // If needed always inserted last -> easy to screw up other filters
    if(last->data->nch!=output.nch){
      if(!strcmp(last->info->name,"format"))
	af = af_prepend(last,"channels");
      else
	af = af_append(last,"channels");
      // Init the new filter
      if(!af || (AF_OK != af->control(af,AF_CONTROL_CHANNELS,&output.nch)))
	return -1;
      if(AF_OK != af_reinit(af))
	return -1;
    }
    
    // Check output format fix if not OK
    if((last->data->format != output.format) || (last->data->bps != output.bps)){
      if(strcmp(last->info->name,"format"))
	af = af_append(last,"format");
      else
	af = last;
      // Init the new filter
      if(!af ||(AF_OK != af->control(af,AF_CONTROL_FORMAT,&output)))
	return -1;
      if(AF_OK != af_reinit(af))
	return -1;
    }

    // Re init again just in case
    if(AF_OK != af_reinit(first))
      return -1;

    if((last->data->format != output.format) || (last->data->bps != output.bps) ||
       (last->data->nch!=output.nch) || (last->data->rate!=output.rate)){
      // Something is stuffed audio out will not work 
      mp_msg(MSGT_AFILTER,MSGL_ERR,"Unable to setup filter system can not meet sound-card demands, please report this error on MPlayer development mailing list. \n");
      af_uninit();
      return -1;
    }
  }
  return 0;
}

// Filter data chunk through the filters in the list
af_data_t* af_play(af_data_t* data)
{
  af_instance_t* af=first; 
  // Iterate through all filters 
  do{
    data=af->play(af,data);
    af=af->next;
  }while(af);
  return data;
}

/* Helper function used to calculate the exact buffer length needed
   when buffers are resized */
inline int af_lencalc(frac_t mul, int len){
  register int q = len*mul.n;
  return q/mul.d + q%mul.d;
}

/* Calculate how long the output from the filters will be given the
   input length "len" */
int af_outputlen(int len)
{
  af_instance_t* af=first; 
  frac_t mul = {1,1};
  // Iterate through all filters 
  do{
    mul.n *= af->mul.n;
    mul.d *= af->mul.d;
    af=af->next;
  }while(af);
  return af_lencalc(mul,len);
}

/* Calculate how long the input to the filters should be to produce a
   certain output length, i.e. the return value of this function is
   the input length required to produce the output length "len". */
int af_inputlen(int len)
{
  af_instance_t* af=first; 
  frac_t mul = {1,1};
  // Iterate through all filters 
  do{
    mul.d *= af->mul.n;
    mul.n *= af->mul.d;
    af=af->next;
  }while(af);
  return af_lencalc(mul,len);
}

/* Helper function called by the macro with the same name this
   function should not be called directly */
inline int af_resize_local_buffer(af_instance_t* af, af_data_t* data)
{
  // Calculate new length
  register int len = af_lencalc(af->mul,data->len);
  mp_msg(MSGT_AFILTER,MSGL_V,"Reallocating memory in module %s, old len = %i, new len = %i\n",af->info->name,af->data->len,len);
  // If there is a buffer free it
  if(af->data->audio) 
    free(af->data->audio);
  // Create new buffer and check that it is OK
  af->data->audio = malloc(len);
  if(!af->data->audio){
    mp_msg(MSGT_AFILTER,MSGL_ERR,"Could not allocate memory \n");
    return AF_ERROR;
  }
  af->data->len=len;
  return AF_OK;
}
