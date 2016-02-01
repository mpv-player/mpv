#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

from waflib import TaskGen,Task
from waflib.Tools import c_preproc
from waflib.Tools.ccroot import link_task,stlink_task
@TaskGen.extension('.cpp','.cc','.cxx','.C','.c++')
def cxx_hook(self,node):
	return self.create_compiled_task('cxx',node)
if not'.c'in TaskGen.task_gen.mappings:
	TaskGen.task_gen.mappings['.c']=TaskGen.task_gen.mappings['.cpp']
class cxx(Task.Task):
	run_str='${CXX} ${ARCH_ST:ARCH} ${CXXFLAGS} ${CPPFLAGS} ${FRAMEWORKPATH_ST:FRAMEWORKPATH} ${CPPPATH_ST:INCPATHS} ${DEFINES_ST:DEFINES} ${CXX_SRC_F}${SRC} ${CXX_TGT_F}${TGT[0].abspath()}'
	vars=['CXXDEPS']
	ext_in=['.h']
	scan=c_preproc.scan
class cxxprogram(link_task):
	run_str='${LINK_CXX} ${LINKFLAGS} ${CXXLNK_SRC_F}${SRC} ${CXXLNK_TGT_F}${TGT[0].abspath()} ${RPATH_ST:RPATH} ${FRAMEWORKPATH_ST:FRAMEWORKPATH} ${FRAMEWORK_ST:FRAMEWORK} ${ARCH_ST:ARCH} ${STLIB_MARKER} ${STLIBPATH_ST:STLIBPATH} ${STLIB_ST:STLIB} ${SHLIB_MARKER} ${LIBPATH_ST:LIBPATH} ${LIB_ST:LIB} ${LDFLAGS}'
	vars=['LINKDEPS']
	ext_out=['.bin']
	inst_to='${BINDIR}'
class cxxshlib(cxxprogram):
	inst_to='${LIBDIR}'
class cxxstlib(stlink_task):
	pass
