#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import os,re,sys
from waflib import Utils,Logs,Errors
NOT_RUN=0
MISSING=1
CRASHED=2
EXCEPTION=3
SKIPPED=8
SUCCESS=9
ASK_LATER=-1
SKIP_ME=-2
RUN_ME=-3
COMPILE_TEMPLATE_SHELL='''
def f(tsk):
	env = tsk.env
	gen = tsk.generator
	bld = gen.bld
	cwdx = getattr(bld, 'cwdx', bld.bldnode) # TODO single cwd value in waf 1.9
	wd = getattr(tsk, 'cwd', None)
	p = env.get_flat
	tsk.last_cmd = cmd = \'\'\' %s \'\'\' % s
	return tsk.exec_command(cmd, cwd=wd, env=env.env or None)
'''
COMPILE_TEMPLATE_NOSHELL='''
def f(tsk):
	env = tsk.env
	gen = tsk.generator
	bld = gen.bld
	cwdx = getattr(bld, 'cwdx', bld.bldnode) # TODO single cwd value in waf 1.9
	wd = getattr(tsk, 'cwd', None)
	def to_list(xx):
		if isinstance(xx, str): return [xx]
		return xx
	tsk.last_cmd = lst = []
	%s
	lst = [x for x in lst if x]
	return tsk.exec_command(lst, cwd=wd, env=env.env or None)
'''
classes={}
class store_task_type(type):
	def __init__(cls,name,bases,dict):
		super(store_task_type,cls).__init__(name,bases,dict)
		name=cls.__name__
		if name.endswith('_task'):
			name=name.replace('_task','')
		if name!='evil'and name!='TaskBase':
			global classes
			if getattr(cls,'run_str',None):
				(f,dvars)=compile_fun(cls.run_str,cls.shell)
				cls.hcode=Utils.h_cmd(cls.run_str)
				cls.orig_run_str=cls.run_str
				cls.run_str=None
				cls.run=f
				cls.vars=list(set(cls.vars+dvars))
				cls.vars.sort()
			elif getattr(cls,'run',None)and not'hcode'in cls.__dict__:
				cls.hcode=Utils.h_cmd(cls.run)
			getattr(cls,'register',classes)[name]=cls
evil=store_task_type('evil',(object,),{})
class TaskBase(evil):
	color='GREEN'
	ext_in=[]
	ext_out=[]
	before=[]
	after=[]
	hcode=''
	def __init__(self,*k,**kw):
		self.hasrun=NOT_RUN
		try:
			self.generator=kw['generator']
		except KeyError:
			self.generator=self
	def __repr__(self):
		return'\n\t{task %r: %s %s}'%(self.__class__.__name__,id(self),str(getattr(self,'fun','')))
	def __str__(self):
		if hasattr(self,'fun'):
			return self.fun.__name__
		return self.__class__.__name__
	def __hash__(self):
		return id(self)
	def keyword(self):
		if hasattr(self,'fun'):
			return'Function'
		return'Processing'
	def exec_command(self,cmd,**kw):
		bld=self.generator.bld
		try:
			if not kw.get('cwd',None):
				kw['cwd']=bld.cwd
		except AttributeError:
			bld.cwd=kw['cwd']=bld.variant_dir
		return bld.exec_command(cmd,**kw)
	def runnable_status(self):
		return RUN_ME
	def process(self):
		m=self.master
		if m.stop:
			m.out.put(self)
			return
		try:
			del self.generator.bld.task_sigs[self.uid()]
		except KeyError:
			pass
		try:
			self.generator.bld.returned_tasks.append(self)
			self.log_display(self.generator.bld)
			ret=self.run()
		except Exception:
			self.err_msg=Utils.ex_stack()
			self.hasrun=EXCEPTION
			m.error_handler(self)
			m.out.put(self)
			return
		if ret:
			self.err_code=ret
			self.hasrun=CRASHED
		else:
			try:
				self.post_run()
			except Errors.WafError:
				pass
			except Exception:
				self.err_msg=Utils.ex_stack()
				self.hasrun=EXCEPTION
			else:
				self.hasrun=SUCCESS
		if self.hasrun!=SUCCESS:
			m.error_handler(self)
		m.out.put(self)
	def run(self):
		if hasattr(self,'fun'):
			return self.fun(self)
		return 0
	def post_run(self):
		pass
	def log_display(self,bld):
		if self.generator.bld.progress_bar==3:
			return
		s=self.display()
		if s:
			if bld.logger:
				logger=bld.logger
			else:
				logger=Logs
			if self.generator.bld.progress_bar==1:
				c1=Logs.colors.cursor_off
				c2=Logs.colors.cursor_on
				logger.info(s,extra={'stream':sys.stderr,'terminator':'','c1':c1,'c2':c2})
			else:
				logger.info(s,extra={'terminator':'','c1':'','c2':''})
	def display(self):
		col1=Logs.colors(self.color)
		col2=Logs.colors.NORMAL
		master=self.master
		def cur():
			tmp=-1
			if hasattr(master,'ready'):
				tmp-=master.ready.qsize()
			return master.processed+tmp
		if self.generator.bld.progress_bar==1:
			return self.generator.bld.progress_line(cur(),master.total,col1,col2)
		if self.generator.bld.progress_bar==2:
			ela=str(self.generator.bld.timer)
			try:
				ins=','.join([n.name for n in self.inputs])
			except AttributeError:
				ins=''
			try:
				outs=','.join([n.name for n in self.outputs])
			except AttributeError:
				outs=''
			return'|Total %s|Current %s|Inputs %s|Outputs %s|Time %s|\n'%(master.total,cur(),ins,outs,ela)
		s=str(self)
		if not s:
			return None
		total=master.total
		n=len(str(total))
		fs='[%%%dd/%%%dd] %%s%%s%%s%%s\n'%(n,n)
		kw=self.keyword()
		if kw:
			kw+=' '
		return fs%(cur(),total,kw,col1,s,col2)
	def attr(self,att,default=None):
		ret=getattr(self,att,self)
		if ret is self:return getattr(self.__class__,att,default)
		return ret
	def hash_constraints(self):
		cls=self.__class__
		tup=(str(cls.before),str(cls.after),str(cls.ext_in),str(cls.ext_out),cls.__name__,cls.hcode)
		h=hash(tup)
		return h
	def format_error(self):
		msg=getattr(self,'last_cmd','')
		name=getattr(self.generator,'name','')
		if getattr(self,"err_msg",None):
			return self.err_msg
		elif not self.hasrun:
			return'task in %r was not executed for some reason: %r'%(name,self)
		elif self.hasrun==CRASHED:
			try:
				return' -> task in %r failed (exit status %r): %r\n%r'%(name,self.err_code,self,msg)
			except AttributeError:
				return' -> task in %r failed: %r\n%r'%(name,self,msg)
		elif self.hasrun==MISSING:
			return' -> missing files in %r: %r\n%r'%(name,self,msg)
		else:
			return'invalid status for task in %r: %r'%(name,self.hasrun)
	def colon(self,var1,var2):
		tmp=self.env[var1]
		if not tmp:
			return[]
		if isinstance(var2,str):
			it=self.env[var2]
		else:
			it=var2
		if isinstance(tmp,str):
			return[tmp%x for x in it]
		else:
			lst=[]
			for y in it:
				lst.extend(tmp)
				lst.append(y)
			return lst
class Task(TaskBase):
	vars=[]
	shell=False
	def __init__(self,*k,**kw):
		TaskBase.__init__(self,*k,**kw)
		self.env=kw['env']
		self.inputs=[]
		self.outputs=[]
		self.dep_nodes=[]
		self.run_after=set([])
	def __str__(self):
		name=self.__class__.__name__
		if self.outputs:
			if(name.endswith('lib')or name.endswith('program'))or not self.inputs:
				node=self.outputs[0]
				return node.path_from(node.ctx.launch_node())
		if not(self.inputs or self.outputs):
			return self.__class__.__name__
		if len(self.inputs)==1:
			node=self.inputs[0]
			return node.path_from(node.ctx.launch_node())
		src_str=' '.join([a.path_from(a.ctx.launch_node())for a in self.inputs])
		tgt_str=' '.join([a.path_from(a.ctx.launch_node())for a in self.outputs])
		if self.outputs:sep=' -> '
		else:sep=''
		return'%s: %s%s%s'%(self.__class__.__name__.replace('_task',''),src_str,sep,tgt_str)
	def keyword(self):
		name=self.__class__.__name__
		if name.endswith('lib')or name.endswith('program'):
			return'Linking'
		if len(self.inputs)==1 and len(self.outputs)==1:
			return'Compiling'
		if not self.inputs:
			if self.outputs:
				return'Creating'
			else:
				return'Running'
		return'Processing'
	def __repr__(self):
		try:
			ins=",".join([x.name for x in self.inputs])
			outs=",".join([x.name for x in self.outputs])
		except AttributeError:
			ins=",".join([str(x)for x in self.inputs])
			outs=",".join([str(x)for x in self.outputs])
		return"".join(['\n\t{task %r: '%id(self),self.__class__.__name__," ",ins," -> ",outs,'}'])
	def uid(self):
		try:
			return self.uid_
		except AttributeError:
			m=Utils.md5()
			up=m.update
			up(self.__class__.__name__)
			for x in self.inputs+self.outputs:
				up(x.abspath())
			self.uid_=m.digest()
			return self.uid_
	def set_inputs(self,inp):
		if isinstance(inp,list):self.inputs+=inp
		else:self.inputs.append(inp)
	def set_outputs(self,out):
		if isinstance(out,list):self.outputs+=out
		else:self.outputs.append(out)
	def set_run_after(self,task):
		assert isinstance(task,TaskBase)
		self.run_after.add(task)
	def signature(self):
		try:return self.cache_sig
		except AttributeError:pass
		self.m=Utils.md5()
		self.m.update(self.hcode)
		self.sig_explicit_deps()
		self.sig_vars()
		if self.scan:
			try:
				self.sig_implicit_deps()
			except Errors.TaskRescan:
				return self.signature()
		ret=self.cache_sig=self.m.digest()
		return ret
	def runnable_status(self):
		for t in self.run_after:
			if not t.hasrun:
				return ASK_LATER
		bld=self.generator.bld
		try:
			new_sig=self.signature()
		except Errors.TaskNotReady:
			return ASK_LATER
		key=self.uid()
		try:
			prev_sig=bld.task_sigs[key]
		except KeyError:
			Logs.debug("task: task %r must run as it was never run before or the task code changed"%self)
			return RUN_ME
		for node in self.outputs:
			try:
				if node.sig!=new_sig:
					return RUN_ME
			except AttributeError:
				Logs.debug("task: task %r must run as the output nodes do not exist"%self)
				return RUN_ME
		if new_sig!=prev_sig:
			return RUN_ME
		return SKIP_ME
	def post_run(self):
		bld=self.generator.bld
		sig=self.signature()
		for node in self.outputs:
			try:
				os.stat(node.abspath())
			except OSError:
				self.hasrun=MISSING
				self.err_msg='-> missing file: %r'%node.abspath()
				raise Errors.WafError(self.err_msg)
			node.sig=node.cache_sig=sig
		bld.task_sigs[self.uid()]=self.cache_sig
	def sig_explicit_deps(self):
		bld=self.generator.bld
		upd=self.m.update
		for x in self.inputs+self.dep_nodes:
			try:
				upd(x.get_bld_sig())
			except(AttributeError,TypeError):
				raise Errors.WafError('Missing node signature for %r (required by %r)'%(x,self))
		if bld.deps_man:
			additional_deps=bld.deps_man
			for x in self.inputs+self.outputs:
				try:
					d=additional_deps[id(x)]
				except KeyError:
					continue
				for v in d:
					if isinstance(v,bld.root.__class__):
						try:
							v=v.get_bld_sig()
						except AttributeError:
							raise Errors.WafError('Missing node signature for %r (required by %r)'%(v,self))
					elif hasattr(v,'__call__'):
						v=v()
					upd(v)
		return self.m.digest()
	def sig_vars(self):
		bld=self.generator.bld
		env=self.env
		upd=self.m.update
		act_sig=bld.hash_env_vars(env,self.__class__.vars)
		upd(act_sig)
		dep_vars=getattr(self,'dep_vars',None)
		if dep_vars:
			upd(bld.hash_env_vars(env,dep_vars))
		return self.m.digest()
	scan=None
	def sig_implicit_deps(self):
		bld=self.generator.bld
		key=self.uid()
		prev=bld.task_sigs.get((key,'imp'),[])
		if prev:
			try:
				if prev==self.compute_sig_implicit_deps():
					return prev
			except Errors.TaskNotReady:
				raise
			except EnvironmentError:
				for x in bld.node_deps.get(self.uid(),[]):
					if not x.is_bld():
						try:
							os.stat(x.abspath())
						except OSError:
							try:
								del x.parent.children[x.name]
							except KeyError:
								pass
			del bld.task_sigs[(key,'imp')]
			raise Errors.TaskRescan('rescan')
		(nodes,names)=self.scan()
		if Logs.verbose:
			Logs.debug('deps: scanner for %s returned %s %s'%(str(self),str(nodes),str(names)))
		bld.node_deps[key]=nodes
		bld.raw_deps[key]=names
		self.are_implicit_nodes_ready()
		try:
			bld.task_sigs[(key,'imp')]=sig=self.compute_sig_implicit_deps()
		except Exception:
			if Logs.verbose:
				for k in bld.node_deps.get(self.uid(),[]):
					try:
						k.get_bld_sig()
					except Exception:
						Logs.warn('Missing signature for node %r (may cause rebuilds)'%k)
		else:
			return sig
	def compute_sig_implicit_deps(self):
		upd=self.m.update
		bld=self.generator.bld
		self.are_implicit_nodes_ready()
		for k in bld.node_deps.get(self.uid(),[]):
			upd(k.get_bld_sig())
		return self.m.digest()
	def are_implicit_nodes_ready(self):
		bld=self.generator.bld
		try:
			cache=bld.dct_implicit_nodes
		except AttributeError:
			bld.dct_implicit_nodes=cache={}
		try:
			dct=cache[bld.cur]
		except KeyError:
			dct=cache[bld.cur]={}
			for tsk in bld.cur_tasks:
				for x in tsk.outputs:
					dct[x]=tsk
		modified=False
		for x in bld.node_deps.get(self.uid(),[]):
			if x in dct:
				self.run_after.add(dct[x])
				modified=True
		if modified:
			for tsk in self.run_after:
				if not tsk.hasrun:
					raise Errors.TaskNotReady('not ready')
if sys.hexversion>0x3000000:
	def uid(self):
		try:
			return self.uid_
		except AttributeError:
			m=Utils.md5()
			up=m.update
			up(self.__class__.__name__.encode('iso8859-1','xmlcharrefreplace'))
			for x in self.inputs+self.outputs:
				up(x.abspath().encode('iso8859-1','xmlcharrefreplace'))
			self.uid_=m.digest()
			return self.uid_
	uid.__doc__=Task.uid.__doc__
	Task.uid=uid
def is_before(t1,t2):
	to_list=Utils.to_list
	for k in to_list(t2.ext_in):
		if k in to_list(t1.ext_out):
			return 1
	if t1.__class__.__name__ in to_list(t2.after):
		return 1
	if t2.__class__.__name__ in to_list(t1.before):
		return 1
	return 0
def set_file_constraints(tasks):
	ins=Utils.defaultdict(set)
	outs=Utils.defaultdict(set)
	for x in tasks:
		for a in getattr(x,'inputs',[])+getattr(x,'dep_nodes',[]):
			ins[id(a)].add(x)
		for a in getattr(x,'outputs',[]):
			outs[id(a)].add(x)
	links=set(ins.keys()).intersection(outs.keys())
	for k in links:
		for a in ins[k]:
			a.run_after.update(outs[k])
def set_precedence_constraints(tasks):
	cstr_groups=Utils.defaultdict(list)
	for x in tasks:
		h=x.hash_constraints()
		cstr_groups[h].append(x)
	keys=list(cstr_groups.keys())
	maxi=len(keys)
	for i in range(maxi):
		t1=cstr_groups[keys[i]][0]
		for j in range(i+1,maxi):
			t2=cstr_groups[keys[j]][0]
			if is_before(t1,t2):
				a=i
				b=j
			elif is_before(t2,t1):
				a=j
				b=i
			else:
				continue
			aval=set(cstr_groups[keys[a]])
			for x in cstr_groups[keys[b]]:
				x.run_after.update(aval)
def funex(c):
	dc={}
	exec(c,dc)
	return dc['f']
re_novar=re.compile(r"^(SRC|TGT)\W+.*?$")
reg_act=re.compile(r"(?P<backslash>\\)|(?P<dollar>\$\$)|(?P<subst>\$\{(?P<var>\w+)(?P<code>.*?)\})",re.M)
def compile_fun_shell(line):
	extr=[]
	def repl(match):
		g=match.group
		if g('dollar'):return"$"
		elif g('backslash'):return'\\\\'
		elif g('subst'):extr.append((g('var'),g('code')));return"%s"
		return None
	line=reg_act.sub(repl,line)or line
	parm=[]
	dvars=[]
	app=parm.append
	for(var,meth)in extr:
		if var=='SRC':
			if meth:app('tsk.inputs%s'%meth)
			else:app('" ".join([a.path_from(cwdx) for a in tsk.inputs])')
		elif var=='TGT':
			if meth:app('tsk.outputs%s'%meth)
			else:app('" ".join([a.path_from(cwdx) for a in tsk.outputs])')
		elif meth:
			if meth.startswith(':'):
				if var not in dvars:
					dvars.append(var)
				m=meth[1:]
				if m=='SRC':
					m='[a.path_from(cwdx) for a in tsk.inputs]'
				elif m=='TGT':
					m='[a.path_from(cwdx) for a in tsk.outputs]'
				elif re_novar.match(m):
					m='[tsk.inputs%s]'%m[3:]
				elif re_novar.match(m):
					m='[tsk.outputs%s]'%m[3:]
				elif m[:3]not in('tsk','gen','bld'):
					dvars.append(meth[1:])
					m='%r'%m
				app('" ".join(tsk.colon(%r, %s))'%(var,m))
			else:
				app('%s%s'%(var,meth))
		else:
			if var not in dvars:
				dvars.append(var)
			app("p('%s')"%var)
	if parm:parm="%% (%s) "%(',\n\t\t'.join(parm))
	else:parm=''
	c=COMPILE_TEMPLATE_SHELL%(line,parm)
	Logs.debug('action: %s'%c.strip().splitlines())
	return(funex(c),dvars)
def compile_fun_noshell(line):
	extr=[]
	def repl(match):
		g=match.group
		if g('dollar'):return"$"
		elif g('backslash'):return'\\'
		elif g('subst'):extr.append((g('var'),g('code')));return"<<|@|>>"
		return None
	line2=reg_act.sub(repl,line)
	params=line2.split('<<|@|>>')
	assert(extr)
	buf=[]
	dvars=[]
	app=buf.append
	for x in range(len(extr)):
		params[x]=params[x].strip()
		if params[x]:
			app("lst.extend(%r)"%params[x].split())
		(var,meth)=extr[x]
		if var=='SRC':
			if meth:app('lst.append(tsk.inputs%s)'%meth)
			else:app("lst.extend([a.path_from(cwdx) for a in tsk.inputs])")
		elif var=='TGT':
			if meth:app('lst.append(tsk.outputs%s)'%meth)
			else:app("lst.extend([a.path_from(cwdx) for a in tsk.outputs])")
		elif meth:
			if meth.startswith(':'):
				if not var in dvars:
					dvars.append(var)
				m=meth[1:]
				if m=='SRC':
					m='[a.path_from(cwdx) for a in tsk.inputs]'
				elif m=='TGT':
					m='[a.path_from(cwdx) for a in tsk.outputs]'
				elif re_novar.match(m):
					m='[tsk.inputs%s]'%m[3:]
				elif re_novar.match(m):
					m='[tsk.outputs%s]'%m[3:]
				elif m[:3]not in('tsk','gen','bld'):
					dvars.append(m)
					m='%r'%m
				app('lst.extend(tsk.colon(%r, %s))'%(var,m))
			else:
				app('lst.extend(gen.to_list(%s%s))'%(var,meth))
		else:
			app('lst.extend(to_list(env[%r]))'%var)
			if not var in dvars:
				dvars.append(var)
	if extr:
		if params[-1]:
			app("lst.extend(%r)"%params[-1].split())
	fun=COMPILE_TEMPLATE_NOSHELL%"\n\t".join(buf)
	Logs.debug('action: %s'%fun.strip().splitlines())
	return(funex(fun),dvars)
def compile_fun(line,shell=False):
	if isinstance(line,str):
		if line.find('<')>0 or line.find('>')>0 or line.find('&&')>0:
			shell=True
	else:
		dvars_lst=[]
		funs_lst=[]
		for x in line:
			if isinstance(x,str):
				fun,dvars=compile_fun(x,shell)
				dvars_lst+=dvars
				funs_lst.append(fun)
			else:
				funs_lst.append(x)
		def composed_fun(task):
			for x in funs_lst:
				ret=x(task)
				if ret:
					return ret
			return None
		return composed_fun,dvars
	if shell:
		return compile_fun_shell(line)
	else:
		return compile_fun_noshell(line)
def task_factory(name,func=None,vars=None,color='GREEN',ext_in=[],ext_out=[],before=[],after=[],shell=False,scan=None):
	params={'vars':vars or[],'color':color,'name':name,'ext_in':Utils.to_list(ext_in),'ext_out':Utils.to_list(ext_out),'before':Utils.to_list(before),'after':Utils.to_list(after),'shell':shell,'scan':scan,}
	if isinstance(func,str)or isinstance(func,tuple):
		params['run_str']=func
	else:
		params['run']=func
	cls=type(Task)(name,(Task,),params)
	global classes
	classes[name]=cls
	return cls
def always_run(cls):
	old=cls.runnable_status
	def always(self):
		ret=old(self)
		if ret==SKIP_ME:
			ret=RUN_ME
		return ret
	cls.runnable_status=always
	return cls
def update_outputs(cls):
	old_post_run=cls.post_run
	def post_run(self):
		old_post_run(self)
		for node in self.outputs:
			node.sig=node.cache_sig=Utils.h_file(node.abspath())
			self.generator.bld.task_sigs[node.abspath()]=self.uid()
	cls.post_run=post_run
	old_runnable_status=cls.runnable_status
	def runnable_status(self):
		status=old_runnable_status(self)
		if status!=RUN_ME:
			return status
		try:
			bld=self.generator.bld
			prev_sig=bld.task_sigs[self.uid()]
			if prev_sig==self.signature():
				for x in self.outputs:
					if not x.is_child_of(bld.bldnode):
						x.sig=Utils.h_file(x.abspath())
					if not x.sig or bld.task_sigs[x.abspath()]!=self.uid():
						return RUN_ME
				return SKIP_ME
		except OSError:
			pass
		except IOError:
			pass
		except KeyError:
			pass
		except IndexError:
			pass
		except AttributeError:
			pass
		return RUN_ME
	cls.runnable_status=runnable_status
	return cls
