from waflib import Utils

def __run(cmd):
    try:
        output = Utils.subprocess.check_output(cmd, universal_newlines=True)
        return output.strip()
    except Exception:
        return ""

def __add_swift_flags(ctx):
    ctx.env.SWIFT_FLAGS = ('-frontend -c -sdk %s -enable-objc-interop'
                           ' -emit-objc-header -parse-as-library'
                           ' -target x86_64-apple-macosx10.10') % (ctx.env.MACOS_SDK)
    ctx.env.SWIFT_VERSION = __run([ctx.env.SWIFT, '-version']).split(' ')[3]
    major, minor = [int(n) for n in ctx.env.SWIFT_VERSION.split('.')[:2]]

    # the -swift-version parameter is only supported on swift 3.1 and newer
    if major >= 3 and minor >= 1 or major >= 4:
        ctx.env.SWIFT_FLAGS += ' -swift-version 3'

    if ctx.is_optimization():
        ctx.env.SWIFT_FLAGS += ' -O'

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
        __find_swift_compiler(ctx)
