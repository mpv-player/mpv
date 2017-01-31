#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import random,atexit
try:
	from queue import Queue
except ImportError:
	from Queue import Queue
from waflib import Utils,Task,Errors,Logs
GAP=10
class TaskConsumer(Utils.threading.Thread):
	def __init__(self):
		Utils.threading.Thread.__init__(self)
		self.ready=Queue()
		self.setDaemon(1)
		self.start()
	def run(self):
		try:
			self.loop()
		except Exception:
			pass
	def loop(self):
		while 1:
			tsk=self.ready.get()
			if not isinstance(tsk,Task.TaskBase):
				tsk(self)
			else:
				tsk.process()
pool=Queue()
def get_pool():
	try:
		return pool.get(False)
	except Exception:
		return TaskConsumer()
def put_pool(x):
	pool.put(x)
def _free_resources():
	global pool
	lst=[]
	while pool.qsize():
		lst.append(pool.get())
	for x in lst:
		x.ready.put(None)
	for x in lst:
		x.join()
	pool=None
atexit.register(_free_resources)
class Parallel(object):
	def __init__(self,bld,j=2):
		self.numjobs=j
		self.bld=bld
		self.outstanding=[]
		self.frozen=[]
		self.out=Queue(0)
		self.count=0
		self.processed=1
		self.stop=False
		self.error=[]
		self.biter=None
		self.dirty=False
	def get_next_task(self):
		if not self.outstanding:
			return None
		return self.outstanding.pop(0)
	def postpone(self,tsk):
		if random.randint(0,1):
			self.frozen.insert(0,tsk)
		else:
			self.frozen.append(tsk)
	def refill_task_list(self):
		while self.count>self.numjobs*GAP:
			self.get_out()
		while not self.outstanding:
			if self.count:
				self.get_out()
			elif self.frozen:
				try:
					cond=self.deadlock==self.processed
				except AttributeError:
					pass
				else:
					if cond:
						msg='check the build order for the tasks'
						for tsk in self.frozen:
							if not tsk.run_after:
								msg='check the methods runnable_status'
								break
						lst=[]
						for tsk in self.frozen:
							lst.append('%s\t-> %r'%(repr(tsk),[id(x)for x in tsk.run_after]))
						raise Errors.WafError('Deadlock detected: %s%s'%(msg,''.join(lst)))
				self.deadlock=self.processed
			if self.frozen:
				self.outstanding+=self.frozen
				self.frozen=[]
			elif not self.count:
				self.outstanding.extend(self.biter.next())
				self.total=self.bld.total()
				break
	def add_more_tasks(self,tsk):
		if getattr(tsk,'more_tasks',None):
			self.outstanding+=tsk.more_tasks
			self.total+=len(tsk.more_tasks)
	def get_out(self):
		tsk=self.out.get()
		if not self.stop:
			self.add_more_tasks(tsk)
		self.count-=1
		self.dirty=True
		return tsk
	def add_task(self,tsk):
		try:
			self.pool
		except AttributeError:
			self.init_task_pool()
		self.ready.put(tsk)
	def init_task_pool(self):
		pool=self.pool=[get_pool()for i in range(self.numjobs)]
		self.ready=Queue(0)
		def setq(consumer):
			consumer.ready=self.ready
		for x in pool:
			x.ready.put(setq)
		return pool
	def free_task_pool(self):
		def setq(consumer):
			consumer.ready=Queue(0)
			self.out.put(self)
		try:
			pool=self.pool
		except AttributeError:
			pass
		else:
			for x in pool:
				self.ready.put(setq)
			for x in pool:
				self.get_out()
			for x in pool:
				put_pool(x)
			self.pool=[]
	def skip(self,tsk):
		tsk.hasrun=Task.SKIPPED
	def error_handler(self,tsk):
		if hasattr(tsk,'scan')and hasattr(tsk,'uid'):
			key=(tsk.uid(),'imp')
			try:
				del self.bld.task_sigs[key]
			except KeyError:
				pass
		if not self.bld.keep:
			self.stop=True
		self.error.append(tsk)
	def task_status(self,tsk):
		try:
			return tsk.runnable_status()
		except Exception:
			self.processed+=1
			tsk.err_msg=Utils.ex_stack()
			if not self.stop and self.bld.keep:
				self.skip(tsk)
				if self.bld.keep==1:
					if Logs.verbose>1 or not self.error:
						self.error.append(tsk)
					self.stop=True
				else:
					if Logs.verbose>1:
						self.error.append(tsk)
				return Task.EXCEPTION
			tsk.hasrun=Task.EXCEPTION
			self.error_handler(tsk)
			return Task.EXCEPTION
	def start(self):
		self.total=self.bld.total()
		while not self.stop:
			self.refill_task_list()
			tsk=self.get_next_task()
			if not tsk:
				if self.count:
					continue
				else:
					break
			if tsk.hasrun:
				self.processed+=1
				continue
			if self.stop:
				break
			st=self.task_status(tsk)
			if st==Task.RUN_ME:
				tsk.position=(self.processed,self.total)
				self.count+=1
				tsk.master=self
				self.processed+=1
				if self.numjobs==1:
					tsk.process()
				else:
					self.add_task(tsk)
			if st==Task.ASK_LATER:
				self.postpone(tsk)
			elif st==Task.SKIP_ME:
				self.processed+=1
				self.skip(tsk)
				self.add_more_tasks(tsk)
		while self.error and self.count:
			self.get_out()
		assert(self.count==0 or self.stop)
		self.free_task_pool()
