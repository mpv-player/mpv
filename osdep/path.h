#ifndef OSDEP_PATH_H
#define OSDEP_PATH_H

#include "config.h"

#ifdef _WIN32
char *mp_get_win_config_path(const char *filename);
#endif

#ifdef CONFIG_COCOA
// Returns absolute path of a resource file in a Mac OS X application bundle.
char *mp_get_macosx_bundled_path(const char *filename);
#endif

#endif
