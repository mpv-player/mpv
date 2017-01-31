#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

typos={'feature':'features','sources':'source','targets':'target','include':'includes','export_include':'export_includes','define':'defines','importpath':'includes','installpath':'install_path','iscopy':'is_copy',}
meths_typos=['__call__','program','shlib','stlib','objects']
import sys
from waflib import Logs,Build,Node,Task,TaskGen,ConfigSet,Errors,Utils
import waflib.Tools.ccroot
def check_same_targets(self):
	mp=Utils.defaultdict(list)
	uids={}
	def check_task(tsk):
		if not isinstance(tsk,Task.Task):
			return
		for node in tsk.outputs:
			mp[node].append(tsk)
		try:
			uids[tsk.uid()].append(tsk)
		except KeyError:
			uids[tsk.uid()]=[tsk]
	for g in self.groups:
		for tg in g:
			try:
				for tsk in tg.tasks:
					check_task(tsk)
			except AttributeError:
				check_task(tg)
	dupe=False
	for(k,v)in mp.items():
		if len(v)>1:
			dupe=True
			msg='* Node %r is created more than once%s. The task generators are:'%(k,Logs.verbose==1 and" (full message on 'waf -v -v')"or"")
			Logs.error(msg)
			for x in v:
				if Logs.verbose>1:
					Logs.error('  %d. %r'%(1+v.index(x),x.generator))
				else:
					Logs.error('  %d. %r in %r'%(1+v.index(x),x.generator.name,getattr(x.generator,'path',None)))
	if not dupe:
		for(k,v)in uids.items():
			if len(v)>1:
				Logs.error('* Several tasks use the same identifier. Please check the information on\n   https://waf.io/apidocs/Task.html?highlight=uid#waflib.Task.Task.uid')
				for tsk in v:
					Logs.error('  - object %r (%r) defined in %r'%(tsk.__class__.__name__,tsk,tsk.generator))
def check_invalid_constraints(self):
	feat=set([])
	for x in list(TaskGen.feats.values()):
		feat.union(set(x))
	for(x,y)in TaskGen.task_gen.prec.items():
		feat.add(x)
		feat.union(set(y))
	ext=set([])
	for x in TaskGen.task_gen.mappings.values():
		ext.add(x.__name__)
	invalid=ext&feat
	if invalid:
		Logs.error('The methods %r have invalid annotations:  @extension <-> @feature/@before_method/@after_method'%list(invalid))
	for cls in list(Task.classes.values()):
		if sys.hexversion>0x3000000 and issubclass(cls,Task.Task)and isinstance(cls.hcode,str):
			raise Errors.WafError('Class %r has hcode value %r of type <str>, expecting <bytes> (use Utils.h_cmd() ?)'%(cls,cls.hcode))
		for x in('before','after'):
			for y in Utils.to_list(getattr(cls,x,[])):
				if not Task.classes.get(y,None):
					Logs.error('Erroneous order constraint %r=%r on task class %r'%(x,y,cls.__name__))
		if getattr(cls,'rule',None):
			Logs.error('Erroneous attribute "rule" on task class %r (rename to "run_str")'%cls.__name__)
def replace(m):
	oldcall=getattr(Build.BuildContext,m)
	def call(self,*k,**kw):
		ret=oldcall(self,*k,**kw)
		for x in typos:
			if x in kw:
				if x=='iscopy'and'subst'in getattr(self,'features',''):
					continue
				Logs.error('Fix the typo %r -> %r on %r'%(x,typos[x],ret))
		return ret
	setattr(Build.BuildContext,m,call)
def enhance_lib():
	for m in meths_typos:
		replace(m)
	def ant_glob(self,*k,**kw):
		if k:
			lst=Utils.to_list(k[0])
			for pat in lst:
				if'..'in pat.split('/'):
					Logs.error("In ant_glob pattern %r: '..' means 'two dots', not 'parent directory'"%k[0])
		if kw.get('remove',True):
			try:
				if self.is_child_of(self.ctx.bldnode)and not kw.get('quiet',False):
					Logs.error('Using ant_glob on the build folder (%r) is dangerous (quiet=True to disable this warning)'%self)
			except AttributeError:
				pass
		return self.old_ant_glob(*k,**kw)
	Node.Node.old_ant_glob=Node.Node.ant_glob
	Node.Node.ant_glob=ant_glob
	old=Task.is_before
	def is_before(t1,t2):
		ret=old(t1,t2)
		if ret and old(t2,t1):
			Logs.error('Contradictory order constraints in classes %r %r'%(t1,t2))
		return ret
	Task.is_before=is_before
	def check_err_features(self):
		lst=self.to_list(self.features)
		if'shlib'in lst:
			Logs.error('feature shlib -> cshlib, dshlib or cxxshlib')
		for x in('c','cxx','d','fc'):
			if not x in lst and lst and lst[0]in[x+y for y in('program','shlib','stlib')]:
				Logs.error('%r features is probably missing %r'%(self,x))
	TaskGen.feature('*')(check_err_features)
	def check_err_order(self):
		if not hasattr(self,'rule')and not'subst'in Utils.to_list(self.features):
			for x in('before','after','ext_in','ext_out'):
				if hasattr(self,x):
					Logs.warn('Erroneous order constraint %r on non-rule based task generator %r'%(x,self))
		else:
			for x in('before','after'):
				for y in self.to_list(getattr(self,x,[])):
					if not Task.classes.get(y,None):
						Logs.error('Erroneous order constraint %s=%r on %r (no such class)'%(x,y,self))
	TaskGen.feature('*')(check_err_order)
	def check_compile(self):
		check_invalid_constraints(self)
		try:
			ret=self.orig_compile()
		finally:
			check_same_targets(self)
		return ret
	Build.BuildContext.orig_compile=Build.BuildContext.compile
	Build.BuildContext.compile=check_compile
	def use_rec(self,name,**kw):
		try:
			y=self.bld.get_tgen_by_name(name)
		except Errors.WafError:
			pass
		else:
			idx=self.bld.get_group_idx(self)
			odx=self.bld.get_group_idx(y)
			if odx>idx:
				msg="Invalid 'use' across build groups:"
				if Logs.verbose>1:
					msg+='\n  target %r\n  uses:\n  %r'%(self,y)
				else:
					msg+=" %r uses %r (try 'waf -v -v' for the full error)"%(self.name,name)
				raise Errors.WafError(msg)
		self.orig_use_rec(name,**kw)
	TaskGen.task_gen.orig_use_rec=TaskGen.task_gen.use_rec
	TaskGen.task_gen.use_rec=use_rec
	def getattri(self,name,default=None):
		if name=='append'or name=='add':
			raise Errors.WafError('env.append and env.add do not exist: use env.append_value/env.append_unique')
		elif name=='prepend':
			raise Errors.WafError('env.prepend does not exist: use env.prepend_value')
		if name in self.__slots__:
			return object.__getattr__(self,name,default)
		else:
			return self[name]
	ConfigSet.ConfigSet.__getattr__=getattri
def options(opt):
	enhance_lib()
def configure(conf):
	pass
