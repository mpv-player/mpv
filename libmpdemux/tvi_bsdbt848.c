/*
	(C)2002 Charles R. Henrich (henrich@msu.edu)
	*BSD (hopefully, requires working driver!) BrookTree capture support.

	Still in (active) development!

	v1.0	Feb 19 2002		First Release, need to add support for changing
							audio parameters.
*/

#include "config.h"

#if defined(USE_TV) && defined(HAVE_TV_BSDBT848)

#define TRUE  (1==1)
#define FALSE (1==0)

#define PAL_WIDTH  768
#define PAL_HEIGHT 576
#define PAL_FPS	25

#define NTSC_WIDTH  640
#define NTSC_HEIGHT 480
#define NTSC_FPS	30

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>

#include <machine/ioctl_meteor.h>
#include <machine/ioctl_bt848.h>
#include <machine/soundcard.h>

#include "../libvo/img_format.h"
#include "tv.h"

/* information about this file */
static tvi_info_t info = {
	"Brooktree848 Support",
	"bt848",
	"Charles Henrich",
	"in development"
};

/* private data's */
typedef struct {

/* Audio */
	char *dspdev;
	int dspready;
	int dspfd;
	int dspsamplesize;
	int dspstereo;
	int dspspeed;
	int dspfmt;
	int dspframesize;

/* Video */
	char *btdev;
	int videoready;
	int btfd;
	int source;
	int maxfps;
	int fps;
	int iformat;
	int maxheight;
	int maxwidth;
	struct meteor_geomet geom;
	struct meteor_capframe capframe;
	int buffersize;
	unsigned char *buffer;
	int currentframe;

/* Inputs */

	int input;

/* Tuner */

	char *tunerdev;
	int tunerfd;
	int tunerready;
	u_long tunerfreq;
	struct bktr_chnlset cset;

} priv_t;

#include "tvi_def.h"

static priv_t *G_private=NULL;


static void processframe(int signal)
{
G_private->currentframe++;

return;
}

/* handler creator - entry point ! */
tvi_handle_t *tvi_init_bsdbt848(char *device)
{
	return(new_handle());
}

static int control(priv_t *priv, int cmd, void *arg)
{
	switch(cmd)
	{

/* Tuner Controls */

	case TVI_CONTROL_IS_TUNER:
		if(priv->tunerready == FALSE) return TVI_CONTROL_FALSE;
		return(TVI_CONTROL_TRUE);

	case TVI_CONTROL_TUN_GET_FREQ:
		{
		if(ioctl(priv->tunerfd, TVTUNER_GETFREQ, &priv->tunerfreq) < 0)
			{
			perror("GETFREQ:ioctl");
			return(TVI_CONTROL_FALSE);
			}

		(int)*(void **)arg = priv->tunerfreq;
		return(TVI_CONTROL_TRUE);
		}
	
	case TVI_CONTROL_TUN_SET_FREQ:
		{
		priv->tunerfreq = (int)*(void **)arg;

		if(ioctl(priv->tunerfd, TVTUNER_SETFREQ, &priv->tunerfreq) < 0) 
			{
			perror("SETFREQ:ioctl");
			return(0);
			}

		return(TVI_CONTROL_TRUE);		
		}

	case TVI_CONTROL_TUN_GET_TUNER:
	case TVI_CONTROL_TUN_SET_TUNER:

/* Inputs */

    case TVI_CONTROL_SPC_GET_INPUT:
		{
		if(ioctl(priv->btfd, METEORGINPUT, &priv->input) < 0)
			{
			perror("GINPUT:ioctl");
			return(TVI_CONTROL_FALSE);
			}

		(int)*(void **)arg = priv->input;
		return(TVI_CONTROL_TRUE);
		}
	
    case TVI_CONTROL_SPC_SET_INPUT:
		{
		priv->input = getinput((int)*(void **)arg);

		if(ioctl(priv->btfd, METEORSINPUT, &priv->input) < 0) 
			{
			perror("tunerfreq:ioctl");
			return(0);
			}

		return(TVI_CONTROL_TRUE);		
		}

/* Audio Controls */

	case TVI_CONTROL_IS_AUDIO:
		if(priv->dspready == FALSE) return TVI_CONTROL_FALSE;
		return(TVI_CONTROL_TRUE);

	case TVI_CONTROL_AUD_GET_FORMAT:
		{
		(int)*(void **)arg = AFMT_S16_LE;
		return(TVI_CONTROL_TRUE);
		}
	case TVI_CONTROL_AUD_GET_CHANNELS:
		{
		(int)*(void **)arg = 2;
		return(TVI_CONTROL_TRUE);
		}
	case TVI_CONTROL_AUD_GET_SAMPLERATE:
		{
		(int)*(void **)arg = 44100;
		return(TVI_CONTROL_TRUE);
		}
	case TVI_CONTROL_AUD_GET_SAMPLESIZE:
		{
		(int)*(void **)arg = priv->dspsamplesize;
		return(TVI_CONTROL_TRUE);
		}

/* Video Controls */

	case TVI_CONTROL_IS_VIDEO:
		if(priv->videoready == FALSE) return TVI_CONTROL_FALSE;
		return(TVI_CONTROL_TRUE);

	case TVI_CONTROL_TUN_SET_NORM:
		{
		int req_mode = (int)*(void **)arg;

		priv->iformat = METEOR_FMT_AUTOMODE;

		if(req_mode == TV_NORM_PAL) 
			{
			priv->iformat = METEOR_FMT_PAL;
			priv->maxheight = PAL_HEIGHT;
			priv->maxwidth = PAL_WIDTH;
			priv->maxfps = PAL_FPS;
			priv->fps = PAL_FPS;

			if(priv->fps > priv->maxfps) priv->fps = priv->maxfps;

			if(priv->geom.rows > priv->maxheight) 
				{
				priv->geom.rows = priv->maxheight;
				}

			if(priv->geom.columns > priv->maxwidth) 
				{
				priv->geom.columns = priv->maxwidth;
				}
			}

		if(req_mode == TV_NORM_NTSC) 
			{
			priv->iformat = METEOR_FMT_NTSC;
			priv->maxheight = NTSC_HEIGHT;
			priv->maxwidth = NTSC_WIDTH;
			priv->maxfps = NTSC_FPS;
			priv->fps = NTSC_FPS;

			priv->dspframesize = priv->dspspeed*priv->dspsamplesize/8/
								 priv->fps * (priv->dspstereo+1);

			if(priv->fps > priv->maxfps) priv->fps = priv->maxfps;

			if(priv->geom.rows > priv->maxheight) 
				{
				priv->geom.rows = priv->maxheight;
				}

			if(priv->geom.columns > priv->maxwidth) 
				{
				priv->geom.columns = priv->maxwidth;
				}
			}

		if(req_mode == TV_NORM_SECAM) priv->iformat = METEOR_FMT_SECAM;

		if(ioctl(priv->btfd, METEORSFMT, &priv->iformat) < 0) 
			{
			perror("format:ioctl");
			return(TVI_CONTROL_FALSE);
			}
	
		if(ioctl(priv->btfd, METEORSETGEO, &priv->geom) < 0) 
			{
			perror("geo:ioctl");
			return(0);
			}

		if(ioctl(priv->btfd, METEORSFPS, &priv->fps) < 0) 
			{
			perror("fps:ioctl");
			return(0);
			}

		return(TVI_CONTROL_TRUE);
		}
	
	case TVI_CONTROL_VID_GET_FORMAT:
		(int)*(void **)arg = IMGFMT_UYVY;
		return(TVI_CONTROL_TRUE);

	case TVI_CONTROL_VID_SET_FORMAT:
		{
		int req_fmt = (int)*(void **)arg;

		if(req_fmt != IMGFMT_UYVY) return(TVI_CONTROL_FALSE);

		return(TVI_CONTROL_TRUE);
		}
	case TVI_CONTROL_VID_SET_WIDTH:
		priv->geom.columns = (int)*(void **)arg;

		if(priv->geom.columns > priv->maxwidth) 
			{
			priv->geom.columns = priv->maxwidth;
			}

		if(ioctl(priv->btfd, METEORSETGEO, &priv->geom) < 0) 
			{
			perror("width:ioctl");
			return(0);
			}

		return(TVI_CONTROL_TRUE);

	case TVI_CONTROL_VID_GET_WIDTH:
		(int)*(void **)arg = priv->geom.columns;
		return(TVI_CONTROL_TRUE);

	case TVI_CONTROL_VID_SET_HEIGHT:
		priv->geom.rows = (int)*(void **)arg;

		if(priv->geom.rows > priv->maxheight) 
			{
			priv->geom.rows = priv->maxheight;
			}

		if(priv->geom.rows <= priv->maxheight / 2)
			{
			priv->geom.oformat |= METEOR_GEO_EVEN_ONLY;
			}  

		if(ioctl(priv->btfd, METEORSETGEO, &priv->geom) < 0) 
			{
			perror("height:ioctl");
			return(0);
			}

		return(TVI_CONTROL_TRUE);		

	case TVI_CONTROL_VID_GET_HEIGHT:
		(int)*(void **)arg = priv->geom.rows;
		return(TVI_CONTROL_TRUE);		

	case TVI_CONTROL_VID_GET_FPS:
		(int)*(void **)arg = (int)priv->fps;
		return(TVI_CONTROL_TRUE);		

/*
	case TVI_CONTROL_VID_SET_FPS:
		priv->fps = (int)*(void **)arg;

		if(priv->fps > priv->maxfps) priv->fps = priv->maxfps;

		if(ioctl(priv->btfd, METEORSFPS, &priv->fps) < 0) 
			{
			perror("fps:ioctl");
			return(0);
			}

		return(TVI_CONTROL_TRUE);		
*/

	case TVI_CONTROL_VID_CHK_WIDTH:
	case TVI_CONTROL_VID_CHK_HEIGHT:
		return(TVI_CONTROL_TRUE);

	}
	return(TVI_CONTROL_UNKNOWN);
}

static int init(priv_t *priv)
{
int marg;

G_private = priv; /* Oooh, sick */

/* Video Configuration */

priv->videoready = TRUE;
priv->btdev = strdup("/dev/bktr0");
priv->iformat = METEOR_FMT_PAL;
priv->maxheight = PAL_HEIGHT;
priv->maxwidth = PAL_WIDTH;
priv->maxfps = PAL_FPS;
priv->source = METEOR_INPUT_DEV0;
priv->fps = priv->maxfps;

priv->currentframe=0;

priv->geom.columns = priv->maxwidth;
priv->geom.rows = priv->maxheight;
priv->geom.frames = 1;
priv->geom.oformat = METEOR_GEO_YUV_PACKED;

priv->btfd = open(priv->btdev, O_RDONLY);

if(priv->btfd < 0)
	{
	perror("bktr open");
	priv->videoready = FALSE;
	}

if(priv->videoready == TRUE && 
   ioctl(priv->btfd, METEORSFMT, &priv->iformat) < 0) 
	{
	perror("FMT:ioctl");
	}

if(priv->videoready == TRUE &&
   ioctl(priv->btfd, METEORSINPUT, &priv->source) < 0) 
	{
	perror("SINPUT:ioctl");
	}

if(priv->videoready == TRUE &&
   ioctl(priv->btfd, METEORSFPS, &priv->fps) < 0) 
	{
	perror("SFPS:ioctl");
	}

if(priv->videoready == TRUE &&
   ioctl(priv->btfd, METEORSETGEO, &priv->geom) < 0) 
	{
	perror("SGEO:ioctl");
	}

if(priv->videoready == TRUE)
	{
	priv->buffersize = (priv->geom.columns * priv->geom.rows * 2);

	priv->buffer = (u_char *)mmap((caddr_t)0, priv->buffersize, PROT_READ,
							MAP_SHARED, priv->btfd, (off_t)0);

	if(priv->buffer == (u_char *) MAP_FAILED)
		{
		perror("mmap");
		priv->videoready = FALSE;
		}

	}

/* Tuner Configuration */

priv->tunerdev = strdup("/dev/tuner0");
priv->tunerready = TRUE;

priv->tunerfd = open(priv->tunerdev, O_RDONLY);

if(priv->tunerfd < 0)
	{
	perror("tune open");
	priv->tunerready = FALSE;
	}

/* Audio Configuration */

priv->dspready = TRUE;
priv->dspdev = strdup("/dev/dsp");
priv->dspsamplesize = 16;
priv->dspstereo = 1;
priv->dspspeed = 44100;
priv->dspfmt = AFMT_S16_LE;
priv->dspframesize = priv->dspspeed*priv->dspsamplesize/8/priv->fps * 
					 (priv->dspstereo+1);

if((priv->dspfd = open ("/dev/dsp", O_RDWR, 0)) < 0)
	{
	perror("/dev/dsp open");
	priv->dspready = FALSE;
	} 

marg = (256 << 16) | 13;

if (ioctl(priv->dspfd, SNDCTL_DSP_SETFRAGMENT, &marg ) < 0 ) 
	{
	perror("setfrag");
	priv->dspready = FALSE;
	}

if((priv->dspready == TRUE) &&
   (ioctl(priv->dspfd, SNDCTL_DSP_SAMPLESIZE, &priv->dspsamplesize) == -1) ||
   (ioctl(priv->dspfd, SNDCTL_DSP_STEREO, &priv->dspstereo) == -1) ||
   (ioctl(priv->dspfd, SNDCTL_DSP_SPEED, &priv->dspspeed) == -1) ||
   (ioctl(priv->dspfd, SNDCTL_DSP_SETFMT, &priv->dspfmt) == -1))
	{
	perror ("configuration of /dev/dsp failed");
	close(priv->dspfd);
	priv->dspready = FALSE;
	}

return(1);
}

/* that's the real start, we'got the format parameters (checked with control) */
static int start(priv_t *priv)
{
int marg;

if(priv->videoready == FALSE) return(0);

signal(SIGUSR1, processframe);
signal(SIGALRM, processframe);

marg = SIGUSR1;

if(ioctl(priv->btfd, METEORSSIGNAL, &marg) < 0) 
	{
	perror("METEORSSIGNAL failed");
	return(0);
	}

marg = METEOR_CAP_CONTINOUS;

if(ioctl(priv->btfd, METEORCAPTUR, &marg) < 0) 
	{
	perror("METEORCAPTUR failed");
	return(0);
	}

return(1);
}

static int uninit(priv_t *priv)
{
int marg;

if(priv->videoready == FALSE) return(0);

marg = METEOR_SIG_MODE_MASK;

if(ioctl( priv->btfd, METEORSSIGNAL, &marg) < 0 ) 
	{
	perror("METEORSSIGNAL");
	return(0);
	}

marg = METEOR_CAP_STOP_CONT;

if(ioctl(priv->btfd, METEORCAPTUR, &marg) < 0 ) 
	{
	perror("METEORCAPTUR STOP");
	return(0);
	}

close(priv->btfd);
close(priv->dspfd);

priv->dspfd = -1;
priv->btfd = -1;

priv->dspready = priv->videoready = FALSE;

return(1);
}


static int grab_video_frame(priv_t *priv, char *buffer, int len)
{
sigset_t sa_mask;

if(priv->videoready == FALSE) return(0);

alarm(1);
sigfillset(&sa_mask);
sigdelset(&sa_mask,SIGINT);
sigdelset(&sa_mask,SIGUSR1);
sigdelset(&sa_mask,SIGALRM);
sigsuspend(&sa_mask);
alarm(0);

memcpy(buffer, priv->buffer, len);

return(priv->currentframe);
}

static int get_video_framesize(priv_t *priv)
{
return(priv->geom.columns*priv->geom.rows*16/8);
}

static int grab_audio_frame(priv_t *priv, char *buffer, int len)
{
struct audio_buf_info abi;
int bytesread;
int ret;

if(priv->dspready == FALSE) return 0;

/* Get exactly one frame of audio, which forces video sync to audio.. */

bytesread=read(priv->dspfd, buffer, len); 

while(bytesread < len)
	{
	ret=read(priv->dspfd, &buffer[bytesread], len-bytesread);

	if(ret == -1)
		{
		perror("Audio read failed!");
		return 0;
		}

	bytesread+=ret;
	}

if(ioctl(priv->dspfd, SNDCTL_DSP_GETISPACE, &abi) < 0) 
	{
	perror("abi:ioctl");
	return(TVI_CONTROL_FALSE);
	}

return(abi.bytes/len);
}

static int get_audio_framesize(priv_t *priv)
{
if(priv->dspready == FALSE) return 0;

return(priv->dspframesize);
}

static int getinput(int innumber)
{
switch(innumber)
	{
	case 0: return METEOR_INPUT_DEV0; 	/* RCA   */
	case 1: return METEOR_INPUT_DEV1; 	/* Tuner */
	case 2: return METEOR_INPUT_DEV2; 	/* In 1  */
	case 3: return METEOR_INPUT_DEV3; 	/* In 2  */
	case 4: return METEOR_INPUT_DEV_RGB; 	/* RGB   */
	case 5: return METEOR_INPUT_DEV_SVIDEO; /* SVid  */
	}

return 0;
}

#endif /* USE_TV */
