#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import re
from waflib import Utils,Logs
from waflib.Tools import fc
fc_compiler={'win32':['gfortran','ifort'],'darwin':['gfortran','g95','ifort'],'linux':['gfortran','g95','ifort'],'java':['gfortran','g95','ifort'],'default':['gfortran'],'aix':['gfortran']}
def default_compilers():
	build_platform=Utils.unversioned_sys_platform()
	possible_compiler_list=fc_compiler.get(build_platform,fc_compiler['default'])
	return' '.join(possible_compiler_list)
def configure(conf):
	try:test_for_compiler=conf.options.check_fortran_compiler or default_compilers()
	except AttributeError:conf.fatal("Add options(opt): opt.load('compiler_fc')")
	for compiler in re.split('[ ,]+',test_for_compiler):
		conf.env.stash()
		conf.start_msg('Checking for %r (Fortran compiler)'%compiler)
		try:
			conf.load(compiler)
		except conf.errors.ConfigurationError ,e:
			conf.env.revert()
			conf.end_msg(False)
			Logs.debug('compiler_fortran: %r'%e)
		else:
			if conf.env['FC']:
				conf.end_msg(conf.env.get_flat('FC'))
				conf.env.COMPILER_FORTRAN=compiler
				break
			conf.end_msg(False)
	else:
		conf.fatal('could not configure a Fortran compiler!')
def options(opt):
	test_for_compiler=default_compilers()
	opt.load_special_tools('fc_*.py')
	fortran_compiler_opts=opt.add_option_group('Configuration options')
	fortran_compiler_opts.add_option('--check-fortran-compiler',default=None,help='list of Fortran compiler to try [%s]'%test_for_compiler,dest="check_fortran_compiler")
	for x in test_for_compiler.split():
		opt.load('%s'%x)
