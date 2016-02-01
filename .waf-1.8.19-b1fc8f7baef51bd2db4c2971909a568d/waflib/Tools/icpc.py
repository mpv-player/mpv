#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import sys
from waflib.Tools import ccroot,ar,gxx
from waflib.Configure import conf
@conf
def find_icpc(conf):
	if sys.platform=='cygwin':
		conf.fatal('The Intel compiler does not work on Cygwin')
	cxx=conf.find_program('icpc',var='CXX')
	conf.get_cc_version(cxx,icc=True)
	conf.env.CXX_NAME='icc'
def configure(conf):
	conf.find_icpc()
	conf.find_ar()
	conf.gxx_common_flags()
	conf.gxx_modifier_platform()
	conf.cxx_load_tools()
	conf.cxx_add_flags()
	conf.link_add_flags()
