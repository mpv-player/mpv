#ifndef OSDEP_PATH_H
#define OSDEP_PATH_H

struct mpv_global;

// Windows config directories
char *mp_get_win_exe_dir(void *talloc_ctx);
char *mp_get_win_exe_subdir(void *talloc_ctx);
char *mp_get_win_app_dir(void *talloc_ctx);

// Returns Mac OS X application bundle directory.
char *mp_get_macosx_bundle_dir(void *talloc_ctx);

#endif
