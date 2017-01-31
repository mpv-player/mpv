#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import os,re,traceback,sys
from waflib import Utils,ansiterm
if not os.environ.get('NOSYNC',False):
	if sys.stdout.isatty()and id(sys.stdout)==id(sys.__stdout__):
		sys.stdout=ansiterm.AnsiTerm(sys.stdout)
	if sys.stderr.isatty()and id(sys.stderr)==id(sys.__stderr__):
		sys.stderr=ansiterm.AnsiTerm(sys.stderr)
import logging
LOG_FORMAT=os.environ.get('WAF_LOG_FORMAT','%(asctime)s %(c1)s%(zone)s%(c2)s %(message)s')
HOUR_FORMAT=os.environ.get('WAF_HOUR_FORMAT','%H:%M:%S')
zones=''
verbose=0
colors_lst={'USE':True,'BOLD':'\x1b[01;1m','RED':'\x1b[01;31m','GREEN':'\x1b[32m','YELLOW':'\x1b[33m','PINK':'\x1b[35m','BLUE':'\x1b[01;34m','CYAN':'\x1b[36m','GREY':'\x1b[37m','NORMAL':'\x1b[0m','cursor_on':'\x1b[?25h','cursor_off':'\x1b[?25l',}
indicator='\r\x1b[K%s%s%s'
try:
	unicode
except NameError:
	unicode=None
def enable_colors(use):
	if use==1:
		if not(sys.stderr.isatty()or sys.stdout.isatty()):
			use=0
		if Utils.is_win32 and os.name!='java':
			term=os.environ.get('TERM','')
		else:
			term=os.environ.get('TERM','dumb')
		if term in('dumb','emacs'):
			use=0
	if use>=1:
		os.environ['TERM']='vt100'
	colors_lst['USE']=use
try:
	get_term_cols=ansiterm.get_term_cols
except AttributeError:
	def get_term_cols():
		return 80
get_term_cols.__doc__="""
	Get the console width in characters.

	:return: the number of characters per line
	:rtype: int
	"""
def get_color(cl):
	if not colors_lst['USE']:return''
	return colors_lst.get(cl,'')
class color_dict(object):
	def __getattr__(self,a):
		return get_color(a)
	def __call__(self,a):
		return get_color(a)
colors=color_dict()
re_log=re.compile(r'(\w+): (.*)',re.M)
class log_filter(logging.Filter):
	def __init__(self,name=None):
		pass
	def filter(self,rec):
		rec.zone=rec.module
		if rec.levelno>=logging.INFO:
			return True
		m=re_log.match(rec.msg)
		if m:
			rec.zone=m.group(1)
			rec.msg=m.group(2)
		if zones:
			return getattr(rec,'zone','')in zones or'*'in zones
		elif not verbose>2:
			return False
		return True
class log_handler(logging.StreamHandler):
	def emit(self,record):
		try:
			try:
				self.stream=record.stream
			except AttributeError:
				if record.levelno>=logging.WARNING:
					record.stream=self.stream=sys.stderr
				else:
					record.stream=self.stream=sys.stdout
			self.emit_override(record)
			self.flush()
		except(KeyboardInterrupt,SystemExit):
			raise
		except:
			self.handleError(record)
	def emit_override(self,record,**kw):
		self.terminator=getattr(record,'terminator','\n')
		stream=self.stream
		if unicode:
			msg=self.formatter.format(record)
			fs='%s'+self.terminator
			try:
				if(isinstance(msg,unicode)and getattr(stream,'encoding',None)):
					fs=fs.decode(stream.encoding)
					try:
						stream.write(fs%msg)
					except UnicodeEncodeError:
						stream.write((fs%msg).encode(stream.encoding))
				else:
					stream.write(fs%msg)
			except UnicodeError:
				stream.write((fs%msg).encode("UTF-8"))
		else:
			logging.StreamHandler.emit(self,record)
class formatter(logging.Formatter):
	def __init__(self):
		logging.Formatter.__init__(self,LOG_FORMAT,HOUR_FORMAT)
	def format(self,rec):
		try:
			msg=rec.msg.decode('utf-8')
		except Exception:
			msg=rec.msg
		use=colors_lst['USE']
		if(use==1 and rec.stream.isatty())or use==2:
			c1=getattr(rec,'c1',None)
			if c1 is None:
				c1=''
				if rec.levelno>=logging.ERROR:
					c1=colors.RED
				elif rec.levelno>=logging.WARNING:
					c1=colors.YELLOW
				elif rec.levelno>=logging.INFO:
					c1=colors.GREEN
			c2=getattr(rec,'c2',colors.NORMAL)
			msg='%s%s%s'%(c1,msg,c2)
		else:
			msg=msg.replace('\r','\n')
			msg=re.sub(r'\x1B\[(K|.*?(m|h|l))','',msg)
		if rec.levelno>=logging.INFO:
			return msg
		rec.msg=msg
		rec.c1=colors.PINK
		rec.c2=colors.NORMAL
		return logging.Formatter.format(self,rec)
log=None
def debug(*k,**kw):
	if verbose:
		k=list(k)
		k[0]=k[0].replace('\n',' ')
		global log
		log.debug(*k,**kw)
def error(*k,**kw):
	global log
	log.error(*k,**kw)
	if verbose>2:
		st=traceback.extract_stack()
		if st:
			st=st[:-1]
			buf=[]
			for filename,lineno,name,line in st:
				buf.append('  File "%s", line %d, in %s'%(filename,lineno,name))
				if line:
					buf.append('	%s'%line.strip())
			if buf:log.error("\n".join(buf))
def warn(*k,**kw):
	global log
	log.warn(*k,**kw)
def info(*k,**kw):
	global log
	log.info(*k,**kw)
def init_log():
	global log
	log=logging.getLogger('waflib')
	log.handlers=[]
	log.filters=[]
	hdlr=log_handler()
	hdlr.setFormatter(formatter())
	log.addHandler(hdlr)
	log.addFilter(log_filter())
	log.setLevel(logging.DEBUG)
def make_logger(path,name):
	logger=logging.getLogger(name)
	hdlr=logging.FileHandler(path,'w')
	formatter=logging.Formatter('%(message)s')
	hdlr.setFormatter(formatter)
	logger.addHandler(hdlr)
	logger.setLevel(logging.DEBUG)
	return logger
def make_mem_logger(name,to_log,size=8192):
	from logging.handlers import MemoryHandler
	logger=logging.getLogger(name)
	hdlr=MemoryHandler(size,target=to_log)
	formatter=logging.Formatter('%(message)s')
	hdlr.setFormatter(formatter)
	logger.addHandler(hdlr)
	logger.memhandler=hdlr
	logger.setLevel(logging.DEBUG)
	return logger
def free_logger(logger):
	try:
		for x in logger.handlers:
			x.close()
			logger.removeHandler(x)
	except Exception:
		pass
def pprint(col,msg,label='',sep='\n'):
	info("%s%s%s %s"%(colors(col),msg,colors.NORMAL,label),extra={'terminator':sep})
