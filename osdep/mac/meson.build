# custom swift targets
bridge = join_paths(source_root, 'osdep/mac/app_bridge_objc.h')
module = join_paths(build_root, 'osdep/mac/swift.swiftmodule')

swift_flags = ['-c', '-emit-library', '-static', '-sdk', macos_sdk_path,
               '-emit-objc-header', '-parse-as-library']

if swift_ver.version_compare('>=6.0')
    swift_flags += ['-swift-version', '5']
endif

if get_option('debug')
    swift_flags += '-g'
endif

if get_option('optimization') != '0'
    swift_flags += '-O'
endif

foreach feature, allowed: features
    if allowed
        swift_flags += ['-D', 'HAVE_@0@'.format(feature.underscorify().to_upper())]
    endif
endforeach

extra_flags = get_option('swift-flags').split()
swift_flags += extra_flags

swift_compile = [swift_prog, swift_flags, '-module-name', 'swift',
                 '-emit-module-path', '@OUTPUT0@', '-import-objc-header', bridge,
                 '-emit-objc-header-path', '@OUTPUT1@', '-o', '@OUTPUT2@',
                 '@INPUT@', '-I.', '-I' + source_root, '-I' + source_root / 'include',
                 '-I' + libplacebo.get_variable('includedir',
                            default_value: source_root / 'subprojects' / 'libplacebo' / 'src' / 'include'),
                 '-I' + libavutil.get_variable('includedir')]

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
