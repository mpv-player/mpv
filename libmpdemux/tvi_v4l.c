/*
  v4l interface for libmpemux/tvi

  (C) Alex Beregszaszi <alex@naxine.org>
  
  Some ideas are based on xawtv/libng's grab-v4l.c written by
    Gerd Knorr <kraxel@bytesex.org>

  CODE IS UNDER DEVELOPMENT, NO FEATURE REQUESTS PLEASE!
*/

#include "config.h"

#ifdef USE_TV

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/videodev.h>
#include <unistd.h>
#include <sys/mman.h>

#include "tv.h"

static tvi_info_t info = {
	"Video for Linux TV Input",
	"v4l",
	"Alex Beregszaszi <alex@naxine.org>",
	"under development"
};

struct vid_fmt {
    int fmtid;
    int width;
    int height;
    int bytesperline;
};

typedef struct {
    /* general */
    char			*video_device;
    int				fd;
    struct video_capability	capability;
    struct video_channel	*channels;
    struct video_tuner		tuner;

    /* video */
    struct video_picture	picture;
    int				format;		/* output format */
    int				width;
    int				height;
    int				bytesperline;

    struct video_mbuf		mbuf;
    unsigned char		*mmap;
    struct video_mmap		*buf;
    int				nbuf;
    int				queue;

    /* audio */
    struct video_audio		audio;
} priv_t;

#include "tvi_def.h"

static const char *device_cap[] = {
    "capture", "tuner", "teletext", "overlay", "chromakey", "clipping",
    "frameram", "scales", "monochrome", "subcapture", "mpeg-decoder",
    "mpeg-encoder", "mjpeg-decoder", "mjpeg-encoder", NULL
};

static const char *device_pal[] = {
    "-", "grey", "hi240", "rgb16", "rgb24", "rgb32", "rgb15", "yuv422",
    "yuyv", "uyvy", "yuv420", "yuv411", "raw", "yuv422p", "yuv411p",
    "yuv420p", "yuv410p", NULL
};
#define PALETTE(x) ((x < sizeof(device_pal)/sizeof(char*)) ? device_pal[x] : "UNKNOWN")

static int palette2depth(int palette)
{
    if (palette == VIDEO_PALETTE_YUV420P)
	return 12;
    return 32;
}

static int format2palette(int format)
{
    if (format == IMGFMT_YV12)
	return VIDEO_PALETTE_YUV420P;
    return VIDEO_PALETTE_RGB24;
}

#if 0
struct STRTAB {
    long	nr;
    const char	*str;
};

static struct STRTAB stereo[] = {
    { 0,			"auto"		},
    { VIDEO_SOUND_MONO,		"mono"		},
    { VIDEO_SOUND_STEREO,	"stereo"	},
    { VIDEO_SOUND_LANG1,	"lang1"		},
    { VIDEO_SOUND_LANG2,	"lang1"		},
    { -1,			NULL		}    
};

static struct STRTAB norms_v4l[] = {
    { VIDEO_MODE_PAL,		"PAL"		},
    { VIDEO_MODE_NTSC,		"NTSC"		},
    { VIDEO_MODE_SECAM,		"SECAM"		},
    { VIDEO_MODE_AUTO,		"AUTO"		},
    { -1,			NULL		}    
};

static struct STRTAB norms_bttv[] = {
    { VIDEO_MODE_PAL,		"PAL"		},
    { VIDEO_MODE_NTSC,		"NTSC"		},
    { VIDEO_MODE_SECAM,		"SECAM"		},
    { 3,			"PAL-NC"	},
    { 4,			"PAL-M"		},
    { 5,			"PAL-N"		},
    { 6,			"NTSC-JP"	},
    { -1,			NULL		}    
};

static unsigned short _format2palette[VIDEO_FMT_COUNT] = {
    0,				/* unused */
    VIDEO_PALETTE_HI240,	/* RGB8 */
    VIDEO_PALETTE_GREY,
    VIDEO_PALETTE_RGB555,
    VIDEO_PALETTE_RGB565,
    0,
    0,
    VIDEO_PALETTE_RGB24,
    VIDEO_PALETTE_RGB32,
    0,
    0,
    0,
    0,
    VIDEO_PALETTE_YUV422,
    VIDEO_PALETTE_YUV422P,
    VIDEO_PALETTE_YUV420P,
};
#define FMT2PAL(fmt) ((fmt < sizeof(format2palette)/sizeof(unsigned short)) ? \
			format2palette[fmt] : 0);

const unsigned int vfmt_to_depth[] = {
    0,
    8,
    8,
    16,
    16,
    16,
    16,
    24,
    32,
    24,
    32,
    16,
    32,
    16,
    16,
    12,
    0,
    0,
};
#endif

static int one = 1, zero = 0;

static int alarms;

static void sigalarm(int signal)
{
    alarms++;
    printf("v4l: timeout (got SIGALRM), hardware/driver problems?\n");
}

static void siginit(void)
{
    struct sigaction act, old;
    
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigalarm;
    sigemptyset(&act.sa_mask);
    sigaction(SIGALRM, &act, &old);
}

tvi_handle_t *tvi_init_v4l(char *device)
{
    tvi_handle_t *h;
    priv_t *priv;
    
    h = new_handle();
    if (!h)
	return(NULL);

    priv = h->priv;

    /* set video device name */
    if (!device)
    {
	priv->video_device = malloc(strlen("/dev/video0"));
	sprintf(priv->video_device, "/dev/video0");
    }
    else
    {
	priv->video_device = malloc(strlen(device));
	strcpy(priv->video_device, device);
    }

    return(h);
}

static int init(priv_t *priv, tvi_param_t *params)
{
    int i;

    priv->fd = open(priv->video_device, O_RDONLY);
    if (priv->fd == -1)
    {
	printf("v4l: unable to open '%s': %s\n",
	    priv->video_device, strerror(errno));
	goto err;
    }

    printf("fd: %d\n", priv->fd);
    
    /* get capabilities (priv->capability is needed!) */
    if (ioctl(priv->fd, VIDIOCGCAP, &priv->capability) == -1)
    {
	printf("ioctl get capabilites error: %s\n", strerror(errno));
	goto err;
    }

    fcntl(priv->fd, F_SETFD, FD_CLOEXEC);
    siginit();

#if 0
    for (i=0; params[i].opt; i++)
    {
	if (!strcmp(params[i].opt, "input"))
	    priv->input = (int)*(void **)params[i].value;
    }
    printf("priv->input: %d\n", priv->input);
#endif

    printf("Selected device: %s\n", priv->capability.name);    
    printf(" Capabilites: ");
    for (i = 0; device_cap[i] != NULL; i++)
	if (priv->capability.type & (1 << i))
	    printf("%s ", device_cap[i]);
    printf("\n");
    printf(" Device type: %d\n", priv->capability.type);
    printf(" Supported sizes: %dx%d => %dx%d\n",
	priv->capability.minwidth, priv->capability.minheight,
	priv->capability.maxwidth, priv->capability.maxheight);
    priv->width = priv->capability.minwidth;
    priv->height = priv->capability.minheight;
    printf(" Inputs: %d\n", priv->capability.channels);

    priv->channels = malloc(sizeof(struct video_channel)*priv->capability.channels);
    memset(priv->channels, 0, sizeof(struct video_channel)*priv->capability.channels);
    for (i = 0; i < priv->capability.channels; i++)
    {
	priv->channels[i].channel = i;
	ioctl(priv->fd, VIDIOCGCHAN, &priv->channels[i]);
	printf("  %d: %s: %s%s%s%s (tuner:%d, norm:%d)\n", i,
	    priv->channels[i].name,
	    (priv->channels[i].flags & VIDEO_VC_TUNER) ? "tuner " : "",
	    (priv->channels[i].flags & VIDEO_VC_AUDIO) ? "audio " : "",
	    (priv->channels[i].flags & VIDEO_TYPE_TV) ? "tv " : "",
	    (priv->channels[i].flags & VIDEO_TYPE_CAMERA) ? "camera " : "",
	    priv->channels[i].tuners,
	    priv->channels[i].norm);
    }

    if (!(priv->capability.type & VID_TYPE_CAPTURE))
    {
	printf("Only grabbing supported (for overlay use another program)\n");
	goto err;
    }
    
    /* map grab buffer */
    if (ioctl(priv->fd, VIDIOCGMBUF, &priv->mbuf) == -1)
    {
	printf("ioctl get mbuf failed: %s\n", strerror(errno));
	goto err;
    }

    printf("mbuf: size=%d, frames=%d\n",
	priv->mbuf.size, priv->mbuf.frames);
    priv->mmap = mmap(0, priv->mbuf.size, PROT_READ|PROT_WRITE,
		MAP_SHARED, priv->fd, 0);
    if (priv->mmap == -1)
    {
	printf("Unabel to map memory for buffers: %s\n", strerror(errno));
        priv->mmap = malloc(priv->mbuf.size); /* our buffer */
    }
    if (!priv->mmap)
    {
	printf("Unable to allocate memory for buffers: %s\n", strerror(errno));
	goto err;
    }
    printf("our buffer: %p\n", priv->mmap);

    /* num of buffers */
    priv->nbuf = priv->mbuf.frames;
    
    /* video buffers */
    priv->buf = malloc(priv->nbuf * sizeof(struct video_mmap));
    memset(priv->buf, 0, priv->nbuf * sizeof(struct video_mmap));
    
    return(1);

err:
    if (priv->fd != -1)
	close(priv->fd);
    return(0);
}

static int uninit(priv_t *priv)
{
#warning "Implement uninit!"
}

static int start(priv_t *priv)
{
    int i;

    
    if (ioctl(priv->fd, VIDIOCGPICT, &priv->picture) == -1)
    {
	printf("ioctl get picture failed: %s\n", strerror(errno));
	return(0);
    }

    priv->picture.palette = format2palette(priv->format);
    priv->picture.depth = palette2depth(priv->picture.palette);
    priv->bytesperline = priv->width * priv->picture.depth / 8;
    
    printf("Picture values:\n");
    printf(" Depth: %d, Palette: %d (Format: %s)\n", priv->picture.depth,
	priv->picture.palette, vo_format_name(priv->format));
    printf(" Brightness: %d, Hue: %d, Colour: %d, Contrast: %d\n",
	priv->picture.brightness, priv->picture.hue,
	priv->picture.colour, priv->picture.contrast);
    

    if (ioctl(priv->fd, VIDIOCSPICT, &priv->picture) == -1)
    {
	printf("ioctl set picture failed: %s\n", strerror(errno));
	return(0);
    }

    priv->nbuf = priv->mbuf.frames;
    for (i=0; i < priv->nbuf; i++)
    {
	priv->buf[i].format = priv->picture.palette;
	priv->buf[i].frame = i;
	priv->buf[i].width = priv->width;
	priv->buf[i].height = priv->height;
	printf("buffer: %d => %p\n", i, &priv->buf[i]);
    } 
    
    /* start capture */
    if (ioctl(priv->fd, VIDIOCCAPTURE, &one) == -1)
    {
	printf("ioctl capture failed: %s\n", strerror(errno));
	return(0);
    }
}

static int control(priv_t *priv, int cmd, void *arg)
{
    printf("debug: control(priv=%p, cmd=%d, arg=%p)\n",
	priv, cmd, arg);
    switch(cmd)
    {
	/* ========== GENERIC controls =========== */
	case TVI_CONTROL_IS_VIDEO:
	{
	    if (priv->capability.type & VID_TYPE_CAPTURE)
		return(TVI_CONTROL_TRUE);
	    return(TVI_CONTROL_FALSE);
	}
	case TVI_CONTROL_IS_AUDIO:
	    return(TVI_CONTROL_FALSE);	/* IMPLEMENT CHECK! */
	case TVI_CONTROL_IS_TUNER:
	{
	    if (priv->capability.type & VID_TYPE_TUNER)
		return(TVI_CONTROL_TRUE);
	    return(TVI_CONTROL_FALSE);
	}

	/* ========== VIDEO controls =========== */
	case TVI_CONTROL_VID_GET_FORMAT:
	{
	    int output_fmt = -1;

#if 0	    
	    switch(priv->palette)
	    {
		case VIDEO_PALETTE_RGB555:
		    output_fmt = IMGFMT_RGB15;
		    break;
		case VIDEO_PALETTE_RGB565:
		    output_fmt = IMGFMT_RGB16;
		    break;
		case VIDEO_PALETTE_RGB24:
		    output_fmt = IMGFMT_RGB24;
		    break;
		case VIDEO_PALETTE_RGB32:
		    output_fmt = IMGFMT_RGB32;
		    break;
		case VIDEO_PALETTE_UYVY:
		    output_fmt = IMGFMT_UYVY;
		    break;
		case VIDEO_PALETTE_YUV420P:
		    output_fmt = IMGFMT_YV12;
		    break;
		default:
		    printf("no suitable output format found (%s)\n",
			PALETTE(priv->palette));
		    return(TVI_CONTROL_FALSE);
	    }
#endif
	    output_fmt = priv->format;
	    (int)*(void **)arg = output_fmt;
	    printf("Output format: %s\n", vo_format_name(output_fmt));
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_VID_SET_FORMAT:
	    priv->format = (int)*(void **)arg;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_PLANES:
	    (int)*(void **)arg = 1;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_BITS:
	    (int)*(void **)arg = 12;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_WIDTH:
	    (int)*(void **)arg = priv->width;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_CHK_WIDTH:
	{
	    int req_width = (int)*(void **)arg;
	    
	    printf("Requested width: %d\n", req_width);
	    if ((req_width > priv->capability.minwidth) &&
		(req_width < priv->capability.maxwidth))
		return(TVI_CONTROL_TRUE);
	    return(TVI_CONTROL_FALSE);
	}
	case TVI_CONTROL_VID_SET_WIDTH:
	    priv->width = (int)*(void **)arg;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_HEIGHT:
	    (int)*(void **)arg = priv->height;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_CHK_HEIGHT:
	{
	    int req_height = (int)*(void **)arg;
	    
	    printf("Requested height: %d\n", req_height);
	    if ((req_height > priv->capability.minheight) &&
		(req_height < priv->capability.maxheight))
		return(TVI_CONTROL_TRUE);
	    return(TVI_CONTROL_FALSE);
	}
	case TVI_CONTROL_VID_SET_HEIGHT:
	    priv->height = (int)*(void **)arg;
	    return(TVI_CONTROL_TRUE);

	/* ========== TUNER controls =========== */
	case TVI_CONTROL_TUN_GET_FREQ:
	{
	    unsigned long freq;
	    
	    if (ioctl(priv->fd, VIDIOCGFREQ, &freq) == -1)
	    {
		printf("ioctl get freq failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    
	    /* tuner uses khz not mhz ! */
	    if (priv->tuner.flags & VIDEO_TUNER_LOW)
	        freq /= 1000;
	    (unsigned long)*(void **)arg = freq;
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_TUN_SET_FREQ:
	{
	    /* argument is in MHz ! */
	    unsigned long freq = (unsigned long)*(void **)arg;
	    
	    printf("requested frequency: %lu MHz\n", (float)freq/16);
	    
	    /* tuner uses khz not mhz ! */
	    if (priv->tuner.flags & VIDEO_TUNER_LOW)
	        freq *= 1000;
	    printf(" requesting from driver: freq=%.3f\n", (float)freq/16);
	    if (ioctl(priv->fd, VIDIOCSFREQ, &freq) == -1)
	    {
		printf("ioctl set freq failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_TUN_GET_TUNER:
	{
	    if (ioctl(priv->fd, VIDIOCGTUNER, &priv->tuner) == -1)
	    {
		printf("ioctl get tuner failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    
	    printf("Tuner (%s) range: %lu -> %lu\n", priv->tuner.name,
		priv->tuner.rangelow, priv->tuner.rangehigh);
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_TUN_SET_TUNER:
	{
	    if (ioctl(priv->fd, VIDIOCSTUNER, &priv->tuner) == -1)
	    {
		printf("ioctl get tuner failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_TUN_SET_NORM:
	{
	    int req_mode = (int)*(void **)arg;
	    
	    if ((!(priv->tuner.flags & VIDEO_TUNER_NORM)) ||
		((req_mode == VIDEO_MODE_PAL) && !(priv->tuner.flags & VIDEO_TUNER_PAL)) ||
		((req_mode == VIDEO_MODE_NTSC) && !(priv->tuner.flags & VIDEO_TUNER_NTSC)) ||
		((req_mode == VIDEO_MODE_SECAM) && !(priv->tuner.flags & VIDEO_TUNER_SECAM)))
	    {
		printf("tuner isn't capable to set norm!\n");
		return(TVI_CONTROL_FALSE);
	    }

	    priv->tuner.mode = req_mode;
	    
	    if (control(priv->fd, TVI_CONTROL_TUN_SET_TUNER, &priv->tuner) != TVI_CONTROL_TRUE)
		return(TVI_CONTROL_FALSE);
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_TUN_GET_NORM:
	{
	    (int)*(void **)arg = priv->tuner.mode;

	    return(TVI_CONTROL_TRUE);
	}
	
	/* ========== AUDIO controls =========== */
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
	    (int)*(void **)arg = 76000;
	    return(TVI_CONTROL_TRUE);
	}
	
	/* ========== SPECIFIC controls =========== */
	case TVI_CONTROL_SPC_GET_INPUT:
	{
	    int req_chan = (int)*(void **)arg;
	    int i;

	    for (i = 0; i < priv->capability.channels; i++)
	    {
		if (priv->channels[i].channel == req_chan)
		    break;
	    }

	    if (ioctl(priv->fd, VIDIOCGCHAN, &priv->channels[i]) == -1)
	    {
		printf("ioctl get channel failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    return(TVI_CONTROL_TRUE);
	}

	case TVI_CONTROL_SPC_SET_INPUT:
	{
	    struct video_channel chan;
	    int req_chan = (int)*(void **)arg;
	    int i;
	    
	    if (req_chan >= priv->capability.channels)
	    {
		printf("Invalid input requested: %d, valid: 0-%d\n",
		    req_chan, priv->capability.channels);
		return(TVI_CONTROL_FALSE);
	    }

	    for (i = 0; i < priv->capability.channels; i++)
	    {
		if (priv->channels[i].channel == req_chan)
		    chan = priv->channels[i];
	    }

	    if (ioctl(priv->fd, VIDIOCSCHAN, &chan) == -1)
	    {
		printf("ioctl set chan failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    printf("Using input '%s'\n", chan.name);

	    /* update tuner state */
	    if (priv->capability.type & VID_TYPE_TUNER)
		control(priv, TVI_CONTROL_TUN_GET_TUNER, 0);

	    /* update local channel list */	
	    control(priv, TVI_CONTROL_SPC_GET_INPUT, &req_chan);
	    return(TVI_CONTROL_TRUE);
	}
    }

    return(TVI_CONTROL_UNKNOWN);
}

static int grab_video_frame(priv_t *priv, char *buffer, int len)
{
    int frame = priv->queue % priv->nbuf;

    printf("grab_video_frame(priv=%p, buffer=%p, len=%d\n",
	priv, buffer, len);

    printf("buf: %p + frame: %d => %p\n",
	priv->buf, frame, &priv->buf[frame]);
    if (ioctl(priv->fd, VIDIOCMCAPTURE, &priv->buf[frame]) == -1)
    {
	printf("ioctl mcapture failed: %s\n", strerror(errno));
	return(0);
    }
    
    ioctl(priv->fd, VIDIOCSYNC, &priv->buf[frame]);
    priv->queue++;
    
    printf("mmap: %p + offset: %d => %p\n",
	priv->mmap, priv->mbuf.offsets[frame],
	priv->mmap+priv->mbuf.offsets[frame]);
    memcpy(buffer, priv->mmap+priv->mbuf.offsets[frame], len);

    return(len);
}

static int get_video_framesize(priv_t *priv)
{
    return priv->bytesperline * priv->height;
}

static int grab_audio_frame(priv_t *priv, char *buffer, int len)
{
}

static int get_audio_framesize(priv_t *priv)
{
    return 65536;
}

#endif /* USE_TV */
