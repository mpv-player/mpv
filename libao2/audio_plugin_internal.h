// prototypes:
static int control(int cmd,int arg);
static int init();
static void uninit();
static void reset();
static int play();

#define LIBAO_PLUGIN_EXTERN(x) ao_functions_t audio_plugin_##x =\
{\
	&info,\
	control,\
	init,\
        uninit,\
	reset,\
	play,\
};
