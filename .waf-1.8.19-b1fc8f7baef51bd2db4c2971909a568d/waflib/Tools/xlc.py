#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

from waflib.Tools import ccroot,ar
from waflib.Configure import conf
@conf
def find_xlc(conf):
	cc=conf.find_program(['xlc_r','xlc'],var='CC')
	conf.get_xlc_version(cc)
	conf.env.CC_NAME='xlc'
@conf
def xlc_common_flags(conf):
	v=conf.env
	v['CC_SRC_F']=[]
	v['CC_TGT_F']=['-c','-o']
	if not v['LINK_CC']:v['LINK_CC']=v['CC']
	v['CCLNK_SRC_F']=[]
	v['CCLNK_TGT_F']=['-o']
	v['CPPPATH_ST']='-I%s'
	v['DEFINES_ST']='-D%s'
	v['LIB_ST']='-l%s'
	v['LIBPATH_ST']='-L%s'
	v['STLIB_ST']='-l%s'
	v['STLIBPATH_ST']='-L%s'
	v['RPATH_ST']='-Wl,-rpath,%s'
	v['SONAME_ST']=[]
	v['SHLIB_MARKER']=[]
	v['STLIB_MARKER']=[]
	v['LINKFLAGS_cprogram']=['-Wl,-brtl']
	v['cprogram_PATTERN']='%s'
	v['CFLAGS_cshlib']=['-fPIC']
	v['LINKFLAGS_cshlib']=['-G','-Wl,-brtl,-bexpfull']
	v['cshlib_PATTERN']='lib%s.so'
	v['LINKFLAGS_cstlib']=[]
	v['cstlib_PATTERN']='lib%s.a'
def configure(conf):
	conf.find_xlc()
	conf.find_ar()
	conf.xlc_common_flags()
	conf.cc_load_tools()
	conf.cc_add_flags()
	conf.link_add_flags()
