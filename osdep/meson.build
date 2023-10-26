if posix and not darwin
    subdir('unix')
endif

if darwin
    subdir('darwin')
endif

if features['win32-desktop']
    subdir('win32')
endif

if features['uwp']
    subdir('uwp')
endif

if not posix and not features['win32-desktop']
    sources += files('subprocess-dummy.c')
endif
