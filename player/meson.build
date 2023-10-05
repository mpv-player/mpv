subdir('javascript')
subdir('lua')

# Older versions of meson don't allow multiple build targets with the same name in the same
# file. Generate it here for compatibility reasons for windows.
if win32 and get_option('cplayer') and meson.version().version_compare('< 1.3.0')
    wrapper_sources= '../osdep/win32-console-wrapper.c'
    executable('mpv', wrapper_sources, c_args: '-municode', link_args: '-municode',
               name_suffix: 'com', install: true)
    warning('mpv.com executable will be generated in the player subdirectory.')
endif
