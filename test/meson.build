# So we don't have to reorganize the entire directory tree.
incdir = include_directories('../')
outdir = join_paths(build_root, 'test', 'out')
refdir = join_paths(source_root, 'test', 'ref')

# Convenient testing libraries. An adhoc collection of
# mpv objects that test_utils.c needs. Paths and subprocesses
# are required in order to run a diff command when comparing
# different files. Stuff will probably break if core things are
# carelessly moved around.
test_utils_args = []
test_utils_files = [
    'audio/chmap.c',
    'audio/format.c',
    'common/common.c',
    'misc/bstr.c',
    'misc/dispatch.c',
    'misc/json.c',
    'misc/node.c',
    'misc/random.c',
    'misc/thread_tools.c',
    'options/m_config_core.c',
    'options/m_config_frontend.c',
    'options/m_option.c',
    'options/path.c',
    'osdep/io.c',
    'osdep/subprocess.c',
    'osdep/timer.c',
    timer_source,
    path_source,
    subprocess_source,
    'ta/ta.c',
    'ta/ta_talloc.c',
    'ta/ta_utils.c'
]

test_utils_deps = [libavutil, libm]

if win32
    test_utils_files += 'osdep/windows_utils.c'
endif

if features['pthread-debug']
    test_utils_files += 'osdep/threads-posix.c'
endif

# The zimg code requires using threads.
if not features['win32-threads']
    test_utils_deps += pthreads
endif

if features['win32-desktop']
    test_utils_deps += cc.find_library('imm32')
    test_utils_deps += cc.find_library('ntdll')
endif
test_utils_objects = libmpv.extract_objects(test_utils_files)
test_utils = static_library('test-utils', 'test_utils.c', include_directories: incdir,
                            c_args: test_utils_args, objects: test_utils_objects,
                            dependencies: test_utils_deps)

# For getting imgfmts and stuff.
img_utils_files = [
    'misc/thread_pool.c',
    'video/csputils.c',
    'video/fmt-conversion.c',
    'video/img_format.c',
    'video/mp_image.c',
    'video/out/placebo/utils.c',
    'video/sws_utils.c'
]
if features['zimg']
    img_utils_files += ['video/repack.c', 'video/zimg.c']
endif

img_utils_objects = libmpv.extract_objects(img_utils_files)
img_utils = static_library('img-utils', 'img_utils.c', include_directories: incdir,
                           dependencies: [libavcodec], objects: img_utils_objects)

# The actual tests.
chmap_files = [
    'audio/chmap_sel.c'
]
if features['av-channel-layout']
    chmap_files += 'audio/chmap_avchannel.c'
endif
chmap_objects = libmpv.extract_objects(chmap_files)
chmap = executable('chmap', 'chmap.c', include_directories: incdir,
                   dependencies: [libavutil], objects: chmap_objects,
                   link_with: test_utils)
test('chmap', chmap)

gl_video_objects = libmpv.extract_objects('video/out/gpu/ra.c',
                                          'video/out/gpu/utils.c')
gl_video = executable('gl-video', 'gl_video.c', objects: gl_video_objects,
                      dependencies: [libavutil], include_directories: incdir,
                      link_with: [img_utils, test_utils])
test('gl-video', gl_video)

json = executable('json', 'json.c', include_directories: incdir, link_with: test_utils)
test('json', json)

linked_list = executable('linked-list', files('linked_list.c'), include_directories: incdir)
test('linked-list', linked_list)

timer = executable('timer', files('timer.c'), include_directories: incdir, link_with: test_utils)
test('timer', timer)

paths_objects = libmpv.extract_objects('options/path.c', path_source)
paths = executable('paths', 'paths.c', include_directories: incdir,
                   objects: paths_objects, link_with: test_utils)
test('paths', paths)

if get_option('libmpv')
    libmpv_test = executable('libmpv-test', 'libmpv_test.c',
                             include_directories: incdir, link_with: libmpv)
    file = join_paths(source_root, 'etc', 'mpv-icon-8bit-16x16.png')
    test('libmpv', libmpv_test, args: file, timeout: 60)
endif

# Minimum required libavutil version that works with these tests.
# Will need to be manually updated when ffmpeg adds/removes more formats in the future.
if libavutil.version().version_compare('>= 58.27.100')

# The CI can randomly fail if libavutil isn't explicitly linked again here.
    img_format = executable('img-format', 'img_format.c', include_directories: incdir,
                            dependencies: [libavutil, libplacebo], link_with: [img_utils, test_utils])
    test('img-format', img_format, args: [refdir, outdir], suite: 'ffmpeg')


    scale_sws_objects = libmpv.extract_objects('video/image_writer.c',
                                               'video/repack.c')
    scale_sws = executable('scale-sws', ['scale_sws.c', 'scale_test.c'], include_directories: incdir,
                           objects: scale_sws_objects, dependencies: [libavutil, libavformat, libswscale, jpeg, zimg, libplacebo],
                           link_with: [img_utils, test_utils])
    test('scale-sws', scale_sws, args: [refdir, outdir], suite: 'ffmpeg')

    if features['zimg']
        repack_objects = libmpv.extract_objects('sub/draw_bmp.c')
        repack = executable('repack', 'repack.c', include_directories: incdir, objects: repack_objects,
                            dependencies: [libavutil, libswscale, zimg, libplacebo], link_with: [img_utils, test_utils])
        test('repack', repack, args: [refdir, outdir], suite: 'ffmpeg')

        scale_zimg_objects = libmpv.extract_objects('video/image_writer.c')
        scale_zimg = executable('scale-zimg', ['scale_test.c', 'scale_zimg.c'], include_directories: incdir,
                                objects: scale_zimg_objects, dependencies:[libavutil, libavformat, libswscale, jpeg, zimg, libplacebo],
                                link_with: [img_utils, test_utils])
        test('scale-zimg', scale_zimg, args: [refdir, outdir], suite: 'ffmpeg')
    endif
endif
