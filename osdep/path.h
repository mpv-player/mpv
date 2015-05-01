#ifndef OSDEP_PATH_H
#define OSDEP_PATH_H

// Return a platform-specific path, identified by the type parameter. If the
// return value is allocated, talloc_ctx is used as talloc parent context.
//
// The following type values are defined:
//  "home"          the native mpv-specific user config dir
//  "old_home"      same as "home", but lesser priority (compatibility)
//  "osxbundle"     OSX bundle path
//  "global"        the least priority, global config file location
//
// It is allowed to return a static string, so the caller must set talloc_ctx
// to something other than NULL to avoid memory leaks.
typedef const char *(*mp_get_platform_path_cb)(void *talloc_ctx, const char *type);

// Conforming to mp_get_platform_path_cb.
const char *mp_get_platform_path_win(void *talloc_ctx, const char *type);
const char *mp_get_platform_path_osx(void *talloc_ctx, const char *type);
const char *mp_get_platform_path_unix(void *talloc_ctx, const char *type);

#endif
