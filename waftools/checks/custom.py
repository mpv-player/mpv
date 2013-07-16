from waftools.checks.generic import *

__all__ = ["check_pthreads", "check_pthreads_w32_static", "check_iconv",
"check_lua", "check_oss"]

pthreads_program = load_fragment('pthreads.c')

def check_pthreads(ctx, dependency_identifier):
    platform_cflags = {
        'linux':   '-D_REENTRANT',
        'freebsd': '-D_THREAD_SAFE',
        'netbsd':  '-D_THREAD_SAFE',
        'openbsd': '-D_THREAD_SAFE',
    }.get(ctx.env.DEST_OS, '')
    # XXX: the configure script also checks for just 'pthread' is that
    # really needed?
    libs    = ['pthreadGC2', 'pthread']
    checkfn = check_cc(fragment=pthreads_program, cflags=platform_cflags)
    return check_libs(libs, checkfn)(ctx, dependency_identifier)

def check_pthreads_w32_static(ctx, dependency_identifier):
    platform_cflags = '-DPTW32_STATIC_LIB'
    libs    = ['pthreadGC2 lws2_32']
    checkfn = check_cc(fragment=pthreads_program, cflags=platform_cflags)
    return check_libs(libs, checkfn)(ctx, dependency_identifier)

def check_iconv(ctx, dependency_identifier):
    iconv_program = load_fragment('iconv.c')
    libdliconv = " ".join(ctx.env.LIB_LIBDL + ['iconv'])
    libs       = ['iconv', libdliconv]
    checkfn = check_cc(fragment=iconv_program)
    return check_libs(libs, checkfn)(ctx, dependency_identifier)

def check_lua(ctx, dependency_identifier):
    if 'libquvi4' in ctx.env.satisfied_deps:
        additional_lua_test_header = '#include <quvi/quvi.h>'
        additional_lua_test_code   = load_fragment('lua_libquvi4.c')
    elif 'libquvi9' in ctx.env.satisfied_deps:
        additional_lua_test_header = '#include <quvi.h>'
        additional_lua_test_code   = load_fragment('lua_libquvi9.c')
    else:
        additional_lua_test_header = ''
        additional_lua_test_code   = ''

    fragment = load_fragment('lua.c').format(
        additional_lua_test_header='',
        additional_lua_test_code='')

    lua_versions = [
        ( '51',     'lua >= 5.1.0 lua < 5.2.0'),
        ( '51deb',  'lua5.1 >= 5.1.0'), # debian
        ( 'luajit', 'luajit >= 2.0.0' ),
        # assume all our dependencies (libquvi in particular) link with 5.1
        ( '52',     'lua >= 5.2.0' ),
        ( '52deb',  'lua5.2 >= 5.2.0'), # debian
    ]

    if ctx.options.LUA_VER:
        lua_versions = \
            [lv for lv in lua_versions if lv[0] == ctx.options.LUA_VER]

    for lua_version, pkgconfig_query in lua_versions:
       if compose_checks(
            check_pkg_config(pkgconfig_query, uselib_store=lua_version),
            check_cc(fragment=fragment, use=lua_version))\
                (ctx, dependency_identifier):
            # XXX: this is a bit of a hack, ask waf developers if I can copy
            # the uselib_store to 'lua'
            ctx.mark_satisfied(lua_version)
            ctx.add_optional_message(dependency_identifier,
                                     'version found: ' + lua_version)
            return True
    return False

# from here on there is the OSS check.. just stop reading here unless you want
# to die inside a little
def __fail_oss_check__(ctx):
    ctx.define('PATH_DEV_DSP', '')
    ctx.define('PATH_DEV_MIXER', '')
    return False

def __get_osslibdir__():
    try:
        cmd = ['sh', '-c', "'source /etc/oss.conf && echo $OSSLIBDIR'"]
        p = Utils.subprocess.Popen(cmd, stdin=Utils.subprocess.PIPE,
                                        stdout=Utils.subprocess.PIPE,
                                        stderr=Utils.subprocess.PIPE)
        return p.communicate()[0]
    except Exception:
        return ""

def __check_oss_headers__(ctx, dependency_identifier):
    import os

    real_oss = ctx.check_cc(fragment=load_fragment('oss_audio_header.c'),
                            use='soundcard')

    if real_oss:
        if os.path.exists('/etc/oss.conf'):
            osslibdir   = __get_osslibdir__()
            ossincdir   = os.path.join(osslibdir, 'include')
            soundcard_h = os.path.join(ossincdir, 'sys', 'soundcard.h')
            if os.path.exists(soundcard_h):
                ctx.env.CFLAGS.append('-I{0}'.format(ossincdir))

    return True

def __check_oss_bsd__(ctxdependency_identifier):
    # add the oss audio library through a check
    ctx.define('PATH_DEV_DSP', '/dev/sound')
    if check_cc(lib='ossaudio')(ctx, dependency_identifier):
        return True
    else:
        return __fail_oss_check__(ctx)

def check_oss(ctx, dependency_identifier):
    func = check_cc(fragment=load_fragment('oss_audio.c'), use='soundcard')
    if func(ctx, dependency_identifier):
        ctx.define('PATH_DEV_DSP',   '/dev/dsp')
        ctx.define('PATH_DEV_MIXER', '/dev/mixer')

        if ctx.env.DEST_OS in ['openbsd', 'netbsd']:
            return __check_oss_bsd_library__(ctx, dependency_identifier)
        else:
            return __check_oss_headers__(ctx, dependency_identifier)

    return __fail_oss_check__(ctx)
