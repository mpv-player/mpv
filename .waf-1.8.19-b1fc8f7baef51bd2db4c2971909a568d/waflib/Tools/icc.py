#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import sys
from waflib.Tools import ccroot,ar,gcc
from waflib.Configure import conf
@conf
def find_icc(conf):
	if sys.platform=='cygwin':
		conf.fatal('The Intel compiler does not work on Cygwin')
	cc=conf.find_program(['icc','ICL'],var='CC')
	conf.get_cc_version(cc,icc=True)
	conf.env.CC_NAME='icc'
def configure(conf):
	conf.find_icc()
	conf.find_ar()
	conf.gcc_common_flags()
	conf.gcc_modifier_platform()
	conf.cc_load_tools()
	conf.cc_add_flags()
	conf.link_add_flags()
