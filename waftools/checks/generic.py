import os
import inflector
from waflib.ConfigSet import ConfigSet
from waflib import Utils

__all__ = [
    "check_pkg_config", "check_pkg_config_mixed", "check_pkg_config_mixed_all",
    "check_pkg_config_cflags", "check_cc", "check_statement", "check_libs",
    "check_headers", "compose_checks", "check_true", "any_version",
    "load_fragment", "check_stub", "check_ctx_vars", "check_program",
    "check_pkg_config_datadir"]

any_version = None

def even(n):
    return n % 2 == 0

def __define_options__(dependency_identifier):
    return inflector.define_dict(dependency_identifier)

def __merge_options__(dependency_identifier, *args):
    options_accu = inflector.storage_dict(dependency_identifier)
    options_accu['mandatory'] = False
    [options_accu.update(arg) for arg in args if arg]
    return options_accu

def _filter_cc_arguments(ctx, opts):
    if ctx.env.DEST_OS != Utils.unversioned_sys_platform():
        # cross compiling, remove execute=True if present
        if opts.get('execute'):
            opts['execute'] = False
    return opts

def check_program(name, var):
    def fn(ctx, dependency_identifier):
        return ctx.find_program(name, var=var, mandatory=False)
    return fn

def check_libs(libs, function):
    libs = [None] + libs
    def fn(ctx, dependency_identifier):
        for lib in libs:
            kwargs = lib and {'lib': lib} or {}
            if function(ctx, dependency_identifier, **kwargs):
                return True
        return False
    return fn

def check_statement(header, statement, **kw_ext):
    def fn(ctx, dependency_identifier, **kw):
        headers = header
        if not isinstance(headers, list):
            headers = [header]
        hs = "\n".join(["#include <{0}>".format(h) for h in headers])
        fragment = ("{0}\n"
                    "int main(int argc, char **argv)\n"
                    "{{ {1}; return 0; }}").format(hs, statement)
        opts = __merge_options__(dependency_identifier,
                                 {'fragment':fragment},
                                 __define_options__(dependency_identifier),
                                 kw_ext, kw)
        return ctx.check_cc(**_filter_cc_arguments(ctx, opts))
    return fn

def check_cc(**kw_ext):
    def fn(ctx, dependency_identifier, **kw):
        options = __merge_options__(dependency_identifier,
                                    __define_options__(dependency_identifier),
                                    kw_ext, kw)
        return ctx.check_cc(**_filter_cc_arguments(ctx, options))
    return fn

def check_pkg_config(*args, **kw_ext):
    return _check_pkg_config([], ["--libs", "--cflags"], *args, **kw_ext)

def check_pkg_config_mixed(_dyn_libs, *args, **kw_ext):
    return _check_pkg_config([_dyn_libs], ["--libs", "--cflags"], *args, **kw_ext)

def check_pkg_config_mixed_all(*all_args, **kw_ext):
    args = [all_args[i] for i in [n for n in range(0, len(all_args)) if n % 3]]
    return _check_pkg_config(all_args[::3], ["--libs", "--cflags"], *args, **kw_ext)

def check_pkg_config_cflags(*args, **kw_ext):
    return _check_pkg_config([], ["--cflags"], *args, **kw_ext)

def check_pkg_config_datadir(*args, **kw_ext):
    return _check_pkg_config([], ["--variable=pkgdatadir"], *args, **kw_ext)

def _check_pkg_config(_dyn_libs, _pkgc_args, *args, **kw_ext):
    def fn(ctx, dependency_identifier, **kw):
        argsl     = list(args)
        packages  = args[::2]
        verchecks = args[1::2]
        sargs     = []
        pkgc_args = _pkgc_args
        dyn_libs  = {}
        for i in range(0, len(packages)):
            if i < len(verchecks):
                sargs.append(packages[i] + ' ' + verchecks[i])
            else:
                sargs.append(packages[i])
            if _dyn_libs and _dyn_libs[i]:
                dyn_libs[packages[i]] = _dyn_libs[i]
        if ctx.dependency_satisfied('static-build') and not dyn_libs:
            pkgc_args += ["--static"]

        defaults = {
            'path':    ctx.env.PKG_CONFIG,
            'package': " ".join(packages),
            'args':    sargs + pkgc_args }
        opts = __merge_options__(dependency_identifier, defaults, kw_ext, kw)

        # Warning! Megahack incoming: when parsing flags in `parse_flags` waf
        # uses append_unique. This appends the flags only if they aren't
        # already present in the list. This causes breakage if one checks for
        # multiple pkg-config packages in a single call as stuff like -lm is
        # added only at its first occurrence.
        original_append_unique  = ConfigSet.append_unique
        ConfigSet.append_unique = ConfigSet.append_value
        result = ctx.check_cfg(**opts)
        ConfigSet.append_unique = original_append_unique

        defkey = inflector.define_key(dependency_identifier)
        if result:
            ctx.define(defkey, 1)
            for x in dyn_libs.keys():
                ctx.env['LIB_'+x] += dyn_libs[x]
        else:
            ctx.add_optional_message(dependency_identifier,
                                     "'{0}' not found".format(" ".join(sargs)))
            ctx.undefine(defkey)
        return result
    return fn

def check_headers(*headers, **kw_ext):
    def undef_others(ctx, headers, found):
        not_found_hs = set(headers) - set([found])
        for not_found_h in not_found_hs:
            ctx.undefine(inflector.define_key(not_found_h))

    def fn(ctx, dependency_identifier):
        for header in headers:
            defaults = {'header_name': header, 'features': 'c cprogram'}
            options  = __merge_options__(dependency_identifier, defaults, kw_ext)
            if ctx.check(**options):
                undef_others(ctx, headers, header)
                ctx.define(inflector.define_key(dependency_identifier), 1)
                return True
        undef_others(ctx, headers, None)
        return False
    return fn

def check_true(ctx, dependency_identifier):
    ctx.define(inflector.define_key(dependency_identifier), 1)
    return True

def check_ctx_vars(*variables):
    def fn(ctx, dependency_identifier):
        missing = []
        for variable in variables:
            if variable not in ctx.env:
                missing.append(variable)

        if any(missing):
            ctx.add_optional_message(dependency_identifier,
                'missing {0}'.format(', '.join(missing)))
            return False
        else:
            return True

    return fn

def check_stub(ctx, dependency_identifier):
    ctx.undefine(inflector.define_key(dependency_identifier))
    return False

def compose_checks(*checks):
    def fn(ctx, dependency_identifier):
        return all([check(ctx, dependency_identifier) for check in checks])
    return fn

def load_fragment(fragment):
    file_path = os.path.join(os.path.dirname(__file__), '..', 'fragments',
                             fragment)
    fp = open(file_path,"r")
    fragment_code = fp.read()
    fp.close()
    return fragment_code
