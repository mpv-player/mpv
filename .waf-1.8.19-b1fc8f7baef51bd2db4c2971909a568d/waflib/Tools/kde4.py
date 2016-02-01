#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import os,re
from waflib import Task,Utils
from waflib.TaskGen import feature
@feature('msgfmt')
def apply_msgfmt(self):
	for lang in self.to_list(self.langs):
		node=self.path.find_resource(lang+'.po')
		task=self.create_task('msgfmt',node,node.change_ext('.mo'))
		langname=lang.split('/')
		langname=langname[-1]
		inst=getattr(self,'install_path','${KDE4_LOCALE_INSTALL_DIR}')
		self.bld.install_as(inst+os.sep+langname+os.sep+'LC_MESSAGES'+os.sep+getattr(self,'appname','set_your_appname')+'.mo',task.outputs[0],chmod=getattr(self,'chmod',Utils.O644))
class msgfmt(Task.Task):
	color='BLUE'
	run_str='${MSGFMT} ${SRC} -o ${TGT}'
def configure(self):
	kdeconfig=self.find_program('kde4-config')
	prefix=self.cmd_and_log(kdeconfig+['--prefix']).strip()
	fname='%s/share/apps/cmake/modules/KDELibsDependencies.cmake'%prefix
	try:os.stat(fname)
	except OSError:
		fname='%s/share/kde4/apps/cmake/modules/KDELibsDependencies.cmake'%prefix
		try:os.stat(fname)
		except OSError:self.fatal('could not open %s'%fname)
	try:
		txt=Utils.readf(fname)
	except EnvironmentError:
		self.fatal('could not read %s'%fname)
	txt=txt.replace('\\\n','\n')
	fu=re.compile('#(.*)\n')
	txt=fu.sub('',txt)
	setregexp=re.compile('([sS][eE][tT]\s*\()\s*([^\s]+)\s+\"([^"]+)\"\)')
	found=setregexp.findall(txt)
	for(_,key,val)in found:
		self.env[key]=val
	self.env['LIB_KDECORE']=['kdecore']
	self.env['LIB_KDEUI']=['kdeui']
	self.env['LIB_KIO']=['kio']
	self.env['LIB_KHTML']=['khtml']
	self.env['LIB_KPARTS']=['kparts']
	self.env['LIBPATH_KDECORE']=[os.path.join(self.env.KDE4_LIB_INSTALL_DIR,'kde4','devel'),self.env.KDE4_LIB_INSTALL_DIR]
	self.env['INCLUDES_KDECORE']=[self.env['KDE4_INCLUDE_INSTALL_DIR']]
	self.env.append_value('INCLUDES_KDECORE',[self.env['KDE4_INCLUDE_INSTALL_DIR']+os.sep+'KDE'])
	self.find_program('msgfmt',var='MSGFMT')
