#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import os,shlex,shutil,traceback,errno,sys,stat
from waflib import Utils,Configure,Logs,Options,ConfigSet,Context,Errors,Build,Node
build_dir_override=None
no_climb_commands=['configure']
default_cmd="build"
def waf_entry_point(current_directory,version,wafdir):
	Logs.init_log()
	if Context.WAFVERSION!=version:
		Logs.error('Waf script %r and library %r do not match (directory %r)'%(version,Context.WAFVERSION,wafdir))
		sys.exit(1)
	if'--version'in sys.argv:
		Context.run_dir=current_directory
		ctx=Context.create_context('options')
		ctx.curdir=current_directory
		ctx.parse_args()
		sys.exit(0)
	if len(sys.argv)>1:
		potential_wscript=os.path.join(current_directory,sys.argv[1])
		if os.path.basename(potential_wscript)=='wscript'and os.path.isfile(potential_wscript):
			current_directory=os.path.normpath(os.path.dirname(potential_wscript))
			sys.argv.pop(1)
	Context.waf_dir=wafdir
	Context.launch_dir=current_directory
	no_climb=os.environ.get('NOCLIMB',None)
	if not no_climb:
		for k in no_climb_commands:
			for y in sys.argv:
				if y.startswith(k):
					no_climb=True
					break
	for i,x in enumerate(sys.argv):
		if x.startswith('--top='):
			Context.run_dir=Context.top_dir=Utils.sane_path(x[6:])
			sys.argv[i]='--top='+Context.run_dir
		if x.startswith('--out='):
			Context.out_dir=Utils.sane_path(x[6:])
			sys.argv[i]='--out='+Context.out_dir
	cur=current_directory
	while cur and not Context.top_dir:
		lst=os.listdir(cur)
		if Options.lockfile in lst:
			env=ConfigSet.ConfigSet()
			try:
				env.load(os.path.join(cur,Options.lockfile))
				ino=os.stat(cur)[stat.ST_INO]
			except Exception:
				pass
			else:
				for x in(env.run_dir,env.top_dir,env.out_dir):
					if Utils.is_win32:
						if cur==x:
							load=True
							break
					else:
						try:
							ino2=os.stat(x)[stat.ST_INO]
						except OSError:
							pass
						else:
							if ino==ino2:
								load=True
								break
				else:
					Logs.warn('invalid lock file in %s'%cur)
					load=False
				if load:
					Context.run_dir=env.run_dir
					Context.top_dir=env.top_dir
					Context.out_dir=env.out_dir
					break
		if not Context.run_dir:
			if Context.WSCRIPT_FILE in lst:
				Context.run_dir=cur
		next=os.path.dirname(cur)
		if next==cur:
			break
		cur=next
		if no_climb:
			break
	if not Context.run_dir:
		if'-h'in sys.argv or'--help'in sys.argv:
			Logs.warn('No wscript file found: the help message may be incomplete')
			Context.run_dir=current_directory
			ctx=Context.create_context('options')
			ctx.curdir=current_directory
			ctx.parse_args()
			sys.exit(0)
		Logs.error('Waf: Run from a directory containing a file named %r'%Context.WSCRIPT_FILE)
		sys.exit(1)
	try:
		os.chdir(Context.run_dir)
	except OSError:
		Logs.error('Waf: The folder %r is unreadable'%Context.run_dir)
		sys.exit(1)
	try:
		set_main_module(os.path.normpath(os.path.join(Context.run_dir,Context.WSCRIPT_FILE)))
	except Errors.WafError ,e:
		Logs.pprint('RED',e.verbose_msg)
		Logs.error(str(e))
		sys.exit(1)
	except Exception ,e:
		Logs.error('Waf: The wscript in %r is unreadable'%Context.run_dir,e)
		traceback.print_exc(file=sys.stdout)
		sys.exit(2)
	try:
		run_commands()
	except Errors.WafError ,e:
		if Logs.verbose>1:
			Logs.pprint('RED',e.verbose_msg)
		Logs.error(e.msg)
		sys.exit(1)
	except SystemExit:
		raise
	except Exception ,e:
		traceback.print_exc(file=sys.stdout)
		sys.exit(2)
	except KeyboardInterrupt:
		Logs.pprint('RED','Interrupted')
		sys.exit(68)
def set_main_module(file_path):
	Context.g_module=Context.load_module(file_path)
	Context.g_module.root_path=file_path
	def set_def(obj):
		name=obj.__name__
		if not name in Context.g_module.__dict__:
			setattr(Context.g_module,name,obj)
	for k in(update,dist,distclean,distcheck):
		set_def(k)
	if not'init'in Context.g_module.__dict__:
		Context.g_module.init=Utils.nada
	if not'shutdown'in Context.g_module.__dict__:
		Context.g_module.shutdown=Utils.nada
	if not'options'in Context.g_module.__dict__:
		Context.g_module.options=Utils.nada
def parse_options():
	Context.create_context('options').execute()
	for var in Options.envvars:
		(name,value)=var.split('=',1)
		os.environ[name.strip()]=value
	if not Options.commands:
		Options.commands=[default_cmd]
	Options.commands=[x for x in Options.commands if x!='options']
	Logs.verbose=Options.options.verbose
	if Options.options.zones:
		Logs.zones=Options.options.zones.split(',')
		if not Logs.verbose:
			Logs.verbose=1
	elif Logs.verbose>0:
		Logs.zones=['runner']
	if Logs.verbose>2:
		Logs.zones=['*']
def run_command(cmd_name):
	ctx=Context.create_context(cmd_name)
	ctx.log_timer=Utils.Timer()
	ctx.options=Options.options
	ctx.cmd=cmd_name
	try:
		ctx.execute()
	finally:
		ctx.finalize()
	return ctx
def run_commands():
	parse_options()
	run_command('init')
	while Options.commands:
		cmd_name=Options.commands.pop(0)
		ctx=run_command(cmd_name)
		Logs.info('%r finished successfully (%s)'%(cmd_name,str(ctx.log_timer)))
	run_command('shutdown')
def _can_distclean(name):
	for k in'.o .moc .exe'.split():
		if name.endswith(k):
			return True
	return False
def distclean_dir(dirname):
	for(root,dirs,files)in os.walk(dirname):
		for f in files:
			if _can_distclean(f):
				fname=os.path.join(root,f)
				try:
					os.remove(fname)
				except OSError:
					Logs.warn('Could not remove %r'%fname)
	for x in(Context.DBFILE,'config.log'):
		try:
			os.remove(x)
		except OSError:
			pass
	try:
		shutil.rmtree('c4che')
	except OSError:
		pass
def distclean(ctx):
	'''removes the build directory'''
	lst=os.listdir('.')
	for f in lst:
		if f==Options.lockfile:
			try:
				proj=ConfigSet.ConfigSet(f)
			except IOError:
				Logs.warn('Could not read %r'%f)
				continue
			if proj['out_dir']!=proj['top_dir']:
				try:
					shutil.rmtree(proj['out_dir'])
				except IOError:
					pass
				except OSError ,e:
					if e.errno!=errno.ENOENT:
						Logs.warn('Could not remove %r'%proj['out_dir'])
			else:
				distclean_dir(proj['out_dir'])
			for k in(proj['out_dir'],proj['top_dir'],proj['run_dir']):
				p=os.path.join(k,Options.lockfile)
				try:
					os.remove(p)
				except OSError ,e:
					if e.errno!=errno.ENOENT:
						Logs.warn('Could not remove %r'%p)
		if not Options.commands:
			for x in'.waf-1. waf-1. .waf3-1. waf3-1.'.split():
				if f.startswith(x):
					shutil.rmtree(f,ignore_errors=True)
class Dist(Context.Context):
	'''creates an archive containing the project source code'''
	cmd='dist'
	fun='dist'
	algo='tar.bz2'
	ext_algo={}
	def execute(self):
		self.recurse([os.path.dirname(Context.g_module.root_path)])
		self.archive()
	def archive(self):
		import tarfile
		arch_name=self.get_arch_name()
		try:
			self.base_path
		except AttributeError:
			self.base_path=self.path
		node=self.base_path.make_node(arch_name)
		try:
			node.delete()
		except OSError:
			pass
		files=self.get_files()
		if self.algo.startswith('tar.'):
			tar=tarfile.open(arch_name,'w:'+self.algo.replace('tar.',''))
			for x in files:
				self.add_tar_file(x,tar)
			tar.close()
		elif self.algo=='zip':
			import zipfile
			zip=zipfile.ZipFile(arch_name,'w',compression=zipfile.ZIP_DEFLATED)
			for x in files:
				archive_name=self.get_base_name()+'/'+x.path_from(self.base_path)
				zip.write(x.abspath(),archive_name,zipfile.ZIP_DEFLATED)
			zip.close()
		else:
			self.fatal('Valid algo types are tar.bz2, tar.gz, tar.xz or zip')
		try:
			from hashlib import sha1 as sha
		except ImportError:
			from sha import sha
		try:
			digest=" (sha=%r)"%sha(node.read()).hexdigest()
		except Exception:
			digest=''
		Logs.info('New archive created: %s%s'%(self.arch_name,digest))
	def get_tar_path(self,node):
		return node.abspath()
	def add_tar_file(self,x,tar):
		p=self.get_tar_path(x)
		tinfo=tar.gettarinfo(name=p,arcname=self.get_tar_prefix()+'/'+x.path_from(self.base_path))
		tinfo.uid=0
		tinfo.gid=0
		tinfo.uname='root'
		tinfo.gname='root'
		fu=None
		try:
			fu=open(p,'rb')
			tar.addfile(tinfo,fileobj=fu)
		finally:
			if fu:
				fu.close()
	def get_tar_prefix(self):
		try:
			return self.tar_prefix
		except AttributeError:
			return self.get_base_name()
	def get_arch_name(self):
		try:
			self.arch_name
		except AttributeError:
			self.arch_name=self.get_base_name()+'.'+self.ext_algo.get(self.algo,self.algo)
		return self.arch_name
	def get_base_name(self):
		try:
			self.base_name
		except AttributeError:
			appname=getattr(Context.g_module,Context.APPNAME,'noname')
			version=getattr(Context.g_module,Context.VERSION,'1.0')
			self.base_name=appname+'-'+version
		return self.base_name
	def get_excl(self):
		try:
			return self.excl
		except AttributeError:
			self.excl=Node.exclude_regs+' **/waf-1.8.* **/.waf-1.8* **/waf3-1.8.* **/.waf3-1.8* **/*~ **/*.rej **/*.orig **/*.pyc **/*.pyo **/*.bak **/*.swp **/.lock-w*'
			if Context.out_dir:
				nd=self.root.find_node(Context.out_dir)
				if nd:
					self.excl+=' '+nd.path_from(self.base_path)
			return self.excl
	def get_files(self):
		try:
			files=self.files
		except AttributeError:
			files=self.base_path.ant_glob('**/*',excl=self.get_excl())
		return files
def dist(ctx):
	'''makes a tarball for redistributing the sources'''
	pass
class DistCheck(Dist):
	fun='distcheck'
	cmd='distcheck'
	def execute(self):
		self.recurse([os.path.dirname(Context.g_module.root_path)])
		self.archive()
		self.check()
	def check(self):
		import tempfile,tarfile
		t=None
		try:
			t=tarfile.open(self.get_arch_name())
			for x in t:
				t.extract(x)
		finally:
			if t:
				t.close()
		cfg=[]
		if Options.options.distcheck_args:
			cfg=shlex.split(Options.options.distcheck_args)
		else:
			cfg=[x for x in sys.argv if x.startswith('-')]
		instdir=tempfile.mkdtemp('.inst',self.get_base_name())
		ret=Utils.subprocess.Popen([sys.executable,sys.argv[0],'configure','install','uninstall','--destdir='+instdir]+cfg,cwd=self.get_base_name()).wait()
		if ret:
			raise Errors.WafError('distcheck failed with code %i'%ret)
		if os.path.exists(instdir):
			raise Errors.WafError('distcheck succeeded, but files were left in %s'%instdir)
		shutil.rmtree(self.get_base_name())
def distcheck(ctx):
	'''checks if the project compiles (tarball from 'dist')'''
	pass
def update(ctx):
	lst=Options.options.files
	if lst:
		lst=lst.split(',')
	else:
		path=os.path.join(Context.waf_dir,'waflib','extras')
		lst=[x for x in Utils.listdir(path)if x.endswith('.py')]
	for x in lst:
		tool=x.replace('.py','')
		if not tool:
			continue
		try:
			dl=Configure.download_tool
		except AttributeError:
			ctx.fatal('The command "update" is dangerous; include the tool "use_config" in your project!')
		try:
			dl(tool,force=True,ctx=ctx)
		except Errors.WafError:
			Logs.error('Could not find the tool %r in the remote repository'%x)
		else:
			Logs.warn('Updated %r'%tool)
def autoconfigure(execute_method):
	def execute(self):
		if not Configure.autoconfig:
			return execute_method(self)
		env=ConfigSet.ConfigSet()
		do_config=False
		try:
			env.load(os.path.join(Context.top_dir,Options.lockfile))
		except Exception:
			Logs.warn('Configuring the project')
			do_config=True
		else:
			if env.run_dir!=Context.run_dir:
				do_config=True
			else:
				h=0
				for f in env['files']:
					h=Utils.h_list((h,Utils.readf(f,'rb')))
				do_config=h!=env.hash
		if do_config:
			Options.commands.insert(0,self.cmd)
			Options.commands.insert(0,'configure')
			if Configure.autoconfig=='clobber':
				Options.options.__dict__=env.options
			return
		return execute_method(self)
	return execute
Build.BuildContext.execute=autoconfigure(Build.BuildContext.execute)
