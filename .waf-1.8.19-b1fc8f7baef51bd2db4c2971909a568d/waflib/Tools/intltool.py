#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import os,re
from waflib import Context,Task,Utils,Logs
import waflib.Tools.ccroot
from waflib.TaskGen import feature,before_method,taskgen_method
from waflib.Logs import error
from waflib.Configure import conf
_style_flags={'ba':'-b','desktop':'-d','keys':'-k','quoted':'--quoted-style','quotedxml':'--quotedxml-style','rfc822deb':'-r','schemas':'-s','xml':'-x',}
@taskgen_method
def ensure_localedir(self):
	if not self.env.LOCALEDIR:
		if self.env.DATAROOTDIR:
			self.env.LOCALEDIR=os.path.join(self.env.DATAROOTDIR,'locale')
		else:
			self.env.LOCALEDIR=os.path.join(self.env.PREFIX,'share','locale')
@before_method('process_source')
@feature('intltool_in')
def apply_intltool_in_f(self):
	try:self.meths.remove('process_source')
	except ValueError:pass
	self.ensure_localedir()
	podir=getattr(self,'podir','.')
	podirnode=self.path.find_dir(podir)
	if not podirnode:
		error("could not find the podir %r"%podir)
		return
	cache=getattr(self,'intlcache','.intlcache')
	self.env.INTLCACHE=[os.path.join(str(self.path.get_bld()),podir,cache)]
	self.env.INTLPODIR=podirnode.bldpath()
	self.env.append_value('INTLFLAGS',getattr(self,'flags',self.env.INTLFLAGS_DEFAULT))
	if'-c'in self.env.INTLFLAGS:
		self.bld.fatal('Redundant -c flag in intltool task %r'%self)
	style=getattr(self,'style',None)
	if style:
		try:
			style_flag=_style_flags[style]
		except KeyError:
			self.bld.fatal('intltool_in style "%s" is not valid'%style)
		self.env.append_unique('INTLFLAGS',[style_flag])
	for i in self.to_list(self.source):
		node=self.path.find_resource(i)
		task=self.create_task('intltool',node,node.change_ext(''))
		inst=getattr(self,'install_path',None)
		if inst:
			self.bld.install_files(inst,task.outputs)
@feature('intltool_po')
def apply_intltool_po(self):
	try:self.meths.remove('process_source')
	except ValueError:pass
	self.ensure_localedir()
	appname=getattr(self,'appname',getattr(Context.g_module,Context.APPNAME,'set_your_app_name'))
	podir=getattr(self,'podir','.')
	inst=getattr(self,'install_path','${LOCALEDIR}')
	linguas=self.path.find_node(os.path.join(podir,'LINGUAS'))
	if linguas:
		file=open(linguas.abspath())
		langs=[]
		for line in file.readlines():
			if not line.startswith('#'):
				langs+=line.split()
		file.close()
		re_linguas=re.compile('[-a-zA-Z_@.]+')
		for lang in langs:
			if re_linguas.match(lang):
				node=self.path.find_resource(os.path.join(podir,re_linguas.match(lang).group()+'.po'))
				task=self.create_task('po',node,node.change_ext('.mo'))
				if inst:
					filename=task.outputs[0].name
					(langname,ext)=os.path.splitext(filename)
					inst_file=inst+os.sep+langname+os.sep+'LC_MESSAGES'+os.sep+appname+'.mo'
					self.bld.install_as(inst_file,task.outputs[0],chmod=getattr(self,'chmod',Utils.O644),env=task.env)
	else:
		Logs.pprint('RED',"Error no LINGUAS file found in po directory")
class po(Task.Task):
	run_str='${MSGFMT} -o ${TGT} ${SRC}'
	color='BLUE'
class intltool(Task.Task):
	run_str='${INTLTOOL} ${INTLFLAGS} ${INTLCACHE_ST:INTLCACHE} ${INTLPODIR} ${SRC} ${TGT}'
	color='BLUE'
@conf
def find_msgfmt(conf):
	conf.find_program('msgfmt',var='MSGFMT')
@conf
def find_intltool_merge(conf):
	if not conf.env.PERL:
		conf.find_program('perl',var='PERL')
	conf.env.INTLCACHE_ST='--cache=%s'
	conf.env.INTLFLAGS_DEFAULT=['-q','-u']
	conf.find_program('intltool-merge',interpreter='PERL',var='INTLTOOL')
def configure(conf):
	conf.find_msgfmt()
	conf.find_intltool_merge()
	if conf.env.CC or conf.env.CXX:
		conf.check(header_name='locale.h')
