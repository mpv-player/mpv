# Older versions of meson don't allow multiple build targets with the same name in the same
# file. Generate it here for compatibility reasons for windows.
wrapper_sources += '../osdep/win32-gui-wrapper.c'
executable('mpv', wrapper_sources, c_args: '-municode', link_args: '-municode',
           win_subsystem: 'windows,6.0', install: true)
warning('mpv.exe executable will be generated in the player subdirectory.')
