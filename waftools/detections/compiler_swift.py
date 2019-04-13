import re
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

def __add_swift_library_linking_flags(ctx, swift_library):
    ctx.env.append_value('LINKFLAGS', [
        '-L%s' % swift_library,
        '-Xlinker', '-force_load_swift_libs', '-lc++',
    ])

def __find_swift_library(ctx):
    swift_library_paths = [
        'Toolchains/XcodeDefault.xctoolchain/usr/lib/swift_static/macosx',
        'usr/lib/swift_static/macosx'
    ]
    dev_path = __run(['xcode-select', '-p'])[1:]

    ctx.start_msg('Checking for Swift Library')
    for path in swift_library_paths:
        swift_library = ctx.root.find_dir([dev_path, path])
        if swift_library is not None:
            ctx.end_msg(swift_library.abspath())
            __add_swift_library_linking_flags(ctx, swift_library.abspath())
            return
    ctx.end_msg(False)

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
