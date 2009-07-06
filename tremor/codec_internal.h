/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2002    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: libvorbis codec headers

 ********************************************************************/

#ifndef _V_CODECI_H_
#define _V_CODECI_H_

#include "codebook.h"

typedef void vorbis_look_mapping;
typedef void vorbis_look_floor;
typedef void vorbis_look_residue;
typedef void vorbis_look_transform;

/* mode ************************************************************/
typedef struct {
  int blockflag;
  int windowtype;
  int transformtype;
  int mapping;
} vorbis_info_mode;

typedef void vorbis_info_floor;
typedef void vorbis_info_residue;
typedef void vorbis_info_mapping;

typedef struct backend_lookup_state {
  /* local lookup storage */
  ogg_int32_t               *window[2];

  /* backend lookups are tied to the mode, not the backend or naked mapping */
  int                     modebits;
  vorbis_look_mapping   **mode;

} backend_lookup_state;

/* codec_setup_info contains all the setup information specific to the
   specific compression/decompression mode in progress (eg,
   psychoacoustic settings, channel setup, options, codebook
   etc).
*********************************************************************/

typedef struct codec_setup_info {

  /* Vorbis supports only short and long blocks, but allows the
     encoder to choose the sizes */

  long blocksizes[2];

  /* modes are the primary means of supporting on-the-fly different
     blocksizes, different channel mappings (LR or M/A),
     different residue backends, etc.  Each mode consists of a
     blocksize flag and a mapping (along with the mapping setup */

  int        modes;
  int        maps;
  int        times;
  int        floors;
  int        residues;
  int        books;

  vorbis_info_mode       *mode_param[64];
  int                     map_type[64];
  vorbis_info_mapping    *map_param[64];
  int                     time_type[64];
  int                     floor_type[64];
  vorbis_info_floor      *floor_param[64];
  int                     residue_type[64];
  vorbis_info_residue    *residue_param[64];
  static_codebook        *book_param[256];
  codebook               *fullbooks;

  int    passlimit[32];     /* iteration limit per couple/quant pass */
  int    coupling_passes;
} codec_setup_info;

#endif
