#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import traceback,sys
class WafError(Exception):
	def __init__(self,msg='',ex=None):
		self.msg=msg
		assert not isinstance(msg,Exception)
		self.stack=[]
		if ex:
			if not msg:
				self.msg=str(ex)
			if isinstance(ex,WafError):
				self.stack=ex.stack
			else:
				self.stack=traceback.extract_tb(sys.exc_info()[2])
		self.stack+=traceback.extract_stack()[:-1]
		self.verbose_msg=''.join(traceback.format_list(self.stack))
	def __str__(self):
		return str(self.msg)
class BuildError(WafError):
	def __init__(self,error_tasks=[]):
		self.tasks=error_tasks
		WafError.__init__(self,self.format_error())
	def format_error(self):
		lst=['Build failed']
		for tsk in self.tasks:
			txt=tsk.format_error()
			if txt:lst.append(txt)
		return'\n'.join(lst)
class ConfigurationError(WafError):
	pass
class TaskRescan(WafError):
	pass
class TaskNotReady(WafError):
	pass
