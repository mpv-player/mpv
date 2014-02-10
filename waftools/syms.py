#! /usr/bin/env python
# encoding: utf-8
# Source: waflib/extras/syms.py from waf git 610d0d59f (New BSD License)

"""
this tool supports the export_symbols_regex to export the symbols in a shared library.
by default, all symbols are exported by gcc, and nothing by msvc.
to use the tool, do something like:

def build(ctx):
	ctx(features='c cshlib syms', source='a.c b.c', export_symbols_regex='mylib_.*', target='testlib')

only the symbols starting with 'mylib_' will be exported.
"""

import os
import re
from waflib.Context import STDOUT
from waflib.Task import Task
from waflib.Errors import WafError
from waflib.TaskGen import feature, after_method

class gen_sym(Task):
	def run(self):
		obj = self.inputs[0]
		kw = {}
		if 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
			re_nm = re.compile(r'External\s+\|\s+_(' + self.generator.export_symbols_regex + r')\b')
			cmd = [self.env.DUMPBIN or 'dumpbin', '/symbols', obj.abspath()]

			# Dumpbin requires custom environment sniffed out by msvc.py earlier
			if self.env['PATH']:
				env = dict(self.env.env or os.environ)
				env.update(PATH = os.pathsep.join(self.env['PATH']))
				kw['env'] = env

		else:
			if self.env.DEST_BINFMT == 'pe': #gcc uses nm, and has a preceding _ on windows
				re_nm = re.compile(r'T\s+_(' + self.generator.export_symbols_regex + r')\b')
			else:
				re_nm = re.compile(r'T\s+(' + self.generator.export_symbols_regex + r')\b')
			cmd = [self.env.NM or 'nm', '-g', obj.abspath()]
		syms = re_nm.findall(self.generator.bld.cmd_and_log(cmd, quiet=STDOUT, **kw))
		self.outputs[0].write('%r' % syms)

class compile_sym(Task):
	def run(self):
		syms = {}
		for x in self.inputs:
			slist = eval(x.read())
			for s in slist:
				syms[s] = 1
		lsyms = list(syms.keys())
		lsyms.sort()
		if self.env.DEST_BINFMT == 'pe':
			self.outputs[0].write('EXPORTS\n' + '\n'.join(lsyms))
		elif self.env.DEST_BINFMT == 'elf':
			self.outputs[0].write('{ global:\n' + ';\n'.join(lsyms) + ";\nlocal: *; };\n")
		else:
			raise WafError('NotImplemented')

@feature('syms')
@after_method('process_source', 'process_use', 'apply_link', 'process_uselib_local')
def do_the_symbol_stuff(self):
	ins = [x.outputs[0] for x in self.compiled_tasks]
	self.gen_sym_tasks = [self.create_task('gen_sym', x, x.change_ext('.%d.sym' % self.idx)) for x in ins]

	tsk = self.create_task('compile_sym',
			       [x.outputs[0] for x in self.gen_sym_tasks],
			       self.path.find_or_declare(getattr(self, 'sym_filename', self.target + '.def')))
	self.link_task.set_run_after(tsk)
	self.link_task.dep_nodes.append(tsk.outputs[0])
	if 'msvc' in (self.env.CC_NAME, self.env.CXX_NAME):
		self.link_task.env.append_value('LINKFLAGS', ['/def:' + tsk.outputs[0].bldpath()])
	elif self.env.DEST_BINFMT == 'pe': #gcc on windows takes *.def as an additional input
		self.link_task.inputs.append(tsk.outputs[0])
	elif self.env.DEST_BINFMT == 'elf':
		self.link_task.env.append_value('LINKFLAGS', ['-Wl,-version-script', '-Wl,' + tsk.outputs[0].bldpath()])
	else:
		raise WafError('NotImplemented')

