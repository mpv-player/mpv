/* Include file for mplayer specific defines and includes */
#ifndef __af_mp_h__
#define __af_mp_h__

#include "config.h"
#include "mp_msg.h"
#include "cpudetect.h"

/* Set the initialization type from mplayers cpudetect */
#ifdef AF_INIT_TYPE
#undef AF_INIT_TYPE
#define AF_INIT_TYPE \
  ((gCpuCaps.has3DNow || gCpuCaps.hasSSE)?AF_INIT_FAST:AF_INIT_SLOW)
#endif 

#ifdef af_msg
#undef af_msg
#endif
#define af_msg(lev, args... ) \
  mp_msg(MSGT_AFILTER,(((lev)<0)?((lev)+3):(((lev)==0)?MSGL_INFO:((lev)+5))), ##args )

#endif /* __af_mp_h__ */
