#ifndef OSDEP_PATH_H
#define OSDEP_PATH_H

#define MAX_CONFIG_PATHS 32

struct mpv_global;

void mp_add_win_config_dirs(struct mpv_global *global, char **dirs, int i);

// Returns Mac OS X application bundle directory.
char *mp_get_macosx_bundle_dir(void *talloc_ctx);

#endif
