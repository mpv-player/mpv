#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import os,re
from waflib import Task,Utils,Node,Errors
from waflib.TaskGen import after_method,before_method,feature,taskgen_method,extension
from waflib.Tools import c_aliases,c_preproc,c_config,c_osx,c_tests
from waflib.Configure import conf
SYSTEM_LIB_PATHS=['/usr/lib64','/usr/lib','/usr/local/lib64','/usr/local/lib']
USELIB_VARS=Utils.defaultdict(set)
USELIB_VARS['c']=set(['INCLUDES','FRAMEWORKPATH','DEFINES','CPPFLAGS','CCDEPS','CFLAGS','ARCH'])
USELIB_VARS['cxx']=set(['INCLUDES','FRAMEWORKPATH','DEFINES','CPPFLAGS','CXXDEPS','CXXFLAGS','ARCH'])
USELIB_VARS['d']=set(['INCLUDES','DFLAGS'])
USELIB_VARS['includes']=set(['INCLUDES','FRAMEWORKPATH','ARCH'])
USELIB_VARS['cprogram']=USELIB_VARS['cxxprogram']=set(['LIB','STLIB','LIBPATH','STLIBPATH','LINKFLAGS','RPATH','LINKDEPS','FRAMEWORK','FRAMEWORKPATH','ARCH','LDFLAGS'])
USELIB_VARS['cshlib']=USELIB_VARS['cxxshlib']=set(['LIB','STLIB','LIBPATH','STLIBPATH','LINKFLAGS','RPATH','LINKDEPS','FRAMEWORK','FRAMEWORKPATH','ARCH','LDFLAGS'])
USELIB_VARS['cstlib']=USELIB_VARS['cxxstlib']=set(['ARFLAGS','LINKDEPS'])
USELIB_VARS['dprogram']=set(['LIB','STLIB','LIBPATH','STLIBPATH','LINKFLAGS','RPATH','LINKDEPS'])
USELIB_VARS['dshlib']=set(['LIB','STLIB','LIBPATH','STLIBPATH','LINKFLAGS','RPATH','LINKDEPS'])
USELIB_VARS['dstlib']=set(['ARFLAGS','LINKDEPS'])
USELIB_VARS['asm']=set(['ASFLAGS'])
@taskgen_method
def create_compiled_task(self,name,node):
	out='%s.%d.o'%(node.name,self.idx)
	task=self.create_task(name,node,node.parent.find_or_declare(out))
	try:
		self.compiled_tasks.append(task)
	except AttributeError:
		self.compiled_tasks=[task]
	return task
@taskgen_method
def to_incnodes(self,inlst):
	lst=[]
	seen=set([])
	for x in self.to_list(inlst):
		if x in seen or not x:
			continue
		seen.add(x)
		if isinstance(x,Node.Node):
			lst.append(x)
		else:
			if os.path.isabs(x):
				lst.append(self.bld.root.make_node(x)or x)
			else:
				if x[0]=='#':
					p=self.bld.bldnode.make_node(x[1:])
					v=self.bld.srcnode.make_node(x[1:])
				else:
					p=self.path.get_bld().make_node(x)
					v=self.path.make_node(x)
				if p.is_child_of(self.bld.bldnode):
					p.mkdir()
				lst.append(p)
				lst.append(v)
	return lst
@feature('c','cxx','d','asm','fc','includes')
@after_method('propagate_uselib_vars','process_source')
def apply_incpaths(self):
	lst=self.to_incnodes(self.to_list(getattr(self,'includes',[]))+self.env['INCLUDES'])
	self.includes_nodes=lst
	self.env['INCPATHS']=[x.abspath()for x in lst]
class link_task(Task.Task):
	color='YELLOW'
	inst_to=None
	chmod=Utils.O755
	def add_target(self,target):
		if isinstance(target,str):
			pattern=self.env[self.__class__.__name__+'_PATTERN']
			if not pattern:
				pattern='%s'
			folder,name=os.path.split(target)
			if self.__class__.__name__.find('shlib')>0 and getattr(self.generator,'vnum',None):
				nums=self.generator.vnum.split('.')
				if self.env.DEST_BINFMT=='pe':
					name=name+'-'+nums[0]
				elif self.env.DEST_OS=='openbsd':
					pattern='%s.%s'%(pattern,nums[0])
					if len(nums)>=2:
						pattern+='.%s'%nums[1]
			if folder:
				tmp=folder+os.sep+pattern%name
			else:
				tmp=pattern%name
			target=self.generator.path.find_or_declare(tmp)
		self.set_outputs(target)
class stlink_task(link_task):
	run_str='${AR} ${ARFLAGS} ${AR_TGT_F}${TGT} ${AR_SRC_F}${SRC}'
	chmod=Utils.O644
def rm_tgt(cls):
	old=cls.run
	def wrap(self):
		try:os.remove(self.outputs[0].abspath())
		except OSError:pass
		return old(self)
	setattr(cls,'run',wrap)
rm_tgt(stlink_task)
@feature('c','cxx','d','fc','asm')
@after_method('process_source')
def apply_link(self):
	for x in self.features:
		if x=='cprogram'and'cxx'in self.features:
			x='cxxprogram'
		elif x=='cshlib'and'cxx'in self.features:
			x='cxxshlib'
		if x in Task.classes:
			if issubclass(Task.classes[x],link_task):
				link=x
				break
	else:
		return
	objs=[t.outputs[0]for t in getattr(self,'compiled_tasks',[])]
	self.link_task=self.create_task(link,objs)
	self.link_task.add_target(self.target)
	try:
		inst_to=self.install_path
	except AttributeError:
		inst_to=self.link_task.__class__.inst_to
	if inst_to:
		self.install_task=self.bld.install_files(inst_to,self.link_task.outputs[:],env=self.env,chmod=self.link_task.chmod,task=self.link_task)
@taskgen_method
def use_rec(self,name,**kw):
	if name in self.tmp_use_not or name in self.tmp_use_seen:
		return
	try:
		y=self.bld.get_tgen_by_name(name)
	except Errors.WafError:
		self.uselib.append(name)
		self.tmp_use_not.add(name)
		return
	self.tmp_use_seen.append(name)
	y.post()
	y.tmp_use_objects=objects=kw.get('objects',True)
	y.tmp_use_stlib=stlib=kw.get('stlib',True)
	try:
		link_task=y.link_task
	except AttributeError:
		y.tmp_use_var=''
	else:
		objects=False
		if not isinstance(link_task,stlink_task):
			stlib=False
			y.tmp_use_var='LIB'
		else:
			y.tmp_use_var='STLIB'
	p=self.tmp_use_prec
	for x in self.to_list(getattr(y,'use',[])):
		if self.env["STLIB_"+x]:
			continue
		try:
			p[x].append(name)
		except KeyError:
			p[x]=[name]
		self.use_rec(x,objects=objects,stlib=stlib)
@feature('c','cxx','d','use','fc')
@before_method('apply_incpaths','propagate_uselib_vars')
@after_method('apply_link','process_source')
def process_use(self):
	use_not=self.tmp_use_not=set([])
	self.tmp_use_seen=[]
	use_prec=self.tmp_use_prec={}
	self.uselib=self.to_list(getattr(self,'uselib',[]))
	self.includes=self.to_list(getattr(self,'includes',[]))
	names=self.to_list(getattr(self,'use',[]))
	for x in names:
		self.use_rec(x)
	for x in use_not:
		if x in use_prec:
			del use_prec[x]
	out=[]
	tmp=[]
	for x in self.tmp_use_seen:
		for k in use_prec.values():
			if x in k:
				break
		else:
			tmp.append(x)
	while tmp:
		e=tmp.pop()
		out.append(e)
		try:
			nlst=use_prec[e]
		except KeyError:
			pass
		else:
			del use_prec[e]
			for x in nlst:
				for y in use_prec:
					if x in use_prec[y]:
						break
				else:
					tmp.append(x)
	if use_prec:
		raise Errors.WafError('Cycle detected in the use processing %r'%use_prec)
	out.reverse()
	link_task=getattr(self,'link_task',None)
	for x in out:
		y=self.bld.get_tgen_by_name(x)
		var=y.tmp_use_var
		if var and link_task:
			if var=='LIB'or y.tmp_use_stlib or x in names:
				self.env.append_value(var,[y.target[y.target.rfind(os.sep)+1:]])
				self.link_task.dep_nodes.extend(y.link_task.outputs)
				tmp_path=y.link_task.outputs[0].parent.path_from(self.bld.bldnode)
				self.env.append_unique(var+'PATH',[tmp_path])
		else:
			if y.tmp_use_objects:
				self.add_objects_from_tgen(y)
		if getattr(y,'export_includes',None):
			self.includes.extend(y.to_incnodes(y.export_includes))
		if getattr(y,'export_defines',None):
			self.env.append_value('DEFINES',self.to_list(y.export_defines))
	for x in names:
		try:
			y=self.bld.get_tgen_by_name(x)
		except Errors.WafError:
			if not self.env['STLIB_'+x]and not x in self.uselib:
				self.uselib.append(x)
		else:
			for k in self.to_list(getattr(y,'use',[])):
				if not self.env['STLIB_'+k]and not k in self.uselib:
					self.uselib.append(k)
@taskgen_method
def accept_node_to_link(self,node):
	return not node.name.endswith('.pdb')
@taskgen_method
def add_objects_from_tgen(self,tg):
	try:
		link_task=self.link_task
	except AttributeError:
		pass
	else:
		for tsk in getattr(tg,'compiled_tasks',[]):
			for x in tsk.outputs:
				if self.accept_node_to_link(x):
					link_task.inputs.append(x)
@taskgen_method
def get_uselib_vars(self):
	_vars=set([])
	for x in self.features:
		if x in USELIB_VARS:
			_vars|=USELIB_VARS[x]
	return _vars
@feature('c','cxx','d','fc','javac','cs','uselib','asm')
@after_method('process_use')
def propagate_uselib_vars(self):
	_vars=self.get_uselib_vars()
	env=self.env
	app=env.append_value
	feature_uselib=self.features+self.to_list(getattr(self,'uselib',[]))
	for var in _vars:
		y=var.lower()
		val=getattr(self,y,[])
		if val:
			app(var,self.to_list(val))
		for x in feature_uselib:
			val=env['%s_%s'%(var,x)]
			if val:
				app(var,val)
@feature('cshlib','cxxshlib','fcshlib')
@after_method('apply_link')
def apply_implib(self):
	if not self.env.DEST_BINFMT=='pe':
		return
	dll=self.link_task.outputs[0]
	if isinstance(self.target,Node.Node):
		name=self.target.name
	else:
		name=os.path.split(self.target)[1]
	implib=self.env['implib_PATTERN']%name
	implib=dll.parent.find_or_declare(implib)
	self.env.append_value('LINKFLAGS',self.env['IMPLIB_ST']%implib.bldpath())
	self.link_task.outputs.append(implib)
	if getattr(self,'defs',None)and self.env.DEST_BINFMT=='pe':
		node=self.path.find_resource(self.defs)
		if not node:
			raise Errors.WafError('invalid def file %r'%self.defs)
		if'msvc'in(self.env.CC_NAME,self.env.CXX_NAME):
			self.env.append_value('LINKFLAGS','/def:%s'%node.path_from(self.bld.bldnode))
			self.link_task.dep_nodes.append(node)
		else:
			self.link_task.inputs.append(node)
	if getattr(self,'install_task',None):
		try:
			inst_to=self.install_path_implib
		except AttributeError:
			try:
				inst_to=self.install_path
			except AttributeError:
				inst_to='${IMPLIBDIR}'
				self.install_task.dest='${BINDIR}'
				if not self.env.IMPLIBDIR:
					self.env.IMPLIBDIR=self.env.LIBDIR
		self.implib_install_task=self.bld.install_files(inst_to,implib,env=self.env,chmod=self.link_task.chmod,task=self.link_task)
re_vnum=re.compile('^([1-9]\\d*|0)([.]([1-9]\\d*|0)){0,2}?$')
@feature('cshlib','cxxshlib','dshlib','fcshlib','vnum')
@after_method('apply_link','propagate_uselib_vars')
def apply_vnum(self):
	if not getattr(self,'vnum','')or os.name!='posix'or self.env.DEST_BINFMT not in('elf','mac-o'):
		return
	link=self.link_task
	if not re_vnum.match(self.vnum):
		raise Errors.WafError('Invalid vnum %r for target %r'%(self.vnum,getattr(self,'name',self)))
	nums=self.vnum.split('.')
	node=link.outputs[0]
	cnum=getattr(self,'cnum',str(nums[0]))
	cnums=cnum.split('.')
	if len(cnums)>len(nums)or nums[0:len(cnums)]!=cnums:
		raise Errors.WafError('invalid compatibility version %s'%cnum)
	libname=node.name
	if libname.endswith('.dylib'):
		name3=libname.replace('.dylib','.%s.dylib'%self.vnum)
		name2=libname.replace('.dylib','.%s.dylib'%cnum)
	else:
		name3=libname+'.'+self.vnum
		name2=libname+'.'+cnum
	if self.env.SONAME_ST:
		v=self.env.SONAME_ST%name2
		self.env.append_value('LINKFLAGS',v.split())
	if self.env.DEST_OS!='openbsd':
		outs=[node.parent.find_or_declare(name3)]
		if name2!=name3:
			outs.append(node.parent.find_or_declare(name2))
		self.create_task('vnum',node,outs)
	if getattr(self,'install_task',None):
		self.install_task.hasrun=Task.SKIP_ME
		bld=self.bld
		path=self.install_task.dest
		if self.env.DEST_OS=='openbsd':
			libname=self.link_task.outputs[0].name
			t1=bld.install_as('%s%s%s'%(path,os.sep,libname),node,env=self.env,chmod=self.link_task.chmod)
			self.vnum_install_task=(t1,)
		else:
			t1=bld.install_as(path+os.sep+name3,node,env=self.env,chmod=self.link_task.chmod)
			t3=bld.symlink_as(path+os.sep+libname,name3)
			if name2!=name3:
				t2=bld.symlink_as(path+os.sep+name2,name3)
				self.vnum_install_task=(t1,t2,t3)
			else:
				self.vnum_install_task=(t1,t3)
	if'-dynamiclib'in self.env['LINKFLAGS']:
		try:
			inst_to=self.install_path
		except AttributeError:
			inst_to=self.link_task.__class__.inst_to
		if inst_to:
			p=Utils.subst_vars(inst_to,self.env)
			path=os.path.join(p,name2)
			self.env.append_value('LINKFLAGS',['-install_name',path])
			self.env.append_value('LINKFLAGS','-Wl,-compatibility_version,%s'%cnum)
			self.env.append_value('LINKFLAGS','-Wl,-current_version,%s'%self.vnum)
class vnum(Task.Task):
	color='CYAN'
	quient=True
	ext_in=['.bin']
	def keyword(self):
		return'Symlinking'
	def run(self):
		for x in self.outputs:
			path=x.abspath()
			try:
				os.remove(path)
			except OSError:
				pass
			try:
				os.symlink(self.inputs[0].name,path)
			except OSError:
				return 1
class fake_shlib(link_task):
	def runnable_status(self):
		for t in self.run_after:
			if not t.hasrun:
				return Task.ASK_LATER
		for x in self.outputs:
			x.sig=Utils.h_file(x.abspath())
		return Task.SKIP_ME
class fake_stlib(stlink_task):
	def runnable_status(self):
		for t in self.run_after:
			if not t.hasrun:
				return Task.ASK_LATER
		for x in self.outputs:
			x.sig=Utils.h_file(x.abspath())
		return Task.SKIP_ME
@conf
def read_shlib(self,name,paths=[],export_includes=[],export_defines=[]):
	return self(name=name,features='fake_lib',lib_paths=paths,lib_type='shlib',export_includes=export_includes,export_defines=export_defines)
@conf
def read_stlib(self,name,paths=[],export_includes=[],export_defines=[]):
	return self(name=name,features='fake_lib',lib_paths=paths,lib_type='stlib',export_includes=export_includes,export_defines=export_defines)
lib_patterns={'shlib':['lib%s.so','%s.so','lib%s.dylib','lib%s.dll','%s.dll'],'stlib':['lib%s.a','%s.a','lib%s.dll','%s.dll','lib%s.lib','%s.lib'],}
@feature('fake_lib')
def process_lib(self):
	node=None
	names=[x%self.name for x in lib_patterns[self.lib_type]]
	for x in self.lib_paths+[self.path]+SYSTEM_LIB_PATHS:
		if not isinstance(x,Node.Node):
			x=self.bld.root.find_node(x)or self.path.find_node(x)
			if not x:
				continue
		for y in names:
			node=x.find_node(y)
			if node:
				node.sig=Utils.h_file(node.abspath())
				break
		else:
			continue
		break
	else:
		raise Errors.WafError('could not find library %r'%self.name)
	self.link_task=self.create_task('fake_%s'%self.lib_type,[],[node])
	self.target=self.name
class fake_o(Task.Task):
	def runnable_status(self):
		return Task.SKIP_ME
@extension('.o','.obj')
def add_those_o_files(self,node):
	tsk=self.create_task('fake_o',[],node)
	try:
		self.compiled_tasks.append(tsk)
	except AttributeError:
		self.compiled_tasks=[tsk]
@feature('fake_obj')
@before_method('process_source')
def process_objs(self):
	for node in self.to_nodes(self.source):
		self.add_those_o_files(node)
	self.source=[]
@conf
def read_object(self,obj):
	if not isinstance(obj,self.path.__class__):
		obj=self.path.find_resource(obj)
	return self(features='fake_obj',source=obj,name=obj.name)
@feature('cxxprogram','cprogram')
@after_method('apply_link','process_use')
def set_full_paths_hpux(self):
	if self.env.DEST_OS!='hp-ux':
		return
	base=self.bld.bldnode.abspath()
	for var in['LIBPATH','STLIBPATH']:
		lst=[]
		for x in self.env[var]:
			if x.startswith('/'):
				lst.append(x)
			else:
				lst.append(os.path.normpath(os.path.join(base,x)))
		self.env[var]=lst
