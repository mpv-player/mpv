from waflib.Errors import ConfigurationError, WafError
from waflib.Configure import conf
from waflib.Build import BuildContext
from waflib.Logs import pprint
from inflectors import DependencyInflector

class DependencyError(Exception):
    pass

class Dependency(object):
    def __init__(self, ctx, known_deps, satisfied_deps, dependency):
        self.ctx = ctx
        self.known_deps = known_deps
        self.satisfied_deps = satisfied_deps
        self.identifier, self.desc = dependency['name'], dependency['desc']
        self.attributes = self.__parse_attributes__(dependency)

        known_deps.add(self.identifier)
        for dep_key in ['deps', 'deps_any', 'deps_neg']:
            if dep_key in self.attributes:
                deps = self.attributes[dep_key]
                self.ctx.ensure_dependency_is_known(*deps)


    def __parse_attributes__(self, dependency):
        if 'os_specific_checks' in dependency:
            all_chks = dependency['os_specific_checks']
            chks = [check for check in all_chks if check in self.satisfied_deps]
            if any(chks):
                return all_chks[chks[0]]
        return dependency

    def check(self):
        self.ctx.start_msg('Checking for {0}'.format(self.desc))

        try:
            self.check_group_disabled()
            self.check_disabled()
            self.check_any_dependencies()
            self.check_dependencies()
            self.check_negative_dependencies()
        except DependencyError:
            # No check was run, since the prerequisites of the dependency are
            # not satisfied. Make sure the define is 'undefined' so that we
            # get a `#define YYY 0` in `config.h`.
            def_key = DependencyInflector(self.identifier).define_key()
            self.ctx.undefine(def_key)
            self.fatal_if_needed()
            return

        self.check_autodetect_func()

    def check_group_disabled(self):
        if 'groups' in self.attributes:
            groups = self.attributes['groups']
            disabled = (self.enabled_option(g) == False for g in groups)
            if any(disabled):
                self.skip()
                raise DependencyError

    def check_disabled(self):
        if self.enabled_option() == False:
            self.skip()
            raise DependencyError

        if self.enabled_option() == True:
            self.attributes['req'] = True
            self.attributes['fmsg'] = "You manually enabled the feature '{0}', but \
the autodetection check failed.".format(self.identifier)

    def check_any_dependencies(self):
        if 'deps_any' in self.attributes:
            deps = set(self.attributes['deps_any'])
            if len(deps & self.satisfied_deps) == 0:
                self.skip("not found any of {0}".format(", ".join(deps)))
                raise DependencyError

    def check_dependencies(self):
        if 'deps' in self.attributes:
            deps = set(self.attributes['deps'])
            if not deps <= self.satisfied_deps:
                missing_deps = deps - self.satisfied_deps
                self.skip("{0} not found".format(", ".join(missing_deps)))
                raise DependencyError

    def check_negative_dependencies(self):
        if 'deps_neg' in self.attributes:
            deps = set(self.attributes['deps_neg'])
            conflicting_deps = deps & self.satisfied_deps
            if len(conflicting_deps) > 0:
                self.skip("{0} found".format(", ".join(conflicting_deps)), 'CYAN')
                raise DependencyError

    def check_autodetect_func(self):
        if self.attributes['func'](self.ctx, self.identifier):
            self.success(self.identifier)
        else:
            self.fail()
            self.fatal_if_needed()

    def enabled_option(self, identifier=None):
        try:
            return getattr(self.ctx.options, self.enabled_option_repr(identifier))
        except AttributeError:
            pass
        return None

    def enabled_option_repr(self, identifier):
        return "enable_{0}".format(identifier or self.identifier)

    def success(self, depname):
        self.ctx.mark_satisfied(depname)
        self.ctx.end_msg(self.__message__('yes'))

    def fail(self, reason='no'):
        self.ctx.end_msg(self.__message__(reason), 'RED')

    def fatal_if_needed(self):
        if self.enabled_option() == False:
            return
        if self.attributes.get('req', False):
            raise ConfigurationError(self.attributes['fmsg'])

    def skip(self, reason='disabled', color='YELLOW'):
        self.ctx.end_msg(self.__message__(reason), color)

    def __message__(self, message):
        optional_message = self.ctx.deps_msg.get(self.identifier)
        if optional_message:
            return "{0} ({1})".format(message, optional_message)
        else:
            return message

def configure(ctx):
    def __detect_target_os_dependency__(ctx):
        target = "os-{0}".format(ctx.env.DEST_OS)
        ctx.start_msg('Detected target OS:')
        ctx.end_msg(target)
        ctx.known_deps.add(target)
        ctx.satisfied_deps.add(target)

    ctx.deps_msg = {}
    ctx.known_deps = set()
    ctx.satisfied_deps = set()
    __detect_target_os_dependency__(ctx)

@conf
def ensure_dependency_is_known(ctx, *depnames):
    deps = set([d for d in depnames if not d.startswith('os-')])
    if not deps <= ctx.known_deps:
        raise ConfigurationError(
            "error in dependencies definition: some dependencies in"
            " {0} are unknown.".format(deps))


@conf
def mark_satisfied(ctx, dependency_identifier):
    ctx.satisfied_deps.add(dependency_identifier)

@conf
def add_optional_message(ctx, dependency_identifier, message):
    ctx.deps_msg[dependency_identifier] = message

@conf
def parse_dependencies(ctx, dependencies):
    def __check_dependency__(ctx, dependency):
        Dependency(ctx,
                   ctx.known_deps,
                   ctx.satisfied_deps,
                   dependency).check()

    [__check_dependency__(ctx, dependency) for dependency in dependencies]

@conf
def dependency_satisfied(ctx, dependency_identifier):
    ctx.ensure_dependency_is_known(dependency_identifier)
    return dependency_identifier in ctx.satisfied_deps

@conf
def store_dependencies_lists(ctx):
    ctx.env.known_deps     = list(ctx.known_deps)
    ctx.env.satisfied_deps = list(ctx.satisfied_deps)

@conf
def unpack_dependencies_lists(ctx):
    ctx.known_deps     = set(ctx.env.known_deps)
    ctx.satisfied_deps = set(ctx.env.satisfied_deps)

def filtered_sources(ctx, sources):
    def __source_file__(source):
        if isinstance(source, tuple):
            return source[0]
        else:
            return source

    def __check_filter__(dependency):
        if dependency.find('!') == 0:
            dependency = dependency.lstrip('!')
            ctx.ensure_dependency_is_known(dependency)
            return dependency not in ctx.satisfied_deps
        else:
            ctx.ensure_dependency_is_known(dependency)
            return dependency in ctx.satisfied_deps

    def __unpack_and_check_filter__(source):
        try:
            _, dependency = source
            return __check_filter__(dependency)
        except ValueError:
            return True

    return [__source_file__(source) for source in sources \
            if __unpack_and_check_filter__(source)]

def env_fetch(tx):
    def fn(ctx):
        deps = ctx.env.satisfied_deps
        lists = [ctx.env[tx(dep)] for dep in deps if (tx(dep) in ctx.env)]
        return [item for sublist in lists for item in sublist]
    return fn

def dependencies_use(ctx):
    return [DependencyInflector(dep).storage_key() for \
                dep in ctx.env.satisfied_deps]

BuildContext.filtered_sources = filtered_sources
BuildContext.dependencies_use = dependencies_use
BuildContext.dependencies_includes  = env_fetch(lambda x: "INCLUDES_{0}".format(x))
BuildContext.dependency_satisfied = dependency_satisfied
