sources += files('keyboard.c',
                 'path.c',
                 'timer.c',
                 'utils.c')

if features['win32-desktop']
    sources += files('language.c',
                     'main-fn.c',
                     'subprocess.c',
                     'terminal.c')
endif

if features['glob-win32']
    sources += files('glob.c')
endif

windows = import('windows')
res_flags = ['--codepage=65001']

# Unintuitively, this compile operates out of the osdep subdirectory.
# Hence, these includes are needed.
res_includes = [source_root, build_root]

resources = [source_root / 'etc/mpv-icon-8bit-16x16.png',
             source_root / 'etc/mpv-icon-8bit-32x32.png',
             source_root / 'etc/mpv-icon-8bit-64x64.png',
             source_root / 'etc/mpv-icon-8bit-128x128.png',
             source_root / 'etc/mpv-icon.ico',
             'mpv.exe.manifest']

sources += windows.compile_resources('mpv.rc', args: res_flags, depend_files: resources,
                                     depends: version_h, include_directories: res_includes)
