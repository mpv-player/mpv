#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import os,sys,errno,traceback,inspect,re,shutil,datetime,gc,platform
import subprocess
from collections import deque,defaultdict
try:
	import _winreg as winreg
except ImportError:
	try:
		import winreg
	except ImportError:
		winreg=None
from waflib import Errors
try:
	from collections import UserDict
except ImportError:
	from UserDict import UserDict
try:
	from hashlib import md5
except ImportError:
	try:
		from md5 import md5
	except ImportError:
		pass
try:
	import threading
except ImportError:
	if not'JOBS'in os.environ:
		os.environ['JOBS']='1'
	class threading(object):
		pass
	class Lock(object):
		def acquire(self):
			pass
		def release(self):
			pass
	threading.Lock=threading.Thread=Lock
else:
	run_old=threading.Thread.run
	def run(*args,**kwargs):
		try:
			run_old(*args,**kwargs)
		except(KeyboardInterrupt,SystemExit):
			raise
		except Exception:
			sys.excepthook(*sys.exc_info())
	threading.Thread.run=run
SIG_NIL='iluvcuteoverload'
O644=420
O755=493
rot_chr=['\\','|','/','-']
rot_idx=0
try:
	from collections import OrderedDict as ordered_iter_dict
except ImportError:
	class ordered_iter_dict(dict):
		def __init__(self,*k,**kw):
			self.lst=[]
			dict.__init__(self,*k,**kw)
		def clear(self):
			dict.clear(self)
			self.lst=[]
		def __setitem__(self,key,value):
			dict.__setitem__(self,key,value)
			try:
				self.lst.remove(key)
			except ValueError:
				pass
			self.lst.append(key)
		def __delitem__(self,key):
			dict.__delitem__(self,key)
			try:
				self.lst.remove(key)
			except ValueError:
				pass
		def __iter__(self):
			for x in self.lst:
				yield x
		def keys(self):
			return self.lst
is_win32=os.sep=='\\'or sys.platform=='win32'
def readf(fname,m='r',encoding='ISO8859-1'):
	if sys.hexversion>0x3000000 and not'b'in m:
		m+='b'
		f=open(fname,m)
		try:
			txt=f.read()
		finally:
			f.close()
		if encoding:
			txt=txt.decode(encoding)
		else:
			txt=txt.decode()
	else:
		f=open(fname,m)
		try:
			txt=f.read()
		finally:
			f.close()
	return txt
def writef(fname,data,m='w',encoding='ISO8859-1'):
	if sys.hexversion>0x3000000 and not'b'in m:
		data=data.encode(encoding)
		m+='b'
	f=open(fname,m)
	try:
		f.write(data)
	finally:
		f.close()
def h_file(fname):
	f=open(fname,'rb')
	m=md5()
	try:
		while fname:
			fname=f.read(200000)
			m.update(fname)
	finally:
		f.close()
	return m.digest()
def readf_win32(f,m='r',encoding='ISO8859-1'):
	flags=os.O_NOINHERIT|os.O_RDONLY
	if'b'in m:
		flags|=os.O_BINARY
	if'+'in m:
		flags|=os.O_RDWR
	try:
		fd=os.open(f,flags)
	except OSError:
		raise IOError('Cannot read from %r'%f)
	if sys.hexversion>0x3000000 and not'b'in m:
		m+='b'
		f=os.fdopen(fd,m)
		try:
			txt=f.read()
		finally:
			f.close()
		if encoding:
			txt=txt.decode(encoding)
		else:
			txt=txt.decode()
	else:
		f=os.fdopen(fd,m)
		try:
			txt=f.read()
		finally:
			f.close()
	return txt
def writef_win32(f,data,m='w',encoding='ISO8859-1'):
	if sys.hexversion>0x3000000 and not'b'in m:
		data=data.encode(encoding)
		m+='b'
	flags=os.O_CREAT|os.O_TRUNC|os.O_WRONLY|os.O_NOINHERIT
	if'b'in m:
		flags|=os.O_BINARY
	if'+'in m:
		flags|=os.O_RDWR
	try:
		fd=os.open(f,flags)
	except OSError:
		raise IOError('Cannot write to %r'%f)
	f=os.fdopen(fd,m)
	try:
		f.write(data)
	finally:
		f.close()
def h_file_win32(fname):
	try:
		fd=os.open(fname,os.O_BINARY|os.O_RDONLY|os.O_NOINHERIT)
	except OSError:
		raise IOError('Cannot read from %r'%fname)
	f=os.fdopen(fd,'rb')
	m=md5()
	try:
		while fname:
			fname=f.read(200000)
			m.update(fname)
	finally:
		f.close()
	return m.digest()
readf_unix=readf
writef_unix=writef
h_file_unix=h_file
if hasattr(os,'O_NOINHERIT')and sys.hexversion<0x3040000:
	readf=readf_win32
	writef=writef_win32
	h_file=h_file_win32
try:
	x=''.encode('hex')
except LookupError:
	import binascii
	def to_hex(s):
		ret=binascii.hexlify(s)
		if not isinstance(ret,str):
			ret=ret.decode('utf-8')
		return ret
else:
	def to_hex(s):
		return s.encode('hex')
to_hex.__doc__="""
Return the hexadecimal representation of a string

:param s: string to convert
:type s: string
"""
def listdir_win32(s):
	if not s:
		try:
			import ctypes
		except ImportError:
			return[x+':\\'for x in list('ABCDEFGHIJKLMNOPQRSTUVWXYZ')]
		else:
			dlen=4
			maxdrives=26
			buf=ctypes.create_string_buffer(maxdrives*dlen)
			ndrives=ctypes.windll.kernel32.GetLogicalDriveStringsA(maxdrives*dlen,ctypes.byref(buf))
			return[str(buf.raw[4*i:4*i+2].decode('ascii'))for i in range(int(ndrives/dlen))]
	if len(s)==2 and s[1]==":":
		s+=os.sep
	if not os.path.isdir(s):
		e=OSError('%s is not a directory'%s)
		e.errno=errno.ENOENT
		raise e
	return os.listdir(s)
listdir=os.listdir
if is_win32:
	listdir=listdir_win32
def num2ver(ver):
	if isinstance(ver,str):
		ver=tuple(ver.split('.'))
	if isinstance(ver,tuple):
		ret=0
		for i in range(4):
			if i<len(ver):
				ret+=256**(3-i)*int(ver[i])
		return ret
	return ver
def ex_stack():
	exc_type,exc_value,tb=sys.exc_info()
	exc_lines=traceback.format_exception(exc_type,exc_value,tb)
	return''.join(exc_lines)
def to_list(sth):
	if isinstance(sth,str):
		return sth.split()
	else:
		return sth
def split_path_unix(path):
	return path.split('/')
def split_path_cygwin(path):
	if path.startswith('//'):
		ret=path.split('/')[2:]
		ret[0]='/'+ret[0]
		return ret
	return path.split('/')
re_sp=re.compile('[/\\\\]')
def split_path_win32(path):
	if path.startswith('\\\\'):
		ret=re.split(re_sp,path)[2:]
		ret[0]='\\'+ret[0]
		return ret
	return re.split(re_sp,path)
msysroot=None
def split_path_msys(path):
	if(path.startswith('/')or path.startswith('\\'))and not path.startswith('//')and not path.startswith('\\\\'):
		global msysroot
		if not msysroot:
			msysroot=subprocess.check_output(['cygpath','-w','/']).decode(sys.stdout.encoding or'iso8859-1')
			msysroot=msysroot.strip()
		path=os.path.normpath(msysroot+os.sep+path)
	return split_path_win32(path)
if sys.platform=='cygwin':
	split_path=split_path_cygwin
elif is_win32:
	if os.environ.get('MSYSTEM',None):
		split_path=split_path_msys
	else:
		split_path=split_path_win32
else:
	split_path=split_path_unix
split_path.__doc__="""
Split a path by / or \\. This function is not like os.path.split

:type  path: string
:param path: path to split
:return:     list of strings
"""
def check_dir(path):
	if not os.path.isdir(path):
		try:
			os.makedirs(path)
		except OSError ,e:
			if not os.path.isdir(path):
				raise Errors.WafError('Cannot create the folder %r'%path,ex=e)
def check_exe(name,env=None):
	if not name:
		raise ValueError('Cannot execute an empty string!')
	def is_exe(fpath):
		return os.path.isfile(fpath)and os.access(fpath,os.X_OK)
	fpath,fname=os.path.split(name)
	if fpath and is_exe(name):
		return os.path.abspath(name)
	else:
		env=env or os.environ
		for path in env["PATH"].split(os.pathsep):
			path=path.strip('"')
			exe_file=os.path.join(path,name)
			if is_exe(exe_file):
				return os.path.abspath(exe_file)
	return None
def def_attrs(cls,**kw):
	for k,v in kw.items():
		if not hasattr(cls,k):
			setattr(cls,k,v)
def quote_define_name(s):
	fu=re.sub('[^a-zA-Z0-9]','_',s)
	fu=re.sub('_+','_',fu)
	fu=fu.upper()
	return fu
def h_list(lst):
	m=md5()
	m.update(str(lst))
	return m.digest()
def h_fun(fun):
	try:
		return fun.code
	except AttributeError:
		try:
			h=inspect.getsource(fun)
		except IOError:
			h="nocode"
		try:
			fun.code=h
		except AttributeError:
			pass
		return h
def h_cmd(ins):
	if isinstance(ins,str):
		ret=ins
	elif isinstance(ins,list)or isinstance(ins,tuple):
		ret=str([h_cmd(x)for x in ins])
	else:
		ret=str(h_fun(ins))
	if sys.hexversion>0x3000000:
		ret=ret.encode('iso8859-1','xmlcharrefreplace')
	return ret
reg_subst=re.compile(r"(\\\\)|(\$\$)|\$\{([^}]+)\}")
def subst_vars(expr,params):
	def repl_var(m):
		if m.group(1):
			return'\\'
		if m.group(2):
			return'$'
		try:
			return params.get_flat(m.group(3))
		except AttributeError:
			return params[m.group(3)]
	return reg_subst.sub(repl_var,expr)
def destos_to_binfmt(key):
	if key=='darwin':
		return'mac-o'
	elif key in('win32','cygwin','uwin','msys'):
		return'pe'
	return'elf'
def unversioned_sys_platform():
	s=sys.platform
	if s.startswith('java'):
		from java.lang import System
		s=System.getProperty('os.name')
		if s=='Mac OS X':
			return'darwin'
		elif s.startswith('Windows '):
			return'win32'
		elif s=='OS/2':
			return'os2'
		elif s=='HP-UX':
			return'hp-ux'
		elif s in('SunOS','Solaris'):
			return'sunos'
		else:s=s.lower()
	if s=='powerpc':
		return'darwin'
	if s=='win32'or s=='os2':
		return s
	if s=='cli'and os.name=='nt':
		return'win32'
	return re.split('\d+$',s)[0]
def nada(*k,**kw):
	pass
class Timer(object):
	def __init__(self):
		self.start_time=datetime.datetime.utcnow()
	def __str__(self):
		delta=datetime.datetime.utcnow()-self.start_time
		days=delta.days
		hours,rem=divmod(delta.seconds,3600)
		minutes,seconds=divmod(rem,60)
		seconds+=delta.microseconds*1e-6
		result=''
		if days:
			result+='%dd'%days
		if days or hours:
			result+='%dh'%hours
		if days or hours or minutes:
			result+='%dm'%minutes
		return'%s%.3fs'%(result,seconds)
if is_win32:
	old=shutil.copy2
	def copy2(src,dst):
		old(src,dst)
		shutil.copystat(src,dst)
	setattr(shutil,'copy2',copy2)
if os.name=='java':
	try:
		gc.disable()
		gc.enable()
	except NotImplementedError:
		gc.disable=gc.enable
def read_la_file(path):
	sp=re.compile(r'^([^=]+)=\'(.*)\'$')
	dc={}
	for line in readf(path).splitlines():
		try:
			_,left,right,_=sp.split(line.strip())
			dc[left]=right
		except ValueError:
			pass
	return dc
def nogc(fun):
	def f(*k,**kw):
		try:
			gc.disable()
			ret=fun(*k,**kw)
		finally:
			gc.enable()
		return ret
	f.__doc__=fun.__doc__
	return f
def run_once(fun):
	cache={}
	def wrap(k):
		try:
			return cache[k]
		except KeyError:
			ret=fun(k)
			cache[k]=ret
			return ret
	wrap.__cache__=cache
	wrap.__name__=fun.__name__
	return wrap
def get_registry_app_path(key,filename):
	if not winreg:
		return None
	try:
		result=winreg.QueryValue(key,"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\%s.exe"%filename[0])
	except WindowsError:
		pass
	else:
		if os.path.isfile(result):
			return result
def lib64():
	if os.sep=='/':
		if platform.architecture()[0]=='64bit':
			if os.path.exists('/usr/lib64')and not os.path.exists('/usr/lib32'):
				return'64'
	return''
def sane_path(p):
	return os.path.abspath(os.path.expanduser(p))
