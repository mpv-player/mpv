#include <stdlib.h> /* malloc */
#include <string.h> /* memset */

static int init(priv_t *priv);
static int uninit(priv_t *priv);
static int control(priv_t *priv, int cmd, void *arg);
static int start(priv_t *priv);
static double grab_video_frame(priv_t *priv, char *buffer, int len);
#ifdef HAVE_TV_BSDBT848
static double grabimmediate_video_frame(priv_t *priv, char *buffer, int len);
#endif
static int get_video_framesize(priv_t *priv);
static double grab_audio_frame(priv_t *priv, char *buffer, int len);
static int get_audio_framesize(priv_t *priv);

static tvi_functions_t functions =
{
    init,
    uninit,
    control,
    start,
    grab_video_frame,
#ifdef HAVE_TV_BSDBT848
    grabimmediate_video_frame,
#endif
    get_video_framesize,
    grab_audio_frame,
    get_audio_framesize
};

static tvi_handle_t *new_handle(void)
{
    tvi_handle_t *h = (tvi_handle_t *)malloc(sizeof(tvi_handle_t));

    if (!h)
	return(NULL);
    h->priv = (priv_t *)malloc(sizeof(priv_t));
    if (!h->priv)
    {
	free(h);
	return(NULL);
    }
    memset(h->priv, 0, sizeof(priv_t));
    h->functions = &functions;
    h->seq = 0;
    h->chanlist = -1;
    h->chanlist_s = NULL;
    h->norm = -1;
    h->channel = -1;
    return(h);
}

static void free_handle(tvi_handle_t *h)
{
    if (h) {
	if (h->priv)
	    free(h->priv);
	free(h);
    }
}
