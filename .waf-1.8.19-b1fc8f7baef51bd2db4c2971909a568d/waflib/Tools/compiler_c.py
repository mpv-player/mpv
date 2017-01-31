#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import re
from waflib.Tools import ccroot
from waflib import Utils
from waflib.Logs import debug
c_compiler={'win32':['msvc','gcc','clang'],'cygwin':['gcc'],'darwin':['clang','gcc'],'aix':['xlc','gcc','clang'],'linux':['gcc','clang','icc'],'sunos':['suncc','gcc'],'irix':['gcc','irixcc'],'hpux':['gcc'],'osf1V':['gcc'],'gnu':['gcc','clang'],'java':['gcc','msvc','clang','icc'],'default':['gcc','clang'],}
def default_compilers():
	build_platform=Utils.unversioned_sys_platform()
	possible_compiler_list=c_compiler.get(build_platform,c_compiler['default'])
	return' '.join(possible_compiler_list)
def configure(conf):
	try:test_for_compiler=conf.options.check_c_compiler or default_compilers()
	except AttributeError:conf.fatal("Add options(opt): opt.load('compiler_c')")
	for compiler in re.split('[ ,]+',test_for_compiler):
		conf.env.stash()
		conf.start_msg('Checking for %r (C compiler)'%compiler)
		try:
			conf.load(compiler)
		except conf.errors.ConfigurationError ,e:
			conf.env.revert()
			conf.end_msg(False)
			debug('compiler_c: %r'%e)
		else:
			if conf.env['CC']:
				conf.end_msg(conf.env.get_flat('CC'))
				conf.env['COMPILER_CC']=compiler
				break
			conf.end_msg(False)
	else:
		conf.fatal('could not configure a C compiler!')
def options(opt):
	test_for_compiler=default_compilers()
	opt.load_special_tools('c_*.py',ban=['c_dumbpreproc.py'])
	cc_compiler_opts=opt.add_option_group('Configuration options')
	cc_compiler_opts.add_option('--check-c-compiler',default=None,help='list of C compilers to try [%s]'%test_for_compiler,dest="check_c_compiler")
	for x in test_for_compiler.split():
		opt.load('%s'%x)
