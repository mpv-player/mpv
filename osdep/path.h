#ifndef OSDEP_PATH_H
#define OSDEP_PATH_H

struct mpv_global;

char *mp_get_win_config_path(const char *filename);

// Returns absolute path of a resource file in a Mac OS X application bundle.
char *mp_get_macosx_bundled_path(void *talloc_ctx, struct mpv_global *global,
                                 const char *filename);

#endif
