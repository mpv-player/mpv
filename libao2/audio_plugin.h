#ifndef __audio_plugin_h__
#define __audio_plugin_h__

// Functions supplied by plugins
typedef struct ao_plugin_functions_s
{
	ao_info_t *info;
        int (*control)(int cmd,int arg);
        int (*init)(); 
        void (*uninit)();
        void (*reset)();
        int (*play)();
} ao_plugin_functions_t;

// Global data for all audio plugins
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

extern volatile ao_plugin_data_t ao_plugin_data;

// Plugin confuguration data set by cmd-line parameters
typedef struct ao_plugin_cfg_s
{
  char* plugin_list; 	// List of used plugins read from cfg
  int pl_format_type;	// Output format
  int pl_delay_len;	// Number of samples to delay sound output
  int pl_resample_fout;	// Output frequency from resampling
  int pl_volume_volume; // Initial volume setting
  float pl_extrastereo_mul; // Stereo enhancer multiplier
  int pl_volume_softclip;   // Enable soft clipping
} ao_plugin_cfg_t;

extern ao_plugin_cfg_t ao_plugin_cfg;

// Configuration defaults
#define CFG_DEFAULTS { \
 NULL, \
 AFMT_S16_LE, \
 0, \
 48000, \
 101, \
 2.5, \
 0 \
};

// This block should not be available in the pl_xxxx files
// due to compilation issues
#ifndef PLUGIN
#define NPL 7+1 // Number of PLugins ( +1 list ends with NULL )
// List of plugins 
extern ao_plugin_functions_t audio_plugin_delay;
extern ao_plugin_functions_t audio_plugin_format; 
extern ao_plugin_functions_t audio_plugin_surround;
extern ao_plugin_functions_t audio_plugin_resample;
extern ao_plugin_functions_t audio_plugin_volume;
extern ao_plugin_functions_t audio_plugin_extrastereo;
extern ao_plugin_functions_t audio_plugin_volnorm;


#define AO_PLUGINS { \
   &audio_plugin_delay, \
   &audio_plugin_format, \
   &audio_plugin_surround, \
   &audio_plugin_resample, \
   &audio_plugin_volume, \
   &audio_plugin_extrastereo, \
   &audio_plugin_volnorm, \
   NULL \
}
#endif /* PLUGIN */


// Control parameters used by the plugins
#define AOCONTROL_PLUGIN_SET_LEN 1  // All plugins must respond to this parameter

#endif

