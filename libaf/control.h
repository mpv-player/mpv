#ifndef __af_control_h
#define __af_control_h

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
#define AF_CONTROL_REINIT  		01 + AF_CONTROL_MANDATORY_BASE

// OPTIONAL CALLS

/* Called just after creation with the af_cfg for the stream in which
   the filter resides as input parameter this call can be used by the
   filter to initialize itself using commandline parameters */
#define AF_CONTROL_POST_CREATE 		1 + AF_CONTROL_OPTIONAL_BASE

// Called just before destruction of a filter
#define AF_CONTROL_PRE_DESTROY 		2 + AF_CONTROL_OPTIONAL_BASE


// FILTER SPECIFIC CALLS

// Set output rate in resample
#define AF_CONTROL_RESAMPLE		1 + AF_CONTROL_FILTER_SPECIFIC_BASE

// Set output format in format
#define AF_CONTROL_FORMAT		2 + AF_CONTROL_FILTER_SPECIFIC_BASE

// Set number of output channels in channels
#define AF_CONTROL_CHANNELS		3 + AF_CONTROL_FILTER_SPECIFIC_BASE

// Set delay length in delay
#define AF_CONTROL_DELAY_SET_LEN	4 + AF_CONTROL_FILTER_SPECIFIC_BASE

// Volume 

// Set volume level, arg is a float* with the volume for all the channels
#define AF_CONTROL_VOLUME_SET		5 + AF_CONTROL_FILTER_SPECIFIC_BASE

/* Get volume level for all channels, arg is a float* that will
   contain the volume for all the channels */
#define AF_CONTROL_VOLUME_GET		6 + AF_CONTROL_FILTER_SPECIFIC_BASE

// Turn volume control on and off, arg is binary
#define AF_CONTROL_VOLUME_ON_OFF	7 + AF_CONTROL_FILTER_SPECIFIC_BASE

// Turn soft clipping of the volume on and off, arg is binary
#define AF_CONTROL_VOLUME_SOFTCLIP	8 + AF_CONTROL_FILTER_SPECIFIC_BASE

// Get the probed power level for all channels, arg is a float* 
#define AF_CONTROL_VOLUME_PROBE_GET	9 + AF_CONTROL_FILTER_SPECIFIC_BASE

// Get the maximum probed power level for all channels, arg is a float* 
#define AF_CONTROL_VOLUME_PROBE_GET_MAX	10 + AF_CONTROL_FILTER_SPECIFIC_BASE

// Turn probing on and off, arg is binary
#define AF_CONTROL_VOLUME_PROBE_ON_OFF 	11 + AF_CONTROL_FILTER_SPECIFIC_BASE

#endif /*__af_control_h */
