
#ifndef __AUDIO_OUT_H
#define __AUDIO_OUT_H

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

/* interface towards mplayer and */
typedef struct ao_functions_s
{
	ao_info_t *info;
        int (*control)(int cmd,void *arg);
        int (*init)(int rate,int channels,int format,int flags);
        void (*uninit)(int immed);
        void (*reset)(void);
        int (*get_space)(void);
        int (*play)(void* data,int len,int flags);
        float (*get_delay)(void);
        void (*pause)(void);
        void (*resume)(void);
} ao_functions_t;

/* global data used by mplayer and plugins */
typedef struct ao_data_s
{
  int samplerate;
  int channels;
  int format;
  int bps;
  int outburst; 	  
  int buffersize;         
  int pts;
} ao_data_t;

extern char *ao_subdevice;
extern ao_data_t ao_data;

void list_audio_out(void);
ao_functions_t* init_best_audio_out(char** ao_list,int use_plugin,int rate,int channels,int format,int flags);

// NULL terminated array of all drivers
extern ao_functions_t* audio_out_drivers[];

#define CONTROL_OK 1
#define CONTROL_TRUE 1
#define CONTROL_FALSE 0
#define CONTROL_UNKNOWN -1
#define CONTROL_ERROR -2
#define CONTROL_NA -3

#define AOCONTROL_SET_DEVICE 1
#define AOCONTROL_GET_DEVICE 2
#define AOCONTROL_QUERY_FORMAT 3 /* test for availabilty of a format */
#define AOCONTROL_GET_VOLUME 4
#define AOCONTROL_SET_VOLUME 5
#define AOCONTROL_SET_PLUGIN_DRIVER 6
#define AOCONTROL_SET_PLUGIN_LIST 7

#define AOPLAY_FINAL_CHUNK 1

typedef struct ao_control_vol_s {
	float left;
	float right;
} ao_control_vol_t;

#endif
