
// prototypes:
//static ao_info_t info;
static int control(int cmd, void *arg);
static int init(int rate,int channels,int format,int flags);
static void uninit(int immed);
static void reset(void);
static int get_space(void);
static int play(void* data,int len,int flags);
static float get_delay(void);
static void audio_pause(void);
static void audio_resume(void);

#define LIBAO_EXTERN(x) ao_functions_t audio_out_##x =\
{\
	&info,\
	control,\
	init,\
        uninit,\
	reset,\
	get_space,\
	play,\
	get_delay,\
	audio_pause,\
	audio_resume\
};

