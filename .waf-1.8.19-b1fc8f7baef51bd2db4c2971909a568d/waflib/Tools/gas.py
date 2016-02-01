#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import waflib.Tools.asm
from waflib.Tools import ar
def configure(conf):
	conf.find_program(['gas','gcc'],var='AS')
	conf.env.AS_TGT_F=['-c','-o']
	conf.env.ASLNK_TGT_F=['-o']
	conf.find_ar()
	conf.load('asm')
