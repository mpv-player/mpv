from waflib.Configure import conf

@conf
def get_config_header(self, defines=True, headers=False, define_prefix=''):
    """
    Only difference is it outputs `#define VAR 0` or `#define VAR value`
    instead of `#undef VAR` or `#define VAR val`.
    """
    from waflib.Tools.c_config import DEFKEYS, INCKEYS
    lst = []
    if headers:
        for x in self.env[INCKEYS]:
            lst.append('#include <%s>' % x)

    if defines:
        for x in self.env[DEFKEYS]:
            val = self.is_defined(x) and self.get_define(x) or "0"
            lst.append('#define %s %s' % (define_prefix + x, val))

    return "\n".join(lst)

from waflib import TaskGen

@TaskGen.extension('.m')
def m_hook(self, node):
    """
    Makes waf call the c compiler for objective-c files
    """
    return self.create_compiled_task('c', node)

def build(ctx):
    from waflib import Task

    cls = Task.classes['cprogram']
    class cprogram(cls):
        try:
            run_str = cls.orig_run_str + '${LAST_LINKFLAGS}'
        except AttributeError:
            try:
                run_str = cls.hcode + '${LAST_LINKFLAGS}'
            except TypeError:
                run_str = cls.hcode.decode('iso8859-1') + '${LAST_LINKFLAGS}'

    cls = Task.classes['macplist']
    class macplist(cls):
        def run(self):
            from waflib import Utils
            if getattr(self, 'code', None):
                txt = self.code
            else:
                txt = self.inputs[0].read()
            txt = Utils.subst_vars(txt, self.env)
            self.outputs[0].write(txt)
