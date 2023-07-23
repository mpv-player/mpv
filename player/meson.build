subdir('javascript')
subdir('lua')

# Meson doesn't allow having multiple build targets with the same name in the same file.
# Just generate the com in here for windows builds.
if win32 and get_option('cplayer')
    wrapper_sources= '../osdep/win32-console-wrapper.c'
    executable('mpv', wrapper_sources, c_args: '-municode', link_args: '-municode',
               name_suffix: 'com', install: true)
endif
