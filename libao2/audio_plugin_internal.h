// prototypes:
static int control(int cmd,int arg);
static int init(float*);
static void uninit();
static void reset();
static int play(void* data,int len,int flags);

#define LIBAO_PLUGIN_EXTERN(x) ao_functions_t audio_out_##x =\
{\
	&info,\
	control,\
	init,\
        uninit,\
	reset,\
	play,\
};
