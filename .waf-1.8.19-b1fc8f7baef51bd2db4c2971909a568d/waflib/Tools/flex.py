#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import waflib.TaskGen,os,re
def decide_ext(self,node):
	if'cxx'in self.features:
		return['.lex.cc']
	return['.lex.c']
def flexfun(tsk):
	env=tsk.env
	bld=tsk.generator.bld
	wd=bld.variant_dir
	def to_list(xx):
		if isinstance(xx,str):return[xx]
		return xx
	tsk.last_cmd=lst=[]
	lst.extend(to_list(env['FLEX']))
	lst.extend(to_list(env['FLEXFLAGS']))
	inputs=[a.path_from(bld.bldnode)for a in tsk.inputs]
	if env.FLEX_MSYS:
		inputs=[x.replace(os.sep,'/')for x in inputs]
	lst.extend(inputs)
	lst=[x for x in lst if x]
	txt=bld.cmd_and_log(lst,cwd=wd,env=env.env or None,quiet=0)
	tsk.outputs[0].write(txt.replace('\r\n','\n').replace('\r','\n'))
waflib.TaskGen.declare_chain(name='flex',rule=flexfun,ext_in='.l',decider=decide_ext,)
def configure(conf):
	conf.find_program('flex',var='FLEX')
	conf.env.FLEXFLAGS=['-t']
	if re.search(r"\\msys\\[0-9.]+\\bin\\flex.exe$",conf.env.FLEX[0]):
		conf.env.FLEX_MSYS=True
