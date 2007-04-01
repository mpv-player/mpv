/*
 * vidixlib.h
 * VIDIXLib - Library for VIDeo Interface for *niX
 *   This interface is introduced as universal one to MPEG decoder,
 *   BES == Back End Scaler and YUV2RGB hw accelerators.
 * In the future it may be expanded up to capturing and audio things.
 * Main goal of this this interface imlpementation is providing DGA
 * everywhere where it's possible (unlike X11 and other).
 * Copyright 2002 Nick Kurshev
 * Licence: GPL
 * This interface is based on v4l2, fbvid.h, mga_vid.h projects
 * and personally my ideas.
 * NOTE: This interface is introduces as APP interface.
 * Don't use it for driver.
 * It provides multistreaming. This mean that APP can handle
 * several streams simultaneously. (Example: Video capturing and video
 * playback or capturing, video playback, audio encoding and so on).
*/
#ifndef VIDIXLIB_H
#define VIDIXLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vidix.h"

typedef struct VDXDriver {
  const char *name;
  struct VDXDriver *next;
  int (* probe) (int verbose, int force);
  int (* get_caps) (vidix_capability_t *cap);
  int (*query_fourcc)(vidix_fourcc_t *);
  int (*init)(void);
  void (*destroy)(void);
  int (*config_playback)(vidix_playback_t *);
  int (*playback_on)( void );
  int (*playback_off)( void );
  /* Functions below can be missed in driver ;) */
  int (*frame_sel)( unsigned frame_idx );
  int (*get_eq)( vidix_video_eq_t * );
  int (*set_eq)( const vidix_video_eq_t * );
  int (*get_deint)( vidix_deinterlace_t * );
  int (*set_deint)( const vidix_deinterlace_t * );
  int (*copy_frame)( const vidix_dma_t * );
  int (*get_gkey)( vidix_grkey_t * );
  int (*set_gkey)( const vidix_grkey_t * );
  int (*get_num_fx)( unsigned * );
  int (*get_fx)( vidix_oem_fx_t * );
  int (*set_fx)( const vidix_oem_fx_t * );
} VDXDriver;

typedef struct VDXContext {
  VDXDriver *drv;
  /* might be filled in by much more info later on */
} VDXContext;

typedef VDXContext * VDL_HANDLE;

			/* returns library version */
unsigned   vdlGetVersion( void );

			/* Opens corresponded video driver and returns handle
			   of associated stream.
			   path - specifies path where drivers are located.
			   name - specifies prefered driver name (can be NULL).
			   cap  - specifies driver capability (TYPE_* constants).
			   verbose - specifies verbose level
			   returns !0 if ok else NULL.
			   */
VDL_HANDLE vdlOpen(const char *path,const char *name,unsigned cap,int verbose);
			/* Closes stream and corresponded driver. */
void	  vdlClose(VDL_HANDLE ctx);

			/* Queries driver capabilities. Return 0 if ok else errno */
int	  vdlGetCapability(VDL_HANDLE, vidix_capability_t *);

			/* Queries support for given fourcc. Returns 0 if ok else errno */
int	  vdlQueryFourcc(VDL_HANDLE,vidix_fourcc_t *);

			/* Returns 0 if ok else errno */
int	  vdlConfigPlayback(VDL_HANDLE, vidix_playback_t *);

			/* Returns 0 if ok else errno */
int 	  vdlPlaybackOn(VDL_HANDLE);

			/* Returns 0 if ok else errno */
int 	  vdlPlaybackOff(VDL_HANDLE);

			/* Returns 0 if ok else errno */
int 	  vdlPlaybackFrameSelect(VDL_HANDLE, unsigned frame_idx );

			/* Returns 0 if ok else errno */
int 	  vdlGetGrKeys(VDL_HANDLE, vidix_grkey_t * );

			/* Returns 0 if ok else errno */
int 	  vdlSetGrKeys(VDL_HANDLE, const vidix_grkey_t * );

			/* Returns 0 if ok else errno */
int 	  vdlPlaybackGetEq(VDL_HANDLE, vidix_video_eq_t * );

			/* Returns 0 if ok else errno */
int 	  vdlPlaybackSetEq(VDL_HANDLE, const vidix_video_eq_t * );

			/* Returns 0 if ok else errno */
int	  vdlPlaybackGetDeint(VDL_HANDLE, vidix_deinterlace_t * );

			/* Returns 0 if ok else errno */
int 	  vdlPlaybackSetDeint(VDL_HANDLE, const vidix_deinterlace_t * );

			/* Returns 0 if ok else errno */
int	  vdlQueryNumOemEffects(VDL_HANDLE, unsigned * number );

			/* Returns 0 if ok else errno */
int	  vdlGetOemEffect(VDL_HANDLE, vidix_oem_fx_t * );

			/* Returns 0 if ok else errno */
int	  vdlSetOemEffect(VDL_HANDLE, const vidix_oem_fx_t * );


			/* Returns 0 if ok else errno */
int	  vdlPlaybackCopyFrame(VDL_HANDLE, const vidix_dma_t * );

#ifdef __cplusplus
}
#endif

#endif
