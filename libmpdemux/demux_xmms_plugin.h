/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef PLUGIN_H
#define PLUGIN_H

typedef enum
{
	FMT_U8, FMT_S8, FMT_U16_LE, FMT_U16_BE, FMT_U16_NE, FMT_S16_LE, FMT_S16_BE, FMT_S16_NE
}
AFormat;

typedef struct
{
	void *handle;		/* Filled in by xmms */
	char *filename;		/* Filled in by xmms */
	char *description;	/* The description that is shown in the preferences box */
	void (*init) (void);
	void (*about) (void);	/* Show the about box */
	void (*configure) (void);	/* Show the configuration dialog */
	void (*get_volume) (int *l, int *r);
	void (*set_volume) (int l, int r);	/* Set the volume */
	int (*open_audio) (AFormat fmt, int rate, int nch);	/* Open the device, if the device can't handle the given 
								   parameters the plugin is responsible for downmixing
								   the data to the right format before outputting it */
	void (*write_audio) (void *ptr, int length);	/* The input plugin calls this to write data to the output 
							   buffer */
	void (*close_audio) (void);	/* No comment... */
	void (*flush) (int time);	/* Flush the buffer and set the plugins internal timers to time */
	void (*pause) (short paused);	/* Pause or unpause the output */
	int (*buffer_free) (void);	/* Return the amount of data that can be written to the buffer,
					   two calls to this without a call to write_audio should make
					   the plugin output audio directly */
	int (*buffer_playing) (void);	/* Returns TRUE if the plugin currently is playing some audio,
					   otherwise return FALSE */
	int (*output_time) (void);	/* Return the current playing time */
	int (*written_time) (void);	/* Return the length of all the data that has been written to
					   the buffer */
}
OutputPlugin;

typedef struct
{
	void *handle;		/* Filled in by xmms */
	char *filename;		/* Filled in by xmms */
	char *description;	/* The description that is shown in the preferences box */
	void (*init) (void);	/* Called when the plugin is loaded */
	void (*cleanup) (void);	/* Called when the plugin is unloaded */
	void (*about) (void);	/* Show the about box */
	void (*configure) (void);	/* Show the configure box */
	int (*mod_samples) (void *data, int length, AFormat fmt, int srate, int nch);	/* Modify samples */
	void (*query_format) (AFormat *fmt,int *rate, int *nch);
}
EffectPlugin;

typedef enum
{
	INPUT_VIS_ANALYZER, INPUT_VIS_SCOPE, INPUT_VIS_VU, INPUT_VIS_OFF
}
InputVisType;

typedef struct
{
	void *handle;		/* Filled in by xmms */
	char *filename;		/* Filled in by xmms */
	char *description;	/* The description that is shown in the preferences box */
	void (*init) (void);	/* Called when the plugin is loaded */
	void (*about) (void);	/* Show the about box */
	void (*configure) (void);
	int (*is_our_file) (char *filename);	/* Return 1 if the plugin can handle the file */
	void *(*scan_dir) (char *dirname);	/* Look in Input/cdaudio/cdaudio.c to see how */
	/* to use this */
	void (*play_file) (char *filename);	/* Guess what... */
	void (*stop) (void);	/* Tricky one */
	void (*pause) (short paused);	/* Pause or unpause */
	void (*seek) (int time);	/* Seek to the specified time */
	void (*set_eq) (int on, float preamp, float *bands);	/* Set the equalizer, most plugins won't be able to do this */
	int (*get_time) (void);	/* Get the time, usually returns the output plugins output time */
	void (*get_volume) (int *l, int *r);	/* Input-plugin specific volume functions, just provide a NULL if */
	void (*set_volume) (int l, int r);	/*  you want the output plugin to handle it */
	void (*cleanup) (void);			/* Called when xmms exit */
	InputVisType (*get_vis_type) (void); /* OBSOLETE, DO NOT USE! */
	void (*add_vis_pcm) (int time, AFormat fmt, int nch, int length, void *ptr); /* Send data to the visualization plugins 
											Preferably 512 samples/block */
	void (*set_info) (char *title, int length, int rate, int freq, int nch);	/* Fill in the stuff that is shown in the player window
											   set length to -1 if it's unknown. Filled in by xmms */
	void (*set_info_text) (char *text);	/* Show some text in the song title box in the main window,
						   call it with NULL as argument to reset it to the song title.
						   Filled in by xmms */
	void (*get_song_info) (char *filename, char **title, int *length);	/* Function to grab the title string */
	void (*file_info_box) (char *filename);		/* Bring up an info window for the filename passed in */
	OutputPlugin *output;	/* Handle to the current output plugin. Filled in by xmms */
}
InputPlugin;

/* So that input plugins can get the title formatting information */
char *xmms_get_gentitle_format(void);

/* So that output plugins can communicate with effect plugins */
EffectPlugin *get_current_effect_plugin(void);
int effects_enabled(void);

typedef struct
{
	void *handle;		/* Filled in by xmms */
	char *filename;		/* Filled in by xmms */
	int xmms_session;	/* The session ID for attaching to the control socket */
	char *description;	/* The description that is shown in the preferences box */
	void (*init) (void);	/* Called when the plugin is enabled */
	void (*about) (void);	/* Show the about box */
	void (*configure) (void);
	void (*cleanup) (void);	/* Called when the plugin is disabled or when xmms exits */
}
GeneralPlugin;

typedef struct _VisPlugin
{
	void *handle; 	/* Filled in by xmms */
	char *filename; /* Filled in by xmms */
	int xmms_session; /* The session ID for attaching to the control socket */
	char *description; /* The description that is shown in the preferences box */
	int num_pcm_chs_wanted; /* Numbers of PCM channels wanted in the call to render_pcm */
	int num_freq_chs_wanted; /* Numbers of freq channels wanted in the call to render_freq */
	void (*init)(void); /* Called when the plugin is enabled */
	void (*cleanup)(void); /* Called when the plugin is disabled */
	void (*about)(void); /* Show the about box */
	void (*configure)(void); /* Show the configure box */
	void (*disable_plugin)(struct _VisPlugin *); /* Call this with a pointer to your plugin to disable the plugin */
	void (*playback_start)(void); /* Called when playback starts */
	void (*playback_stop)(void); /* Called when playback stops */
	void (*render_pcm)(short pcm_data[2][512]); /* Render the PCM data, don't do anything time consuming in here */
	void (*render_freq)(short freq_data[2][256]); /* Render the freq data, don't do anything time consuming in here */
} VisPlugin;

#endif
