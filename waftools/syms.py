#! /usr/bin/env python
# encoding: utf-8
# Original source: waflib/extras/syms.py from waf git 610d0d59f (New BSD License)

"""
set the list of symbols exported from a dynamic library
to use the tool, do something like:

def build(ctx):
        ctx(features='c cshlib syms', source='a.c b.c', export_symbols_def='syms.def', target='testlib')

only the symbols listed in the file syms.def will be exported.
"""

from waflib.Context import STDOUT
from waflib.Task import Task
from waflib.Errors import WafError
from waflib.TaskGen import feature, after_method

class compile_sym(Task):
    def run(self):
        lsyms = []
        for line in self.inputs[0].read().split():
            lsyms.append(line.strip())
        lsyms.sort()
        if self.env.DEST_BINFMT == 'pe':
            self.outputs[0].write('EXPORTS\n' + '\n'.join(lsyms))
        elif self.env.DEST_BINFMT == 'elf':
            self.outputs[0].write('{ global:\n' + ';\n'.join(lsyms) + ";\nlocal: *; };\n")
        elif self.env.DEST_BINFMT == 'mac-o':
            self.outputs[0].write('\n'.join("_"+sym for sym in lsyms) + '\n')
        else:
            raise WafError('NotImplemented')

@feature('syms')
@after_method('process_source', 'process_use', 'apply_link', 'process_uselib_local')
def do_the_symbol_stuff(self):
    tsk = self.create_task('compile_sym',
    [self.path.find_node(self.export_symbols_def)],
    self.path.find_or_declare(getattr(self, 'sym_filename', self.target + '.def')))
    self.link_task.set_run_after(tsk)
    self.link_task.dep_nodes.append(tsk.outputs[0])
    if 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
        self.link_task.env.append_value('LINKFLAGS', ['/def:' + tsk.outputs[0].bldpath()])
    elif self.env.DEST_BINFMT == 'pe': #gcc on windows takes *.def as an additional input
        self.link_task.inputs.append(tsk.outputs[0])
    elif self.env.DEST_BINFMT == 'elf':
        self.link_task.env.append_value('LINKFLAGS', ['-Wl,-version-script', '-Wl,' + tsk.outputs[0].bldpath()])
    elif self.env.DEST_BINFMT == 'mac-o':
        self.link_task.env.append_value('LINKFLAGS', ['-exported_symbols_list', tsk.outputs[0].bldpath()])
    else:
        raise WafError('NotImplemented')
