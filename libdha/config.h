#ifndef LIBDHA_CONFIG_H
#define LIBDHA_CONFIG_H

#include "../config.h"

#ifdef TARGET_LINUX
#ifndef __powerpc__
#define CONFIG_DHAHELPER
#endif
#endif

#if defined(__powerpc__) && defined(CONFIG_SVGAHELPER)
#undef CONFIG_SVGAHELPER
#endif

#endif /* LIBDHA_CONFIG_H */
