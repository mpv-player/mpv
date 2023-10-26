sources += files('path.c',
                 'semaphore.c',
                 'timer.c')

if features['cocoa']
    sources += files('utils.c',
                     'language.c',
                     'application.m',
                     'events.m',
                     'menubar.m',
                     'main-fn.c',
                     'path.m')
endif

if features['macos-touchbar']
    sources += files('touchbar.m')
endif

if features['swift'] and swift_sources.length() > 0
    swift_sources += files('libmpv_helper.swift',
                        'log_helper.swift',
                        'mpv_helper.swift',
                        'precise_timer.swift',
                        'swift_compat.swift',
                        'swift_extensions.swift')

    if features['macos-media-player']
        swift_sources += files('remote_command_center.swift')
    endif

    # custom swift targets
    bridge = join_paths(source_root, 'osdep/darwin/swift_bridge.h')
    header = join_paths(build_root, 'osdep/darwin/swift.h')
    module = join_paths(build_root, 'osdep/darwin/swift.swiftmodule')
    target = join_paths(build_root, 'osdep/darwin/swift.o')

    swift_flags = ['-frontend', '-c', '-sdk', macos_sdk_path,
                '-enable-objc-interop', '-emit-objc-header', '-parse-as-library']

    if swift_ver.version_compare('>=6.0')
        swift_flags += ['-swift-version', '5']
    endif

    if get_option('debug')
        swift_flags += '-g'
    endif

    if get_option('optimization') != '0'
        swift_flags += '-O'
    endif

    if macos_10_11_features.allowed()
        swift_flags += ['-D', 'HAVE_MACOS_10_11_FEATURES']
    endif

    if macos_10_14_features.allowed()
        swift_flags += ['-D', 'HAVE_MACOS_10_14_FEATURES']
    endif

    extra_flags = get_option('swift-flags').split()
    swift_flags += extra_flags

    swift_compile = [swift_prog, swift_flags, '-module-name', 'swift',
                    '-emit-module-path', '@OUTPUT0@', '-import-objc-header', bridge,
                    '-emit-objc-header-path', '@OUTPUT1@', '-o', '@OUTPUT2@',
                    '@INPUT@', '-I.', '-I' + source_root,
                    '-I' + libplacebo.get_variable('includedir',
                                default_value: source_root / 'subprojects' / 'libplacebo' / 'src' / 'include')]

    swift_targets = custom_target('swift_targets',
        input: swift_sources,
        output: ['swift.swiftmodule', 'swift.h', 'swift.o'],
        command: swift_compile,
    )
    sources += swift_targets

    swift_lib_dir_py = find_program(join_paths(tools_directory, 'macos-swift-lib-directory.py'))
    swift_lib_dir = run_command(swift_lib_dir_py, swift_prog.full_path(), check: true).stdout()
    message('Detected Swift library directory: ' + swift_lib_dir)

    # linker flags
    swift_link_flags = ['-L' + swift_lib_dir, '-Xlinker', '-rpath',
                        '-Xlinker', swift_lib_dir, '-rdynamic', '-Xlinker',
                        '-add_ast_path', '-Xlinker', module]
    if swift_ver.version_compare('>=5.0')
        swift_link_flags += ['-Xlinker', '-rpath', '-Xlinker',
                            '/usr/lib/swift', '-L/usr/lib/swift']
    endif
    add_project_link_arguments(swift_link_flags, language: ['c', 'objc'])
endif
