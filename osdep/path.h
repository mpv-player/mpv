#ifndef OSDEP_PATH_H
#define OSDEP_PATH_H

#ifdef _WIN32
char *mp_get_win_config_path(const char *filename);
#endif

#endif
