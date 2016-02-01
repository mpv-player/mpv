#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import os
from waflib import Context,Task,Utils,Options,Errors,Logs
from waflib.TaskGen import taskgen_method,before_method,feature,extension
from waflib.Configure import conf
@taskgen_method
def add_marshal_file(self,filename,prefix):
	if not hasattr(self,'marshal_list'):
		self.marshal_list=[]
	self.meths.append('process_marshal')
	self.marshal_list.append((filename,prefix))
@before_method('process_source')
def process_marshal(self):
	for f,prefix in getattr(self,'marshal_list',[]):
		node=self.path.find_resource(f)
		if not node:
			raise Errors.WafError('file not found %r'%f)
		h_node=node.change_ext('.h')
		c_node=node.change_ext('.c')
		task=self.create_task('glib_genmarshal',node,[h_node,c_node])
		task.env.GLIB_GENMARSHAL_PREFIX=prefix
	self.source=self.to_nodes(getattr(self,'source',[]))
	self.source.append(c_node)
class glib_genmarshal(Task.Task):
	def run(self):
		bld=self.inputs[0].__class__.ctx
		get=self.env.get_flat
		cmd1="%s %s --prefix=%s --header > %s"%(get('GLIB_GENMARSHAL'),self.inputs[0].srcpath(),get('GLIB_GENMARSHAL_PREFIX'),self.outputs[0].abspath())
		ret=bld.exec_command(cmd1)
		if ret:return ret
		c='''#include "%s"\n'''%self.outputs[0].name
		self.outputs[1].write(c)
		cmd2="%s %s --prefix=%s --body >> %s"%(get('GLIB_GENMARSHAL'),self.inputs[0].srcpath(),get('GLIB_GENMARSHAL_PREFIX'),self.outputs[1].abspath())
		return bld.exec_command(cmd2)
	vars=['GLIB_GENMARSHAL_PREFIX','GLIB_GENMARSHAL']
	color='BLUE'
	ext_out=['.h']
@taskgen_method
def add_enums_from_template(self,source='',target='',template='',comments=''):
	if not hasattr(self,'enums_list'):
		self.enums_list=[]
	self.meths.append('process_enums')
	self.enums_list.append({'source':source,'target':target,'template':template,'file-head':'','file-prod':'','file-tail':'','enum-prod':'','value-head':'','value-prod':'','value-tail':'','comments':comments})
@taskgen_method
def add_enums(self,source='',target='',file_head='',file_prod='',file_tail='',enum_prod='',value_head='',value_prod='',value_tail='',comments=''):
	if not hasattr(self,'enums_list'):
		self.enums_list=[]
	self.meths.append('process_enums')
	self.enums_list.append({'source':source,'template':'','target':target,'file-head':file_head,'file-prod':file_prod,'file-tail':file_tail,'enum-prod':enum_prod,'value-head':value_head,'value-prod':value_prod,'value-tail':value_tail,'comments':comments})
@before_method('process_source')
def process_enums(self):
	for enum in getattr(self,'enums_list',[]):
		task=self.create_task('glib_mkenums')
		env=task.env
		inputs=[]
		source_list=self.to_list(enum['source'])
		if not source_list:
			raise Errors.WafError('missing source '+str(enum))
		source_list=[self.path.find_resource(k)for k in source_list]
		inputs+=source_list
		env['GLIB_MKENUMS_SOURCE']=[k.abspath()for k in source_list]
		if not enum['target']:
			raise Errors.WafError('missing target '+str(enum))
		tgt_node=self.path.find_or_declare(enum['target'])
		if tgt_node.name.endswith('.c'):
			self.source.append(tgt_node)
		env['GLIB_MKENUMS_TARGET']=tgt_node.abspath()
		options=[]
		if enum['template']:
			template_node=self.path.find_resource(enum['template'])
			options.append('--template %s'%(template_node.abspath()))
			inputs.append(template_node)
		params={'file-head':'--fhead','file-prod':'--fprod','file-tail':'--ftail','enum-prod':'--eprod','value-head':'--vhead','value-prod':'--vprod','value-tail':'--vtail','comments':'--comments'}
		for param,option in params.items():
			if enum[param]:
				options.append('%s %r'%(option,enum[param]))
		env['GLIB_MKENUMS_OPTIONS']=' '.join(options)
		task.set_inputs(inputs)
		task.set_outputs(tgt_node)
class glib_mkenums(Task.Task):
	run_str='${GLIB_MKENUMS} ${GLIB_MKENUMS_OPTIONS} ${GLIB_MKENUMS_SOURCE} > ${GLIB_MKENUMS_TARGET}'
	color='PINK'
	ext_out=['.h']
@taskgen_method
def add_settings_schemas(self,filename_list):
	if not hasattr(self,'settings_schema_files'):
		self.settings_schema_files=[]
	if not isinstance(filename_list,list):
		filename_list=[filename_list]
	self.settings_schema_files.extend(filename_list)
@taskgen_method
def add_settings_enums(self,namespace,filename_list):
	if hasattr(self,'settings_enum_namespace'):
		raise Errors.WafError("Tried to add gsettings enums to '%s' more than once"%self.name)
	self.settings_enum_namespace=namespace
	if type(filename_list)!='list':
		filename_list=[filename_list]
	self.settings_enum_files=filename_list
@feature('glib2')
def process_settings(self):
	enums_tgt_node=[]
	install_files=[]
	settings_schema_files=getattr(self,'settings_schema_files',[])
	if settings_schema_files and not self.env['GLIB_COMPILE_SCHEMAS']:
		raise Errors.WafError("Unable to process GSettings schemas - glib-compile-schemas was not found during configure")
	if hasattr(self,'settings_enum_files'):
		enums_task=self.create_task('glib_mkenums')
		source_list=self.settings_enum_files
		source_list=[self.path.find_resource(k)for k in source_list]
		enums_task.set_inputs(source_list)
		enums_task.env['GLIB_MKENUMS_SOURCE']=[k.abspath()for k in source_list]
		target=self.settings_enum_namespace+'.enums.xml'
		tgt_node=self.path.find_or_declare(target)
		enums_task.set_outputs(tgt_node)
		enums_task.env['GLIB_MKENUMS_TARGET']=tgt_node.abspath()
		enums_tgt_node=[tgt_node]
		install_files.append(tgt_node)
		options='--comments "<!-- @comment@ -->" --fhead "<schemalist>" --vhead "  <@type@ id=\\"%s.@EnumName@\\">" --vprod "    <value nick=\\"@valuenick@\\" value=\\"@valuenum@\\"/>" --vtail "  </@type@>" --ftail "</schemalist>" '%(self.settings_enum_namespace)
		enums_task.env['GLIB_MKENUMS_OPTIONS']=options
	for schema in settings_schema_files:
		schema_task=self.create_task('glib_validate_schema')
		schema_node=self.path.find_resource(schema)
		if not schema_node:
			raise Errors.WafError("Cannot find the schema file '%s'"%schema)
		install_files.append(schema_node)
		source_list=enums_tgt_node+[schema_node]
		schema_task.set_inputs(source_list)
		schema_task.env['GLIB_COMPILE_SCHEMAS_OPTIONS']=[("--schema-file="+k.abspath())for k in source_list]
		target_node=schema_node.change_ext('.xml.valid')
		schema_task.set_outputs(target_node)
		schema_task.env['GLIB_VALIDATE_SCHEMA_OUTPUT']=target_node.abspath()
	def compile_schemas_callback(bld):
		if not bld.is_install:return
		Logs.pprint('YELLOW','Updating GSettings schema cache')
		command=Utils.subst_vars("${GLIB_COMPILE_SCHEMAS} ${GSETTINGSSCHEMADIR}",bld.env)
		self.bld.exec_command(command)
	if self.bld.is_install:
		if not self.env['GSETTINGSSCHEMADIR']:
			raise Errors.WafError('GSETTINGSSCHEMADIR not defined (should have been set up automatically during configure)')
		if install_files:
			self.bld.install_files(self.env['GSETTINGSSCHEMADIR'],install_files)
			if not hasattr(self.bld,'_compile_schemas_registered'):
				self.bld.add_post_fun(compile_schemas_callback)
				self.bld._compile_schemas_registered=True
class glib_validate_schema(Task.Task):
	run_str='rm -f ${GLIB_VALIDATE_SCHEMA_OUTPUT} && ${GLIB_COMPILE_SCHEMAS} --dry-run ${GLIB_COMPILE_SCHEMAS_OPTIONS} && touch ${GLIB_VALIDATE_SCHEMA_OUTPUT}'
	color='PINK'
@extension('.gresource.xml')
def process_gresource_source(self,node):
	if not self.env['GLIB_COMPILE_RESOURCES']:
		raise Errors.WafError("Unable to process GResource file - glib-compile-resources was not found during configure")
	if'gresource'in self.features:
		return
	h_node=node.change_ext('_xml.h')
	c_node=node.change_ext('_xml.c')
	self.create_task('glib_gresource_source',node,[h_node,c_node])
	self.source.append(c_node)
@feature('gresource')
def process_gresource_bundle(self):
	for i in self.to_list(self.source):
		node=self.path.find_resource(i)
		task=self.create_task('glib_gresource_bundle',node,node.change_ext(''))
		inst_to=getattr(self,'install_path',None)
		if inst_to:
			self.bld.install_files(inst_to,task.outputs)
class glib_gresource_base(Task.Task):
	color='BLUE'
	base_cmd='${GLIB_COMPILE_RESOURCES} --sourcedir=${SRC[0].parent.srcpath()} --sourcedir=${SRC[0].bld_dir()}'
	def scan(self):
		bld=self.generator.bld
		kw={}
		try:
			if not kw.get('cwd',None):
				kw['cwd']=bld.cwd
		except AttributeError:
			bld.cwd=kw['cwd']=bld.variant_dir
		kw['quiet']=Context.BOTH
		cmd=Utils.subst_vars('${GLIB_COMPILE_RESOURCES} --sourcedir=%s --sourcedir=%s --generate-dependencies %s'%(self.inputs[0].parent.srcpath(),self.inputs[0].bld_dir(),self.inputs[0].bldpath()),self.env)
		output=bld.cmd_and_log(cmd,**kw)
		nodes=[]
		names=[]
		for dep in output.splitlines():
			if dep:
				node=bld.bldnode.find_node(dep)
				if node:
					nodes.append(node)
				else:
					names.append(dep)
		return(nodes,names)
class glib_gresource_source(glib_gresource_base):
	vars=['GLIB_COMPILE_RESOURCES']
	fun_h=Task.compile_fun_shell(glib_gresource_base.base_cmd+' --target=${TGT[0].abspath()} --generate-header ${SRC}')
	fun_c=Task.compile_fun_shell(glib_gresource_base.base_cmd+' --target=${TGT[1].abspath()} --generate-source ${SRC}')
	ext_out=['.h']
	def run(self):
		return self.fun_h[0](self)or self.fun_c[0](self)
class glib_gresource_bundle(glib_gresource_base):
	run_str=glib_gresource_base.base_cmd+' --target=${TGT} ${SRC}'
	shell=True
@conf
def find_glib_genmarshal(conf):
	conf.find_program('glib-genmarshal',var='GLIB_GENMARSHAL')
@conf
def find_glib_mkenums(conf):
	if not conf.env.PERL:
		conf.find_program('perl',var='PERL')
	conf.find_program('glib-mkenums',interpreter='PERL',var='GLIB_MKENUMS')
@conf
def find_glib_compile_schemas(conf):
	conf.find_program('glib-compile-schemas',var='GLIB_COMPILE_SCHEMAS')
	def getstr(varname):
		return getattr(Options.options,varname,getattr(conf.env,varname,''))
	gsettingsschemadir=getstr('GSETTINGSSCHEMADIR')
	if not gsettingsschemadir:
		datadir=getstr('DATADIR')
		if not datadir:
			prefix=conf.env['PREFIX']
			datadir=os.path.join(prefix,'share')
		gsettingsschemadir=os.path.join(datadir,'glib-2.0','schemas')
	conf.env['GSETTINGSSCHEMADIR']=gsettingsschemadir
@conf
def find_glib_compile_resources(conf):
	conf.find_program('glib-compile-resources',var='GLIB_COMPILE_RESOURCES')
def configure(conf):
	conf.find_glib_genmarshal()
	conf.find_glib_mkenums()
	conf.find_glib_compile_schemas(mandatory=False)
	conf.find_glib_compile_resources(mandatory=False)
def options(opt):
	gr=opt.add_option_group('Installation directories')
	gr.add_option('--gsettingsschemadir',help='GSettings schema location [DATADIR/glib-2.0/schemas]',default='',dest='GSETTINGSSCHEMADIR')
