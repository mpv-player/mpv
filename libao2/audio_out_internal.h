
// prototypes:
//static ao_info_t info;
static int control(int cmd,int arg);
static int init(int rate,int channels,int format,int flags);
static void uninit();
static void reset();
static int get_space();
static int play(void* data,int len,int flags);
static int get_delay();

#define LIBAO_EXTERN(x) ao_functions_t audio_out_##x =\
{\
	&info,\
	control,\
	init,\
        uninit,\
	reset,\
	get_space,\
	play,\
	get_delay\
};

