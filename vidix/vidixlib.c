/*
 * vidixlib.c
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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <dlfcn.h> /* GLIBC specific. Exists under cygwin too! */
#include <dirent.h>

#include "vidixlib.h"

static char drv_name[FILENAME_MAX];

typedef struct vdl_stream_s
{
	void *  handle;
	int	(*get_caps)(vidix_capability_t *);
	int	(*query_fourcc)(vidix_fourcc_t *);
	int	(*config_playback)(vidix_playback_t *);
	int 	(*playback_on)( void );
	int 	(*playback_off)( void );
        /* Functions below can be missed in driver ;) */
	int	(*init)(void);
	void    (*destroy)(void);
	int 	(*frame_sel)( unsigned frame_idx );
	int 	(*get_eq)( vidix_video_eq_t * );
	int 	(*set_eq)( const vidix_video_eq_t * );
	int 	(*copy_frame)( const vidix_dma_t * );
}vdl_stream_t;

#define t_vdl(p) (((vdl_stream_t *)p))

extern unsigned   vdlGetVersion( void )
{
   return VIDIX_VERSION;
}

static int vdl_fill_driver(VDL_HANDLE stream)
{
  t_vdl(stream)->init		= dlsym(t_vdl(stream)->handle,"vixInit");
  t_vdl(stream)->destroy	= dlsym(t_vdl(stream)->handle,"vixDestroy");
  t_vdl(stream)->get_caps	= dlsym(t_vdl(stream)->handle,"vixGetCapability");
  t_vdl(stream)->query_fourcc	= dlsym(t_vdl(stream)->handle,"vixQueryFourcc");
  t_vdl(stream)->config_playback= dlsym(t_vdl(stream)->handle,"vixConfigPlayback");
  t_vdl(stream)->playback_on	= dlsym(t_vdl(stream)->handle,"vixPlaybackOn");
  t_vdl(stream)->playback_off	= dlsym(t_vdl(stream)->handle,"vixPlaybackOff");
  t_vdl(stream)->frame_sel	= dlsym(t_vdl(stream)->handle,"vixPlaybackFrameSelect");
  t_vdl(stream)->get_eq	= dlsym(t_vdl(stream)->handle,"vixPlaybackGetEq");
  t_vdl(stream)->set_eq	= dlsym(t_vdl(stream)->handle,"vixPlaybackSetEq");
  t_vdl(stream)->copy_frame	= dlsym(t_vdl(stream)->handle,"vixPlaybackCopyFrame");
  /* check driver viability */
  if(!( t_vdl(stream)->get_caps && t_vdl(stream)->query_fourcc &&
	t_vdl(stream)->config_playback && t_vdl(stream)->playback_on &&
	t_vdl(stream)->playback_off))
  {
    printf("vidixlib: some features are missed in driver\n");
    return 0;
  }
  return 1;
}

static int vdl_probe_driver(VDL_HANDLE stream,const char *path,const char *name,unsigned cap,int verbose)
{
  vidix_capability_t vid_cap;
  unsigned (*_ver)(void);
  int      (*_probe)(int);
  int      (*_cap)(vidix_capability_t*);
  strcpy(drv_name,path);
  strcat(drv_name,name);
  if(verbose) printf("vidixlib: PROBING: %s\n",drv_name);
  if(!(t_vdl(stream)->handle = dlopen(drv_name,RTLD_LAZY|RTLD_GLOBAL)))
  {
    if(verbose) printf("vidixlib: %s not driver: %s\n",drv_name,strerror(errno));
    return 0;
  }
  _ver = dlsym(t_vdl(stream)->handle,"vixGetVersion");
  _probe = dlsym(t_vdl(stream)->handle,"vixProbe");
  _cap = dlsym(t_vdl(stream)->handle,"vixGetCapability");
  if(_ver) 
  {
    if((*_ver)() != VIDIX_VERSION) 
    { 
      if(verbose) printf("vidixlib: %s has wrong version\n",drv_name);
      err:
      dlclose(t_vdl(stream)->handle);
      t_vdl(stream)->handle = 0;
      return 0;
     }
  }
  else
  {
    fatal_err:
    if(verbose) printf("vidixlib: %s has no function definition\n",drv_name);
    goto err;
  }
  if(_probe) { if((*_probe)(verbose) != 0) goto err; }
  else goto fatal_err;
  if(_cap) { if((*_cap)(&vid_cap) != 0) goto err; }
  else goto fatal_err;
  if((vid_cap.type & cap) != cap)
  {
     if(verbose) printf("vidixlib: Found %s but has no required capability\n",drv_name);
     goto err;
  }
  if(verbose) printf("vidixlib: %s probed o'k\n",drv_name);
  return 1;
}

static int vdl_find_driver(VDL_HANDLE stream,const char *path,unsigned cap,int verbose)
{
  DIR *dstream;
  struct dirent *name;
  int done = 0;
  if(!(dstream = opendir(path))) return 0;
  while(!done)
  {
    name = readdir(dstream);
    if(name) 
    { 
      if(name->d_name[0] != '.')
	if(vdl_probe_driver(stream,path,name->d_name,cap,verbose)) break; 
    }
    else done = 1;
  }
  closedir(dstream);
  return done?0:1;
}

VDL_HANDLE vdlOpen(const char *path,const char *name,unsigned cap,int verbose)
{
  vdl_stream_t *stream;
  int errcode;
  if(!(stream = malloc(sizeof(vdl_stream_t)))) return NULL;
  memset(stream,0,sizeof(vdl_stream_t));
  if(name)
  {
    unsigned (*ver)(void);
    int (*probe)(int);
    unsigned version = 0;
    strcpy(drv_name,path);
    strcat(drv_name,name);
    if(!(t_vdl(stream)->handle = dlopen(drv_name,RTLD_NOW|RTLD_GLOBAL)))
    {
      err:
      free(stream);
      return NULL;
    }
    ver = dlsym(t_vdl(stream)->handle,"vixGetVersion");
    if(ver) version = (*ver)();
    if(version != VIDIX_VERSION)
    {
      drv_err:
      if(t_vdl(stream)->handle) dlclose(t_vdl(stream)->handle);
      goto err;
    }
    probe = dlsym(t_vdl(stream)->handle,"vixProbe");
    if(probe) { if((*probe)(verbose)!=0) goto drv_err; }
    else goto drv_err;
    fill:
    if(!vdl_fill_driver(stream)) goto drv_err;
    goto ok;
  }
  else
    if(vdl_find_driver(stream,path,cap,verbose))
    {
      if(verbose) printf("vidixlib: will use %s driver\n",drv_name);
      goto fill;
    }  
    else goto err;
  ok:
  if(t_vdl(stream)->init)
  {
   if(verbose) printf("vidixlib: Attempt to initialize driver at: %p\n",t_vdl(stream)->init);
   if((errcode=t_vdl(stream)->init())!=0)
   {
    if(verbose) printf("vidixlib: Can't init driver: %s\n",strerror(errcode));
    goto drv_err;
   }
  } 
  if(verbose) printf("vidixlib: '%s'successfully loaded\n",drv_name);
  return stream;
}

void vdlClose(VDL_HANDLE stream)
{
  if(t_vdl(stream)->destroy) t_vdl(stream)->destroy();
  dlclose(t_vdl(stream)->handle);
  memset(stream,0,sizeof(vdl_stream_t)); /* <- it's not stupid */
  free(stream);
}

int  vdlGetCapability(VDL_HANDLE handle, vidix_capability_t *cap)
{
  return t_vdl(handle)->get_caps(cap);
}

int  vdlQueryFourcc(VDL_HANDLE handle,vidix_fourcc_t *f)
{
  return t_vdl(handle)->query_fourcc(f);
}

int  vdlConfigPlayback(VDL_HANDLE handle,vidix_playback_t *p)
{
  return t_vdl(handle)->config_playback(p);
}

int  vdlPlaybackOn(VDL_HANDLE handle)
{
  return t_vdl(handle)->playback_on();
}

int  vdlPlaybackOff(VDL_HANDLE handle)
{
  return t_vdl(handle)->playback_off();
}

int  vdlPlaybackFrameSelect(VDL_HANDLE handle, unsigned frame_idx )
{
  return t_vdl(handle)->frame_sel ? t_vdl(handle)->frame_sel(frame_idx) : ENOSYS;
}

int  vdlPlaybackGetEq(VDL_HANDLE handle, vidix_video_eq_t * e)
{
  return t_vdl(handle)->get_eq ? t_vdl(handle)->get_eq(e) : ENOSYS;
}

int  vdlPlaybackSetEq(VDL_HANDLE handle, const vidix_video_eq_t * e)
{
  return t_vdl(handle)->set_eq ? t_vdl(handle)->set_eq(e) : ENOSYS;
}

int  vdlPlaybackCopyFrame(VDL_HANDLE handle, const vidix_dma_t * f)
{
  return t_vdl(handle)->copy_frame ? t_vdl(handle)->copy_frame(f) : ENOSYS;
}
