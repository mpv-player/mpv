#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

from waflib.Tools import ccroot,ar
from waflib.Configure import conf
@conf
def find_gcc(conf):
	cc=conf.find_program(['gcc','cc'],var='CC')
	conf.get_cc_version(cc,gcc=True)
	conf.env.CC_NAME='gcc'
@conf
def gcc_common_flags(conf):
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
	v['SONAME_ST']='-Wl,-h,%s'
	v['SHLIB_MARKER']='-Wl,-Bdynamic'
	v['STLIB_MARKER']='-Wl,-Bstatic'
	v['cprogram_PATTERN']='%s'
	v['CFLAGS_cshlib']=['-fPIC']
	v['LINKFLAGS_cshlib']=['-shared']
	v['cshlib_PATTERN']='lib%s.so'
	v['LINKFLAGS_cstlib']=['-Wl,-Bstatic']
	v['cstlib_PATTERN']='lib%s.a'
	v['LINKFLAGS_MACBUNDLE']=['-bundle','-undefined','dynamic_lookup']
	v['CFLAGS_MACBUNDLE']=['-fPIC']
	v['macbundle_PATTERN']='%s.bundle'
@conf
def gcc_modifier_win32(conf):
	v=conf.env
	v['cprogram_PATTERN']='%s.exe'
	v['cshlib_PATTERN']='%s.dll'
	v['implib_PATTERN']='lib%s.dll.a'
	v['IMPLIB_ST']='-Wl,--out-implib,%s'
	v['CFLAGS_cshlib']=[]
	v.append_value('LINKFLAGS',['-Wl,--enable-auto-import'])
@conf
def gcc_modifier_cygwin(conf):
	gcc_modifier_win32(conf)
	v=conf.env
	v['cshlib_PATTERN']='cyg%s.dll'
	v.append_value('LINKFLAGS_cshlib',['-Wl,--enable-auto-image-base'])
	v['CFLAGS_cshlib']=[]
@conf
def gcc_modifier_darwin(conf):
	v=conf.env
	v['CFLAGS_cshlib']=['-fPIC']
	v['LINKFLAGS_cshlib']=['-dynamiclib']
	v['cshlib_PATTERN']='lib%s.dylib'
	v['FRAMEWORKPATH_ST']='-F%s'
	v['FRAMEWORK_ST']=['-framework']
	v['ARCH_ST']=['-arch']
	v['LINKFLAGS_cstlib']=[]
	v['SHLIB_MARKER']=[]
	v['STLIB_MARKER']=[]
	v['SONAME_ST']=[]
@conf
def gcc_modifier_aix(conf):
	v=conf.env
	v['LINKFLAGS_cprogram']=['-Wl,-brtl']
	v['LINKFLAGS_cshlib']=['-shared','-Wl,-brtl,-bexpfull']
	v['SHLIB_MARKER']=[]
@conf
def gcc_modifier_hpux(conf):
	v=conf.env
	v['SHLIB_MARKER']=[]
	v['STLIB_MARKER']=[]
	v['CFLAGS_cshlib']=['-fPIC','-DPIC']
	v['cshlib_PATTERN']='lib%s.sl'
@conf
def gcc_modifier_openbsd(conf):
	conf.env.SONAME_ST=[]
@conf
def gcc_modifier_osf1V(conf):
	v=conf.env
	v['SHLIB_MARKER']=[]
	v['STLIB_MARKER']=[]
	v['SONAME_ST']=[]
@conf
def gcc_modifier_platform(conf):
	gcc_modifier_func=getattr(conf,'gcc_modifier_'+conf.env.DEST_OS,None)
	if gcc_modifier_func:
		gcc_modifier_func()
def configure(conf):
	conf.find_gcc()
	conf.find_ar()
	conf.gcc_common_flags()
	conf.gcc_modifier_platform()
	conf.cc_load_tools()
	conf.cc_add_flags()
	conf.link_add_flags()
