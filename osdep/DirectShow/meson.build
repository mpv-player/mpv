mpv_dshow = static_library('mpv_dshow',
    sources: files(
        'DSUtilLite/DShowUtil.cpp',
        'baseclasses/strmctl.cpp',
        'baseclasses/videoctl.cpp',
    ),
    build_by_default : false,
    dependencies: [
        cc.find_library('strmiids'),
        cc.find_library('quartz'),
    ],
    include_directories: include_directories('.'),
)
