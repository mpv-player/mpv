/*
 * This audio filter exports the incoming signal to other processes
 * using memory mapping. The memory mapped area contains a header:
 *   int nch,
 *   int size,
 *   unsigned long long counter (updated every time the  contents of
 *                               the area changes),
 * the rest is payload (non-interleaved).
 *
 * Authors: Anders; Gustavo Sverzut Barbieri <gustavo.barbieri@ic.unicamp.br>
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include "config.h"

#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "osdep/io.h"

#include "talloc.h"
#include "af.h"
#include "options/path.h"

#define DEF_SZ 512 // default buffer size (in samples)
#define SHARED_FILE "mpv-af_export" /* default file name
                                           (relative to ~/.mpv/ */

#define SIZE_HEADER (2 * sizeof(int) + sizeof(unsigned long long))

// Data for specific instances of this filter
typedef struct af_export_s
{
  unsigned long long  count; // Used for sync
  void* buf[AF_NCH];    // Buffers for storing the data before it is exported
  int   sz;             // Size of buffer in samples
  int   wi;             // Write index
  int   fd;             // File descriptor to shared memory area
  char* filename;       // File to export data
  uint8_t *mmap_area;   // MMap shared area
} af_export_t;


/* Initialization and runtime control
   af audio filter instance
   cmd control command
   arg argument
*/
static int control(struct af_instance* af, int cmd, void* arg)
{
  af_export_t* s = af->priv;
  switch (cmd){
  case AF_CONTROL_REINIT:{
    int i=0;
    int mapsize;

    // Free previous buffers
    free(s->buf[0]);

    // unmap previous area
    if(s->mmap_area)
      munmap(s->mmap_area, SIZE_HEADER + (af->data->bps*s->sz*af->data->nch));
    // close previous file descriptor
    if(s->fd)
      close(s->fd);

    // Accept only int16_t as input format (which sucks)
    mp_audio_copy_config(af->data, (struct mp_audio*)arg);
    mp_audio_set_format(af->data, AF_FORMAT_S16);

    // Allocate new buffers (as one continuous block)
    s->buf[0] = calloc(s->sz*af->data->nch, af->data->bps);
    if(NULL == s->buf[0]) {
      MP_FATAL(af, "Out of memory\n");
      return AF_ERROR;
    }
    for(i = 1; i < af->data->nch; i++)
      s->buf[i] = (uint8_t *)s->buf[0] + i*s->sz*af->data->bps;

    if (!s->filename) {
        MP_FATAL(af, "No filename set.\n");
        return AF_ERROR;
    }

    // Init memory mapping
    s->fd = open(s->filename, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0640);
    MP_INFO(af, "Exporting to file: %s\n", s->filename);
    if(s->fd < 0) {
      MP_FATAL(af, "Could not open/create file: %s\n",
             s->filename);
      return AF_ERROR;
    }

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
      MP_FATAL(af, "Could not mmap file %s\n", s->filename);
    MP_INFO(af, "Memory mapped to file: %s (%p)\n",
           s->filename, s->mmap_area);

    // Initialize header
    *((int*)s->mmap_area) = af->data->nch;
    *((int*)s->mmap_area + 1) = s->sz * af->data->bps * af->data->nch;
    msync(s->mmap_area, mapsize, MS_ASYNC);

    // Use test_output to return FALSE if necessary
    return af_test_output(af, (struct mp_audio*)arg);
  }
  }
  return AF_UNKNOWN;
}

/* Free allocated memory and clean up other stuff too.
   af audio filter instance
*/
static void uninit( struct af_instance* af )
{
    af_export_t* s = af->priv;

    free(s->buf[0]);

    // Free mmaped area
    if(s->mmap_area)
      munmap(s->mmap_area, sizeof(af_export_t));

    if(s->fd > -1)
      close(s->fd);
}

/* Filter data through filter
   af audio filter instance
   data audio data
*/
static int filter(struct af_instance *af, struct mp_audio *data)
{
  if (!data)
    return 0;
  struct mp_audio*      c   = data;          // Current working data
  af_export_t*  s   = af->priv;     // Setup for this instance
  int16_t*      a   = c->planes[0];          // Incomming sound
  int           nch = c->nch;        // Number of channels
  int           len = c->samples*c->nch; // Number of sample in data chunk
  int           sz  = s->sz;         // buffer size (in samples)
  int           flag = 0;            // Set to 1 if buffer is filled

  int           ch, i;

  // Fill all buffers
  for(ch = 0; ch < nch; ch++){
    int         wi = s->wi;      // Reset write index
    int16_t*    b  = s->buf[ch]; // Current buffer

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

  af_add_output_frame(af, data);
  return 0;
}

/* Allocate memory and set function pointers
   af audio filter instance
   returns AF_OK or AF_ERROR
*/
static int af_open( struct af_instance* af )
{
  af->control = control;
  af->uninit  = uninit;
  af->filter_frame = filter;
  af_export_t *priv = af->priv;

  if (!priv->filename || !priv->filename[0]) {
      MP_FATAL(af, "no export filename given");
      return AF_ERROR;
  }

  return AF_OK;
}

#define OPT_BASE_STRUCT af_export_t
const struct af_info af_info_export = {
    .info = "Sound export filter",
    .name = "export",
    .open = af_open,
    .priv_size = sizeof(af_export_t),
    .options = (const struct m_option[]) {
        OPT_STRING("filename", filename, 0),
        OPT_INTRANGE("buffersamples", sz, 0, 1, 2048, OPTDEF_INT(DEF_SZ)),
        {0}
    },
};
