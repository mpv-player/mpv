/*
  Video 4 Linux input

  (C) Alex Beregszaszi <alex@naxine.org>
  
  Some ideas are based on xawtv/libng's grab-v4l.c written by
    Gerd Knorr <kraxel@bytesex.org>

  CODE IS UNDER DEVELOPMENT, NO FEATURE REQUESTS PLEASE!
*/

#include "config.h"

#if defined(USE_TV) && defined(HAVE_TV_V4L)

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/videodev.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>

#include "mp_msg.h"
#include "../libao2/afmt.h"
#include "../libvo/img_format.h"
#include "../libvo/fastmemcpy.h"

#include "tv.h"

static tvi_info_t info = {
	"Video for Linux TV Input",
	"v4l",
	"Alex Beregszaszi <alex@naxine.org>",
	"under development"
};

typedef struct {
    /* general */
    char			*video_device;
    int				fd;
    struct video_capability	capability;
    struct video_channel	*channels;
    int				act_channel;
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
    struct video_audio		*audio;
    int				act_audio;
} priv_t;

#include "tvi_def.h"

static const char *device_cap2name[] = {
    "capture", "tuner", "teletext", "overlay", "chromakey", "clipping",
    "frameram", "scales", "monochrome", "subcapture", "mpeg-decoder",
    "mpeg-encoder", "mjpeg-decoder", "mjpeg-encoder", NULL
};

static const char *device_palette2name[] = {
    "-", "grey", "hi240", "rgb16", "rgb24", "rgb32", "rgb15", "yuv422",
    "yuyv", "uyvy", "yuv420", "yuv411", "raw", "yuv422p", "yuv411p",
    "yuv420p", "yuv410p", NULL
};
#define PALETTE(x) ((x < sizeof(device_pal)/sizeof(char*)) ? device_pal[x] : "UNKNOWN")

static const char *audio_mode2name[] = {
    "unknown", "mono", "stereo", "language1", "language2", NULL
};

static int palette2depth(int palette)
{
    switch(palette)
    {
	/* component */
	case VIDEO_PALETTE_RGB555:
	    return(15);
	case VIDEO_PALETTE_RGB565:
	    return(16);
	case VIDEO_PALETTE_RGB24:
	    return(24);
	case VIDEO_PALETTE_RGB32:
	    return(32);
	/* planar */
	case VIDEO_PALETTE_YUV422P:
	case VIDEO_PALETTE_YUV411P:
	case VIDEO_PALETTE_YUV420P:
	case VIDEO_PALETTE_YUV410P:
	    return(12);
	/* packed */
	case VIDEO_PALETTE_YUV422:
	case VIDEO_PALETTE_YUYV:
	case VIDEO_PALETTE_UYVY:
	case VIDEO_PALETTE_YUV420:
	case VIDEO_PALETTE_YUV411:
	    return(16);
    }
    return(-1);
}

static int format2palette(int format)
{
    switch(format)
    {
	case IMGFMT_RGB15:
	    return(VIDEO_PALETTE_RGB555);
	case IMGFMT_RGB16:
	    return(VIDEO_PALETTE_RGB565);
	case IMGFMT_RGB24:
	    return(VIDEO_PALETTE_RGB24);
	case IMGFMT_RGB32:
	    return(VIDEO_PALETTE_RGB32);
	case IMGFMT_YV12:
	case IMGFMT_I420:
	    return(VIDEO_PALETTE_YUV420P);
	case IMGFMT_UYVY:
	    return(VIDEO_PALETTE_YUV422);
    }
    return(-1);
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
	priv->video_device = strdup("/dev/video0");
    else
	priv->video_device = strdup(device);

    /* allocation failed */
    if (!priv->video_device) {
	free_handle(h);
	return(NULL);
    }

    return(h);
}

static int init(priv_t *priv, tvi_param_t *params)
{
    int i;

    priv->fd = open(priv->video_device, O_RDWR);
    if (priv->fd == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "unable to open '%s': %s\n",
	    priv->video_device, strerror(errno));
	goto err;
    }

    mp_msg(MSGT_TV, MSGL_V, "Video fd: %d\n", priv->fd);
    
    /* get capabilities (priv->capability is needed!) */
    if (ioctl(priv->fd, VIDIOCGCAP, &priv->capability) == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "ioctl get capabilites failed: %s\n", strerror(errno));
	goto err;
    }

    fcntl(priv->fd, F_SETFD, FD_CLOEXEC);

    mp_msg(MSGT_TV, MSGL_INFO, "Selected device: %s\n", priv->capability.name);
    mp_msg(MSGT_TV, MSGL_INFO, " Capabilites: ");
    for (i = 0; device_cap2name[i] != NULL; i++)
	if (priv->capability.type & (1 << i))
	    mp_msg(MSGT_TV, MSGL_INFO, "%s ", device_cap2name[i]);
    mp_msg(MSGT_TV, MSGL_INFO, "\n");
    mp_msg(MSGT_TV, MSGL_INFO, " Device type: %d\n", priv->capability.type);
    mp_msg(MSGT_TV, MSGL_INFO, " Supported sizes: %dx%d => %dx%d\n",
	priv->capability.minwidth, priv->capability.minheight,
	priv->capability.maxwidth, priv->capability.maxheight);
    priv->width = priv->capability.minwidth;
    priv->height = priv->capability.minheight;
    mp_msg(MSGT_TV, MSGL_INFO, " Inputs: %d\n", priv->capability.channels);

    priv->channels = (struct video_channel *)malloc(sizeof(struct video_channel)*priv->capability.channels);
    if (!priv->channels)
	goto malloc_failed;
    memset(priv->channels, 0, sizeof(struct video_channel)*priv->capability.channels);
    for (i = 0; i < priv->capability.channels; i++)
    {
	priv->channels[i].channel = i;
	if (ioctl(priv->fd, VIDIOCGCHAN, &priv->channels[i]) == -1)
	{
	    mp_msg(MSGT_TV, MSGL_ERR, "ioctl get channel failed: %s\n", strerror(errno));
	    break;
	}
	mp_msg(MSGT_TV, MSGL_INFO, "  %d: %s: %s%s%s%s (tuner:%d, norm:%d)\n", i,
	    priv->channels[i].name,
	    (priv->channels[i].flags & VIDEO_VC_TUNER) ? "tuner " : "",
	    (priv->channels[i].flags & VIDEO_VC_AUDIO) ? "audio " : "",
	    (priv->channels[i].flags & VIDEO_TYPE_TV) ? "tv " : "",
	    (priv->channels[i].flags & VIDEO_TYPE_CAMERA) ? "camera " : "",
	    priv->channels[i].tuners,
	    priv->channels[i].norm);
    }

    if (priv->capability.audios)
    {
	mp_msg(MSGT_TV, MSGL_INFO, " Audio devices: %d\n", priv->capability.audios);
	
	priv->act_audio = 0;

	for (i = 0; i < priv->capability.audios; i++)
	{
	    priv->audio = realloc(priv->audio, sizeof(struct video_audio)*(i+1));
	    priv->audio[i].audio = i;
	    if (ioctl(priv->fd, VIDIOCGAUDIO, &priv->audio[i]) == -1)
	    {
		mp_msg(MSGT_TV, MSGL_ERR, "ioctl get audio failed: %s\n", strerror(errno));
		break;
	    }
	    
	    mp_msg(MSGT_TV, MSGL_V, "  %d: %s: ", priv->audio[i].audio,
		priv->audio[i].name);
	    if (priv->audio[i].flags & VIDEO_AUDIO_MUTABLE)
		mp_msg(MSGT_TV, MSGL_V, "muted=%s ",
		    (priv->audio[i].flags & VIDEO_AUDIO_MUTE) ? "yes" : "no");
	    mp_msg(MSGT_TV, MSGL_V, "volume=%d bass=%d treble=%d balance=%d mode=%s\n",
		priv->audio[i].volume, priv->audio[i].bass, priv->audio[i].treble,
		priv->audio[i].balance, audio_mode2name[priv->audio[i].mode]);
		
	    /* un-mute channels */
	    priv->audio[i].flags &= ~VIDEO_AUDIO_MUTE;
	    ioctl(priv->fd, VIDIOCGAUDIO, &priv->audio[i]);
	}
    }

    if (!(priv->capability.type & VID_TYPE_CAPTURE))
    {
	mp_msg(MSGT_TV, MSGL_ERR, "Only grabbing supported (for overlay use another program)\n");
	goto err;
    }
    
    /* map grab buffer */
    if (ioctl(priv->fd, VIDIOCGMBUF, &priv->mbuf) == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "ioctl get mbuf failed: %s\n", strerror(errno));
	goto err;
    }

    mp_msg(MSGT_TV, MSGL_V, "mbuf: size=%d, frames=%d\n",
	priv->mbuf.size, priv->mbuf.frames);
    priv->mmap = mmap(0, priv->mbuf.size, PROT_READ|PROT_WRITE,
		MAP_SHARED, priv->fd, 0);
    if (priv->mmap == (unsigned char *)-1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "Unabel to map memory for buffers: %s\n", strerror(errno));
	goto err;
    }
    mp_msg(MSGT_TV, MSGL_DBG2, "our buffer: %p\n", priv->mmap);

    /* num of buffers */
    priv->nbuf = priv->mbuf.frames;
    
    /* video buffers */
    priv->buf = (struct video_mmap *)malloc(priv->nbuf * sizeof(struct video_mmap));
    if (!priv->buf)
	goto malloc_failed;
    memset(priv->buf, 0, priv->nbuf * sizeof(struct video_mmap));
    
    return(1);


malloc_failed:
    if (priv->channels)
	free(priv->channels);
    if (priv->buf)
	free(priv->buf);
err:
    if (priv->fd != -1)
	close(priv->fd);
    return(0);
}

static int uninit(priv_t *priv)
{
    close(priv->fd);
#warning "Implement uninit!"

    return(1);
}

static int start(priv_t *priv)
{
    int i;

    
    if (ioctl(priv->fd, VIDIOCGPICT, &priv->picture) == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "ioctl get picture failed: %s\n", strerror(errno));
	return(0);
    }

    priv->picture.palette = format2palette(priv->format);
    priv->picture.depth = palette2depth(priv->picture.palette);
    priv->bytesperline = priv->width * priv->picture.depth / 8;
    
    mp_msg(MSGT_TV, MSGL_INFO, "Picture values:\n");
    mp_msg(MSGT_TV, MSGL_INFO, " Depth: %d, Palette: %d (Format: %s)\n", priv->picture.depth,
	priv->picture.palette, vo_format_name(priv->format));
    mp_msg(MSGT_TV, MSGL_INFO, " Brightness: %d, Hue: %d, Colour: %d, Contrast: %d\n",
	priv->picture.brightness, priv->picture.hue,
	priv->picture.colour, priv->picture.contrast);
    

    if (ioctl(priv->fd, VIDIOCSPICT, &priv->picture) == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "ioctl set picture failed: %s\n", strerror(errno));
	return(0);
    }

    priv->nbuf = priv->mbuf.frames;
    for (i=0; i < priv->nbuf; i++)
    {
	priv->buf[i].format = priv->picture.palette;
	priv->buf[i].frame = i;
	priv->buf[i].width = priv->width;
	priv->buf[i].height = priv->height;
	mp_msg(MSGT_TV, MSGL_DBG2, "buffer: %d => %p\n", i, &priv->buf[i]);
    } 

#if 0
    {
	struct video_play_mode pmode;
	
	pmode.mode = VID_PLAY_NORMAL;
	pmode.p1 = 1;
	pmode.p2 = 0;
	if (ioctl(priv->fd, VIDIOCSPLAYMODE, &pmode) == -1)
	{
	    mp_msg(MSGT_TV, MSGL_ERR, "ioctl set play mode failed: %s\n", strerror(errno));
//	    return(0);
	}
    }
#endif

#if 0
    {
	struct video_window win;

	win.x = 0;
	win.y = 0;
	win.width = priv->width;
	win.height = priv->height;
	win.chromakey = -1;
	win.flags = 0;
	
	ioctl(priv->fd, VIDIOCSWIN, &win);
    }

    /* start capture */
    if (ioctl(priv->fd, VIDIOCCAPTURE, &one) == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "ioctl capture failed: %s\n", strerror(errno));
	return(0);
    }
#endif

    return(1);
}

static int control(priv_t *priv, int cmd, void *arg)
{
    mp_msg(MSGT_TV, MSGL_DBG2, "debug: control(priv=%p, cmd=%d, arg=%p)\n",
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
#if 0 /* also disable audio for as it's not working! */
	    if (priv->channels[priv->act_channel].flags & VIDEO_VC_AUDIO)
		return(TVI_CONTROL_TRUE);
#endif
	    return(TVI_CONTROL_FALSE);
	case TVI_CONTROL_IS_TUNER:
	{
//	    if (priv->capability.type & VID_TYPE_TUNER)
	    if (priv->channels[priv->act_channel].flags & VIDEO_VC_TUNER)
		return(TVI_CONTROL_TRUE);
	    return(TVI_CONTROL_FALSE);
	}

	/* ========== VIDEO controls =========== */
	case TVI_CONTROL_VID_GET_FORMAT:
	{
	    int output_fmt = -1;

	    output_fmt = priv->format;
	    (int)*(void **)arg = output_fmt;
	    mp_msg(MSGT_TV, MSGL_INFO, "Output format: %s\n", vo_format_name(output_fmt));
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_VID_SET_FORMAT:
	    priv->format = (int)*(void **)arg;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_PLANES:
	    (int)*(void **)arg = 1; /* FIXME, also not needed at this time */
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_BITS:
	    (int)*(void **)arg = palette2depth(format2palette(priv->format));
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_WIDTH:
	    (int)*(void **)arg = priv->width;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_CHK_WIDTH:
	{
	    int req_width = (int)*(void **)arg;
	    
	    mp_msg(MSGT_TV, MSGL_INFO, "Requested width: %d\n", req_width);
	    if ((req_width >= priv->capability.minwidth) &&
		(req_width <= priv->capability.maxwidth))
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
	    
	    mp_msg(MSGT_TV, MSGL_INFO, "Requested height: %d\n", req_height);
	    if ((req_height >= priv->capability.minheight) &&
		(req_height <= priv->capability.maxheight))
		return(TVI_CONTROL_TRUE);
	    return(TVI_CONTROL_FALSE);
	}
	case TVI_CONTROL_VID_SET_HEIGHT:
	    priv->height = (int)*(void **)arg;
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_GET_PICTURE:
	    if (ioctl(priv->fd, VIDIOCGPICT, &priv->picture) == -1)
	    {
		mp_msg(MSGT_TV, MSGL_ERR, "ioctl get picture failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_SET_PICTURE:
	    if (ioctl(priv->fd, VIDIOCSPICT, &priv->picture) == -1)
	    {
		mp_msg(MSGT_TV, MSGL_ERR, "ioctl get picture failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_SET_BRIGHTNESS:
	    priv->picture.brightness = (int)*(void **)arg;
	    control(priv, TVI_CONTROL_VID_SET_PICTURE, 0);
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_SET_HUE:
	    priv->picture.hue = (int)*(void **)arg;
	    control(priv, TVI_CONTROL_VID_SET_PICTURE, 0);
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_SET_SATURATION:
	    priv->picture.colour = (int)*(void **)arg;
	    control(priv, TVI_CONTROL_VID_SET_PICTURE, 0);
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_SET_CONTRAST:
	    priv->picture.contrast = (int)*(void **)arg;
	    control(priv, TVI_CONTROL_VID_SET_PICTURE, 0);
	    return(TVI_CONTROL_TRUE);

	/* ========== TUNER controls =========== */
	case TVI_CONTROL_TUN_GET_FREQ:
	{
	    unsigned long freq;
	    
	    if (ioctl(priv->fd, VIDIOCGFREQ, &freq) == -1)
	    {
		mp_msg(MSGT_TV, MSGL_ERR, "ioctl get freq failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    
	    /* tuner uses khz not mhz ! */
//	    if (priv->tuner.flags & VIDEO_TUNER_LOW)
//	        freq /= 1000;
	    (unsigned long)*(void **)arg = freq;
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_TUN_SET_FREQ:
	{
	    /* argument is in MHz ! */
	    unsigned long freq = (unsigned long)*(void **)arg;
	    
	    mp_msg(MSGT_TV, MSGL_V, "requested frequency: %.3f\n", (float)freq/16);
	    
	    /* tuner uses khz not mhz ! */
//	    if (priv->tuner.flags & VIDEO_TUNER_LOW)
//	        freq *= 1000;
//	    mp_msg(MSGT_TV, MSGL_V, " requesting from driver: freq=%.3f\n", (float)freq/16);
	    if (ioctl(priv->fd, VIDIOCSFREQ, &freq) == -1)
	    {
		mp_msg(MSGT_TV, MSGL_ERR, "ioctl set freq failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_TUN_GET_TUNER:
	{
	    if (ioctl(priv->fd, VIDIOCGTUNER, &priv->tuner) == -1)
	    {
		mp_msg(MSGT_TV, MSGL_ERR, "ioctl get tuner failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    
	    mp_msg(MSGT_TV, MSGL_INFO, "Tuner (%s) range: %lu -> %lu\n", priv->tuner.name,
		priv->tuner.rangelow, priv->tuner.rangehigh);
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_TUN_SET_TUNER:
	{
	    if (ioctl(priv->fd, VIDIOCSTUNER, &priv->tuner) == -1)
	    {
		mp_msg(MSGT_TV, MSGL_ERR, "ioctl get tuner failed: %s\n", strerror(errno));
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
		mp_msg(MSGT_TV, MSGL_ERR, "Tuner isn't capable to set norm!\n");
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
	    
	    priv->act_channel = i;

	    if (ioctl(priv->fd, VIDIOCGCHAN, &priv->channels[i]) == -1)
	    {
		mp_msg(MSGT_TV, MSGL_ERR, "ioctl get channel failed: %s\n", strerror(errno));
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
		mp_msg(MSGT_TV, MSGL_ERR, "Invalid input requested: %d, valid: 0-%d\n",
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
		mp_msg(MSGT_TV, MSGL_ERR, "ioctl set chan failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    mp_msg(MSGT_TV, MSGL_INFO, "Using input '%s'\n", chan.name);

	    priv->act_channel = i;

	    /* update tuner state */
//	    if (priv->capability.type & VID_TYPE_TUNER)
	    if (priv->channels[priv->act_channel].flags & VIDEO_VC_TUNER)
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
    int nextframe = (priv->queue+1) % priv->nbuf;

    mp_dbg(MSGT_TV, MSGL_DBG2, "grab_video_frame(priv=%p, buffer=%p, len=%d)\n",
	priv, buffer, len);

    mp_dbg(MSGT_TV, MSGL_DBG3, "buf: %p + frame: %d => %p\n",
	priv->buf, nextframe, &priv->buf[nextframe]);
    if (ioctl(priv->fd, VIDIOCMCAPTURE, &priv->buf[nextframe]) == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "ioctl mcapture failed: %s\n", strerror(errno));
	return(0);
    }
    
    if (ioctl(priv->fd, VIDIOCSYNC, &priv->buf[frame].frame) == -1)
	mp_msg(MSGT_TV, MSGL_ERR, "ioctl sync failed: %s\n", strerror(errno));
    priv->queue++;
    
    mp_dbg(MSGT_TV, MSGL_DBG3, "mmap: %p + offset: %d => %p\n",
	priv->mmap, priv->mbuf.offsets[frame],
	priv->mmap+priv->mbuf.offsets[frame]);

    /* XXX also directrendering would be nicer! */
    /* 3 times copying the same picture to other buffer :( */

    /* copy the actual frame */
    memcpy(buffer, priv->mmap+priv->mbuf.offsets[frame], len);

    return(len);
}

static int get_video_framesize(priv_t *priv)
{
    return(priv->bytesperline * priv->height);
}

static int grab_audio_frame(priv_t *priv, char *buffer, int len)
{
    return(65536);
}

static int get_audio_framesize(priv_t *priv)
{
    return(65536);
}

#endif /* USE_TV */
