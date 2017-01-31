#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import re
from waflib import Utils
from waflib.Tools import fc,fc_config,fc_scan,ar
from waflib.Configure import conf
@conf
def find_ifort(conf):
	fc=conf.find_program('ifort',var='FC')
	conf.get_ifort_version(fc)
	conf.env.FC_NAME='IFORT'
@conf
def ifort_modifier_win32(self):
	v=self.env
	v.IFORT_WIN32=True
	v.FCSTLIB_MARKER=''
	v.FCSHLIB_MARKER=''
	v.FCLIB_ST=v.FCSTLIB_ST='%s.lib'
	v.FCLIBPATH_ST=v.STLIBPATH_ST='/LIBPATH:%s'
	v.FCINCPATH_ST='/I%s'
	v.FCDEFINES_ST='/D%s'
	v.fcprogram_PATTERN=v.fcprogram_test_PATTERN='%s.exe'
	v.fcshlib_PATTERN='%s.dll'
	v.fcstlib_PATTERN=v.implib_PATTERN='%s.lib'
	v.FCLNK_TGT_F='/out:'
	v.FC_TGT_F=['/c','/o','']
	v.FCFLAGS_fcshlib=''
	v.LINKFLAGS_fcshlib='/DLL'
	v.AR_TGT_F='/out:'
	v.IMPLIB_ST='/IMPLIB:%s'
	v.append_value('LINKFLAGS','/subsystem:console')
	if v.IFORT_MANIFEST:
		v.append_value('LINKFLAGS',['/MANIFEST'])
@conf
def ifort_modifier_darwin(conf):
	fc_config.fortran_modifier_darwin(conf)
@conf
def ifort_modifier_platform(conf):
	dest_os=conf.env.DEST_OS or Utils.unversioned_sys_platform()
	ifort_modifier_func=getattr(conf,'ifort_modifier_'+dest_os,None)
	if ifort_modifier_func:
		ifort_modifier_func()
@conf
def get_ifort_version(conf,fc):
	version_re=re.compile(r"\bIntel\b.*\bVersion\s*(?P<major>\d*)\.(?P<minor>\d*)",re.I).search
	if Utils.is_win32:
		cmd=fc
	else:
		cmd=fc+['-logo']
	out,err=fc_config.getoutput(conf,cmd,stdin=False)
	match=version_re(out)or version_re(err)
	if not match:
		conf.fatal('cannot determine ifort version.')
	k=match.groupdict()
	conf.env['FC_VERSION']=(k['major'],k['minor'])
def configure(conf):
	if Utils.is_win32:
		compiler,version,path,includes,libdirs,arch=conf.detect_ifort(True)
		v=conf.env
		v.DEST_CPU=arch
		v.PATH=path
		v.INCLUDES=includes
		v.LIBPATH=libdirs
		v.MSVC_COMPILER=compiler
		try:
			v.MSVC_VERSION=float(version)
		except Exception:
			raise
			v.MSVC_VERSION=float(version[:-3])
		conf.find_ifort_win32()
		conf.ifort_modifier_win32()
	else:
		conf.find_ifort()
		conf.find_program('xiar',var='AR')
		conf.find_ar()
		conf.fc_flags()
		conf.fc_add_flags()
		conf.ifort_modifier_platform()
import os,sys,re,tempfile
from waflib import Task,Logs,Options,Errors
from waflib.Logs import debug,warn
from waflib.TaskGen import after_method,feature
from waflib.Configure import conf
from waflib.Tools import ccroot,ar,winres
all_ifort_platforms=[('intel64','amd64'),('em64t','amd64'),('ia32','x86'),('Itanium','ia64')]
@conf
def gather_ifort_versions(conf,versions):
	version_pattern=re.compile('^...?.?\....?.?')
	try:
		all_versions=Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE,'SOFTWARE\\Wow6432node\\Intel\\Compilers\\Fortran')
	except WindowsError:
		try:
			all_versions=Utils.winreg.OpenKey(Utils.winreg.HKEY_LOCAL_MACHINE,'SOFTWARE\\Intel\\Compilers\\Fortran')
		except WindowsError:
			return
	index=0
	while 1:
		try:
			version=Utils.winreg.EnumKey(all_versions,index)
		except WindowsError:
			break
		index=index+1
		if not version_pattern.match(version):
			continue
		targets=[]
		for target,arch in all_ifort_platforms:
			try:
				if target=='intel64':targetDir='EM64T_NATIVE'
				else:targetDir=target
				Utils.winreg.OpenKey(all_versions,version+'\\'+targetDir)
				icl_version=Utils.winreg.OpenKey(all_versions,version)
				path,type=Utils.winreg.QueryValueEx(icl_version,'ProductDir')
				batch_file=os.path.join(path,'bin','iclvars.bat')
				if os.path.isfile(batch_file):
					try:
						targets.append((target,(arch,get_compiler_env(conf,'intel',version,target,batch_file))))
					except conf.errors.ConfigurationError:
						pass
			except WindowsError:
				pass
		for target,arch in all_ifort_platforms:
			try:
				icl_version=Utils.winreg.OpenKey(all_versions,version+'\\'+target)
				path,type=Utils.winreg.QueryValueEx(icl_version,'ProductDir')
				batch_file=os.path.join(path,'bin','iclvars.bat')
				if os.path.isfile(batch_file):
					try:
						targets.append((target,(arch,get_compiler_env(conf,'intel',version,target,batch_file))))
					except conf.errors.ConfigurationError:
						pass
			except WindowsError:
				continue
		major=version[0:2]
		versions.append(('intel '+major,targets))
def setup_ifort(conf,versions,arch=False):
	platforms=Utils.to_list(conf.env['MSVC_TARGETS'])or[i for i,j in all_ifort_platforms]
	desired_versions=conf.env['MSVC_VERSIONS']or[v for v,_ in versions][::-1]
	versiondict=dict(versions)
	for version in desired_versions:
		try:
			targets=dict(versiondict[version])
			for target in platforms:
				try:
					try:
						realtarget,(p1,p2,p3)=targets[target]
					except conf.errors.ConfigurationError:
						del(targets[target])
					else:
						compiler,revision=version.rsplit(' ',1)
						if arch:
							return compiler,revision,p1,p2,p3,realtarget
						else:
							return compiler,revision,p1,p2,p3
				except KeyError:
					continue
		except KeyError:
			continue
	conf.fatal('msvc: Impossible to find a valid architecture for building (in setup_ifort)')
@conf
def get_ifort_version_win32(conf,compiler,version,target,vcvars):
	try:
		conf.msvc_cnt+=1
	except AttributeError:
		conf.msvc_cnt=1
	batfile=conf.bldnode.make_node('waf-print-msvc-%d.bat'%conf.msvc_cnt)
	batfile.write("""@echo off
set INCLUDE=
set LIB=
call "%s" %s
echo PATH=%%PATH%%
echo INCLUDE=%%INCLUDE%%
echo LIB=%%LIB%%;%%LIBPATH%%
"""%(vcvars,target))
	sout=conf.cmd_and_log(['cmd.exe','/E:on','/V:on','/C',batfile.abspath()])
	batfile.delete()
	lines=sout.splitlines()
	if not lines[0]:
		lines.pop(0)
	MSVC_PATH=MSVC_INCDIR=MSVC_LIBDIR=None
	for line in lines:
		if line.startswith('PATH='):
			path=line[5:]
			MSVC_PATH=path.split(';')
		elif line.startswith('INCLUDE='):
			MSVC_INCDIR=[i for i in line[8:].split(';')if i]
		elif line.startswith('LIB='):
			MSVC_LIBDIR=[i for i in line[4:].split(';')if i]
	if None in(MSVC_PATH,MSVC_INCDIR,MSVC_LIBDIR):
		conf.fatal('msvc: Could not find a valid architecture for building (get_ifort_version_win32)')
	env=dict(os.environ)
	env.update(PATH=path)
	compiler_name,linker_name,lib_name=_get_prog_names(conf,compiler)
	fc=conf.find_program(compiler_name,path_list=MSVC_PATH)
	if'CL'in env:
		del(env['CL'])
	try:
		try:
			conf.cmd_and_log(fc+['/help'],env=env)
		except UnicodeError:
			st=Utils.ex_stack()
			if conf.logger:
				conf.logger.error(st)
			conf.fatal('msvc: Unicode error - check the code page?')
		except Exception ,e:
			debug('msvc: get_ifort_version: %r %r %r -> failure %s'%(compiler,version,target,str(e)))
			conf.fatal('msvc: cannot run the compiler in get_ifort_version (run with -v to display errors)')
		else:
			debug('msvc: get_ifort_version: %r %r %r -> OK',compiler,version,target)
	finally:
		conf.env[compiler_name]=''
	return(MSVC_PATH,MSVC_INCDIR,MSVC_LIBDIR)
def get_compiler_env(conf,compiler,version,bat_target,bat,select=None):
	lazy=getattr(Options.options,'msvc_lazy',True)
	if conf.env.MSVC_LAZY_AUTODETECT is False:
		lazy=False
	def msvc_thunk():
		vs=conf.get_ifort_version_win32(compiler,version,bat_target,bat)
		if select:
			return select(vs)
		else:
			return vs
	return lazytup(msvc_thunk,lazy,([],[],[]))
class lazytup(object):
	def __init__(self,fn,lazy=True,default=None):
		self.fn=fn
		self.default=default
		if not lazy:
			self.evaluate()
	def __len__(self):
		self.evaluate()
		return len(self.value)
	def __iter__(self):
		self.evaluate()
		for i,v in enumerate(self.value):
			yield v
	def __getitem__(self,i):
		self.evaluate()
		return self.value[i]
	def __repr__(self):
		if hasattr(self,'value'):
			return repr(self.value)
		elif self.default:
			return repr(self.default)
		else:
			self.evaluate()
			return repr(self.value)
	def evaluate(self):
		if hasattr(self,'value'):
			return
		self.value=self.fn()
@conf
def get_ifort_versions(conf,eval_and_save=True):
	if conf.env['IFORT_INSTALLED_VERSIONS']:
		return conf.env['IFORT_INSTALLED_VERSIONS']
	lst=[]
	conf.gather_ifort_versions(lst)
	if eval_and_save:
		def checked_target(t):
			target,(arch,paths)=t
			try:
				paths.evaluate()
			except conf.errors.ConfigurationError:
				return None
			else:
				return t
		lst=[(version,list(filter(checked_target,targets)))for version,targets in lst]
		conf.env['IFORT_INSTALLED_VERSIONS']=lst
	return lst
@conf
def detect_ifort(conf,arch=False):
	versions=get_ifort_versions(conf,False)
	return setup_ifort(conf,versions,arch)
def _get_prog_names(conf,compiler):
	if compiler=='intel':
		compiler_name='ifort'
		linker_name='XILINK'
		lib_name='XILIB'
	else:
		compiler_name='CL'
		linker_name='LINK'
		lib_name='LIB'
	return compiler_name,linker_name,lib_name
@conf
def find_ifort_win32(conf):
	v=conf.env
	path=v['PATH']
	compiler=v['MSVC_COMPILER']
	version=v['MSVC_VERSION']
	compiler_name,linker_name,lib_name=_get_prog_names(conf,compiler)
	v.IFORT_MANIFEST=(compiler=='intel'and version>=11)
	fc=conf.find_program(compiler_name,var='FC',path_list=path)
	env=dict(conf.environ)
	if path:env.update(PATH=';'.join(path))
	if not conf.cmd_and_log(fc+['/nologo','/help'],env=env):
		conf.fatal('not intel fortran compiler could not be identified')
	v['FC_NAME']='IFORT'
	if not v['LINK_FC']:
		conf.find_program(linker_name,var='LINK_FC',path_list=path,mandatory=True)
	if not v['AR']:
		conf.find_program(lib_name,path_list=path,var='AR',mandatory=True)
		v['ARFLAGS']=['/NOLOGO']
	if v.IFORT_MANIFEST:
		conf.find_program('MT',path_list=path,var='MT')
		v['MTFLAGS']=['/NOLOGO']
	try:
		conf.load('winres')
	except Errors.WafError:
		warn('Resource compiler not found. Compiling resource file is disabled')
@after_method('apply_link')
@feature('fc')
def apply_flags_ifort(self):
	if not self.env.IFORT_WIN32 or not getattr(self,'link_task',None):
		return
	is_static=isinstance(self.link_task,ccroot.stlink_task)
	subsystem=getattr(self,'subsystem','')
	if subsystem:
		subsystem='/subsystem:%s'%subsystem
		flags=is_static and'ARFLAGS'or'LINKFLAGS'
		self.env.append_value(flags,subsystem)
	if not is_static:
		for f in self.env.LINKFLAGS:
			d=f.lower()
			if d[1:]=='debug':
				pdbnode=self.link_task.outputs[0].change_ext('.pdb')
				self.link_task.outputs.append(pdbnode)
				if getattr(self,'install_task',None):
					self.pdb_install_task=self.bld.install_files(self.install_task.dest,pdbnode,env=self.env)
				break
@feature('fcprogram','fcshlib','fcprogram_test')
@after_method('apply_link')
def apply_manifest_ifort(self):
	if self.env.IFORT_WIN32 and getattr(self,'link_task',None):
		self.link_task.env.FC=self.env.LINK_FC
	if self.env.IFORT_WIN32 and self.env.IFORT_MANIFEST and getattr(self,'link_task',None):
		out_node=self.link_task.outputs[0]
		man_node=out_node.parent.find_or_declare(out_node.name+'.manifest')
		self.link_task.outputs.append(man_node)
		self.link_task.do_manifest=True
def exec_mf(self):
	env=self.env
	mtool=env['MT']
	if not mtool:
		return 0
	self.do_manifest=False
	outfile=self.outputs[0].abspath()
	manifest=None
	for out_node in self.outputs:
		if out_node.name.endswith('.manifest'):
			manifest=out_node.abspath()
			break
	if manifest is None:
		return 0
	mode=''
	if'fcprogram'in self.generator.features or'fcprogram_test'in self.generator.features:
		mode='1'
	elif'fcshlib'in self.generator.features:
		mode='2'
	debug('msvc: embedding manifest in mode %r'%mode)
	lst=[]+mtool
	lst.extend(Utils.to_list(env['MTFLAGS']))
	lst.extend(['-manifest',manifest])
	lst.append('-outputresource:%s;%s'%(outfile,mode))
	return self.exec_command(lst)
def quote_response_command(self,flag):
	if flag.find(' ')>-1:
		for x in('/LIBPATH:','/IMPLIB:','/OUT:','/I'):
			if flag.startswith(x):
				flag='%s"%s"'%(x,flag[len(x):])
				break
		else:
			flag='"%s"'%flag
	return flag
def exec_response_command(self,cmd,**kw):
	try:
		tmp=None
		if sys.platform.startswith('win')and isinstance(cmd,list)and len(' '.join(cmd))>=8192:
			program=cmd[0]
			cmd=[self.quote_response_command(x)for x in cmd]
			(fd,tmp)=tempfile.mkstemp()
			os.write(fd,'\r\n'.join(i.replace('\\','\\\\')for i in cmd[1:]))
			os.close(fd)
			cmd=[program,'@'+tmp]
		ret=super(self.__class__,self).exec_command(cmd,**kw)
	finally:
		if tmp:
			try:
				os.remove(tmp)
			except OSError:
				pass
	return ret
def exec_command_ifort(self,*k,**kw):
	if isinstance(k[0],list):
		lst=[]
		carry=''
		for a in k[0]:
			if a=='/Fo'or a=='/doc'or a[-1]==':':
				carry=a
			else:
				lst.append(carry+a)
				carry=''
		k=[lst]
	if self.env['PATH']:
		env=dict(self.env.env or os.environ)
		env.update(PATH=';'.join(self.env['PATH']))
		kw['env']=env
	if not'cwd'in kw:
		kw['cwd']=self.generator.bld.variant_dir
	ret=self.exec_response_command(k[0],**kw)
	if not ret and getattr(self,'do_manifest',None):
		ret=self.exec_mf()
	return ret
def wrap_class(class_name):
	cls=Task.classes.get(class_name,None)
	if not cls:
		return None
	derived_class=type(class_name,(cls,),{})
	def exec_command(self,*k,**kw):
		if self.env.IFORT_WIN32:
			return self.exec_command_ifort(*k,**kw)
		else:
			return super(derived_class,self).exec_command(*k,**kw)
	derived_class.exec_command=exec_command
	derived_class.exec_response_command=exec_response_command
	derived_class.quote_response_command=quote_response_command
	derived_class.exec_command_ifort=exec_command_ifort
	derived_class.exec_mf=exec_mf
	if hasattr(cls,'hcode'):
		derived_class.hcode=cls.hcode
	return derived_class
for k in'fc fcprogram fcprogram_test fcshlib fcstlib'.split():
	wrap_class(k)
