/*
 * This audio filter exports the incoming signal to other processes
 * using memory mapping. The memory mapped area contains a header:
 *   int nch,
 *   int size,
 *   unsigned long long counter (updated every time the  contents of
 *                               the area changes),
 * the rest is payload (non-interleaved).
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include "config.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "af.h"
#include "get_path.h"

#define DEF_SZ 512 // default buffer size (in samples)
#define SHARED_FILE "mplayer-af_export" /* default file name 
					   (relative to ~/.mplayer/ */

#define SIZE_HEADER (2 * sizeof(int) + sizeof(unsigned long long))

// Data for specific instances of this filter
typedef struct af_export_s
{
  unsigned long long  count; // Used for sync
  void* buf[AF_NCH]; 	// Buffers for storing the data before it is exported
  int 	sz;	      	// Size of buffer in samples
  int 	wi;  		// Write index
  int	fd;           	// File descriptor to shared memory area
  char* filename;      	// File to export data
  void* mmap_area;     	// MMap shared area
} af_export_t;


/* Initialization and runtime control
   af audio filter instance
   cmd control command
   arg argument
*/
static int control(struct af_instance_s* af, int cmd, void* arg)
{
  af_export_t* s = af->setup;
  switch (cmd){
  case AF_CONTROL_REINIT:{
    int i=0;
    int mapsize;

    // Free previous buffers
    if (s->buf && s->buf[0])
      free(s->buf[0]);

    // unmap previous area
    if(s->mmap_area)
      munmap(s->mmap_area, SIZE_HEADER + (af->data->bps*s->sz*af->data->nch));
    // close previous file descriptor
    if(s->fd)
      close(s->fd);	

    // Accept only int16_t as input format (which sucks)
    af->data->rate   = ((af_data_t*)arg)->rate;
    af->data->nch    = ((af_data_t*)arg)->nch;
    af->data->format = AF_FORMAT_S16_NE;
    af->data->bps    = 2;
	
    // If buffer length isn't set, set it to the default value
    if(s->sz == 0)
      s->sz = DEF_SZ;
	
    // Allocate new buffers (as one continuous block)
    s->buf[0] = calloc(s->sz*af->data->nch, af->data->bps);
    if(NULL == s->buf[0])
      af_msg(AF_MSG_FATAL, "[export] Out of memory\n");
    for(i = 1; i < af->data->nch; i++)
      s->buf[i] = s->buf[0] + i*s->sz*af->data->bps;
	
    // Init memory mapping
    s->fd = open(s->filename, O_RDWR | O_CREAT | O_TRUNC, 0640);
    af_msg(AF_MSG_INFO, "[export] Exporting to file: %s\n", s->filename);
    if(s->fd < 0)
      af_msg(AF_MSG_FATAL, "[export] Could not open/create file: %s\n", 
	     s->filename);
    
    // header + buffer
    mapsize = (SIZE_HEADER + (af->data->bps * s->sz * af->data->nch));
    
    // grow file to needed size
    for(i = 0; i < mapsize; i++){
      char null = 0;
      write(s->fd, (void*) &null, 1);
    }
	
    // mmap size
    s->mmap_area = mmap(0, mapsize, PROT_READ|PROT_WRITE,MAP_SHARED, s->fd, 0);
    if(s->mmap_area == NULL)
      af_msg(AF_MSG_FATAL, "[export] Could not mmap file %s\n", s->filename);
    af_msg(AF_MSG_INFO, "[export] Memory mapped to file: %s (%p)\n", 
	   s->filename, s->mmap_area);

    // Initialize header
    *((int*)s->mmap_area) = af->data->nch;
    *((int*)s->mmap_area + 1) = s->sz * af->data->bps * af->data->nch;
    msync(s->mmap_area, mapsize, MS_ASYNC);

    // Use test_output to return FALSE if necessary
    return af_test_output(af, (af_data_t*)arg);
  }
  case AF_CONTROL_COMMAND_LINE:{
    int i=0;
    char *str = arg;
    
    if (!str){
      if(s->filename) 
	free(s->filename);

      s->filename = get_path(SHARED_FILE);
      return AF_OK;
    }
	
    while((str[i]) && (str[i] != ':'))
      i++;

    if(s->filename)
      free(s->filename);

    s->filename = calloc(i + 1, 1);
    memcpy(s->filename, str, i);
    s->filename[i] = 0;
	
    sscanf(str + i + 1, "%d", &(s->sz));
  
    return af->control(af, AF_CONTROL_EXPORT_SZ | AF_CONTROL_SET, &s->sz);
  }
  case AF_CONTROL_EXPORT_SZ | AF_CONTROL_SET:
    s->sz = * (int *) arg;
    if((s->sz <= 0) || (s->sz > 2048))
      af_msg( AF_MSG_ERROR, "[export] Buffer size must be between"
	      " 1 and 2048\n" );

    return AF_OK;
  case AF_CONTROL_EXPORT_SZ | AF_CONTROL_GET:
    *(int*) arg = s->sz;
    return AF_OK;
      
  }
  return AF_UNKNOWN;
}

/* Free allocated memory and clean up other stuff too.
   af audio filter instance
*/
static void uninit( struct af_instance_s* af )
{
  if (af->data){
    free(af->data);
    af->data = NULL;
  }

  if(af->setup){
    af_export_t* s = af->setup;
    if (s->buf && s->buf[0])
      free(s->buf[0]);
    
    // Free mmaped area
    if(s->mmap_area)
      munmap(s->mmap_area, sizeof(af_export_t));	  

    if(s->fd > -1)
      close(s->fd);

    if(s->filename)
	free(s->filename);

    free(af->setup);
    af->setup = NULL;
  }
}

/* Filter data through filter
   af audio filter instance
   data audio data
*/
static af_data_t* play( struct af_instance_s* af, af_data_t* data )
{
  af_data_t*   	c   = data;	     // Current working data
  af_export_t* 	s   = af->setup;     // Setup for this instance
  int16_t* 	a   = c->audio;	     // Incomming sound
  int 		nch = c->nch;	     // Number of channels
  int		len = c->len/c->bps; // Number of sample in data chunk
  int 		sz  = s->sz;         // buffer size (in samples)
  int 		flag = 0;	     // Set to 1 if buffer is filled
  
  int 		ch, i;

  // Fill all buffers
  for(ch = 0; ch < nch; ch++){
    int 	wi = s->wi;    	 // Reset write index
    int16_t* 	b  = s->buf[ch]; // Current buffer 

    // Copy data to export buffers 
    for(i = ch; i < len; i += nch){
      b[wi++] = a[i];
      if(wi >= sz){ // Don't write outside the end of the buffer
	flag = 1;
	break;
      }
    }
    s->wi = wi % s->sz;
  }

  // Export buffer to mmaped area
  if(flag){
    // update buffer in mapped area
    memcpy(s->mmap_area + SIZE_HEADER, s->buf[0], sz * c->bps * nch);
    s->count++; // increment counter (to sync)
    memcpy(s->mmap_area + SIZE_HEADER - sizeof(s->count), 
	   &(s->count), sizeof(s->count));
  }

  // We don't modify data, just export it
  return data;
}

/* Allocate memory and set function pointers
   af audio filter instance 
   returns AF_OK or AF_ERROR
*/
static int af_open( af_instance_t* af )
{
  af->control = control;
  af->uninit  = uninit;
  af->play    = play;
  af->mul=1;
  af->data    = calloc(1, sizeof(af_data_t));
  af->setup   = calloc(1, sizeof(af_export_t));
  if((af->data == NULL) || (af->setup == NULL))
    return AF_ERROR;

  ((af_export_t *)af->setup)->filename = get_path(SHARED_FILE);

  return AF_OK;
}

// Description of this filter
af_info_t af_info_export = {
    "Sound export filter",
    "export",
    "Anders; Gustavo Sverzut Barbieri <gustavo.barbieri@ic.unicamp.br>",
    "",
    AF_FLAGS_REENTRANT,
    af_open
};
