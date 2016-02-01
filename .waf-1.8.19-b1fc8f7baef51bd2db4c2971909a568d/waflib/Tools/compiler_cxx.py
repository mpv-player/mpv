#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import re
from waflib.Tools import ccroot
from waflib import Utils
from waflib.Logs import debug
cxx_compiler={'win32':['msvc','g++','clang++'],'cygwin':['g++'],'darwin':['clang++','g++'],'aix':['xlc++','g++','clang++'],'linux':['g++','clang++','icpc'],'sunos':['sunc++','g++'],'irix':['g++'],'hpux':['g++'],'osf1V':['g++'],'gnu':['g++','clang++'],'java':['g++','msvc','clang++','icpc'],'default':['g++','clang++']}
def default_compilers():
	build_platform=Utils.unversioned_sys_platform()
	possible_compiler_list=cxx_compiler.get(build_platform,cxx_compiler['default'])
	return' '.join(possible_compiler_list)
def configure(conf):
	try:test_for_compiler=conf.options.check_cxx_compiler or default_compilers()
	except AttributeError:conf.fatal("Add options(opt): opt.load('compiler_cxx')")
	for compiler in re.split('[ ,]+',test_for_compiler):
		conf.env.stash()
		conf.start_msg('Checking for %r (C++ compiler)'%compiler)
		try:
			conf.load(compiler)
		except conf.errors.ConfigurationError ,e:
			conf.env.revert()
			conf.end_msg(False)
			debug('compiler_cxx: %r'%e)
		else:
			if conf.env['CXX']:
				conf.end_msg(conf.env.get_flat('CXX'))
				conf.env['COMPILER_CXX']=compiler
				break
			conf.end_msg(False)
	else:
		conf.fatal('could not configure a C++ compiler!')
def options(opt):
	test_for_compiler=default_compilers()
	opt.load_special_tools('cxx_*.py')
	cxx_compiler_opts=opt.add_option_group('Configuration options')
	cxx_compiler_opts.add_option('--check-cxx-compiler',default=None,help='list of C++ compilers to try [%s]'%test_for_compiler,dest="check_cxx_compiler")
	for x in test_for_compiler.split():
		opt.load('%s'%x)
