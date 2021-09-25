from waftools import inflector
from waftools.checks.generic import *
from waflib import Utils
from distutils.version import StrictVersion
import os

__all__ = ["check_pthreads", "check_iconv", "check_lua",
           "check_cocoa", "check_wl_protocols", "check_swift",
           "check_egl_provider"]

pthreads_program = load_fragment('pthreads.c')

def check_pthread_flag(ctx, dependency_identifier):
    checks = [
        check_cc(fragment = pthreads_program, cflags = '-pthread'),
        check_cc(fragment = pthreads_program, cflags = '-pthread',
                                            linkflags = '-pthread') ]

    for fn in checks:
        if fn(ctx, dependency_identifier):
            return True
    return False

def check_pthreads(ctx, dependency_identifier):
    if ctx.dependency_satisfied('win32-internal-pthreads'):
        h = ctx.path.find_node('osdep/win32/include').abspath()
        # define IN_WINPTHREAD to workaround mingw stupidity (we never want it
        # to define features specific to its own pthread stuff)
        ctx.env.CFLAGS += ['-isystem', h, '-I', h, '-DIN_WINPTHREAD']
        return True
    if check_pthread_flag(ctx, dependency_identifier):
        return True
    platform_cflags = {
        'linux':   '-D_REENTRANT',
        'freebsd': '-D_THREAD_SAFE',
        'netbsd':  '-D_THREAD_SAFE',
        'openbsd': '-D_THREAD_SAFE',
    }.get(ctx.env.DEST_OS, '')
    libs    = ['pthreadGC2', 'pthread']
    checkfn = check_cc(fragment=pthreads_program, cflags=platform_cflags)
    checkfn_nocflags = check_cc(fragment=pthreads_program)
    for fn in [checkfn, checkfn_nocflags]:
        if check_libs(libs, fn)(ctx, dependency_identifier):
            return True
    return False

def check_iconv(ctx, dependency_identifier):
    iconv_program = load_fragment('iconv.c')
    libdliconv = " ".join(ctx.env.LIB_LIBDL + ['iconv'])
    libs       = ['iconv', libdliconv]
    args       = {'fragment': iconv_program}
    if ctx.env.DEST_OS == 'openbsd' or ctx.env.DEST_OS == 'freebsd':
        args['cflags'] = '-I/usr/local/include'
        args['linkflags'] = '-L/usr/local/lib'
    elif ctx.env.DEST_OS == 'win32':
        args['linkflags'] = " ".join(['-L' + x for x in ctx.env.LIBRARY_PATH])
    checkfn = check_cc(**args)
    return check_libs(libs, checkfn)(ctx, dependency_identifier)

def check_lua(ctx, dependency_identifier):
    # mainline lua 5.1/5.2 doesn't have a .pc file, so each distro chooses
    # a different name, either non-versioned (lua.pc) or lua5x/lua5.x/lua-5.x
    # and we need to check them all. luadef* are the non-versioned .pc files,
    # and the rest represent the .pc file exactly e.g. --lua=lua-5.1
    # The non lua* names are legacy in mpv configure, and kept for compat.
    lua_versions = [
        ( 'luadef52','lua >= 5.2.0 lua < 5.3.0' ), # package "lua"
        ( '52',     'lua >= 5.2.0 lua < 5.3.0' ),
        ( 'lua52',  'lua52 >= 5.2.0'),
        ( '52arch', 'lua52 >= 5.2.0'), # Arch
        ( 'lua5.2', 'lua5.2 >= 5.2.0'),
        ( '52deb',  'lua5.2 >= 5.2.0'), # debian
        ( 'lua-5.2','lua-5.2 >= 5.2.0'),
        ( '52fbsd', 'lua-5.2 >= 5.2.0'), # FreeBSD
        ( 'luajit', 'luajit >= 2.0.0' ),
        ( 'luadef51','lua >= 5.1.0 lua < 5.2.0'), # package "lua"
        ( '51',     'lua >= 5.1.0 lua < 5.2.0'),
        ( 'lua51',  'lua51 >= 5.1.0'),
        ( '51obsd', 'lua51 >= 5.1.0'), # OpenBSD
        ( 'lua5.1', 'lua5.1 >= 5.1.0'),
        ( '51deb',  'lua5.1 >= 5.1.0'), # debian
        ( 'lua-5.1','lua-5.1 >= 5.1.0'),
        ( '51fbsd', 'lua-5.1 >= 5.1.0'), # FreeBSD
    ]

    if ctx.options.LUA_VER:
        lua_versions = \
            [lv for lv in lua_versions if lv[0] == ctx.options.LUA_VER]

    for lua_version, pkgconfig_query in lua_versions:
        display_version = lua_version
        lua_version = inflector.sanitize_id(lua_version)
        if check_pkg_config(pkgconfig_query, uselib_store=lua_version) \
            (ctx, dependency_identifier):
            # XXX: this is a bit of a hack, ask waf developers if I can copy
            # the uselib_store to 'lua'
            ctx.mark_satisfied(lua_version)
            ctx.add_optional_message(dependency_identifier,
                                     'version found: ' + display_version)
            return True
    return False

def check_wl_protocols(ctx, dependency_identifier):
    def fn(ctx, dependency_identifier):
        ret = check_pkg_config_datadir("wayland-protocols", ">= 1.15")
        ret = ret(ctx, dependency_identifier)
        if ret != None:
            ctx.env.WL_PROTO_DIR = ret.split()[0]
        return ret
    return fn(ctx, dependency_identifier)

def check_cocoa(ctx, dependency_identifier):
    fn = check_cc(
        fragment         = load_fragment('cocoa.m'),
        compile_filename = 'test.m',
        framework_name   = ['Cocoa', 'IOKit', 'OpenGL', 'QuartzCore'],
        includes         = [ctx.srcnode.abspath()],
        linkflags        = '-fobjc-arc')

    res = fn(ctx, dependency_identifier)
    if res and ctx.env.MACOS_SDK:
        # on macOS we explicitly need to set the SDK path, otherwise it can lead
        # to linking warnings or errors
        ctx.env.append_value('LAST_LINKFLAGS', [
            '-isysroot', ctx.env.MACOS_SDK,
            '-L/usr/lib',
            '-L/usr/local/lib'
        ])

    return res

def check_swift(version):
    def fn(ctx, dependency_identifier):
        minVer = StrictVersion(version)
        if ctx.env.SWIFT_VERSION:
            if StrictVersion(ctx.env.SWIFT_VERSION) >= minVer:
                ctx.add_optional_message(dependency_identifier,
                                         'version found: ' + str(ctx.env.SWIFT_VERSION))
                return True
        ctx.add_optional_message(dependency_identifier,
                                 "'swift >= " + str(minVer) + "' not found, found " +
                                 str(ctx.env.SWIFT_VERSION or None))
        return False
    return fn

def check_egl_provider(minVersion=None, name='egl', check=None):
    def fn(ctx, dependency_identifier, **kw):
        if not hasattr(ctx, 'egl_provider'):
            egl_provider_check = check or check_pkg_config(name)
            if egl_provider_check(ctx, dependency_identifier):
                ctx.egl_provider = name
                for ver in ['1.5', '1.4', '1.3', '1.2', '1.1', '1.0']:
                    stmt = 'int x[EGL_VERSION_{0}]'.format(ver.replace('.','_'))
                    check_stmt = check_statement(['EGL/egl.h'], stmt)
                    if check_stmt(ctx, dependency_identifier):
                        ctx.egl_provider_version = StrictVersion(ver)
                        break
                return True
            else:
                return False
        else:
            minVersionSV = minVersion and StrictVersion(minVersion)
            if not minVersionSV or ctx.egl_provider_version and \
                    ctx.egl_provider_version >= minVersionSV:
                defkey = inflector.define_key(dependency_identifier)
                ctx.define(defkey, 1)
                return True
            else:
                return False
    return fn
