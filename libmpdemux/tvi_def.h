static int init(priv_t *priv);
static int exit(priv_t *priv);
static int control(priv_t *priv, int cmd, void *arg);
static int grab_video_frame(priv_t *priv, char *buffer, int len);
static int get_video_framesize(priv_t *priv);
static int grab_audio_frame(priv_t *priv, char *buffer, int len);
static int get_audio_framesize(priv_t *priv);

static tvi_functions_t functions =
{
    init,
    exit,
    control,
    grab_video_frame,
    get_video_framesize,
    grab_audio_frame,
    get_audio_framesize
};

static tvi_handle_t *new_handle()
{
    tvi_handle_t *h = malloc(sizeof(tvi_handle_t));

    if (!h)
	return(NULL);
    h->priv = malloc(sizeof(priv_t));
    if (!h->priv)
    {
	free(h);
	return(NULL);
    }
    memset(h->priv, 0, sizeof(priv_t));
    h->info = &info;
    h->functions = &functions;
    return(h);
}

static void free_handle(tvi_handle_t *h)
{
    if (h->priv)
	free(h->priv);
    if (h)
	free(h);
}
