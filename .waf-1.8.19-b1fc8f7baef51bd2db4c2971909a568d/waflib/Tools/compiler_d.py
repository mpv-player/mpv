#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import re
from waflib import Utils,Logs
d_compiler={'default':['gdc','dmd','ldc2']}
def default_compilers():
	build_platform=Utils.unversioned_sys_platform()
	possible_compiler_list=d_compiler.get(build_platform,d_compiler['default'])
	return' '.join(possible_compiler_list)
def configure(conf):
	try:test_for_compiler=conf.options.check_d_compiler or default_compilers()
	except AttributeError:conf.fatal("Add options(opt): opt.load('compiler_d')")
	for compiler in re.split('[ ,]+',test_for_compiler):
		conf.env.stash()
		conf.start_msg('Checking for %r (D compiler)'%compiler)
		try:
			conf.load(compiler)
		except conf.errors.ConfigurationError ,e:
			conf.env.revert()
			conf.end_msg(False)
			Logs.debug('compiler_d: %r'%e)
		else:
			if conf.env.D:
				conf.end_msg(conf.env.get_flat('D'))
				conf.env['COMPILER_D']=compiler
				break
			conf.end_msg(False)
	else:
		conf.fatal('could not configure a D compiler!')
def options(opt):
	test_for_compiler=default_compilers()
	d_compiler_opts=opt.add_option_group('Configuration options')
	d_compiler_opts.add_option('--check-d-compiler',default=None,help='list of D compilers to try [%s]'%test_for_compiler,dest='check_d_compiler')
	for x in test_for_compiler.split():
		opt.load('%s'%x)
