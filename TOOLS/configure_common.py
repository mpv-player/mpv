import atexit
import os
import shutil
import subprocess
import sys
import tempfile

# ...the fuck?
NoneType = type(None)
function = type(lambda: 0)

programs_info = [
    # env. name     default
    ("CC",          "cc"),
    ("PKG_CONFIG",  "pkg-config"),
    ("WINDRES",     "windres"),
    ("WAYSCAN",     "wayland-scanner"),
]

install_paths_info = [
    # env/opt       default
    ("PREFIX",      "/usr/local"),
    ("BINDIR",      "$(PREFIX)/bin"),
    ("LIBDIR",      "$(PREFIX)/lib"),
    ("CONFDIR",     "$(PREFIX)/etc/$(PROJNAME)"),
    ("INCDIR",      "$(PREFIX)/include"),
    ("DATADIR",     "$(PREFIX)/share"),
    ("MANDIR",      "$(DATADIR)/man"),
    ("DOCDIR",      "$(DATADIR)/doc/$(PROJNAME)"),
    ("HTMLDIR",     "$(DOCDIR)"),
    ("ZSHDIR",      "$(DATADIR)/zsh"),
    ("CONFLOADDIR", "$(CONFDIR)"),
]

# for help output only; code grabs them manually
other_env_vars = [
    # env           # help text
    ("CFLAGS",      "User C compiler flags to append."),
    ("CPPFLAGS",    "Also treated as C compiler flags."),
    ("LDFLAGS",     "C compiler flags for link command."),
    ("TARGET",      "Prefix for default build tools (for cross compilation)"),
    ("CROSS_COMPILE", "Same as TARGET."),
]

class _G:
    help_mode = False   # set if --help is specified on the command line

    log_file = None     # opened log file

    temp_path = None    # set to a private, writable temporary directory
    build_dir = None
    root_dir = None
    out_of_tree = False

    install_paths = {}  # var name to path, see install_paths_info

    programs = {}       # key is symbolic name, like CC, value is string of
                        # executable name - only set if check_program was called

    exe_format = "elf"

    cflags = []
    ldflags = []

    config_h = ""       # new contents of config.h (written at the end)
    config_mak = ""     # new contents of config.mak (written at the end)

    sources = []

    state_stack = []

    feature_opts = {}   # keyed by option name, values are:
                        #   "yes": force enable, like --enable-<feature>
                        #   "no": force disable, like: --disable-<feature>
                        #   "auto": force auto detection, like --with-<feature>=auto
                        #   "default": default (same as option not given)

    dep_enabled = {}    # keyed by dependency identifier; value is a bool
                        # missing key means the check was not run yet


# Convert a string to a C string literal. Adds the required "".
def _c_quote_string(s):
    s = s.replace("\\", "\\\\")
    s = s.replace("\"", "\\\"")
    return "\"%s\"" % s

# Convert a string to a make variable. Escaping is annoying: sometimes, you add
# e..g arbitrary paths (=> everything escaped), but sometimes you want to keep
# make variable use like $(...) unescaped.
def _c_quote_makefile_var(s):
    s = s.replace("\\", "\\\\")
    s = s.replace("\"", "\\\"")
    s = s.replace(" ", "\ ") # probably
    return s

def die(msg):
    sys.stderr.write("Fatal error: %s\n" % msg)
    sys.stderr.write("Not updating build files.\n")
    if _G.log_file:
        _G.log_file.write("--- Stopping due to error: %s\n" % msg)
    sys.exit(1)

# To be called before any user checks are performed.
def begin():
    _G.root_dir = "."
    _G.build_dir = "build"

    for var, val in install_paths_info:
        _G.install_paths[var] = val

    for arg in sys.argv[1:]:
        if arg.startswith("-"):
            name = arg[1:]
            if name.startswith("-"):
                name = name[1:]
            opt = name.split("=", 1)
            name = opt[0]
            val = opt[1] if len(opt) > 1 else ""
            def noval():
                if val:
                    die("Option --%s does not take a value." % name)
            if name == "help":
                noval()
                _G.help_mode = True
                continue
            elif name.startswith("enable-"):
                noval()
                _G.feature_opts[name[7:]] = "yes"
                continue
            elif name.startswith("disable-"):
                noval()
                _G.feature_opts[name[8:]] = "no"
                continue
            elif name.startswith("with-"):
                if val not in ["yes", "no", "auto", "default"]:
                    die("Option --%s requires 'yes', 'no', 'auto', or 'default'."
                        % name)
                _G.feature_opts[name[5:]] = val
                continue
            uname = name.upper()
            setval = None
            if uname in _G.install_paths:
                def set_install_path(name, val):
                    _G.install_paths[name] = val
                setval = set_install_path
            elif uname == "BUILDDIR":
                def set_build_path(name, val):
                    _G.build_dir = val
                setval = set_build_path
            if not setval:
                die("Unknown option: %s" % arg)
            if not val:
                die("Option --%s requires a value." % name)
            setval(uname, val)
            continue

    if _G.help_mode:
        print("Environment variables controlling choice of build tools:")
        for name, default in programs_info:
            print("  %-30s %s" % (name, default))

        print("")
        print("Environment variables/options controlling install paths:")
        for name, default in install_paths_info:
            print("  %-30s '%s' (also --%s)" % (name, default, name.lower()))

        print("")
        print("Other environment variables:")
        for name, help in other_env_vars:
            print("  %-30s %s" % (name, help))
        print("In addition, pkg-config queries PKG_CONFIG_PATH.")
        print("")
        print("General build options:")
        print("  %-30s %s" % ("--builddir=PATH", "Build directory (default: build)"))
        print("  %-30s %s" % ("", "(Requires using 'make BUILDDIR=PATH')"))
        print("")
        print("Specific build configuration:")
        # check() invocations will print the options they understand.
        return

    _G.temp_path = tempfile.mkdtemp(prefix = "mpv-configure-")
    def _cleanup():
        shutil.rmtree(_G.temp_path)
    atexit.register(_cleanup)

    # (os.path.samefile() is "UNIX only")
    if os.path.realpath(sys.path[0]) != os.path.realpath(os.getcwd()):
        print("This looks like an out of tree build.")
        print("This doesn't actually work.")
        # Keep the build dir; this makes it less likely to accidentally trash
        # an existing dir, especially if dist-clean (wipes build dir) is used.
        # Also, this will work even if the same-directory check above was wrong.
        _G.build_dir = os.path.join(os.getcwd(), _G.build_dir)
        _G.root_dir = sys.path[0]
        _G.out_of_tree = True

    os.makedirs(_G.build_dir, exist_ok = True)
    _G.log_file = open(os.path.join(_G.build_dir, "config.log"), "w")

    _G.config_h += "// Generated by configure.\n" + \
                   "#pragma once\n\n"


# Check whether the first argument is the same type of any in the following
# arguments. This _always_ returns val, but throws an exception if type checking
# fails.
# This is not very pythonic, but I'm trying to prevent bugs, so bugger off.
def typecheck(val, *types):
    vt = type(val)
    for t in types:
        if vt == t:
            return val
    raise Exception("Value '%s' of type %s not any of %s" % (val, type(val), types))

# If val is None, return []
# If val is a list, return val.
# Otherwise, return [val]
def normalize_list_arg(val):
    if val is None:
        return []
    if type(val) == list:
        return val
    return [val]

def push_build_flags():
    _G.state_stack.append(
        (_G.cflags[:], _G.ldflags[:], _G.config_h, _G.config_mak,
         _G.programs.copy()))

def pop_build_flags_discard():
    top = _G.state_stack[-1]
    _G.state_stack = _G.state_stack[:-1]

    (_G.cflags[:], _G.ldflags[:], _G.config_h, _G.config_mak,
     _G.programs) = top

def pop_build_flags_merge():
    top = _G.state_stack[-1]
    _G.state_stack = _G.state_stack[:-1]

# Return build dir.
def get_build_dir():
    assert _G.build_dir is not None # too early?
    return _G.build_dir

# Root directory, i.e. top level source directory, or where configure/Makefile
# are located.
def get_root_dir():
    assert _G.root_dir is not None # too early?
    return _G.root_dir

# Set which type of executable format the target uses.
# Used for conventions which refuse to abstract properly.
def set_exe_format(fmt):
    assert fmt in ["elf", "pe", "macho"]
    _G.exe_format = fmt

# A check is a check, dependency, or anything else that adds source files,
# preprocessor symbols, libraries, include paths, or simply serves as
# dependency check for other checks.
# Always call this function with named arguments.
# Arguments:
#   name: String or None. Symbolic name of the check. The name can be used as
#         dependency identifier by other checks. This is the first argument, and
#         usually passed directly, instead of as named argument.
#         If this starts with a "-" flag, options with names derived from this
#         are generated:
#           --enable-$option
#           --disable-$option
#           --with-$option=<yes|no|auto|default>
#         Where "$option" is the name without flag characters, and occurrences
#         of "_" are replaced with "-".
#         If this ends with a "*" flag, the result of this check is emitted as
#         preprocessor symbol to config.h. It will have the name "HAVE_$DEF",
#         and will be either set to 0 (check failed) or 1 (check succeeded),
#         and $DEF is the name without flag characters and all uppercase.
#   desc: String or None. If specified, "Checking for <desc>..." is printed
#         while running configure. If not specified, desc is auto-generated from
#         the name.
#   default: Boolean or None. If True or None, the check is soft-enabled (that
#            means it can still be disabled by options, dependency checks, or
#            the check function). If False, the check is disabled by default,
#            but can be enabled by an option.
#   deps, deps_any, deps_neg: String, array of strings, or None. If a check is
#       enabled by default/command line options, these checks are performed in
#       the following order: deps_neg, deps_any, deps
#       deps requires all dependencies in the list to be enabled.
#       deps_any requires 1 or more dependencies to be enabled.
#       deps_neg requires that all dependencies are disabled.
#   fn: Function or None. The function is run after dependency checks. If it
#       returns True, the check is enabled, if it's False, it will be disabled.
#       Typically, your function for example check for the existence of
#       libraries, and add them to the final list of CFLAGS/LDFLAGS.
#       None behaves like "lambda: True".
#       Note that this needs to be a function. If not, it'd be run before the
#       check() function is even called. That would mean the function runs even
#       if the check was disabled, and could add unneeded things to CFLAGS.
#       If this function returns False, all added build flags are removed again,
#       which makes it easy to compose checks.
#       You're not supposed to call check() itself from fn.
#   sources: String, Array of Strings, or None.
#            If the check is enabled, add these sources to the build.
#            Duplicate sources are removed at end of configuration.
#   required: String or None. If this is a string, the check is required, and
#             if it's not enabled, the string is printed as error message.
def check(name = None, option = None, desc = None, deps = None, deps_any = None,
          deps_neg = None, sources = None, fn = None, required = None,
          default = None):

    deps = normalize_list_arg(deps)
    deps_any = normalize_list_arg(deps_any)
    deps_neg = normalize_list_arg(deps_neg)
    sources = normalize_list_arg(sources)

    typecheck(name, str, NoneType)
    typecheck(option, str, NoneType)
    typecheck(desc, str, NoneType)
    typecheck(deps, NoneType, list)
    typecheck(deps_any, NoneType, list)
    typecheck(deps_neg, NoneType, list)
    typecheck(sources, NoneType, list)
    typecheck(fn, NoneType, function)
    typecheck(required, str, NoneType)
    typecheck(default, bool, NoneType)

    option_name = None
    define_name = None
    if name is not None:
        opt_flag = name.startswith("-")
        if opt_flag:
            name = name[1:]
        def_flag = name.endswith("*")
        if def_flag:
            name = name[:-1]
        if opt_flag:
            option_name = name.replace("_", "-")
        if def_flag:
            define_name = "HAVE_" + name.replace("-", "_").upper()

    if desc is None and name is not None:
        desc = name

    if _G.help_mode:
        if not option_name:
            return

        defaction = "enable"
        if required is not None:
            # If they are required, but also have option set, these are just
            # "strongly required" options.
            defaction = "enable"
        elif default == False:
            defaction = "disable"
        elif deps or deps_any or deps_neg or fn:
            defaction = "autodetect"
        act = "enable" if defaction == "disable" else "disable"
        opt = "--%s-%s" % (act, option_name)
        print("  %-30s %s %s [%s]" % (opt, act, desc, defaction))
        return

    _G.log_file.write("\n--- Test: %s\n" % (name if name else "(unnnamed)"))

    if desc:
        sys.stdout.write("Checking for %s... " % desc)
    outcome = "yes"

    force_opt = required is not None
    use_dep = True if default is None else default

    # Option handling.
    if option_name:
        # (The option gets removed, so we can determine whether all options were
        # applied in the end.)
        val = _G.feature_opts.pop(option_name, "default")
        if val == "yes":
            use_dep = True
            force_opt = True
        elif val == "no":
            use_dep = False
            force_opt = False
        elif val == "auto":
            use_dep = True
        elif val == "default":
            pass
        else:
            assert False

    if not use_dep:
        outcome = "disabled"

    # Dependency resolution.
    # But first, check whether all dependency identifiers really exist.
    for d in deps_neg + deps_any + deps:
        dep_enabled(d) # discard result
    if use_dep:
        for d in deps_neg:
            if dep_enabled(d):
                use_dep = False
                outcome = "conflicts with %s" % d
                break
    if use_dep:
        any_found = False
        for d in deps_any:
            if dep_enabled(d):
                any_found = True
                break
        if len(deps_any) > 0 and not any_found:
            use_dep = False
            outcome = "not any of %s found" % (", ".join(deps_any))
    if use_dep:
        for d in deps:
            if not dep_enabled(d):
                use_dep = False
                outcome = "%s not found" % d
                break

    # Running actual checks.
    if use_dep and fn:
        push_build_flags()
        if fn():
            pop_build_flags_merge()
        else:
            pop_build_flags_discard()
            use_dep = False
            outcome = "no"

    # Outcome reporting and terminating if dependency not found.
    if name:
        _G.dep_enabled[name] = use_dep
    if define_name:
        add_config_h_define(define_name, 1 if use_dep else 0)
    if use_dep:
        _G.sources += sources
    if desc:
        sys.stdout.write("%s\n" % outcome)
    _G.log_file.write("--- Outcome: %s (%s=%d)\n" %
                      (outcome, name if name else "(unnnamed)", use_dep))

    if required is not None and not use_dep:
        print("Warning: %s" % required)

    if force_opt and not use_dep:
        die("This feature is required.")


# Runs the process like with execv() (just that args[0] is used for both command
# and first arg. passed to the process).
# Returns the process stdout output on success, or None on non-0 exit status.
# In particular, this logs the command and its output/exit status to the log
# file.
def _run_process(args):
    p = subprocess.Popen(args, stdout = subprocess.PIPE,
                         stderr = subprocess.PIPE,
                         stdin = -1)
    (p_out, p_err) = p.communicate()
    # We don't really want this. But Python 3 in particular makes it too much of
    # a PITA (think power drill in anus) to consistently use byte strings, so
    # we need to use "unicode" strings. Yes, a bad program could just blow up
    # our butt here by outputting invalid UTF-8.
    # Weakly support Python 2 too (gcc outputs UTF-8, which crashes Python 2).
    if type(b"") != str:
        p_out = p_out.decode("utf-8")
        p_err = p_err.decode("utf-8")
    status = p.wait()
    _G.log_file.write("--- Command: %s\n" % " ".join(args))
    if p_out:
        _G.log_file.write("--- stdout:\n%s" % p_out)
    if p_err:
        _G.log_file.write("--- stderr:\n%s" % p_err)
    _G.log_file.write("--- Exit status: %s\n" % status)
    return p_out if status == 0 else None

# Run the C compiler, possibly including linking. Return whether the compiler
# exited with success status (0 exit code) as boolean. What exactly it does
# depends on the arguments. Generally, it constructs a source file and tries
# to compile it. With no arguments, it compiles, but doesn't link, a source
# file that contains a dummy main function.
# Note: these tests are cumulative.
# Arguments:
#   include: String, array of strings, or None. For each string
#            "#include <$value>" is added to the top of the source file.
#   decl: String, array of strings, or None. Added to the top of the source
#         file, global scope, separated by newlines.
#   expr: String or None. Added to the body of the main function. Despite the
#         name, needs to be a full statement, needs to end with ";".
#   defined: String or None. Adds code that fails if "#ifdef $value" fails.
#   flags: String, array of strings, or None. Each string is added to the
#          compiler command line.
#          Also, if the test succeeds, all arguments are added to the CFLAGS
#          (if language==c) written to config.mak.
#   link: String, array of strings, or None. Each string is added to the
#         compiler command line, and the compiler is made to link (not passing
#         "-c").
#         A value of [] triggers linking without further libraries.
#         A value of None disables the linking step.
#         Also, if the test succeeds, all link strings are added to the LDFLAGS
#         written to config.mak.
#   language: "c" for C, "m" for Objective-C.
def check_cc(include = None, decl = None, expr = None, defined = None,
             flags = None, link = None, language = "c"):
    assert language in ["c", "m"]

    use_linking = link is not None

    contents = ""
    for inc in normalize_list_arg(include):
        contents += "#include <%s>\n" % inc
    for dec in normalize_list_arg(decl):
        contents += "%s\n" % dec
    for define in normalize_list_arg(defined):
        contents += ("#ifndef %s\n" % define) + \
                    "#error failed\n" + \
                    "#endif\n"
    if expr or use_linking:
        contents += "int main(int argc, char **argv) {\n";
        if expr:
            contents += expr + "\n"
        contents += "return 0; }\n"
    source = os.path.join(_G.temp_path, "test." + language)
    _G.log_file.write("--- Test file %s:\n%s" % (source, contents))
    with open(source, "w") as f:
        f.write(contents)

    flags = normalize_list_arg(flags)
    link = normalize_list_arg(link)

    outfile = os.path.join(_G.temp_path, "test")
    args = [get_program("CC"), source]
    args += _G.cflags + flags
    if use_linking:
        args += _G.ldflags + link
        args += ["-o%s" % outfile]
    else:
        args += ["-c", "-o%s.o" % outfile]
    if _run_process(args) is None:
        return False

    _G.cflags += flags
    _G.ldflags += link
    return True

# Run pkg-config with function arguments passed as command arguments. Typically,
# you specify pkg-config version expressions, like "libass >= 0.14". Returns
# success as boolean.
# If this succeeds, the --cflags and --libs are added to CFLAGS and LDFLAGS.
def check_pkg_config(*args):
    args = list(args)
    pkg_config_cmd = [get_program("PKG_CONFIG")]

    cflags = _run_process(pkg_config_cmd + ["--cflags"] + args)
    if cflags is None:
        return False
    ldflags = _run_process(pkg_config_cmd + ["--libs"] + args)
    if ldflags is None:
        return False

    _G.cflags += cflags.split()
    _G.ldflags += ldflags.split()
    return True

def get_pkg_config_variable(arg, varname):
    typecheck(arg, str)
    pkg_config_cmd = [get_program("PKG_CONFIG")]

    res = _run_process(pkg_config_cmd + ["--variable=" + varname] + [arg])
    if res is not None:
        res = res.strip()
    return res

# Check for a specific build tool. You pass in a symbolic name (e.g. "CC"),
# which is then resolved to a full name and added as variable to config.mak.
# The function returns a bool for success. You're not supposed to use the
# program from configure; instead you're supposed to have rules in the makefile
# using the generated variables.
# (Some configure checks use the program directly anyway with get_program().)
def check_program(env_name):
    for name, default in programs_info:
        if name == env_name:
            val = os.environ.get(env_name, None)
            if val is None:
                prefix = os.environ.get("TARGET", None)
                if prefix is None:
                    prefix = os.environ.get("CROSS_COMPILE", "")
                # Shitty hack: default to gcc if a prefix is given, as binutils
                # toolchains generally provide only a -gcc wrapper.
                if prefix and default == "cc":
                    default = "gcc"
                val = prefix + default
            # Interleave with output. Sort of unkosher, but dare to stop me.
            sys.stdout.write("(%s) " % val)
            _G.log_file.write("--- Trying '%s' for '%s'...\n" % (val, env_name))
            try:
                _run_process([val])
            except OSError as err:
                _G.log_file.write("%s\n" % err)
                return False
            _G.programs[env_name] = val
            add_config_mak_var(env_name, val)
            return True
    assert False, "Unknown program name '%s'" % env_name

# Get the resolved value for a program. Explodes in your face if there wasn't
# a successful and merged check_program() call before.
def get_program(env_name):
    val = _G.programs.get(env_name, None)
    assert val is not None, "Called get_program(%s) without successful check." % env_name
    return val

# Return whether all passed dependency identifiers are fulfilled.
def dep_enabled(*deps):
    for d in deps:
        val = _G.dep_enabled.get(d, None)
        assert val is not None, "Internal error: unknown dependency %s" % d
        if not val:
            return False
    return True

# Add all of the passed strings to CFLAGS.
def add_cflags(*fl):
    _G.cflags += list(fl)

# Add a preprocessor symbol of the given name to config.h.
# If val is a string, it's quoted as string literal.
# If val is None, it's defined without value.
def add_config_h_define(name, val):
    if type(val) == type("") or type(val) == type(b""):
        val = _c_quote_string(val)
    if val is None:
        val = ""
    _G.config_h += "#define %s %s\n" % (name, val)

# Add a makefile variable of the given name to config.mak.
# If val is a string, it's quoted as string literal.
def add_config_mak_var(name, val):
    if type(val) == type("") or type(val) == type(b""):
        val = _c_quote_makefile_var(val)
    _G.config_mak += "%s = %s\n" % (name, val)

# Add these source files to the build.
def add_sources(*sources):
    _G.sources += list(sources)

# Get an environment variable and parse it as flags array.
def _get_env_flags(name):
    res = os.environ.get(name, "").split()
    if len(res) == 1 and len(res[0]) == 0:
        res = []
    return res

# To be called at the end of user checks.
def finish():
    if not is_running():
        return

    is_fatal = False
    for key, val in _G.feature_opts.items():
        print("Unknown feature set on command line: %s" % key)
        if val == "yes":
            is_fatal = True
    if is_fatal:
        die("Unknown feature was force-enabled.")

    _G.config_h += "\n"
    add_config_h_define("CONFIGURATION", " ".join(sys.argv))
    add_config_h_define("MPV_CONFDIR", "$(CONFLOADDIR)")
    enabled_features = [x[0] for x in filter(lambda x: x[1], _G.dep_enabled.items())]
    add_config_h_define("FULLCONFIG", " ".join(sorted(enabled_features)))

    with open(os.path.join(_G.build_dir, "config.h"), "w") as f:
        f.write(_G.config_h)

    add_config_mak_var("BUILD", _G.build_dir)
    add_config_mak_var("ROOT", _G.root_dir)
    _G.config_mak += "\n"

    add_config_mak_var("EXESUF", ".exe" if _G.exe_format == "pe" else "")

    for name, _ in install_paths_info:
        add_config_mak_var(name, _G.install_paths[name])
    _G.config_mak += "\n"

    _G.config_mak += "CFLAGS = %s %s %s\n" % (" ".join(_G.cflags),
                                              os.environ.get("CPPFLAGS", ""),
                                              os.environ.get("CFLAGS", ""))
    _G.config_mak += "\n"
    _G.config_mak += "LDFLAGS = %s %s\n" % (" ".join(_G.ldflags),
                                            os.environ.get("LDFLAGS", ""))
    _G.config_mak += "\n"

    sources = []
    for s in _G.sources:
        # Prefix all source files with "$(ROOT)/". This is important for out of
        # tree builds, where configure/make is run from "somewhere else", and
        # not the source directory.
        # Generated sources need to be prefixed with "$(BUILD)/" (for the same
        # reason). Since we do not know whether a source file is generated, the
        # convention is that the user takes care of prefixing it.
        if not s.startswith("$(BUILD)"):
            assert not s.startswith("$") # no other variables which make sense
            assert not s.startswith("generated/") # requires $(BUILD) prefix
            s = "$(ROOT)/%s" % s
        sources.append(s)

    _G.config_mak += "SOURCES = \\\n"
    for s in sorted(list(set(sources))):
        _G.config_mak += "   %s \\\n" % s

    _G.config_mak += "\n"

    with open(os.path.join(_G.build_dir, "config.mak"), "w") as f:
        f.write("# Generated by configure.\n\n" + _G.config_mak)

    if _G.out_of_tree:
        try:
            os.symlink(os.path.join(_G.root_dir, "Makefile.new"), "Makefile")
        except FileExistsError:
            print("Not overwriting existing Makefile.")

    _G.log_file.write("--- Finishing successfully.\n")
    print("Done. You can run 'make' now.")

# Return whether to actually run configure tests, and whether results of those
# tests are available.
def is_running():
    return not _G.help_mode

# Each argument is an array or tuple, with the first element giving the
# dependency identifier, or "_" to match always fulfilled. The elements after
# this are added as source files if the dependency matches. This stops after
# the first matching argument.
def pick_first_matching_dep(*deps):
    winner = None
    for e in  deps:
        if (e[0] == "_" or dep_enabled(e[0])) and (winner is None):
            # (the odd indirection though winner is so that all dependency
            #  identifiers are checked for existence)
            winner = e[1:]
    if winner is not None:
        add_sources(*winner)
