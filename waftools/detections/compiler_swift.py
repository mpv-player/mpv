import re
import string
import os.path
from waflib import Utils
from distutils.version import StrictVersion


def __run(cmd):
    try:
        output = Utils.subprocess.check_output(cmd, stderr=Utils.subprocess.STDOUT, universal_newlines=True)
        return output.strip()
    except Exception:
        return ""


def __add_swift_flags(ctx):
    ctx.env.SWIFT_FLAGS = [
        "-frontend", "-c", "-sdk", ctx.env.MACOS_SDK,
        "-enable-objc-interop", "-emit-objc-header", "-parse-as-library",
    ]

    verRe = re.compile("(?i)version\s?([\d.]+)")
    ctx.env.SWIFT_VERSION = verRe.search(__run([ctx.env.SWIFT, '-version'])).group(1)

    # prevent possible breakages with future swift versions
    if StrictVersion(ctx.env.SWIFT_VERSION) >= StrictVersion("6.0"):
        ctx.env.SWIFT_FLAGS.extend(["-swift-version", "5"])

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
    ctx.env.append_value('LINKFLAGS', ['-L%s' % swift_library])

    # ABI compatibility
    if StrictVersion(ctx.env.SWIFT_VERSION) >= StrictVersion("5.0"):
        ctx.env.append_value('LINKFLAGS', [
            '-Xlinker', '-rpath', '-Xlinker', '/usr/lib/swift',
            '-L/usr/lib/swift',
        ])

    ctx.env.append_value('LINKFLAGS', [
        '-Xlinker', '-rpath', '-Xlinker', swift_library,
    ])


def __find_swift_library(ctx):
    swift_libraries = {}
    # look for set lib paths in passed environment variables
    if 'SWIFT_LIB_DYNAMIC' in ctx.environ:
        swift_libraries['SWIFT_LIB_DYNAMIC'] = ctx.environ['SWIFT_LIB_DYNAMIC']
    if 'SWIFT_LIB_STATIC' in ctx.environ:
        swift_libraries['SWIFT_LIB_STATIC'] = ctx.environ['SWIFT_LIB_STATIC']

    # search for swift libs relative to the swift compiler executable
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

    # fall back to xcode-select path
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

    # check if library paths were found
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
    if (enableStatic) and 'SWIFT_LIB_STATIC' in swift_libraries:
        __add_static_swift_library_linking_flags(ctx, swift_libraries['SWIFT_LIB_STATIC'])
    else:
        __add_dynamic_swift_library_linking_flags(ctx, swift_libraries['SWIFT_LIB_DYNAMIC'])


def __find_macos_sdk(ctx):
    ctx.start_msg('Checking for macOS SDK')
    sdk = None
    sdk_build_version = None
    sdk_version = None

    # look for set macOS SDK paths and version in passed environment variables
    if 'MACOS_SDK' in ctx.environ:
        sdk = ctx.environ['MACOS_SDK']
    if 'MACOS_SDK_VERSION' in ctx.environ:
        ctx.env.MACOS_SDK_VERSION = ctx.environ['MACOS_SDK_VERSION']

    # find macOS SDK paths and version
    if not sdk:
        sdk = __run(['xcrun', '--sdk', 'macosx', '--show-sdk-path'])
    if not ctx.env.MACOS_SDK_VERSION:
        # show-sdk-build-version: is not available on older command line tools, but return a build version (eg 17A360)
        # show-sdk-version: is always available, but on older dev tools it's only the major version
        sdk_build_version = __run(['xcrun', '--sdk', 'macosx', '--show-sdk-build-version'])
        sdk_version = __run(['xcrun', '--sdk', 'macosx', '--show-sdk-version'])

    if sdk:
        ctx.env.MACOS_SDK = sdk
        build_version = '10.10.0'

        if not ctx.env.MACOS_SDK_VERSION:
            # convert build version to a version string
            # first 2 two digits are the major version, starting with 15 which is 10.11 (offset of 4)
            # 1 char is the minor version, A => 0, B => 1 and ongoing
            # last digits are bugfix version, which are not relevant for us
            # eg 16E185 => 10.12.4, 17A360 => 10.13, 18B71 => 10.14.1
            if sdk_build_version and isinstance(sdk_build_version, str):
                verRe = re.compile("(\d+)(\D+)(\d+)")
                version_parts = verRe.search(sdk_build_version)
                major = int(version_parts.group(1)) - 4
                minor = string.ascii_lowercase.index(version_parts.group(2).lower())
                build_version = '10.' + str(major) + '.' + str(minor)
                # from 20 onwards macOS 11.0 starts
                if int(version_parts.group(1)) >= 20:
                    build_version = '11.' + str(minor)

            if not isinstance(sdk_version, str):
                sdk_version = '10.10.0'

            # pick the higher version, always pick sdk over build if newer
            if StrictVersion(build_version) > StrictVersion(sdk_version):
                ctx.env.MACOS_SDK_VERSION = build_version
            else:
                ctx.env.MACOS_SDK_VERSION = sdk_version

        ctx.end_msg(sdk + ' (version found: ' + ctx.env.MACOS_SDK_VERSION + ')')
    else:
        ctx.end_msg(False)


def __find_swift_compiler(ctx):
    ctx.start_msg('Checking for swift (Swift compiler)')
    swift = ''

    # look for set swift paths in passed environment variables
    if 'SWIFT' in ctx.environ:
        swift = ctx.environ['SWIFT']

    # find swift executable
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
