/* functions supplied by plugins */
typedef struct ao_plugin_functions_s
{
	ao_info_t *info;
        int (*control)(int cmd,int arg);
        int (*init)(); 
        void (*uninit)();
        void (*reset)();
        int (*play)();
} ao_plugin_functions_t;

/* Global data for all audio plugins */
typedef struct ao_plugin_data_s
{
  void* data;       /* current data block read only ok to change */
  int len;          /* setup and current buffer length */
  int rate;	    /* setup data rate */
  int channels;	    /* setup number of channels */
  int format;	    /* setup format */
  double sz_mult;   /* Buffer size multiplier */
  double sz_fix;    /* Fix (as in static) extra buffer size */
  float delay_mult; /* Delay multiplier */
  float delay_fix;  /* Fix delay */
}ao_plugin_data_t;

extern ao_plugin_data_t ao_plugin_data;

//List of plugins 


#define AOCONTROL_PLUGIN_SET_LEN 1
