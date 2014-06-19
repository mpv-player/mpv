#ifndef OSDEP_PATH_H
#define OSDEP_PATH_H

struct mpv_global;

char *mp_get_win_config_dirs(void *talloc_ctx);

// Returns Mac OS X application bundle directory.
char *mp_get_macosx_bundle_dir(void *talloc_ctx);

#endif
