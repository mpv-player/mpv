
typedef struct ao_info_s
{
        /* driver name ("Matrox Millennium G200/G400" */
        const char *name;
        /* short name (for config strings) ("mga") */
        const char *short_name;
        /* author ("Aaron Holtzman <aholtzma@ess.engr.uvic.ca>") */
        const char *author;
        /* any additional comments */
        const char *comment;
} ao_info_t;

typedef struct ao_functions_s {

	ao_info_t *info;

        /*
         */
        int (*control)(int cmd,int arg);

        /*
         */
        int (*init)(int rate,int channels,int format,int flags);

        /*
         */
        void (*uninit)();

        /*
         */
        void (*reset)();

        /*
         */
        int (*get_space)();

        /*
         */
        int (*play)(void* data,int len,int flags);

        /*
         */
        int (*get_delay)();

        /*
         */
        void (*pause)();

        /*
         */
        void (*resume)();

} ao_functions_t;

// NULL terminated array of all drivers
extern ao_functions_t* audio_out_drivers[];

extern int ao_samplerate;
extern int ao_channels;
extern int ao_format;
extern int ao_bps;
extern int ao_outburst;
extern int ao_buffersize;

#define CONTROL_OK 1
#define CONTROL_TRUE 1
#define CONTROL_FALSE 0
#define CONTROL_UNKNOWN -1
#define CONTROL_ERROR -2
#define CONTROL_NA -3

#define AOCONTROL_SET_DEVICE 1
#define AOCONTROL_QUERY_FORMAT 2


