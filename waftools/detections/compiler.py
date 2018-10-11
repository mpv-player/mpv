from waflib import Utils

def __get_cc_env_vars__(cc):
    cmd = cc + ['-dM', '-E', '-']
    try:
        p = Utils.subprocess.Popen(cmd, stdin=Utils.subprocess.PIPE,
                                        stdout=Utils.subprocess.PIPE,
                                        stderr=Utils.subprocess.PIPE)
        p.stdin.write('\n'.encode())
        return p.communicate()[0]
    except Exception:
        return ""

def __test_and_add_flags__(ctx, flags):
    for flag in flags:
        ctx.check_cc(cflags=flag, uselib_store="compiler", mandatory=False)
    ctx.env.CFLAGS += ctx.env.CFLAGS_compiler

def __add_generic_flags__(ctx):
    ctx.env.CFLAGS += ["-D_ISOC99_SOURCE", "-D_GNU_SOURCE",
                       "-D_LARGEFILE_SOURCE", "-D_FILE_OFFSET_BITS=64",
                       "-D_LARGEFILE64_SOURCE",
                       "-Wall"]

    if ctx.check_cc(cflags="-std=c11", mandatory=False):
        ctx.env.CFLAGS += ["-std=c11"]
    else:
        ctx.env.CFLAGS += ["-std=c99"]

    if ctx.is_optimization():
        ctx.env.CFLAGS += ['-O2']

    if ctx.is_debug_build():
        ctx.env.CFLAGS += ['-g']

    __test_and_add_flags__(ctx, ["-Werror=implicit-function-declaration",
                                 "-Wno-error=deprecated-declarations",
                                 "-Wno-error=unused-function",
                                 "-Wempty-body",
                                 "-Wdisabled-optimization",
                                 "-Wstrict-prototypes",
                                 "-Wno-format-zero-length",
                                 "-Werror=format-security",
                                 "-Wno-redundant-decls",
                                 "-Wvla",
                                 "-Wno-format-truncation"])

def __add_gcc_flags__(ctx):
    ctx.env.CFLAGS += ["-Wall", "-Wundef", "-Wmissing-prototypes", "-Wshadow",
                       "-Wno-switch", "-Wparentheses", "-Wpointer-arith",
                       "-Wno-pointer-sign",
                       # GCC bug 66425
                       "-Wno-unused-result"]

def __add_clang_flags__(ctx):
    ctx.env.CFLAGS += ["-Wno-logical-op-parentheses", "-fcolor-diagnostics",
                       "-Wno-tautological-compare",
                       "-Wno-tautological-constant-out-of-range-compare" ]

def __add_mswin_flags__(ctx):
    ctx.env.CFLAGS += ['-D_WIN32_WINNT=0x0602', '-DUNICODE', '-DCOBJMACROS',
                       '-DINITGUID', '-U__STRICT_ANSI__']
    ctx.env.LAST_LINKFLAGS += ['-Wl,--major-os-version=6,--minor-os-version=0',
                 '-Wl,--major-subsystem-version=6,--minor-subsystem-version=0']

def __add_mingw_flags__(ctx):
    __add_mswin_flags__(ctx)
    ctx.env.CFLAGS += ['-municode', '-D__USE_MINGW_ANSI_STDIO=1']
    ctx.env.LAST_LINKFLAGS += ['-municode', '-mwindows']

def __add_cygwin_flags__(ctx):
    __add_mswin_flags__(ctx)
    ctx.env.CFLAGS += ['-mwin32']

__compiler_map__ = {
    '__GNUC__':  __add_gcc_flags__,
    '__clang__': __add_clang_flags__,
    '__MINGW32__': __add_mingw_flags__,
    '__CYGWIN__': __add_cygwin_flags__,
}

def __apply_map__(ctx, fnmap):
    if not getattr(ctx, 'CC_ENV_VARS', None):
        ctx.CC_ENV_VARS = str(__get_cc_env_vars__(ctx.env.CC))
    for k, fn in fnmap.items():
        if ctx.CC_ENV_VARS.find(k) > 0:
            fn(ctx)

def configure(ctx):
    __add_generic_flags__(ctx)
    __apply_map__(ctx, __compiler_map__)
