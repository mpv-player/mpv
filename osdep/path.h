#ifndef OSDEP_PATH_H
#define OSDEP_PATH_H

#define MAX_CONFIG_PATHS 32

struct mpv_global;

// Append paths starting at dirs[i]. The dirs array has place only for at most
// MAX_CONFIG_PATHS paths, but it's guaranteed that at least 4 paths can be
// added without checking for i>=MAX_CONFIG_PATHS.
// Return the new value of i.
int mp_add_win_config_dirs(struct mpv_global *global, char **dirs, int i);
int mp_add_macosx_bundle_dir(struct mpv_global *global, char **dirs, int i);

#endif
