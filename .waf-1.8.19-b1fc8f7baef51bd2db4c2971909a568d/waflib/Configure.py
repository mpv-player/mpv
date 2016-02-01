#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import os,shlex,sys,time,re,shutil
from waflib import ConfigSet,Utils,Options,Logs,Context,Build,Errors
BREAK='break'
CONTINUE='continue'
WAF_CONFIG_LOG='config.log'
autoconfig=False
conf_template='''# project %(app)s configured on %(now)s by
# waf %(wafver)s (abi %(abi)s, python %(pyver)x on %(systype)s)
# using %(args)s
#'''
class ConfigurationContext(Context.Context):
	'''configures the project'''
	cmd='configure'
	error_handlers=[]
	def __init__(self,**kw):
		super(ConfigurationContext,self).__init__(**kw)
		self.environ=dict(os.environ)
		self.all_envs={}
		self.top_dir=None
		self.out_dir=None
		self.tools=[]
		self.hash=0
		self.files=[]
		self.tool_cache=[]
		self.setenv('')
	def setenv(self,name,env=None):
		if name not in self.all_envs or env:
			if not env:
				env=ConfigSet.ConfigSet()
				self.prepare_env(env)
			else:
				env=env.derive()
			self.all_envs[name]=env
		self.variant=name
	def get_env(self):
		return self.all_envs[self.variant]
	def set_env(self,val):
		self.all_envs[self.variant]=val
	env=property(get_env,set_env)
	def init_dirs(self):
		top=self.top_dir
		if not top:
			top=Options.options.top
		if not top:
			top=getattr(Context.g_module,Context.TOP,None)
		if not top:
			top=self.path.abspath()
		top=os.path.abspath(top)
		self.srcnode=(os.path.isabs(top)and self.root or self.path).find_dir(top)
		assert(self.srcnode)
		out=self.out_dir
		if not out:
			out=Options.options.out
		if not out:
			out=getattr(Context.g_module,Context.OUT,None)
		if not out:
			out=Options.lockfile.replace('.lock-waf_%s_'%sys.platform,'').replace('.lock-waf','')
		out=os.path.realpath(out)
		self.bldnode=(os.path.isabs(out)and self.root or self.path).make_node(out)
		self.bldnode.mkdir()
		if not os.path.isdir(self.bldnode.abspath()):
			conf.fatal('Could not create the build directory %s'%self.bldnode.abspath())
	def execute(self):
		self.init_dirs()
		self.cachedir=self.bldnode.make_node(Build.CACHE_DIR)
		self.cachedir.mkdir()
		path=os.path.join(self.bldnode.abspath(),WAF_CONFIG_LOG)
		self.logger=Logs.make_logger(path,'cfg')
		app=getattr(Context.g_module,'APPNAME','')
		if app:
			ver=getattr(Context.g_module,'VERSION','')
			if ver:
				app="%s (%s)"%(app,ver)
		params={'now':time.ctime(),'pyver':sys.hexversion,'systype':sys.platform,'args':" ".join(sys.argv),'wafver':Context.WAFVERSION,'abi':Context.ABI,'app':app}
		self.to_log(conf_template%params)
		self.msg('Setting top to',self.srcnode.abspath())
		self.msg('Setting out to',self.bldnode.abspath())
		if id(self.srcnode)==id(self.bldnode):
			Logs.warn('Setting top == out (remember to use "update_outputs")')
		elif id(self.path)!=id(self.srcnode):
			if self.srcnode.is_child_of(self.path):
				Logs.warn('Are you certain that you do not want to set top="." ?')
		super(ConfigurationContext,self).execute()
		self.store()
		Context.top_dir=self.srcnode.abspath()
		Context.out_dir=self.bldnode.abspath()
		env=ConfigSet.ConfigSet()
		env['argv']=sys.argv
		env['options']=Options.options.__dict__
		env.run_dir=Context.run_dir
		env.top_dir=Context.top_dir
		env.out_dir=Context.out_dir
		env['hash']=self.hash
		env['files']=self.files
		env['environ']=dict(self.environ)
		if not self.env.NO_LOCK_IN_RUN and not getattr(Options.options,'no_lock_in_run'):
			env.store(os.path.join(Context.run_dir,Options.lockfile))
		if not self.env.NO_LOCK_IN_TOP and not getattr(Options.options,'no_lock_in_top'):
			env.store(os.path.join(Context.top_dir,Options.lockfile))
		if not self.env.NO_LOCK_IN_OUT and not getattr(Options.options,'no_lock_in_out'):
			env.store(os.path.join(Context.out_dir,Options.lockfile))
	def prepare_env(self,env):
		if not env.PREFIX:
			if Options.options.prefix or Utils.is_win32:
				env.PREFIX=Utils.sane_path(Options.options.prefix)
			else:
				env.PREFIX=''
		if not env.BINDIR:
			if Options.options.bindir:
				env.BINDIR=Utils.sane_path(Options.options.bindir)
			else:
				env.BINDIR=Utils.subst_vars('${PREFIX}/bin',env)
		if not env.LIBDIR:
			if Options.options.libdir:
				env.LIBDIR=Utils.sane_path(Options.options.libdir)
			else:
				env.LIBDIR=Utils.subst_vars('${PREFIX}/lib%s'%Utils.lib64(),env)
	def store(self):
		n=self.cachedir.make_node('build.config.py')
		n.write('version = 0x%x\ntools = %r\n'%(Context.HEXVERSION,self.tools))
		if not self.all_envs:
			self.fatal('nothing to store in the configuration context!')
		for key in self.all_envs:
			tmpenv=self.all_envs[key]
			tmpenv.store(os.path.join(self.cachedir.abspath(),key+Build.CACHE_SUFFIX))
	def load(self,input,tooldir=None,funs=None,with_sys_path=True):
		tools=Utils.to_list(input)
		if tooldir:tooldir=Utils.to_list(tooldir)
		for tool in tools:
			mag=(tool,id(self.env),tooldir,funs)
			if mag in self.tool_cache:
				self.to_log('(tool %s is already loaded, skipping)'%tool)
				continue
			self.tool_cache.append(mag)
			module=None
			try:
				module=Context.load_tool(tool,tooldir,ctx=self,with_sys_path=with_sys_path)
			except ImportError ,e:
				self.fatal('Could not load the Waf tool %r from %r\n%s'%(tool,sys.path,e))
			except Exception ,e:
				self.to_log('imp %r (%r & %r)'%(tool,tooldir,funs))
				self.to_log(Utils.ex_stack())
				raise
			if funs is not None:
				self.eval_rules(funs)
			else:
				func=getattr(module,'configure',None)
				if func:
					if type(func)is type(Utils.readf):func(self)
					else:self.eval_rules(func)
			self.tools.append({'tool':tool,'tooldir':tooldir,'funs':funs})
	def post_recurse(self,node):
		super(ConfigurationContext,self).post_recurse(node)
		self.hash=Utils.h_list((self.hash,node.read('rb')))
		self.files.append(node.abspath())
	def eval_rules(self,rules):
		self.rules=Utils.to_list(rules)
		for x in self.rules:
			f=getattr(self,x)
			if not f:self.fatal("No such method '%s'."%x)
			try:
				f()
			except Exception ,e:
				ret=self.err_handler(x,e)
				if ret==BREAK:
					break
				elif ret==CONTINUE:
					continue
				else:
					raise
	def err_handler(self,fun,error):
		pass
def conf(f):
	def fun(*k,**kw):
		mandatory=True
		if'mandatory'in kw:
			mandatory=kw['mandatory']
			del kw['mandatory']
		try:
			return f(*k,**kw)
		except Errors.ConfigurationError:
			if mandatory:
				raise
	fun.__name__=f.__name__
	setattr(ConfigurationContext,f.__name__,fun)
	setattr(Build.BuildContext,f.__name__,fun)
	return f
@conf
def add_os_flags(self,var,dest=None,dup=True):
	try:
		flags=shlex.split(self.environ[var])
	except KeyError:
		return
	if dup or''.join(flags)not in''.join(Utils.to_list(self.env[dest or var])):
		self.env.append_value(dest or var,flags)
@conf
def cmd_to_list(self,cmd):
	if isinstance(cmd,str)and cmd.find(' '):
		try:
			os.stat(cmd)
		except OSError:
			return shlex.split(cmd)
		else:
			return[cmd]
	return cmd
@conf
def check_waf_version(self,mini='1.7.99',maxi='1.9.0',**kw):
	self.start_msg('Checking for waf version in %s-%s'%(str(mini),str(maxi)),**kw)
	ver=Context.HEXVERSION
	if Utils.num2ver(mini)>ver:
		self.fatal('waf version should be at least %r (%r found)'%(Utils.num2ver(mini),ver))
	if Utils.num2ver(maxi)<ver:
		self.fatal('waf version should be at most %r (%r found)'%(Utils.num2ver(maxi),ver))
	self.end_msg('ok',**kw)
@conf
def find_file(self,filename,path_list=[]):
	for n in Utils.to_list(filename):
		for d in Utils.to_list(path_list):
			p=os.path.expanduser(os.path.join(d,n))
			if os.path.exists(p):
				return p
	self.fatal('Could not find %r'%filename)
@conf
def find_program(self,filename,**kw):
	exts=kw.get('exts',Utils.is_win32 and'.exe,.com,.bat,.cmd'or',.sh,.pl,.py')
	environ=kw.get('environ',getattr(self,'environ',os.environ))
	ret=''
	filename=Utils.to_list(filename)
	msg=kw.get('msg',', '.join(filename))
	var=kw.get('var','')
	if not var:
		var=re.sub(r'[-.]','_',filename[0].upper())
	path_list=kw.get('path_list','')
	if path_list:
		path_list=Utils.to_list(path_list)
	else:
		path_list=environ.get('PATH','').split(os.pathsep)
	if var in environ:
		filename=environ[var]
		if os.path.isfile(filename):
			ret=[filename]
		else:
			ret=self.cmd_to_list(filename)
	elif self.env[var]:
		ret=self.env[var]
		ret=self.cmd_to_list(ret)
	else:
		if not ret:
			ret=self.find_binary(filename,exts.split(','),path_list)
		if not ret and Utils.winreg:
			ret=Utils.get_registry_app_path(Utils.winreg.HKEY_CURRENT_USER,filename)
		if not ret and Utils.winreg:
			ret=Utils.get_registry_app_path(Utils.winreg.HKEY_LOCAL_MACHINE,filename)
		ret=self.cmd_to_list(ret)
	if ret:
		if len(ret)==1:
			retmsg=ret[0]
		else:
			retmsg=ret
	else:
		retmsg=False
	self.msg("Checking for program '%s'"%msg,retmsg,**kw)
	if not kw.get('quiet',None):
		self.to_log('find program=%r paths=%r var=%r -> %r'%(filename,path_list,var,ret))
	if not ret:
		self.fatal(kw.get('errmsg','')or'Could not find the program %r'%filename)
	interpreter=kw.get('interpreter',None)
	if interpreter is None:
		if not Utils.check_exe(ret[0],env=environ):
			self.fatal('Program %r is not executable'%ret)
		self.env[var]=ret
	else:
		self.env[var]=self.env[interpreter]+ret
	return ret
@conf
def find_binary(self,filenames,exts,paths):
	for f in filenames:
		for ext in exts:
			exe_name=f+ext
			if os.path.isabs(exe_name):
				if os.path.isfile(exe_name):
					return exe_name
			else:
				for path in paths:
					x=os.path.expanduser(os.path.join(path,exe_name))
					if os.path.isfile(x):
						return x
	return None
@conf
def run_build(self,*k,**kw):
	lst=[str(v)for(p,v)in kw.items()if p!='env']
	h=Utils.h_list(lst)
	dir=self.bldnode.abspath()+os.sep+(not Utils.is_win32 and'.'or'')+'conf_check_'+Utils.to_hex(h)
	try:
		os.makedirs(dir)
	except OSError:
		pass
	try:
		os.stat(dir)
	except OSError:
		self.fatal('cannot use the configuration test folder %r'%dir)
	cachemode=getattr(Options.options,'confcache',None)
	if cachemode==1:
		try:
			proj=ConfigSet.ConfigSet(os.path.join(dir,'cache_run_build'))
		except OSError:
			pass
		except IOError:
			pass
		else:
			ret=proj['cache_run_build']
			if isinstance(ret,str)and ret.startswith('Test does not build'):
				self.fatal(ret)
			return ret
	bdir=os.path.join(dir,'testbuild')
	if not os.path.exists(bdir):
		os.makedirs(bdir)
	self.test_bld=bld=Build.BuildContext(top_dir=dir,out_dir=bdir)
	bld.init_dirs()
	bld.progress_bar=0
	bld.targets='*'
	bld.logger=self.logger
	bld.all_envs.update(self.all_envs)
	bld.env=kw['env']
	bld.kw=kw
	bld.conf=self
	kw['build_fun'](bld)
	ret=-1
	try:
		try:
			bld.compile()
		except Errors.WafError:
			ret='Test does not build: %s'%Utils.ex_stack()
			self.fatal(ret)
		else:
			ret=getattr(bld,'retval',0)
	finally:
		if cachemode==1:
			proj=ConfigSet.ConfigSet()
			proj['cache_run_build']=ret
			proj.store(os.path.join(dir,'cache_run_build'))
		else:
			shutil.rmtree(dir)
	return ret
@conf
def ret_msg(self,msg,args):
	if isinstance(msg,str):
		return msg
	return msg(args)
@conf
def test(self,*k,**kw):
	if not'env'in kw:
		kw['env']=self.env.derive()
	if kw.get('validate',None):
		kw['validate'](kw)
	self.start_msg(kw['msg'],**kw)
	ret=None
	try:
		ret=self.run_build(*k,**kw)
	except self.errors.ConfigurationError:
		self.end_msg(kw['errmsg'],'YELLOW',**kw)
		if Logs.verbose>1:
			raise
		else:
			self.fatal('The configuration failed')
	else:
		kw['success']=ret
	if kw.get('post_check',None):
		ret=kw['post_check'](kw)
	if ret:
		self.end_msg(kw['errmsg'],'YELLOW',**kw)
		self.fatal('The configuration failed %r'%ret)
	else:
		self.end_msg(self.ret_msg(kw['okmsg'],kw),**kw)
	return ret
