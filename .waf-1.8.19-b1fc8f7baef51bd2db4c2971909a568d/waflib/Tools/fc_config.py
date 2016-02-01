#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import re,os,sys,shlex
from waflib.Configure import conf
from waflib.TaskGen import feature,before_method
FC_FRAGMENT='        program main\n        end     program main\n'
FC_FRAGMENT2='        PROGRAM MAIN\n        END\n'
@conf
def fc_flags(conf):
	v=conf.env
	v['FC_SRC_F']=[]
	v['FC_TGT_F']=['-c','-o']
	v['FCINCPATH_ST']='-I%s'
	v['FCDEFINES_ST']='-D%s'
	if not v['LINK_FC']:v['LINK_FC']=v['FC']
	v['FCLNK_SRC_F']=[]
	v['FCLNK_TGT_F']=['-o']
	v['FCFLAGS_fcshlib']=['-fpic']
	v['LINKFLAGS_fcshlib']=['-shared']
	v['fcshlib_PATTERN']='lib%s.so'
	v['fcstlib_PATTERN']='lib%s.a'
	v['FCLIB_ST']='-l%s'
	v['FCLIBPATH_ST']='-L%s'
	v['FCSTLIB_ST']='-l%s'
	v['FCSTLIBPATH_ST']='-L%s'
	v['FCSTLIB_MARKER']='-Wl,-Bstatic'
	v['FCSHLIB_MARKER']='-Wl,-Bdynamic'
	v['SONAME_ST']='-Wl,-h,%s'
@conf
def fc_add_flags(conf):
	conf.add_os_flags('FCFLAGS',dup=False)
	conf.add_os_flags('LINKFLAGS',dup=False)
	conf.add_os_flags('LDFLAGS',dup=False)
@conf
def check_fortran(self,*k,**kw):
	self.check_cc(fragment=FC_FRAGMENT,compile_filename='test.f',features='fc fcprogram',msg='Compiling a simple fortran app')
@conf
def check_fc(self,*k,**kw):
	kw['compiler']='fc'
	if not'compile_mode'in kw:
		kw['compile_mode']='fc'
	if not'type'in kw:
		kw['type']='fcprogram'
	if not'compile_filename'in kw:
		kw['compile_filename']='test.f90'
	if not'code'in kw:
		kw['code']=FC_FRAGMENT
	return self.check(*k,**kw)
@conf
def fortran_modifier_darwin(conf):
	v=conf.env
	v['FCFLAGS_fcshlib']=['-fPIC']
	v['LINKFLAGS_fcshlib']=['-dynamiclib']
	v['fcshlib_PATTERN']='lib%s.dylib'
	v['FRAMEWORKPATH_ST']='-F%s'
	v['FRAMEWORK_ST']='-framework %s'
	v['LINKFLAGS_fcstlib']=[]
	v['FCSHLIB_MARKER']=''
	v['FCSTLIB_MARKER']=''
	v['SONAME_ST']=''
@conf
def fortran_modifier_win32(conf):
	v=conf.env
	v['fcprogram_PATTERN']=v['fcprogram_test_PATTERN']='%s.exe'
	v['fcshlib_PATTERN']='%s.dll'
	v['implib_PATTERN']='lib%s.dll.a'
	v['IMPLIB_ST']='-Wl,--out-implib,%s'
	v['FCFLAGS_fcshlib']=[]
	v.append_value('FCFLAGS_fcshlib',['-DDLL_EXPORT'])
	v.append_value('LINKFLAGS',['-Wl,--enable-auto-import'])
@conf
def fortran_modifier_cygwin(conf):
	fortran_modifier_win32(conf)
	v=conf.env
	v['fcshlib_PATTERN']='cyg%s.dll'
	v.append_value('LINKFLAGS_fcshlib',['-Wl,--enable-auto-image-base'])
	v['FCFLAGS_fcshlib']=[]
@conf
def check_fortran_dummy_main(self,*k,**kw):
	if not self.env.CC:
		self.fatal('A c compiler is required for check_fortran_dummy_main')
	lst=['MAIN__','__MAIN','_MAIN','MAIN_','MAIN']
	lst.extend([m.lower()for m in lst])
	lst.append('')
	self.start_msg('Detecting whether we need a dummy main')
	for main in lst:
		kw['fortran_main']=main
		try:
			self.check_cc(fragment='int %s() { return 0; }\n'%(main or'test'),features='c fcprogram',mandatory=True)
			if not main:
				self.env.FC_MAIN=-1
				self.end_msg('no')
			else:
				self.env.FC_MAIN=main
				self.end_msg('yes %s'%main)
			break
		except self.errors.ConfigurationError:
			pass
	else:
		self.end_msg('not found')
		self.fatal('could not detect whether fortran requires a dummy main, see the config.log')
GCC_DRIVER_LINE=re.compile('^Driving:')
POSIX_STATIC_EXT=re.compile('\S+\.a')
POSIX_LIB_FLAGS=re.compile('-l\S+')
@conf
def is_link_verbose(self,txt):
	assert isinstance(txt,str)
	for line in txt.splitlines():
		if not GCC_DRIVER_LINE.search(line):
			if POSIX_STATIC_EXT.search(line)or POSIX_LIB_FLAGS.search(line):
				return True
	return False
@conf
def check_fortran_verbose_flag(self,*k,**kw):
	self.start_msg('fortran link verbose flag')
	for x in('-v','--verbose','-verbose','-V'):
		try:
			self.check_cc(features='fc fcprogram_test',fragment=FC_FRAGMENT2,compile_filename='test.f',linkflags=[x],mandatory=True)
		except self.errors.ConfigurationError:
			pass
		else:
			if self.is_link_verbose(self.test_bld.err)or self.is_link_verbose(self.test_bld.out):
				self.end_msg(x)
				break
	else:
		self.end_msg('failure')
		self.fatal('Could not obtain the fortran link verbose flag (see config.log)')
	self.env.FC_VERBOSE_FLAG=x
	return x
LINKFLAGS_IGNORED=[r'-lang*',r'-lcrt[a-zA-Z0-9\.]*\.o',r'-lc$',r'-lSystem',r'-libmil',r'-LIST:*',r'-LNO:*']
if os.name=='nt':
	LINKFLAGS_IGNORED.extend([r'-lfrt*',r'-luser32',r'-lkernel32',r'-ladvapi32',r'-lmsvcrt',r'-lshell32',r'-lmingw',r'-lmoldname'])
else:
	LINKFLAGS_IGNORED.append(r'-lgcc*')
RLINKFLAGS_IGNORED=[re.compile(f)for f in LINKFLAGS_IGNORED]
def _match_ignore(line):
	for i in RLINKFLAGS_IGNORED:
		if i.match(line):
			return True
	return False
def parse_fortran_link(lines):
	final_flags=[]
	for line in lines:
		if not GCC_DRIVER_LINE.match(line):
			_parse_flink_line(line,final_flags)
	return final_flags
SPACE_OPTS=re.compile('^-[LRuYz]$')
NOSPACE_OPTS=re.compile('^-[RL]')
def _parse_flink_token(lexer,token,tmp_flags):
	if _match_ignore(token):
		pass
	elif token.startswith('-lkernel32')and sys.platform=='cygwin':
		tmp_flags.append(token)
	elif SPACE_OPTS.match(token):
		t=lexer.get_token()
		if t.startswith('P,'):
			t=t[2:]
		for opt in t.split(os.pathsep):
			tmp_flags.append('-L%s'%opt)
	elif NOSPACE_OPTS.match(token):
		tmp_flags.append(token)
	elif POSIX_LIB_FLAGS.match(token):
		tmp_flags.append(token)
	else:
		pass
	t=lexer.get_token()
	return t
def _parse_flink_line(line,final_flags):
	lexer=shlex.shlex(line,posix=True)
	lexer.whitespace_split=True
	t=lexer.get_token()
	tmp_flags=[]
	while t:
		t=_parse_flink_token(lexer,t,tmp_flags)
	final_flags.extend(tmp_flags)
	return final_flags
@conf
def check_fortran_clib(self,autoadd=True,*k,**kw):
	if not self.env.FC_VERBOSE_FLAG:
		self.fatal('env.FC_VERBOSE_FLAG is not set: execute check_fortran_verbose_flag?')
	self.start_msg('Getting fortran runtime link flags')
	try:
		self.check_cc(fragment=FC_FRAGMENT2,compile_filename='test.f',features='fc fcprogram_test',linkflags=[self.env.FC_VERBOSE_FLAG])
	except Exception:
		self.end_msg(False)
		if kw.get('mandatory',True):
			conf.fatal('Could not find the c library flags')
	else:
		out=self.test_bld.err
		flags=parse_fortran_link(out.splitlines())
		self.end_msg('ok (%s)'%' '.join(flags))
		self.env.LINKFLAGS_CLIB=flags
		return flags
	return[]
def getoutput(conf,cmd,stdin=False):
	from waflib import Errors
	if conf.env.env:
		env=conf.env.env
	else:
		env=dict(os.environ)
		env['LANG']='C'
	input=stdin and'\n'or None
	try:
		out,err=conf.cmd_and_log(cmd,env=env,output=0,input=input)
	except Errors.WafError ,e:
		if not(hasattr(e,'stderr')and hasattr(e,'stdout')):
			raise e
		else:
			out=e.stdout
			err=e.stderr
	except Exception:
		conf.fatal('could not determine the compiler version %r'%cmd)
	return(out,err)
ROUTINES_CODE="""\
      subroutine foobar()
      return
      end
      subroutine foo_bar()
      return
      end
"""
MAIN_CODE="""
void %(dummy_func_nounder)s(void);
void %(dummy_func_under)s(void);
int %(main_func_name)s() {
  %(dummy_func_nounder)s();
  %(dummy_func_under)s();
  return 0;
}
"""
@feature('link_main_routines_func')
@before_method('process_source')
def link_main_routines_tg_method(self):
	def write_test_file(task):
		task.outputs[0].write(task.generator.code)
	bld=self.bld
	bld(rule=write_test_file,target='main.c',code=MAIN_CODE%self.__dict__)
	bld(rule=write_test_file,target='test.f',code=ROUTINES_CODE)
	bld(features='fc fcstlib',source='test.f',target='test')
	bld(features='c fcprogram',source='main.c',target='app',use='test')
def mangling_schemes():
	for u in('_',''):
		for du in('','_'):
			for c in("lower","upper"):
				yield(u,du,c)
def mangle_name(u,du,c,name):
	return getattr(name,c)()+u+(name.find('_')!=-1 and du or'')
@conf
def check_fortran_mangling(self,*k,**kw):
	if not self.env.CC:
		self.fatal('A c compiler is required for link_main_routines')
	if not self.env.FC:
		self.fatal('A fortran compiler is required for link_main_routines')
	if not self.env.FC_MAIN:
		self.fatal('Checking for mangling requires self.env.FC_MAIN (execute "check_fortran_dummy_main" first?)')
	self.start_msg('Getting fortran mangling scheme')
	for(u,du,c)in mangling_schemes():
		try:
			self.check_cc(compile_filename=[],features='link_main_routines_func',msg='nomsg',errmsg='nomsg',mandatory=True,dummy_func_nounder=mangle_name(u,du,c,"foobar"),dummy_func_under=mangle_name(u,du,c,"foo_bar"),main_func_name=self.env.FC_MAIN)
		except self.errors.ConfigurationError:
			pass
		else:
			self.end_msg("ok ('%s', '%s', '%s-case')"%(u,du,c))
			self.env.FORTRAN_MANGLING=(u,du,c)
			break
	else:
		self.end_msg(False)
		self.fatal('mangler not found')
	return(u,du,c)
@feature('pyext')
@before_method('propagate_uselib_vars','apply_link')
def set_lib_pat(self):
	self.env['fcshlib_PATTERN']=self.env['pyext_PATTERN']
@conf
def detect_openmp(self):
	for x in('-fopenmp','-openmp','-mp','-xopenmp','-omp','-qsmp=omp'):
		try:
			self.check_fc(msg='Checking for OpenMP flag %s'%x,fragment='program main\n  call omp_get_num_threads()\nend program main',fcflags=x,linkflags=x,uselib_store='OPENMP')
		except self.errors.ConfigurationError:
			pass
		else:
			break
	else:
		self.fatal('Could not find OpenMP')
