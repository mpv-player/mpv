#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

from waflib import Utils,Task,Errors
from waflib.TaskGen import taskgen_method,feature,extension
from waflib.Tools import d_scan,d_config
from waflib.Tools.ccroot import link_task,stlink_task
class d(Task.Task):
	color='GREEN'
	run_str='${D} ${DFLAGS} ${DINC_ST:INCPATHS} ${D_SRC_F:SRC} ${D_TGT_F:TGT}'
	scan=d_scan.scan
class d_with_header(d):
	run_str='${D} ${DFLAGS} ${DINC_ST:INCPATHS} ${D_HDR_F:tgt.outputs[1].bldpath()} ${D_SRC_F:SRC} ${D_TGT_F:tgt.outputs[0].bldpath()}'
class d_header(Task.Task):
	color='BLUE'
	run_str='${D} ${D_HEADER} ${SRC}'
class dprogram(link_task):
	run_str='${D_LINKER} ${LINKFLAGS} ${DLNK_SRC_F}${SRC} ${DLNK_TGT_F:TGT} ${RPATH_ST:RPATH} ${DSTLIB_MARKER} ${DSTLIBPATH_ST:STLIBPATH} ${DSTLIB_ST:STLIB} ${DSHLIB_MARKER} ${DLIBPATH_ST:LIBPATH} ${DSHLIB_ST:LIB}'
	inst_to='${BINDIR}'
class dshlib(dprogram):
	inst_to='${LIBDIR}'
class dstlib(stlink_task):
	pass
@extension('.d','.di','.D')
def d_hook(self,node):
	ext=Utils.destos_to_binfmt(self.env.DEST_OS)=='pe'and'obj'or'o'
	out='%s.%d.%s'%(node.name,self.idx,ext)
	def create_compiled_task(self,name,node):
		task=self.create_task(name,node,node.parent.find_or_declare(out))
		try:
			self.compiled_tasks.append(task)
		except AttributeError:
			self.compiled_tasks=[task]
		return task
	if getattr(self,'generate_headers',None):
		tsk=create_compiled_task(self,'d_with_header',node)
		tsk.outputs.append(node.change_ext(self.env['DHEADER_ext']))
	else:
		tsk=create_compiled_task(self,'d',node)
	return tsk
@taskgen_method
def generate_header(self,filename):
	try:
		self.header_lst.append([filename,self.install_path])
	except AttributeError:
		self.header_lst=[[filename,self.install_path]]
@feature('d')
def process_header(self):
	for i in getattr(self,'header_lst',[]):
		node=self.path.find_resource(i[0])
		if not node:
			raise Errors.WafError('file %r not found on d obj'%i[0])
		self.create_task('d_header',node,node.change_ext('.di'))
