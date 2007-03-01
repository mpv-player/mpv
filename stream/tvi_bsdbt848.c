/*
    (C)2002 Charles R. Henrich (henrich@msu.edu)
    *BSD (hopefully, requires working driver!) BrookTree capture support.

    Still in (active) development!

	v1.1	Mar 13 2002   Fully functional, need to move ring buffer to
						  the kernel driver. 
    v1.0    Feb 19 2002   First Release, need to add support for changing
                            audio parameters.
*/

#include "config.h"

#define RINGSIZE 8
#define FRAGSIZE 4096 /* (2^12 see SETFRAGSIZE below) */

#define TRUE  (1==1)
#define FALSE (1==0)

#define PAL_WIDTH  768
#define PAL_HEIGHT 576
#define PAL_FPS    25

#define NTSC_WIDTH  640
#define NTSC_HEIGHT 480
#define NTSC_FPS    29.97

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/filio.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>

#include <sys/param.h>
#ifdef __NetBSD__
#include <dev/ic/bt8xx.h>
#include <sys/audioio.h>
#elif defined(__DragonFly__)
#include <dev/video/meteor/ioctl_meteor.h>
#include <dev/video/bktr/ioctl_bt848.h>
#elif (__FreeBSD_version >= 502100) || defined(__FreeBSD_kernel__)
#include <dev/bktr/ioctl_meteor.h>
#include <dev/bktr/ioctl_bt848.h>
#else
#include <machine/ioctl_meteor.h>
#include <machine/ioctl_bt848.h>
#endif

#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#else
#ifdef HAVE_SOUNDCARD_H
#include <soundcard.h>
#else
#include <machine/soundcard.h>
#endif
#endif

#include "libaf/af_format.h"
#include "libmpcodecs/img_format.h"
#include "tv.h"

static tvi_handle_t *tvi_init_bsdbt848(char *device, char *adevice);
/* information about this file */
tvi_info_t tvi_info_bsdbt848 = {
    tvi_init_bsdbt848,
    "Brooktree848 Support",
    "bsdbt848",
    "Charles Henrich",
    "in development"
};

typedef struct {
    int dirty;
    double timestamp;
    char *buf;
} RBFRAME;

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
    int dsprate;
    long long dspbytesread;

/* Video */
    char *btdev;
    int videoready;
    int btfd;
    int source;
    float maxfps;
    float fps;
    int iformat;
    int maxheight;
    int maxwidth;
    struct meteor_geomet geom;
    struct meteor_capframe capframe;

/* Frame Buffer */

    int framebufsize;
    float timestamp;
    int curpaintframe;
    int curbufframe;
    unsigned char *livebuf;
    RBFRAME framebuf[RINGSIZE];

/* Inputs */

    int input;

/* Tuner */

    char *tunerdev;
    int tunerfd;
    int tunerready;
    u_long tunerfreq;
    struct bktr_chnlset cset;

/* Other */

    int immediatemode;
    double starttime;

} priv_t;

#include "tvi_def.h"

static priv_t *G_private=NULL;

static int getinput(int innumber);

static void processframe(int signal)
{
struct timeval curtime;

if(G_private->immediatemode == TRUE) return;

gettimeofday(&curtime, NULL);

if(G_private->framebuf[G_private->curpaintframe].dirty == TRUE)
    {
    memcpy(G_private->framebuf[G_private->curpaintframe].buf, 
            G_private->livebuf, G_private->framebufsize);

    G_private->framebuf[G_private->curpaintframe].dirty = FALSE;

    G_private->framebuf[G_private->curpaintframe].timestamp = 
            curtime.tv_sec + curtime.tv_usec*.000001;

    G_private->curpaintframe++;

    if(G_private->curpaintframe >= RINGSIZE) G_private->curpaintframe = 0;
    }

return;
}

/* handler creator - entry point ! */
static tvi_handle_t *tvi_init_bsdbt848(char *device,char* adevice)
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

        *(int *)arg = priv->tunerfreq;
        return(TVI_CONTROL_TRUE);
        }
    
    case TVI_CONTROL_TUN_SET_FREQ:
        {
        priv->tunerfreq = *(int *)arg;

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

        *(int *)arg = priv->input;
        return(TVI_CONTROL_TRUE);
        }
    
    case TVI_CONTROL_SPC_SET_INPUT:
        {
        priv->input = getinput(*(int *)arg);

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
        *(int *)arg = AF_FORMAT_S16_LE;
        return(TVI_CONTROL_TRUE);
        }
    case TVI_CONTROL_AUD_GET_CHANNELS:
        {
        *(int *)arg = 2;
        return(TVI_CONTROL_TRUE);
        }
    case TVI_CONTROL_AUD_SET_SAMPLERATE:
        {
        int dspspeed = *(int *)arg;

           if(ioctl(priv->dspfd, SNDCTL_DSP_SPEED, &dspspeed) == -1) 
            {
            perror("invalidaudiorate");
            return(TVI_CONTROL_FALSE);
            }

        priv->dspspeed = dspspeed;

        priv->dspframesize = priv->dspspeed*priv->dspsamplesize/8/
                                priv->fps * (priv->dspstereo+1);
        priv->dsprate = priv->dspspeed * priv->dspsamplesize/8*
                                (priv->dspstereo+1);

        return(TVI_CONTROL_TRUE);
        }
    case TVI_CONTROL_AUD_GET_SAMPLERATE:
        {
        *(int *)arg = priv->dspspeed;
        return(TVI_CONTROL_TRUE);
        }
    case TVI_CONTROL_AUD_GET_SAMPLESIZE:
        {
        *(int *)arg = priv->dspsamplesize/8;
        return(TVI_CONTROL_TRUE);
        }

/* Video Controls */

    case TVI_CONTROL_IS_VIDEO:
        if(priv->videoready == FALSE) return TVI_CONTROL_FALSE;
        return(TVI_CONTROL_TRUE);

    case TVI_CONTROL_TUN_SET_NORM:
        {
        int req_mode = *(int *)arg;
	u_short tmp_fps;

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
            priv->dsprate = priv->dspspeed * priv->dspsamplesize/8 *
                                (priv->dspstereo+1);

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

	tmp_fps = priv->fps;
        if(ioctl(priv->btfd, METEORSFPS, &tmp_fps) < 0) 
            {
            perror("fps:ioctl");
            return(0);
            }

#ifdef BT848_SAUDIO
	if(priv->tunerready == TRUE &&
	    ioctl(priv->tunerfd, BT848_SAUDIO, &tv_param_audio_id) < 0)
	    {
	    perror("audioid:ioctl");
	    }
#endif

        return(TVI_CONTROL_TRUE);
        }
    
    case TVI_CONTROL_VID_GET_FORMAT:
        *(int *)arg = IMGFMT_UYVY;
        return(TVI_CONTROL_TRUE);

    case TVI_CONTROL_VID_SET_FORMAT:
        {
        int req_fmt = *(int *)arg;

        if(req_fmt != IMGFMT_UYVY) return(TVI_CONTROL_FALSE);

        return(TVI_CONTROL_TRUE);
        }
    case TVI_CONTROL_VID_SET_WIDTH:
        priv->geom.columns = *(int *)arg;

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
        *(int *)arg = priv->geom.columns;
        return(TVI_CONTROL_TRUE);

    case TVI_CONTROL_VID_SET_HEIGHT:
        priv->geom.rows = *(int *)arg;

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
        *(int *)arg = priv->geom.rows;
        return(TVI_CONTROL_TRUE);        

    case TVI_CONTROL_VID_GET_FPS:
        *(float *)arg = priv->fps;
        return(TVI_CONTROL_TRUE);        

/*
    case TVI_CONTROL_VID_SET_FPS:
        priv->fps = *(int *)arg;

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

    case TVI_CONTROL_IMMEDIATE:
        priv->immediatemode = TRUE;
        return(TVI_CONTROL_TRUE);
    }

    return(TVI_CONTROL_UNKNOWN);
}

static int init(priv_t *priv)
{
int marg;
int count;
u_short tmp_fps;

G_private = priv; /* Oooh, sick */

/* Video Configuration */

priv->videoready = TRUE;
priv->btdev = strdup("/dev/bktr0");
priv->immediatemode = FALSE;
priv->iformat = METEOR_FMT_PAL;
priv->maxheight = PAL_HEIGHT;
priv->maxwidth = PAL_WIDTH;
priv->maxfps = PAL_FPS;
priv->source = METEOR_INPUT_DEV0;
priv->fps = priv->maxfps;

priv->starttime=0;
priv->curpaintframe=0;
priv->curbufframe=0;

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

tmp_fps = priv->fps;
if(priv->videoready == TRUE &&
   ioctl(priv->btfd, METEORSFPS, &tmp_fps) < 0) 
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
    priv->framebufsize = (priv->geom.columns * priv->geom.rows * 2);

    priv->livebuf = (u_char *)mmap((caddr_t)0, priv->framebufsize, PROT_READ,
                                MAP_SHARED, priv->btfd, (off_t)0);

    if(priv->livebuf == (u_char *) MAP_FAILED)
        {
        perror("mmap");
        priv->videoready = FALSE;
        }

    for(count=0;count<RINGSIZE;count++)
        {
        priv->framebuf[count].buf = malloc(priv->framebufsize);

        if(priv->framebuf[count].buf == NULL)
            {
            perror("framebufmalloc");
            priv->videoready = FALSE;
            break;
            }

        priv->framebuf[count].dirty = TRUE;
        priv->framebuf[count].timestamp = 0;
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
#ifdef __NetBSD__
priv->dspdev = strdup("/dev/sound");
#else
priv->dspdev = strdup("/dev/dsp");
#endif
priv->dspsamplesize = 16;
priv->dspstereo = 1;
priv->dspspeed = 44100;
priv->dspfmt = AFMT_S16_LE;
priv->dspbytesread = 0;
priv->dsprate = priv->dspspeed * priv->dspsamplesize/8*(priv->dspstereo+1);
priv->dspframesize = priv->dspspeed*priv->dspsamplesize/8/priv->fps * 
                     (priv->dspstereo+1);

if((priv->dspfd = open (priv->dspdev, O_RDONLY, 0)) < 0)
    {
    perror("dsp open");
    priv->dspready = FALSE;
    } 

marg = (256 << 16) | 12;

if (ioctl(priv->dspfd, SNDCTL_DSP_SETFRAGMENT, &marg ) < 0 ) 
    {
    perror("setfrag");
    priv->dspready = FALSE;
    }

if((priv->dspready == TRUE) &&
   ((ioctl(priv->dspfd, SNDCTL_DSP_SAMPLESIZE, &priv->dspsamplesize) == -1) ||
   (ioctl(priv->dspfd, SNDCTL_DSP_STEREO, &priv->dspstereo) == -1) ||
   (ioctl(priv->dspfd, SNDCTL_DSP_SPEED, &priv->dspspeed) == -1) ||
   (ioctl(priv->dspfd, SNDCTL_DSP_SETFMT, &priv->dspfmt) == -1)))
    {
    perror ("configuration of dsp failed");
    close(priv->dspfd);
    priv->dspready = FALSE;
    }

return(1);
}

/* that's the real start, we'got the format parameters (checked with control) */
static int start(priv_t *priv)
{
int tmp;
struct timeval curtime;
int marg;

fprintf(stderr,"START\n");
if(priv->videoready == FALSE) return(0);

signal(SIGUSR1, processframe);
signal(SIGALRM, processframe);

marg = SIGUSR1;

if(ioctl(priv->btfd, METEORSSIGNAL, &marg) < 0) 
    {
    perror("METEORSSIGNAL failed");
    return(0);
    }

read(priv->dspfd, &tmp, 2);

gettimeofday(&curtime, NULL);

priv->starttime = curtime.tv_sec + (curtime.tv_usec *.000001);

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


static double grabimmediate_video_frame(priv_t *priv, char *buffer, int len)
{
struct timeval curtime;
sigset_t sa_mask;

if(priv->videoready == FALSE) return(0);

alarm(1);
sigfillset(&sa_mask);
sigdelset(&sa_mask,SIGINT);
sigdelset(&sa_mask,SIGUSR1);
sigdelset(&sa_mask,SIGALRM);
sigsuspend(&sa_mask);
alarm(0);

memcpy(buffer, priv->livebuf, len);

/* PTS = 0, show the frame NOW, this routine is only used in playback mode
    without audio capture .. */

return(0); 
}

static double grab_video_frame(priv_t *priv, char *buffer, int len)
{
struct timeval curtime;
double timestamp=0;
sigset_t sa_mask;

if(priv->videoready == FALSE) return(0);

if(priv->immediatemode == TRUE) 
    {
    return grabimmediate_video_frame(priv, buffer, len);
    }

while(priv->framebuf[priv->curbufframe].dirty == TRUE)
    {
    alarm(1);
    sigemptyset(&sa_mask);
    sigsuspend(&sa_mask);
    alarm(0);
    }

memcpy(buffer, priv->framebuf[priv->curbufframe].buf, len);
timestamp = priv->framebuf[priv->curbufframe].timestamp;
priv->framebuf[priv->curbufframe].dirty = TRUE;

priv->curbufframe++;
if(priv->curbufframe >= RINGSIZE) priv->curbufframe = 0;

return(timestamp-priv->starttime);
}

static int get_video_framesize(priv_t *priv)
{
return(priv->geom.columns*priv->geom.rows*16/8);
}

static double grab_audio_frame(priv_t *priv, char *buffer, int len)
{
struct timeval curtime;
double curpts;
double timeskew;
int bytesavail;
int bytesread;
int ret;

if(priv->dspready == FALSE) return 0;

gettimeofday(&curtime, NULL);

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

priv->dspbytesread += bytesread;

curpts = curtime.tv_sec + curtime.tv_usec * .000001;

timeskew = priv->dspbytesread * 1.0 / priv->dsprate - (curpts-priv->starttime);

if(timeskew > .125/priv->fps) 
    {
    priv->starttime -= timeskew;
    }
else
    {
    if(timeskew < -.125/priv->fps) 
        {
        priv->starttime -= timeskew;
        }
    }

return(priv->dspbytesread * 1.0 / priv->dsprate);
}

static int get_audio_framesize(priv_t *priv)
{
int bytesavail;
#ifdef __NetBSD__
struct audio_info auinf;
#endif

if(priv->dspready == FALSE) return 0;

#ifdef __NetBSD__
if(ioctl(priv->dspfd, AUDIO_GETINFO, &auinf) < 0) 
    {
    perror("AUDIO_GETINFO");
    return(TVI_CONTROL_FALSE);
    }
else
    bytesavail = auinf.record.seek; /* *priv->dspsamplesize; */
#else
if(ioctl(priv->dspfd, FIONREAD, &bytesavail) < 0) 
    {
    perror("FIONREAD");
    return(TVI_CONTROL_FALSE);
    }
#endif

/* When mencoder wants audio data, it wants data..
   it won't go do anything else until it gets it :( */

if(bytesavail == 0) return FRAGSIZE;

return(bytesavail);
}

static int getinput(int innumber)
{
switch(innumber)
    {
    case 0: return METEOR_INPUT_DEV0;     /* RCA   */
    case 1: return METEOR_INPUT_DEV1;     /* Tuner */
    case 2: return METEOR_INPUT_DEV2;     /* In 1  */
    case 3: return METEOR_INPUT_DEV3;     /* In 2  */
    case 4: return METEOR_INPUT_DEV_RGB;     /* RGB   */
    case 5: return METEOR_INPUT_DEV_SVIDEO; /* SVid  */
    }

return 0;
}
