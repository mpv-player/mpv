#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

from waflib import Task
import waflib.Task
from waflib.Tools.ccroot import link_task,stlink_task
from waflib.TaskGen import extension
class asm(Task.Task):
	color='BLUE'
	run_str='${AS} ${ASFLAGS} ${ASMPATH_ST:INCPATHS} ${DEFINES_ST:DEFINES} ${AS_SRC_F}${SRC} ${AS_TGT_F}${TGT}'
@extension('.s','.S','.asm','.ASM','.spp','.SPP')
def asm_hook(self,node):
	return self.create_compiled_task('asm',node)
class asmprogram(link_task):
	run_str='${ASLINK} ${ASLINKFLAGS} ${ASLNK_TGT_F}${TGT} ${ASLNK_SRC_F}${SRC}'
	ext_out=['.bin']
	inst_to='${BINDIR}'
class asmshlib(asmprogram):
	inst_to='${LIBDIR}'
class asmstlib(stlink_task):
	pass
def configure(conf):
	conf.env['ASMPATH_ST']='-I%s'
