#ifndef __aop_h__
#define __aop_h__

struct af_instance_s;

// Audio data chunk
typedef struct af_data_s
{
  void* audio;  // data buffer
  int len;      // buffer length
  int rate;	// sample rate
  int nch;	// number of channels
  int format;	// format
  int bps; 	// bytes per sample
} af_data_t;

// Fraction, used to calculate buffer lengths
typedef struct frac_s
{
  int n; // Numerator
  int d; // Denominator
} frac_t;

/* Audio filter information not specific for current instance, but for
   a specific filter */ 
typedef struct af_info_s 
{
  const char *info;
  const char *name;
  const char *author;
  const char *comment;
  int (*open)(struct af_instance_s* vf);
} af_info_t;

// Linked list of audio filters
typedef struct af_instance_s
{
  af_info_t* info;
  int (*control)(struct af_instance_s* af, int cmd, void* arg);
  void (*uninit)(struct af_instance_s* af);
  af_data_t* (*play)(struct af_instance_s* af, af_data_t* data);
  void* setup;	  // setup data for this specific instance and filter
  af_data_t* data; // configuration for outgoing data stream
  struct af_instance_s* next;
  struct af_instance_s* prev;  
  frac_t mul; /* length multiplier: how much does this instance change
		 the length of the buffer. */
}af_instance_t;

// Initialization types
#define SLOW  1
#define FAST  2
#define FORCE 3

// Configuration switches
typedef struct af_cfg_s{
  int force;	// Initialization type
  char** list;	/* list of names of plugins that are added to filter
		   list during first initialization of stream */
}af_cfg_t;

// Current audio stream
typedef struct af_stream_s
{
  // The first and last filter in the list
  af_instance_t* first;
  af_instance_t* last;
  // Storage for input and output data formats
  af_data_t input;
  af_data_t output;
  // Cofiguration for this stream
  af_cfg_t cfg;
}af_stream_t;

/*********************************************
// Control parameters 
*/

/* The control system is divided into 3 levels 
   mandatory calls 	 - all filters must answer to all of these
   optional calls  	 - are optional
   filter specific calls - applies only to some filters
*/

#define AF_CONTROL_MANDATORY_BASE	0
#define AF_CONTROL_OPTIONAL_BASE	100
#define AF_CONTROL_FILTER_SPECIFIC_BASE	200

// MANDATORY CALLS

/* Reinitialize filter. The optional argument contains the new
   configuration in form of a af_data_t struct. If the filter does not
   support the new format the struct should be changed and AF_FALSE
   should be returned. If the incoming and outgoing data streams are
   identical the filter can return AF_DETACH. This will remove the
   filter. */
#define AF_CONTROL_REINIT  		1 + AF_CONTROL_MANDATORY_BASE

// OPTIONAL CALLS


// FILTER SPECIFIC CALLS

// Set output rate in resample
#define AF_CONTROL_RESAMPLE		1 + AF_CONTROL_FILTER_SPECIFIC_BASE
// Set output format in format
#define AF_CONTROL_FORMAT		2 + AF_CONTROL_FILTER_SPECIFIC_BASE
// Set number of output channels in channels
#define AF_CONTROL_CHANNELS		3 + AF_CONTROL_FILTER_SPECIFIC_BASE
// Set delay length in delay
#define AF_CONTROL_SET_DELAY_LEN	4 + AF_CONTROL_FILTER_SPECIFIC_BASE
/*********************************************
// Return values
*/

#define AF_DETACH   2
#define AF_OK       1
#define AF_TRUE     1
#define AF_FALSE    0
#define AF_UNKNOWN -1
#define AF_ERROR   -2
#define AF_NA      -3



// Export functions

/* Initialize the stream "s". This function creates a new fileterlist
   if nessesary according to the values set in input and output. Input
   and output should contain the format of the current movie and the
   formate of the preferred output respectively. The function is
   reentreant i.e. if called wit an already initialized stream the
   stream will be reinitialized. The return value is 0 if sucess and
   -1 if failure */
int af_init(af_stream_t* s);
// Uninit and remove all filters
void af_uninit(af_stream_t* s);
// Filter data chunk through the filters in the list
af_data_t* af_play(af_stream_t* s, af_data_t* data);
/* Calculate how long the output from the filters will be given the
   input length "len". The calculated length is >= the actual
   length */
int af_outputlen(af_stream_t* s, int len);
/* Calculate how long the input to the filters should be to produce a
   certain output length, i.e. the return value of this function is
   the input length required to produce the output length "len". The
   calculated length is <= the actual length */
int af_inputlen(af_stream_t* s, int len);



// Helper functions and macros used inside the audio filters

/* Helper function called by the macro with the same name only to be
   called from inside filters */
int af_resize_local_buffer(af_instance_t* af, af_data_t* data);

/* Helper function used to calculate the exact buffer length needed
   when buffers are resized. The returned length is >= than what is
   needed */
int af_lencalc(frac_t mul, af_data_t* data);

/* Memory reallocation macro: if a local buffer is used (i.e. if the
   filter doesn't operate on the incoming buffer this macro must be
   called to ensure the buffer is big enough. */
#define RESIZE_LOCAL_BUFFER(a,d)\
((af->data->len < af_lencalc(af->mul,data))?af_resize_local_buffer(af,data):AF_OK)

#ifndef min
#define min(a,b)(((a)>(b))?(b):(a))
#endif

#ifndef max
#define max(a,b)(((a)>(b))?(a):(b))
#endif

#endif
