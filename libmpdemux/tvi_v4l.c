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
#include <sys/time.h>
#include <linux/videodev.h>
#include <linux/soundcard.h>
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
	"Video 4 Linux input",
	"v4l",
	"Alex Beregszaszi <alex@naxine.org>",
	"under development"
};

#define MAX_AUDIO_CHANNELS	10

typedef struct {
    /* general */
    char			*video_device;
    int				video_fd;
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
    int				fps;

    struct video_mbuf		mbuf;
    unsigned char		*mmap;
    struct video_mmap		*buf;
    int				nbuf;
    int				queue;

    /* audio */
    int				audio_id;
    char			*audio_device;
    struct video_audio		audio[MAX_AUDIO_CHANNELS];
    int				audio_fd;
    int				audio_channels[MAX_AUDIO_CHANNELS];
    int				audio_format[MAX_AUDIO_CHANNELS];
    int				audio_samplesize[MAX_AUDIO_CHANNELS];
    int				audio_samplerate[MAX_AUDIO_CHANNELS];
    int				audio_blocksize;

    /* other */
    double			starttime;
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
	case VIDEO_PALETTE_YUV411P:
	case VIDEO_PALETTE_YUV420P:
	case VIDEO_PALETTE_YUV410P:
	    return(12);
	/* packed */
	case VIDEO_PALETTE_YUV422P:
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
	case IMGFMT_YUY2:
	    return(VIDEO_PALETTE_YUYV);
    }
    return(-1);
}

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
	priv->video_device = strdup("/dev/video");
    else
	priv->video_device = strdup(device);

    /* allocation failed */
    if (!priv->video_device) {
	free_handle(h);
	return(NULL);
    }

    /* set audio device name */
    priv->audio_device = strdup("/dev/dsp");

    return(h);
}

static int init(priv_t *priv)
{
    int i;

    priv->video_fd = open(priv->video_device, O_RDWR);
    mp_msg(MSGT_TV, MSGL_DBG2, "Video fd: %d, %x\n", priv->video_fd,
	priv->video_device);
    if (priv->video_fd == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "unable to open '%s': %s\n",
	    priv->video_device, strerror(errno));
	goto err;
    }
    
    priv->fps = 25; /* pal */
    
    /* get capabilities (priv->capability is needed!) */
    if (ioctl(priv->video_fd, VIDIOCGCAP, &priv->capability) == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "ioctl get capabilites failed: %s\n", strerror(errno));
	goto err;
    }

    fcntl(priv->video_fd, F_SETFD, FD_CLOEXEC);

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
	if (ioctl(priv->video_fd, VIDIOCGCHAN, &priv->channels[i]) == -1)
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

    /* audio chanlist */
    if (priv->capability.audios)
    {
	mp_msg(MSGT_TV, MSGL_INFO, " Audio devices: %d\n", priv->capability.audios);

	for (i = 0; i < priv->capability.audios; i++)
	{
	    if (i >= MAX_AUDIO_CHANNELS)
	    {
		mp_msg(MSGT_TV, MSGL_ERR, "no space for more audio channels (incrase in source!) (%d > %d)\n",
		    i, MAX_AUDIO_CHANNELS);
		i = priv->capability.audios;
		break;
	    }

	    priv->audio[i].audio = i;
	    if (ioctl(priv->video_fd, VIDIOCGAUDIO, &priv->audio[i]) == -1)
	    {
		mp_msg(MSGT_TV, MSGL_ERR, "ioctl get audio failed: %s\n", strerror(errno));
		break;
	    }
	    
	    if (priv->audio[i].volume <= 0)
		priv->audio[i].volume = 100;
	    priv->audio[i].flags &= ~VIDEO_AUDIO_MUTE;
	    ioctl(priv->video_fd, VIDIOCSAUDIO, &priv->audio[i]);
	    
	    switch(priv->audio[i].mode)
	    {
		case VIDEO_SOUND_MONO:
		case VIDEO_SOUND_LANG1:
		case VIDEO_SOUND_LANG2:
		    priv->audio_channels[i] = 1;
		    break;
		case VIDEO_SOUND_STEREO:
		    priv->audio_channels[i] = 2;
		    break;
	    }
	    
	    priv->audio_format[i] = AFMT_S16_LE;
	    priv->audio_samplerate[i] = 44100;
	    priv->audio_samplesize[i] =
		priv->audio_samplerate[i]/8/priv->fps*
		priv->audio_channels[i];

	    /* display stuff */
	    mp_msg(MSGT_TV, MSGL_V, "  %d: %s: ", priv->audio[i].audio,
		priv->audio[i].name);
	    if (priv->audio[i].flags & VIDEO_AUDIO_MUTABLE)
		mp_msg(MSGT_TV, MSGL_V, "muted=%s ",
		    (priv->audio[i].flags & VIDEO_AUDIO_MUTE) ? "yes" : "no");
	    mp_msg(MSGT_TV, MSGL_V, "volume=%d bass=%d treble=%d balance=%d mode=%s\n",
		priv->audio[i].volume, priv->audio[i].bass, priv->audio[i].treble,
		priv->audio[i].balance, audio_mode2name[priv->audio[i].mode]);
	    mp_msg(MSGT_TV, MSGL_V, " channels: %d, samplerate: %d, samplesize: %d, format: %s\n",
		priv->audio_channels[i], priv->audio_samplerate[i], priv->audio_samplesize[i],
		audio_out_format_name(priv->audio_format[i]));
	}
    }

    if (!(priv->capability.type & VID_TYPE_CAPTURE))
    {
	mp_msg(MSGT_TV, MSGL_ERR, "Only grabbing supported (for overlay use another program)\n");
	goto err;
    }
    
    /* map grab buffer */
    if (ioctl(priv->video_fd, VIDIOCGMBUF, &priv->mbuf) == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "ioctl get mbuf failed: %s\n", strerror(errno));
	goto err;
    }

    mp_msg(MSGT_TV, MSGL_V, "mbuf: size=%d, frames=%d\n",
	priv->mbuf.size, priv->mbuf.frames);
    priv->mmap = mmap(0, priv->mbuf.size, PROT_READ|PROT_WRITE,
		MAP_SHARED, priv->video_fd, 0);
    if (priv->mmap == (unsigned char *)-1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "Unable to map memory for buffers: %s\n", strerror(errno));
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
    
    /* audio init */
#if 1
    priv->audio_fd = open(priv->audio_device, O_RDONLY);
    if (priv->audio_fd < 0)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "unable to open '%s': %s\n",
	    priv->audio_device, strerror(errno));
    }
    else
    {
	int ioctl_param;
	
	fcntl(priv->audio_fd, F_SETFL, O_NONBLOCK);

#if 0
	ioctl_param = 0x7fff000d; /* 8k */
	printf("ioctl dsp setfragment: %d\n",
	    ioctl(priv->audio_fd, SNDCTL_DSP_SETFRAGMENT, &ioctl_param));	
#endif

	ioctl_param = 0 ;
	printf("ioctl dsp getfmt: %d\n",
	    ioctl(priv->audio_fd, SNDCTL_DSP_GETFMTS, &ioctl_param));
	
	printf("Supported formats: %x\n", ioctl_param);
	if (!(ioctl_param & priv->audio_format[priv->audio_id]))
	    printf("notsupported format\n");

	ioctl_param = priv->audio_format[priv->audio_id];
	printf("ioctl dsp setfmt: %d\n",
	    ioctl(priv->audio_fd, SNDCTL_DSP_SETFMT, &ioctl_param));

//	ioctl(priv->audio_fd, SNDCTL_DSP_GETISPACE, &ioctl_param);
//	printf("getispace: %d\n", ioctl_param);

	if (priv->audio_channels[priv->audio_id] > 2)
	{
	    ioctl_param = priv->audio_channels[priv->audio_id];
	    printf("ioctl dsp channels: %d\n",
		ioctl(priv->audio_fd, SNDCTL_DSP_CHANNELS, &ioctl_param));
	}
	else
	{
//	    if (priv->audio_channels[priv->audio_id] == 2)
//		ioctl_param = 1;
//	    else
//		ioctl_param = 0;
	
	    ioctl_param = (priv->audio_channels[priv->audio_id] == 2);
	    printf("ioctl dsp stereo: %d (req: %d)\n",
		ioctl(priv->audio_fd, SNDCTL_DSP_STEREO, &ioctl_param),
		ioctl_param);
	}
	
	ioctl_param = priv->audio_samplerate[priv->audio_id];
	printf("ioctl dsp speed: %d\n",
	    ioctl(priv->audio_fd, SNDCTL_DSP_SPEED, &ioctl_param));

#if 0
	ioctl_param = 0;
	ioctl_param = ~PCM_ENABLE_INPUT;
	printf("ioctl dsp trigger: %d\n",
	    ioctl(priv->audio_fd, SNDCTL_DSP_SETTRIGGER, &ioctl_param));
	ioctl_param = PCM_ENABLE_INPUT;
	printf("ioctl dsp trigger: %d\n",
	    ioctl(priv->audio_fd, SNDCTL_DSP_SETTRIGGER, &ioctl_param));
#endif

	printf("ioctl dsp trigger: %d\n",
	    ioctl(priv->audio_fd, SNDCTL_DSP_GETTRIGGER, &ioctl_param));
	printf("trigger: %x\n", ioctl_param);
	ioctl_param = PCM_ENABLE_INPUT;
	printf("ioctl dsp trigger: %d\n",
	    ioctl(priv->audio_fd, SNDCTL_DSP_SETTRIGGER, &ioctl_param));

	printf("ioctl dsp getblocksize: %d\n",
	    ioctl(priv->audio_fd, SNDCTL_DSP_GETBLKSIZE, &priv->audio_blocksize));
	printf("blocksize: %d\n", priv->audio_blocksize);
    }
#endif    
    return(1);


malloc_failed:
    if (priv->channels)
	free(priv->channels);
    if (priv->buf)
	free(priv->buf);
err:
    if (priv->video_fd != -1)
	close(priv->video_fd);
    return(0);
}

static int uninit(priv_t *priv)
{
    close(priv->video_fd);

    priv->audio[priv->audio_id].volume = 0;
    priv->audio[priv->audio_id].flags |= VIDEO_AUDIO_MUTE;
    ioctl(priv->video_fd, VIDIOCSAUDIO, &priv->audio[priv->audio_id]);
    close(priv->audio_fd);

    return(1);
}

static int start(priv_t *priv)
{
    int i;

    
    if (ioctl(priv->video_fd, VIDIOCGPICT, &priv->picture) == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "ioctl get picture failed: %s\n", strerror(errno));
	return(0);
    }

    priv->picture.palette = format2palette(priv->format);
    priv->picture.depth = palette2depth(priv->picture.palette);
    priv->bytesperline = priv->width * priv->picture.depth / 8;
//    if (IMGFMT_IS_BGR(priv->format) || IMGFMT_IS_RGB(priv->format))
//	priv->bytesperline = priv->width * priv->picture.depth / 8;
//    if ((priv->format == IMGFMT_YV12) || (priv->format == IMGFMT_I420) || (priv->format == IMGFMT_IYUV))
//	priv->bytesperline = priv->width * 3 / 2;

    printf("palette: %d, depth: %d, bytesperline: %d\n",
	priv->picture.palette, priv->picture.depth, priv->bytesperline);

    mp_msg(MSGT_TV, MSGL_INFO, "Picture values:\n");
    mp_msg(MSGT_TV, MSGL_INFO, " Depth: %d, Palette: %d (Format: %s)\n", priv->picture.depth,
	priv->picture.palette, vo_format_name(priv->format));
    mp_msg(MSGT_TV, MSGL_INFO, " Brightness: %d, Hue: %d, Colour: %d, Contrast: %d\n",
	priv->picture.brightness, priv->picture.hue,
	priv->picture.colour, priv->picture.contrast);
    

    if (ioctl(priv->video_fd, VIDIOCSPICT, &priv->picture) == -1)
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
	if (ioctl(priv->video_fd, VIDIOCSPLAYMODE, &pmode) == -1)
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
	//win.clipcount = 0;
	
	ioctl(priv->video_fd, VIDIOCSWIN, &win);
    }

    /* start capture */
    if (ioctl(priv->video_fd, VIDIOCCAPTURE, &one) == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "ioctl capture failed: %s\n", strerror(errno));
	return(0);
    }
#endif

    {
      struct timeval curtime;
      gettimeofday(&curtime, NULL);
      priv->starttime=curtime.tv_sec + curtime.tv_usec*.000001;
    }

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
	    if (priv->channels[priv->act_channel].flags & VIDEO_VC_AUDIO)
	    {
		return(TVI_CONTROL_TRUE);
	    }
	    return(TVI_CONTROL_TRUE);
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
	    if (ioctl(priv->video_fd, VIDIOCGPICT, &priv->picture) == -1)
	    {
		mp_msg(MSGT_TV, MSGL_ERR, "ioctl get picture failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    return(TVI_CONTROL_TRUE);
	case TVI_CONTROL_VID_SET_PICTURE:
	    if (ioctl(priv->video_fd, VIDIOCSPICT, &priv->picture) == -1)
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
	case TVI_CONTROL_VID_GET_FPS:
	    (int)*(void **)arg=priv->fps;
	    return(TVI_CONTROL_TRUE);

	/* ========== TUNER controls =========== */
	case TVI_CONTROL_TUN_GET_FREQ:
	{
	    unsigned long freq;
	    
	    if (ioctl(priv->video_fd, VIDIOCGFREQ, &freq) == -1)
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
	    if (ioctl(priv->video_fd, VIDIOCSFREQ, &freq) == -1)
	    {
		mp_msg(MSGT_TV, MSGL_ERR, "ioctl set freq failed: %s\n", strerror(errno));
		return(TVI_CONTROL_FALSE);
	    }
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_TUN_GET_TUNER:
	{
	    if (ioctl(priv->video_fd, VIDIOCGTUNER, &priv->tuner) == -1)
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
	    if (ioctl(priv->video_fd, VIDIOCSTUNER, &priv->tuner) == -1)
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
	    
	    if (control(priv, TVI_CONTROL_TUN_SET_TUNER, &priv->tuner) != TVI_CONTROL_TRUE)
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
	    (int)*(void **)arg = priv->audio_format[priv->audio_id];
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_AUD_GET_CHANNELS:
	{
	    (int)*(void **)arg = priv->audio_channels[priv->audio_id];
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_AUD_GET_SAMPLERATE:
	{
	    (int)*(void **)arg = priv->audio_samplerate[priv->audio_id];
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_AUD_GET_SAMPLESIZE:
	{
	    (int)*(void **)arg = priv->audio_samplesize[priv->audio_id]/8;
	    return(TVI_CONTROL_TRUE);
	}
	case TVI_CONTROL_AUD_SET_SAMPLERATE:
	{
	    int tmp = priv->audio_samplerate[priv->audio_id] = (int)*(void **)arg;
	    
	    if (ioctl(priv->audio_fd, SNDCTL_DSP_SPEED, &tmp) == -1)
		return(TVI_CONTROL_FALSE);
	    priv->audio_samplesize[priv->audio_id] =
		priv->audio_samplerate[priv->audio_id]/8/priv->fps*
		priv->audio_channels[priv->audio_id];
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

	    if (ioctl(priv->video_fd, VIDIOCGCHAN, &priv->channels[i]) == -1)
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

	    if (ioctl(priv->video_fd, VIDIOCSCHAN, &chan) == -1)
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

static double grab_video_frame(priv_t *priv, char *buffer, int len)
{
    struct timeval curtime;
    double timestamp;
    int frame = priv->queue % priv->nbuf;
    int nextframe = (priv->queue+1) % priv->nbuf;

    mp_dbg(MSGT_TV, MSGL_DBG2, "grab_video_frame(priv=%p, buffer=%p, len=%d)\n",
	priv, buffer, len);

    mp_dbg(MSGT_TV, MSGL_DBG3, "buf: %p + frame: %d => %p\n",
	priv->buf, nextframe, &priv->buf[nextframe]);
    if (ioctl(priv->video_fd, VIDIOCMCAPTURE, &priv->buf[nextframe]) == -1)
    {
	mp_msg(MSGT_TV, MSGL_ERR, "ioctl mcapture failed: %s\n", strerror(errno));
	return(0);
    }

    while (ioctl(priv->video_fd, VIDIOCSYNC, &priv->buf[frame].frame) < 0 &&
	(errno == EAGAIN || errno == EINTR));
	mp_dbg(MSGT_TV, MSGL_DBG3, "picture sync failed\n");

    priv->queue++;
    
    gettimeofday(&curtime, NULL);
    timestamp=curtime.tv_sec + curtime.tv_usec*.000001;

    mp_dbg(MSGT_TV, MSGL_DBG3, "mmap: %p + offset: %d => %p\n",
	priv->mmap, priv->mbuf.offsets[frame],
	priv->mmap+priv->mbuf.offsets[frame]);

    /* XXX also directrendering would be nicer! */
    /* 3 times copying the same picture to other buffer :( */

    /* copy the actual frame */
    memcpy(buffer, priv->mmap+priv->mbuf.offsets[frame], len);

    return(timestamp-priv->starttime);
}

static int get_video_framesize(priv_t *priv)
{
    return(priv->bytesperline * priv->height);
}

static double grab_audio_frame(priv_t *priv, char *buffer, int len)
{
    int in_len = 0;
    int max_tries = 2;

    mp_dbg(MSGT_TV, MSGL_DBG2, "grab_audio_frame(priv=%p, buffer=%p, len=%d)\n",
	priv, buffer, len);
    
    while (--max_tries > 0)
//    for (;;)
    {
	in_len = read(priv->audio_fd, buffer, len);
//	printf("in_len: %d\n", in_len);
//	fflush(NULL);

	if (in_len > 0)
	    break;
	if (!((in_len == 0) || (in_len == -1 && (errno == EAGAIN || errno == EINTR))))
	{
	    in_len = 0; /* -EIO */
	    break;
	}
    }

    return 0; //(in_len); // FIXME!
}

static int get_audio_framesize(priv_t *priv)
{
    return(priv->audio_blocksize);
//    return(priv->audio_samplesize[priv->audio_id]);
}

#endif /* USE_TV */
