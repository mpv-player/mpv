#include "config.h"

#ifdef USE_TV

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/videodev.h>
#include <unistd.h>
#include <sys/mman.h>

#include "tv.h"

static tvi_info_t info = {
	"Video for Linux TV Input",
	"v4l",
	"alex",
	"non-completed"
};

typedef struct {
    char			*video_device;
    int				fd;
    struct video_capability	capability;
    struct video_channel	*channels;
    struct video_tuner		tuner;
    struct video_audio		audio;
    struct video_picture	picture;

    int				buffered;
    struct video_mbuf		mbuf;
    unsigned int		*mmap;
    struct video_mmap		*buf;
    
    int				width;
    int				height;
} priv_t;

#include "tvi_def.h"

static const char *device_cap[] = {
    "capture", "tuner", "teletext", "overlay", "chromakey", "clipping",
    "frameram", "scales", "monochrome", NULL
};

tvi_handle_t *tvi_init_v4l(char *device)
{
    tvi_handle_t *h;
    priv_t *priv;
    
    h = new_handle();
    if (!h)
	return(NULL);

    priv = h->priv;

    if (!device)
    {
	priv->video_device = malloc(strlen("/dev/video0"));
	strcpy(priv->video_device, &"/dev/video0");
    }
    else
    {
	priv->video_device = malloc(strlen(device));
	strcpy(priv->video_device, device);
    }

    return(h);
}

static int init(priv_t *priv)
{
    int i;

    priv->fd = open(priv->video_device, O_RDONLY);
    if (priv->fd == -1)
    {
	printf("v4l: open %s: %s\n", priv->video_device, strerror(errno));
	goto err;
    }

    printf("fd: %d\n", priv->fd);
    
    /* get capabilities */
    if (ioctl(priv->fd, VIDIOCGCAP, &priv->capability) == -1)
    {
	printf("v4l: ioctl error: %s\n", strerror(errno));
	goto err;
    }

    fcntl(priv->fd, F_SETFD, FD_CLOEXEC);
    
    printf("capabilites: ");
    for (i = 0; device_cap[i] != NULL; i++)
	if (priv->capability.type & (1 << i))
	    printf(" %s", device_cap[i]);
    printf("\n");
    printf(" type: %d\n", priv->capability.type);
    printf(" size: %dx%d => %dx%d\n",
	priv->capability.minwidth, priv->capability.minheight,
	priv->capability.maxwidth, priv->capability.maxheight);
    priv->width = priv->capability.minwidth;
    priv->height = priv->capability.minheight;
    printf(" channels: %d\n", priv->capability.channels);

    priv->channels = malloc(sizeof(struct video_channel)*priv->capability.channels);
    memset(priv->channels, 0, sizeof(struct video_channel)*priv->capability.channels);
    for (i = 0; i < priv->capability.channels; i++)
    {
	priv->channels[i].channel = i;
	ioctl(priv->fd, VIDIOCGCHAN, &priv->channels[i]);
	printf(" %s: tuners:%d %s%s %s%s\n",
	    priv->channels[i].name,
	    priv->channels[i].tuners,
	    (priv->channels[i].flags & VIDEO_VC_TUNER) ? "tuner " : "",
	    (priv->channels[i].flags & VIDEO_VC_AUDIO) ? "audio " : "",
	    (priv->channels[i].flags & VIDEO_TYPE_TV) ? "tv " : "",
	    (priv->channels[i].flags & VIDEO_TYPE_CAMERA) ? "camera " : "");
    }

    if (priv->capability.type & VID_TYPE_CAPTURE)
    {
	if (ioctl(priv->fd, VIDIOCGMBUF, &priv->mbuf) == 0)
	{
	    printf("mbuf: size=%d, frames=%d (first offset: %p)\n",
		priv->mbuf.size, priv->mbuf.frames, priv->mbuf.offsets[0]);
	    priv->mmap = mmap(0, priv->mbuf.size, PROT_READ|PROT_WRITE,
			    MAP_SHARED, priv->fd, 0);
	    if (priv->mmap == -1)
		perror("mmap");
	}
	else
	    priv->mmap = -1;

	if (priv->mmap != -1)
	{
	    priv->buf = malloc(priv->mbuf.frames * sizeof(struct video_mmap));
	    memset(priv->buf, 0, priv->mbuf.frames * sizeof(struct video_mmap));
	    priv->buffered = 1;
	}
	else
	    priv->buffered = 0;
    }
    
    printf("buffered: %d\n", priv->buffered);
    
    return(1);

err:
    if (priv->fd != -1)
	close(priv->fd);
    return(0);
}

static int exit(priv_t *priv)
{
}

static int tune(priv_t *priv, int freq, int chan, int norm)
{
    if (freq)
    {
	ioctl(priv->fd, VIDIOCSFREQ, &freq);
	return(1);
    }

    if (chan && norm)
    {
	/* set channel & norm ! */
    }

    return(0);
}

static int control(priv_t *priv, int cmd, void *arg)
{
    switch(cmd)
    {
	case TVI_CONTROL_VID_GET_FORMAT:
	    (int)*(void **)arg = 0x0;
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
	    
	    printf("req_width: %d\n", req_width);
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
	    
	    printf("req_height: %d\n", req_height);
	    if ((req_height > priv->capability.minheight) &&
		(req_height < priv->capability.maxheight))
		return(TVI_CONTROL_TRUE);
	    return(TVI_CONTROL_FALSE);
	}
	case TVI_CONTROL_VID_SET_HEIGHT:
	    priv->height = (int)*(void **)arg;
	    return(TVI_CONTROL_TRUE);

	case TVI_CONTROL_TUN_SET_FREQ:
	{
	    long freq = (long)*(void **)arg; /* shit: long -> freq */
	    
	    printf("requested frequency: %f\n", freq);
	    if (ioctl(priv->fd, VIDIOCSFREQ, &freq) != -1)
		return(TVI_CONTROL_TRUE);
	    return(TVI_CONTROL_FALSE);
	}
    }

    return(TVI_CONTROL_UNKNOWN);
}

static int grab_video_frame(priv_t *priv, char *buffer, int len)
{
    priv->buf[0].frame = 0;
    priv->buf[0].width = 320;
    priv->buf[0].height = 240;
    priv->buf[0].format = VIDEO_PALETTE_YUV422;
    
    if (ioctl(priv->fd, VIDIOCMCAPTURE, priv->buf) == -1)
    {
	printf("grab_video_frame failed: %s\n", strerror(errno));
	return 0;
    }
    
    return 1;
}

static int get_video_framesize(priv_t *priv)
{
    return 65536;
}

static int grab_audio_frame(priv_t *priv, char *buffer, int len)
{
}

static int get_audio_framesize(priv_t *priv)
{
    return 65536;
}

#endif /* USE_TV */
