#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import os,sys
from waflib import Utils,Options,Errors,Logs,Task,Node
from waflib.TaskGen import extension,before_method,after_method,feature
from waflib.Configure import conf
FRAG='''
#include <Python.h>
#ifdef __cplusplus
extern "C" {
#endif
	void Py_Initialize(void);
	void Py_Finalize(void);
#ifdef __cplusplus
}
#endif
int main(int argc, char **argv)
{
   (void)argc; (void)argv;
   Py_Initialize();
   Py_Finalize();
   return 0;
}
'''
INST='''
import sys, py_compile
py_compile.compile(sys.argv[1], sys.argv[2], sys.argv[3], True)
'''
DISTUTILS_IMP=['from distutils.sysconfig import get_config_var, get_python_lib']
@before_method('process_source')
@feature('py')
def feature_py(self):
	self.install_path=getattr(self,'install_path','${PYTHONDIR}')
	install_from=getattr(self,'install_from',None)
	if install_from and not isinstance(install_from,Node.Node):
		install_from=self.path.find_dir(install_from)
	self.install_from=install_from
	ver=self.env.PYTHON_VERSION
	if not ver:
		self.bld.fatal('Installing python files requires PYTHON_VERSION, try conf.check_python_version')
	if int(ver.replace('.',''))>31:
		self.install_32=True
@extension('.py')
def process_py(self,node):
	assert(getattr(self,'install_path')),'add features="py"'
	if self.install_path:
		if self.install_from:
			self.bld.install_files(self.install_path,[node],cwd=self.install_from,relative_trick=True)
		else:
			self.bld.install_files(self.install_path,[node],relative_trick=True)
	lst=[]
	if self.env.PYC:
		lst.append('pyc')
	if self.env.PYO:
		lst.append('pyo')
	if self.install_path:
		if self.install_from:
			pyd=Utils.subst_vars("%s/%s"%(self.install_path,node.path_from(self.install_from)),self.env)
		else:
			pyd=Utils.subst_vars("%s/%s"%(self.install_path,node.path_from(self.path)),self.env)
	else:
		pyd=node.abspath()
	for ext in lst:
		if self.env.PYTAG:
			name=node.name[:-3]
			pyobj=node.parent.get_bld().make_node('__pycache__').make_node("%s.%s.%s"%(name,self.env.PYTAG,ext))
			pyobj.parent.mkdir()
		else:
			pyobj=node.change_ext(".%s"%ext)
		tsk=self.create_task(ext,node,pyobj)
		tsk.pyd=pyd
		if self.install_path:
			self.bld.install_files(os.path.dirname(pyd),pyobj,cwd=node.parent.get_bld(),relative_trick=True)
class pyc(Task.Task):
	color='PINK'
	def run(self):
		cmd=[Utils.subst_vars('${PYTHON}',self.env),'-c',INST,self.inputs[0].abspath(),self.outputs[0].abspath(),self.pyd]
		ret=self.generator.bld.exec_command(cmd)
		return ret
class pyo(Task.Task):
	color='PINK'
	def run(self):
		cmd=[Utils.subst_vars('${PYTHON}',self.env),Utils.subst_vars('${PYFLAGS_OPT}',self.env),'-c',INST,self.inputs[0].abspath(),self.outputs[0].abspath(),self.pyd]
		ret=self.generator.bld.exec_command(cmd)
		return ret
@feature('pyext')
@before_method('propagate_uselib_vars','apply_link')
@after_method('apply_bundle')
def init_pyext(self):
	self.uselib=self.to_list(getattr(self,'uselib',[]))
	if not'PYEXT'in self.uselib:
		self.uselib.append('PYEXT')
	self.env.cshlib_PATTERN=self.env.cxxshlib_PATTERN=self.env.macbundle_PATTERN=self.env.pyext_PATTERN
	self.env.fcshlib_PATTERN=self.env.dshlib_PATTERN=self.env.pyext_PATTERN
	try:
		if not self.install_path:
			return
	except AttributeError:
		self.install_path='${PYTHONARCHDIR}'
@feature('pyext')
@before_method('apply_link','apply_bundle')
def set_bundle(self):
	if Utils.unversioned_sys_platform()=='darwin':
		self.mac_bundle=True
@before_method('propagate_uselib_vars')
@feature('pyembed')
def init_pyembed(self):
	self.uselib=self.to_list(getattr(self,'uselib',[]))
	if not'PYEMBED'in self.uselib:
		self.uselib.append('PYEMBED')
@conf
def get_python_variables(self,variables,imports=None):
	if not imports:
		try:
			imports=self.python_imports
		except AttributeError:
			imports=DISTUTILS_IMP
	program=list(imports)
	program.append('')
	for v in variables:
		program.append("print(repr(%s))"%v)
	os_env=dict(os.environ)
	try:
		del os_env['MACOSX_DEPLOYMENT_TARGET']
	except KeyError:
		pass
	try:
		out=self.cmd_and_log(self.env.PYTHON+['-c','\n'.join(program)],env=os_env)
	except Errors.WafError:
		self.fatal('The distutils module is unusable: install "python-devel"?')
	self.to_log(out)
	return_values=[]
	for s in out.splitlines():
		s=s.strip()
		if not s:
			continue
		if s=='None':
			return_values.append(None)
		elif(s[0]=="'"and s[-1]=="'")or(s[0]=='"'and s[-1]=='"'):
			return_values.append(eval(s))
		elif s[0].isdigit():
			return_values.append(int(s))
		else:break
	return return_values
@conf
def test_pyembed(self,mode,msg='Testing pyembed configuration'):
	self.check(header_name='Python.h',define_name='HAVE_PYEMBED',msg=msg,fragment=FRAG,errmsg='Could not build a python embedded interpreter',features='%s %sprogram pyembed'%(mode,mode))
@conf
def test_pyext(self,mode,msg='Testing pyext configuration'):
	self.check(header_name='Python.h',define_name='HAVE_PYEXT',msg=msg,fragment=FRAG,errmsg='Could not build python extensions',features='%s %sshlib pyext'%(mode,mode))
@conf
def python_cross_compile(self,features='pyembed pyext'):
	features=Utils.to_list(features)
	if not('PYTHON_LDFLAGS'in self.environ or'PYTHON_PYEXT_LDFLAGS'in self.environ or'PYTHON_PYEMBED_LDFLAGS'in self.environ):
		return False
	for x in'PYTHON_VERSION PYTAG pyext_PATTERN'.split():
		if not x in self.environ:
			self.fatal('Please set %s in the os environment'%x)
		else:
			self.env[x]=self.environ[x]
	xx=self.env.CXX_NAME and'cxx'or'c'
	if'pyext'in features:
		flags=self.environ.get('PYTHON_PYEXT_LDFLAGS',self.environ.get('PYTHON_LDFLAGS',None))
		if flags is None:
			self.fatal('No flags provided through PYTHON_PYEXT_LDFLAGS as required')
		else:
			self.parse_flags(flags,'PYEXT')
		self.test_pyext(xx)
	if'pyembed'in features:
		flags=self.environ.get('PYTHON_PYEMBED_LDFLAGS',self.environ.get('PYTHON_LDFLAGS',None))
		if flags is None:
			self.fatal('No flags provided through PYTHON_PYEMBED_LDFLAGS as required')
		else:
			self.parse_flags(flags,'PYEMBED')
		self.test_pyembed(xx)
	return True
@conf
def check_python_headers(conf,features='pyembed pyext'):
	features=Utils.to_list(features)
	assert('pyembed'in features)or('pyext'in features),"check_python_headers features must include 'pyembed' and/or 'pyext'"
	env=conf.env
	if not env['CC_NAME']and not env['CXX_NAME']:
		conf.fatal('load a compiler first (gcc, g++, ..)')
	if conf.python_cross_compile(features):
		return
	if not env['PYTHON_VERSION']:
		conf.check_python_version()
	pybin=env.PYTHON
	if not pybin:
		conf.fatal('Could not find the python executable')
	v='prefix SO LDFLAGS LIBDIR LIBPL INCLUDEPY Py_ENABLE_SHARED MACOSX_DEPLOYMENT_TARGET LDSHARED CFLAGS LDVERSION'.split()
	try:
		lst=conf.get_python_variables(["get_config_var('%s') or ''"%x for x in v])
	except RuntimeError:
		conf.fatal("Python development headers not found (-v for details).")
	vals=['%s = %r'%(x,y)for(x,y)in zip(v,lst)]
	conf.to_log("Configuration returned from %r:\n%s\n"%(pybin,'\n'.join(vals)))
	dct=dict(zip(v,lst))
	x='MACOSX_DEPLOYMENT_TARGET'
	if dct[x]:
		env[x]=conf.environ[x]=dct[x]
	env['pyext_PATTERN']='%s'+dct['SO']
	num='.'.join(env['PYTHON_VERSION'].split('.')[:2])
	conf.find_program([''.join(pybin)+'-config','python%s-config'%num,'python-config-%s'%num,'python%sm-config'%num],var='PYTHON_CONFIG',msg="python-config",mandatory=False)
	if env.PYTHON_CONFIG:
		all_flags=[['--cflags','--libs','--ldflags']]
		if sys.hexversion<0x2070000:
			all_flags=[[k]for k in all_flags[0]]
		xx=env.CXX_NAME and'cxx'or'c'
		if'pyembed'in features:
			for flags in all_flags:
				conf.check_cfg(msg='Asking python-config for pyembed %r flags'%' '.join(flags),path=env.PYTHON_CONFIG,package='',uselib_store='PYEMBED',args=flags)
			try:
				conf.test_pyembed(xx)
			except conf.errors.ConfigurationError:
				if dct['Py_ENABLE_SHARED']and dct['LIBDIR']:
					env.append_unique('LIBPATH_PYEMBED',[dct['LIBDIR']])
					conf.test_pyembed(xx)
				else:
					raise
		if'pyext'in features:
			for flags in all_flags:
				conf.check_cfg(msg='Asking python-config for pyext %r flags'%' '.join(flags),path=env.PYTHON_CONFIG,package='',uselib_store='PYEXT',args=flags)
			try:
				conf.test_pyext(xx)
			except conf.errors.ConfigurationError:
				if dct['Py_ENABLE_SHARED']and dct['LIBDIR']:
					env.append_unique('LIBPATH_PYEXT',[dct['LIBDIR']])
					conf.test_pyext(xx)
				else:
					raise
		conf.define('HAVE_PYTHON_H',1)
		return
	all_flags=dct['LDFLAGS']+' '+dct['CFLAGS']
	conf.parse_flags(all_flags,'PYEMBED')
	all_flags=dct['LDFLAGS']+' '+dct['LDSHARED']+' '+dct['CFLAGS']
	conf.parse_flags(all_flags,'PYEXT')
	result=None
	if not dct["LDVERSION"]:
		dct["LDVERSION"]=env['PYTHON_VERSION']
	for name in('python'+dct['LDVERSION'],'python'+env['PYTHON_VERSION']+'m','python'+env['PYTHON_VERSION'].replace('.','')):
		if not result and env['LIBPATH_PYEMBED']:
			path=env['LIBPATH_PYEMBED']
			conf.to_log("\n\n# Trying default LIBPATH_PYEMBED: %r\n"%path)
			result=conf.check(lib=name,uselib='PYEMBED',libpath=path,mandatory=False,msg='Checking for library %s in LIBPATH_PYEMBED'%name)
		if not result and dct['LIBDIR']:
			path=[dct['LIBDIR']]
			conf.to_log("\n\n# try again with -L$python_LIBDIR: %r\n"%path)
			result=conf.check(lib=name,uselib='PYEMBED',libpath=path,mandatory=False,msg='Checking for library %s in LIBDIR'%name)
		if not result and dct['LIBPL']:
			path=[dct['LIBPL']]
			conf.to_log("\n\n# try again with -L$python_LIBPL (some systems don't install the python library in $prefix/lib)\n")
			result=conf.check(lib=name,uselib='PYEMBED',libpath=path,mandatory=False,msg='Checking for library %s in python_LIBPL'%name)
		if not result:
			path=[os.path.join(dct['prefix'],"libs")]
			conf.to_log("\n\n# try again with -L$prefix/libs, and pythonXY name rather than pythonX.Y (win32)\n")
			result=conf.check(lib=name,uselib='PYEMBED',libpath=path,mandatory=False,msg='Checking for library %s in $prefix/libs'%name)
		if result:
			break
	if result:
		env['LIBPATH_PYEMBED']=path
		env.append_value('LIB_PYEMBED',[name])
	else:
		conf.to_log("\n\n### LIB NOT FOUND\n")
	if Utils.is_win32 or dct['Py_ENABLE_SHARED']:
		env['LIBPATH_PYEXT']=env['LIBPATH_PYEMBED']
		env['LIB_PYEXT']=env['LIB_PYEMBED']
	conf.to_log("Include path for Python extensions (found via distutils module): %r\n"%(dct['INCLUDEPY'],))
	env['INCLUDES_PYEXT']=[dct['INCLUDEPY']]
	env['INCLUDES_PYEMBED']=[dct['INCLUDEPY']]
	if env['CC_NAME']=='gcc':
		env.append_value('CFLAGS_PYEMBED',['-fno-strict-aliasing'])
		env.append_value('CFLAGS_PYEXT',['-fno-strict-aliasing'])
	if env['CXX_NAME']=='gcc':
		env.append_value('CXXFLAGS_PYEMBED',['-fno-strict-aliasing'])
		env.append_value('CXXFLAGS_PYEXT',['-fno-strict-aliasing'])
	if env.CC_NAME=="msvc":
		from distutils.msvccompiler import MSVCCompiler
		dist_compiler=MSVCCompiler()
		dist_compiler.initialize()
		env.append_value('CFLAGS_PYEXT',dist_compiler.compile_options)
		env.append_value('CXXFLAGS_PYEXT',dist_compiler.compile_options)
		env.append_value('LINKFLAGS_PYEXT',dist_compiler.ldflags_shared)
	conf.check(header_name='Python.h',define_name='HAVE_PYTHON_H',uselib='PYEMBED',fragment=FRAG,errmsg='Distutils not installed? Broken python installation? Get python-config now!')
@conf
def check_python_version(conf,minver=None):
	assert minver is None or isinstance(minver,tuple)
	pybin=conf.env['PYTHON']
	if not pybin:
		conf.fatal('could not find the python executable')
	cmd=pybin+['-c','import sys\nfor x in sys.version_info: print(str(x))']
	Logs.debug('python: Running python command %r'%cmd)
	lines=conf.cmd_and_log(cmd).split()
	assert len(lines)==5,"found %i lines, expected 5: %r"%(len(lines),lines)
	pyver_tuple=(int(lines[0]),int(lines[1]),int(lines[2]),lines[3],int(lines[4]))
	result=(minver is None)or(pyver_tuple>=minver)
	if result:
		pyver='.'.join([str(x)for x in pyver_tuple[:2]])
		conf.env['PYTHON_VERSION']=pyver
		if'PYTHONDIR'in conf.env:
			pydir=conf.env['PYTHONDIR']
		elif'PYTHONDIR'in conf.environ:
			pydir=conf.environ['PYTHONDIR']
		else:
			if Utils.is_win32:
				(python_LIBDEST,pydir)=conf.get_python_variables(["get_config_var('LIBDEST') or ''","get_python_lib(standard_lib=0) or ''"])
			else:
				python_LIBDEST=None
				(pydir,)=conf.get_python_variables(["get_python_lib(standard_lib=0, prefix=%r) or ''"%conf.env.PREFIX])
			if python_LIBDEST is None:
				if conf.env['LIBDIR']:
					python_LIBDEST=os.path.join(conf.env['LIBDIR'],"python"+pyver)
				else:
					python_LIBDEST=os.path.join(conf.env['PREFIX'],"lib","python"+pyver)
		if'PYTHONARCHDIR'in conf.env:
			pyarchdir=conf.env['PYTHONARCHDIR']
		elif'PYTHONARCHDIR'in conf.environ:
			pyarchdir=conf.environ['PYTHONARCHDIR']
		else:
			(pyarchdir,)=conf.get_python_variables(["get_python_lib(plat_specific=1, standard_lib=0, prefix=%r) or ''"%conf.env.PREFIX])
			if not pyarchdir:
				pyarchdir=pydir
		if hasattr(conf,'define'):
			conf.define('PYTHONDIR',pydir)
			conf.define('PYTHONARCHDIR',pyarchdir)
		conf.env['PYTHONDIR']=pydir
		conf.env['PYTHONARCHDIR']=pyarchdir
	pyver_full='.'.join(map(str,pyver_tuple[:3]))
	if minver is None:
		conf.msg('Checking for python version',pyver_full)
	else:
		minver_str='.'.join(map(str,minver))
		conf.msg('Checking for python version',pyver_tuple,">= %s"%(minver_str,)and'GREEN'or'YELLOW')
	if not result:
		conf.fatal('The python version is too old, expecting %r'%(minver,))
PYTHON_MODULE_TEMPLATE='''
import %s as current_module
version = getattr(current_module, '__version__', None)
if version is not None:
	print(str(version))
else:
	print('unknown version')
'''
@conf
def check_python_module(conf,module_name,condition=''):
	msg="Checking for python module '%s'"%module_name
	if condition:
		msg='%s (%s)'%(msg,condition)
	conf.start_msg(msg)
	try:
		ret=conf.cmd_and_log(conf.env['PYTHON']+['-c',PYTHON_MODULE_TEMPLATE%module_name])
	except Exception:
		conf.end_msg(False)
		conf.fatal('Could not find the python module %r'%module_name)
	ret=ret.strip()
	if condition:
		conf.end_msg(ret)
		if ret=='unknown version':
			conf.fatal('Could not check the %s version'%module_name)
		from distutils.version import LooseVersion
		def num(*k):
			if isinstance(k[0],int):
				return LooseVersion('.'.join([str(x)for x in k]))
			else:
				return LooseVersion(k[0])
		d={'num':num,'ver':LooseVersion(ret)}
		ev=eval(condition,{},d)
		if not ev:
			conf.fatal('The %s version does not satisfy the requirements'%module_name)
	else:
		if ret=='unknown version':
			conf.end_msg(True)
		else:
			conf.end_msg(ret)
def configure(conf):
	v=conf.env
	v['PYTHON']=Options.options.python or os.environ.get('PYTHON',sys.executable)
	if Options.options.pythondir:
		v['PYTHONDIR']=Options.options.pythondir
	if Options.options.pythonarchdir:
		v['PYTHONARCHDIR']=Options.options.pythonarchdir
	conf.find_program('python',var='PYTHON')
	v['PYFLAGS']=''
	v['PYFLAGS_OPT']='-O'
	v['PYC']=getattr(Options.options,'pyc',1)
	v['PYO']=getattr(Options.options,'pyo',1)
	try:
		v.PYTAG=conf.cmd_and_log(conf.env.PYTHON+['-c',"import imp;print(imp.get_tag())"]).strip()
	except Errors.WafError:
		pass
def options(opt):
	pyopt=opt.add_option_group("Python Options")
	pyopt.add_option('--nopyc',dest='pyc',action='store_false',default=1,help='Do not install bytecode compiled .pyc files (configuration) [Default:install]')
	pyopt.add_option('--nopyo',dest='pyo',action='store_false',default=1,help='Do not install optimised compiled .pyo files (configuration) [Default:install]')
	pyopt.add_option('--python',dest="python",help='python binary to be used [Default: %s]'%sys.executable)
	pyopt.add_option('--pythondir',dest='pythondir',help='Installation path for python modules (py, platform-independent .py and .pyc files)')
	pyopt.add_option('--pythonarchdir',dest='pythonarchdir',help='Installation path for python extension (pyext, platform-dependent .so or .dylib files)')
