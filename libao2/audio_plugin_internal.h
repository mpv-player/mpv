// prototypes:
static int control(int cmd, void *arg);
static int init();
static void uninit();
static void reset();
static int play();

#define LIBAO_PLUGIN_EXTERN(x) ao_plugin_functions_t audio_plugin_##x =\
{\
	&info,\
	control,\
	init,\
        uninit,\
	reset,\
	play,\
};
