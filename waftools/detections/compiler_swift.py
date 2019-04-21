import re
import os.path
from waflib import Utils
from distutils.version import StrictVersion

def __run(cmd):
    try:
        output = Utils.subprocess.check_output(cmd, universal_newlines=True)
        return output.strip()
    except Exception:
        return ""

def __add_swift_flags(ctx):
    ctx.env.SWIFT_FLAGS = [
        "-frontend", "-c", "-sdk", ctx.env.MACOS_SDK,
        "-enable-objc-interop", "-emit-objc-header", "-parse-as-library",
        "-target", "x86_64-apple-macosx10.10"
    ]

    verRe = re.compile("(?i)version\s?([\d.]+)")
    ctx.env.SWIFT_VERSION = verRe.search(__run([ctx.env.SWIFT, '-version'])).group(1)

    # the -swift-version parameter is only supported on swift 3.1 and newer
    if StrictVersion(ctx.env.SWIFT_VERSION) >= StrictVersion("3.1"):
        ctx.env.SWIFT_FLAGS.extend([ "-swift-version", "3" ])

    if ctx.is_debug_build():
        ctx.env.SWIFT_FLAGS.append("-g")

    if ctx.is_optimization():
        ctx.env.SWIFT_FLAGS.append("-O")

def __add_static_swift_library_linking_flags(ctx, swift_library):
    ctx.env.append_value('LINKFLAGS', [
        '-L%s' % swift_library,
        '-Xlinker', '-force_load_swift_libs', '-lc++',
    ])

def __add_dynamic_swift_library_linking_flags(ctx, swift_library):
    ctx.env.append_value('LINKFLAGS', [ '-L%s' % swift_library ])

    #ABI compatibility
    if StrictVersion(ctx.env.SWIFT_VERSION) >= StrictVersion("5.0"):
        ctx.env.append_value('LINKFLAGS', [
            '-Xlinker', '-rpath', '-Xlinker', '/usr/lib/swift',
        ])

    ctx.env.append_value('LINKFLAGS', [
        '-Xlinker', '-rpath', '-Xlinker', swift_library,
    ])

def __find_swift_library(ctx):
    swift_libraries = {}
    #look for set lib paths in passed environment variables
    if 'SWIFT_LIB_DYNAMIC' in ctx.environ:
        swift_libraries['SWIFT_LIB_DYNAMIC'] = ctx.environ['SWIFT_LIB_DYNAMIC']
    if 'SWIFT_LIB_STATIC' in ctx.environ:
        swift_libraries['SWIFT_LIB_STATIC'] = ctx.environ['SWIFT_LIB_STATIC']

    #search for swift libs relative to the swift compiler executable
    swift_library_relative_paths = {
        'SWIFT_LIB_DYNAMIC': '../../lib/swift/macosx',
        'SWIFT_LIB_STATIC': '../../lib/swift_static/macosx'
    }

    for lib_type, path in swift_library_relative_paths.items():
        if lib_type not in swift_libraries:
            lib_path = os.path.join(ctx.env.SWIFT, path)
            swift_library = ctx.root.find_dir(lib_path)
            if swift_library is not None:
                swift_libraries[lib_type] = swift_library.abspath()

    #fall back to xcode-select path
    swift_library_paths = {
        'SWIFT_LIB_DYNAMIC': [
            'Toolchains/XcodeDefault.xctoolchain/usr/lib/swift/macosx',
            'usr/lib/swift/macosx'
        ],
        'SWIFT_LIB_STATIC': [
            'Toolchains/XcodeDefault.xctoolchain/usr/lib/swift_static/macosx',
            'usr/lib/swift_static/macosx'
        ]
    }
    dev_path = __run(['xcode-select', '-p'])[1:]

    for lib_type, paths in swift_library_paths.items():
        for path in paths:
            if lib_type not in swift_libraries:
                swift_library = ctx.root.find_dir([dev_path, path])
                if swift_library is not None:
                    swift_libraries[lib_type] = swift_library.abspath()
                    break
            else:
                break

    #check if library paths were found
    ctx.start_msg('Checking for dynamic Swift Library')
    if 'SWIFT_LIB_DYNAMIC' in swift_libraries:
        ctx.end_msg(swift_libraries['SWIFT_LIB_DYNAMIC'])
    else:
        ctx.end_msg(False)

    ctx.start_msg('Checking for static Swift Library')
    if 'SWIFT_LIB_STATIC' in swift_libraries:
        ctx.end_msg(swift_libraries['SWIFT_LIB_STATIC'])
        ctx.env['SWIFT_LIB_STATIC'] = swift_libraries['SWIFT_LIB_STATIC']
    else:
        ctx.end_msg(False)

    enableStatic = getattr(ctx.options, 'enable_swift-static')
    if (enableStatic or enableStatic == None) and 'SWIFT_LIB_STATIC' in swift_libraries:
        __add_static_swift_library_linking_flags(ctx, swift_libraries['SWIFT_LIB_STATIC'])
    else:
        __add_dynamic_swift_library_linking_flags(ctx, swift_libraries['SWIFT_LIB_DYNAMIC'])

def __find_macos_sdk(ctx):
    ctx.start_msg('Checking for macOS SDK')
    sdk = __run(['xcrun', '--sdk', 'macosx', '--show-sdk-path'])
    if sdk:
        ctx.end_msg(sdk)
        ctx.env.MACOS_SDK = sdk
    else:
        ctx.end_msg(False)

def __find_swift_compiler(ctx):
    ctx.start_msg('Checking for swift (Swift compiler)')
    swift = ''

    #look for set swift paths in passed environment variables
    if 'SWIFT' in ctx.environ:
        swift = ctx.environ['SWIFT']

    #find swift executable
    if not swift:
        swift = __run(['xcrun', '-find', 'swift'])

    if swift:
        ctx.end_msg(swift)
        ctx.env.SWIFT = swift
        __add_swift_flags(ctx)
        __find_swift_library(ctx)
    else:
        ctx.end_msg(False)

def configure(ctx):
    if ctx.env.DEST_OS == "darwin":
        __find_macos_sdk(ctx)
        if ctx.options.enable_swift is not False:
            __find_swift_compiler(ctx)
