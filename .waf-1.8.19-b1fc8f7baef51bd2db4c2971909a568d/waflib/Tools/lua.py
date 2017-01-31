#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

from waflib.TaskGen import extension
from waflib import Task
@extension('.lua')
def add_lua(self,node):
	tsk=self.create_task('luac',node,node.change_ext('.luac'))
	inst_to=getattr(self,'install_path',self.env.LUADIR and'${LUADIR}'or None)
	if inst_to:
		self.bld.install_files(inst_to,tsk.outputs)
	return tsk
class luac(Task.Task):
	run_str='${LUAC} -s -o ${TGT} ${SRC}'
	color='PINK'
def configure(conf):
	conf.find_program('luac',var='LUAC')
