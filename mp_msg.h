
// verbosity elevel:

#define MSGL_FATAL 0  // will exit/abort
#define MSGL_ERROR 1  // continues
#define MSGL_WARN 2   // only warning
#define MSGL_INFO 3   // -quiet
#define MSGL_STATUS 4 // v=0
#define MSGL_VERBOSE 5// v=1
#define MSGL_DEBUG2 6 // v=2
#define MSGL_DEBUG3 7 // v=3
#define MSGL_DEBUG4 8 // v=4

// code/module:

#define MSGT_GLOBAL 0        // fatal errors
#define MSGT_CPLAYER 1       // console player
#define MSGT_GPLAYER 2       // gui player

#define MSGT_VO 3	       // libvo
#define MSGT_AO 4	       // libao

#define MSGT_DEMUXER 5    // demuxer.c (general stuff)
#define MSGT_DS 6         // demux stream (add/read packet etc)
#define MSGT_DEMUX 7      // fileformat-specific stuff (demux_*.c)

#define MSGT_MAX 64

void mp_msg_init(int verbose);
void mp_msg_c( int x, const char *format, ... );

#define mp_msg(mod,lev,...) mp_msg_c((mod<<8)|lev,__VA_ARGS__)
#define mp_dbg(mod,lev,...) mp_msg_c((mod<<8)|lev,__VA_ARGS__)

