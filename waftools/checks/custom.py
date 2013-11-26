from waftools.checks.generic import *

__all__ = ["check_pthreads", "check_iconv", "check_lua", "check_oss"]

pthreads_program = load_fragment('pthreads.c')

def check_pthreads(ctx, dependency_identifier):
    platform_cflags = {
        'linux':   '-D_REENTRANT',
        'freebsd': '-D_THREAD_SAFE',
        'netbsd':  '-D_THREAD_SAFE',
        'openbsd': '-D_THREAD_SAFE',
        'win32':   '-DPTW32_STATIC_LIB',
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
    checkfn = check_cc(fragment=iconv_program)
    return check_libs(libs, checkfn)(ctx, dependency_identifier)

def check_lua(ctx, dependency_identifier):
    if ctx.dependency_satisfied('libquvi4'):
        quvi_lib_storage = [ 'libquvi4' ]
        additional_lua_test_header = '#include <quvi/quvi.h>'
        additional_lua_test_code   = load_fragment('lua_libquvi4.c')
    elif ctx.dependency_satisfied('libquvi9'):
        quvi_lib_storage = [ 'libquvi9' ]
        additional_lua_test_header = '#include <quvi.h>'
        additional_lua_test_code   = load_fragment('lua_libquvi9.c')
    else:
        quvi_lib_storage = []
        additional_lua_test_header = ''
        additional_lua_test_code   = ''

    fragment = load_fragment('lua.c').format(
        additional_lua_test_header=additional_lua_test_header,
        additional_lua_test_code=additional_lua_test_code)

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
            check_cc(fragment=fragment,
                     use=[lua_version] + quvi_lib_storage,
                     execute=True))(ctx, dependency_identifier):
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
    from waflib import Utils
    cmd = ['sh', '-c', '. /etc/oss.conf && echo $OSSLIBDIR']
    p = Utils.subprocess.Popen(cmd, stdin=Utils.subprocess.PIPE,
                                    stdout=Utils.subprocess.PIPE,
                                    stderr=Utils.subprocess.PIPE)
    return p.communicate()[0].decode().rstrip()

def __get_osscflags__():
    import os
    osscflags = ''
    osslibdir = __get_osslibdir__()
    if osslibdir:
        ossincdir   = os.path.join(osslibdir, 'include')
        soundcard_h = os.path.join(ossincdir, 'sys', 'soundcard.h')
        if os.path.exists(soundcard_h):
            osscflags = '-I{0}'.format(ossincdir)
    return osscflags

def __check_oss_headers__(ctx, dependency_identifier):
    fn = check_cc(fragment=load_fragment('oss_audio_header.c'),
                  use='soundcard', cflags=__get_osscflags__())
    fn(ctx, dependency_identifier)
    return True

def __check_oss_bsd__(ctxdependency_identifier):
    # add the oss audio library through a check
    ctx.define('PATH_DEV_DSP', '/dev/sound')
    if check_cc(lib='ossaudio')(ctx, dependency_identifier):
        return True
    else:
        return __fail_oss_check__(ctx)

def check_oss(ctx, dependency_identifier):
    func = check_cc(fragment=load_fragment('oss_audio.c'), use='soundcard',
                    cflags=__get_osscflags__())
    if func(ctx, dependency_identifier):
        ctx.define('PATH_DEV_DSP',   '/dev/dsp')
        ctx.define('PATH_DEV_MIXER', '/dev/mixer')

        if ctx.env.DEST_OS in ['openbsd', 'netbsd']:
            return __check_oss_bsd_library__(ctx, dependency_identifier)
        else:
            return __check_oss_headers__(ctx, dependency_identifier)

    return __fail_oss_check__(ctx)
