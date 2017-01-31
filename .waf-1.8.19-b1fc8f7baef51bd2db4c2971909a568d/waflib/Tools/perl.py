#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import os
from waflib import Task,Options,Utils
from waflib.Configure import conf
from waflib.TaskGen import extension,feature,before_method
@before_method('apply_incpaths','apply_link','propagate_uselib_vars')
@feature('perlext')
def init_perlext(self):
	self.uselib=self.to_list(getattr(self,'uselib',[]))
	if not'PERLEXT'in self.uselib:self.uselib.append('PERLEXT')
	self.env['cshlib_PATTERN']=self.env['cxxshlib_PATTERN']=self.env['perlext_PATTERN']
@extension('.xs')
def xsubpp_file(self,node):
	outnode=node.change_ext('.c')
	self.create_task('xsubpp',node,outnode)
	self.source.append(outnode)
class xsubpp(Task.Task):
	run_str='${PERL} ${XSUBPP} -noprototypes -typemap ${EXTUTILS_TYPEMAP} ${SRC} > ${TGT}'
	color='BLUE'
	ext_out=['.h']
@conf
def check_perl_version(self,minver=None):
	res=True
	if minver:
		cver='.'.join(map(str,minver))
	else:
		cver=''
	self.start_msg('Checking for minimum perl version %s'%cver)
	perl=getattr(Options.options,'perlbinary',None)
	if not perl:
		perl=self.find_program('perl',var='PERL')
	if not perl:
		self.end_msg("Perl not found",color="YELLOW")
		return False
	self.env['PERL']=perl
	version=self.cmd_and_log(self.env.PERL+["-e",'printf \"%vd\", $^V'])
	if not version:
		res=False
		version="Unknown"
	elif not minver is None:
		ver=tuple(map(int,version.split(".")))
		if ver<minver:
			res=False
	self.end_msg(version,color=res and"GREEN"or"YELLOW")
	return res
@conf
def check_perl_module(self,module):
	cmd=self.env.PERL+['-e','use %s'%module]
	self.start_msg('perl module %s'%module)
	try:
		r=self.cmd_and_log(cmd)
	except Exception:
		self.end_msg(False)
		return None
	self.end_msg(r or True)
	return r
@conf
def check_perl_ext_devel(self):
	env=self.env
	perl=env.PERL
	if not perl:
		self.fatal('find perl first')
	def cmd_perl_config(s):
		return perl+['-MConfig','-e','print \"%s\"'%s]
	def cfg_str(cfg):
		return self.cmd_and_log(cmd_perl_config(cfg))
	def cfg_lst(cfg):
		return Utils.to_list(cfg_str(cfg))
	def find_xsubpp():
		for var in('privlib','vendorlib'):
			xsubpp=cfg_lst('$Config{%s}/ExtUtils/xsubpp$Config{exe_ext}'%var)
			if xsubpp and os.path.isfile(xsubpp[0]):
				return xsubpp
		return self.find_program('xsubpp')
	env['LINKFLAGS_PERLEXT']=cfg_lst('$Config{lddlflags}')
	env['INCLUDES_PERLEXT']=cfg_lst('$Config{archlib}/CORE')
	env['CFLAGS_PERLEXT']=cfg_lst('$Config{ccflags} $Config{cccdlflags}')
	env['EXTUTILS_TYPEMAP']=cfg_lst('$Config{privlib}/ExtUtils/typemap')
	env['XSUBPP']=find_xsubpp()
	if not getattr(Options.options,'perlarchdir',None):
		env['ARCHDIR_PERL']=cfg_str('$Config{sitearch}')
	else:
		env['ARCHDIR_PERL']=getattr(Options.options,'perlarchdir')
	env['perlext_PATTERN']='%s.'+cfg_str('$Config{dlext}')
def options(opt):
	opt.add_option('--with-perl-binary',type='string',dest='perlbinary',help='Specify alternate perl binary',default=None)
	opt.add_option('--with-perl-archdir',type='string',dest='perlarchdir',help='Specify directory where to install arch specific files',default=None)
