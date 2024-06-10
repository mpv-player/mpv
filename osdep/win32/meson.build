add_languages('cpp', native: false)
cpp = meson.get_compiler('cpp')

features += {'win32-smtc': cpp.has_header('winrt/base.h', required: get_option('win32-smtc'))}
if features['win32-smtc']
    dependencies += cpp.find_library('runtimeobject')
    sources += meson.current_source_dir() / 'smtc.cpp'
endif
