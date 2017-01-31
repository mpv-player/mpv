#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

from waflib.Configure import conf
@conf
def find_ar(conf):
	conf.load('ar')
def configure(conf):
	conf.find_program('ar',var='AR')
	conf.add_os_flags('ARFLAGS')
	if not conf.env.ARFLAGS:
		conf.env.ARFLAGS=['rcs']
