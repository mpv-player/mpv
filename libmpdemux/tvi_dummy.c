#include <stdio.h>

#include "config.h"

#ifdef USE_TV
#include "tv.h"

static tvi_info_t info = {
	"NULL-TV",
	"dummy",
	"alex",
	"non-completed"
};

typedef struct {
} priv_t;

#include "tvi_def.h"

tvi_handle_t *tvi_init_dummy(char *device)
{
    return new_handle();
}

static int init(priv_t *priv)
{
}

static int close(priv_t *priv)
{
}

static int control(priv_t *priv, int cmd, void *arg)
{
    return(TVI_CONTROL_UNKNOWN);
}

static int grab_video_frame(priv_t *priv, char *buffer, int len)
{
    memset(buffer, 0x77, len);
}

static int get_video_framesize(priv_t *priv)
{
    return 0;
}

static int grab_audio_frame(priv_t *priv, char *buffer, int len)
{
    memset(buffer, 0x77, len);
}

static int get_audio_framesize(priv_t *priv)
{
    return 0;
}

#endif /* USE_TV */
