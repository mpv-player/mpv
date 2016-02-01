#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

from waflib.Tools import ccroot,ar,gcc
from waflib.Configure import conf
@conf
def find_clang(conf):
	cc=conf.find_program('clang',var='CC')
	conf.get_cc_version(cc,clang=True)
	conf.env.CC_NAME='clang'
def configure(conf):
	conf.find_clang()
	conf.find_program(['llvm-ar','ar'],var='AR')
	conf.find_ar()
	conf.gcc_common_flags()
	conf.gcc_modifier_platform()
	conf.cc_load_tools()
	conf.cc_add_flags()
	conf.link_add_flags()
