#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

import re
from waflib import Utils,Logs
def filter_comments(filename):
	txt=Utils.readf(filename)
	i=0
	buf=[]
	max=len(txt)
	begin=0
	while i<max:
		c=txt[i]
		if c=='"'or c=="'":
			buf.append(txt[begin:i])
			delim=c
			i+=1
			while i<max:
				c=txt[i]
				if c==delim:break
				elif c=='\\':
					i+=1
				i+=1
			i+=1
			begin=i
		elif c=='/':
			buf.append(txt[begin:i])
			i+=1
			if i==max:break
			c=txt[i]
			if c=='+':
				i+=1
				nesting=1
				c=None
				while i<max:
					prev=c
					c=txt[i]
					if prev=='/'and c=='+':
						nesting+=1
						c=None
					elif prev=='+'and c=='/':
						nesting-=1
						if nesting==0:break
						c=None
					i+=1
			elif c=='*':
				i+=1
				c=None
				while i<max:
					prev=c
					c=txt[i]
					if prev=='*'and c=='/':break
					i+=1
			elif c=='/':
				i+=1
				while i<max and txt[i]!='\n':
					i+=1
			else:
				begin=i-1
				continue
			i+=1
			begin=i
			buf.append(' ')
		else:
			i+=1
	buf.append(txt[begin:])
	return buf
class d_parser(object):
	def __init__(self,env,incpaths):
		self.allnames=[]
		self.re_module=re.compile("module\s+([^;]+)")
		self.re_import=re.compile("import\s+([^;]+)")
		self.re_import_bindings=re.compile("([^:]+):(.*)")
		self.re_import_alias=re.compile("[^=]+=(.+)")
		self.env=env
		self.nodes=[]
		self.names=[]
		self.incpaths=incpaths
	def tryfind(self,filename):
		found=0
		for n in self.incpaths:
			found=n.find_resource(filename.replace('.','/')+'.d')
			if found:
				self.nodes.append(found)
				self.waiting.append(found)
				break
		if not found:
			if not filename in self.names:
				self.names.append(filename)
	def get_strings(self,code):
		self.module=''
		lst=[]
		mod_name=self.re_module.search(code)
		if mod_name:
			self.module=re.sub('\s+','',mod_name.group(1))
		import_iterator=self.re_import.finditer(code)
		if import_iterator:
			for import_match in import_iterator:
				import_match_str=re.sub('\s+','',import_match.group(1))
				bindings_match=self.re_import_bindings.match(import_match_str)
				if bindings_match:
					import_match_str=bindings_match.group(1)
				matches=import_match_str.split(',')
				for match in matches:
					alias_match=self.re_import_alias.match(match)
					if alias_match:
						match=alias_match.group(1)
					lst.append(match)
		return lst
	def start(self,node):
		self.waiting=[node]
		while self.waiting:
			nd=self.waiting.pop(0)
			self.iter(nd)
	def iter(self,node):
		path=node.abspath()
		code="".join(filter_comments(path))
		names=self.get_strings(code)
		for x in names:
			if x in self.allnames:continue
			self.allnames.append(x)
			self.tryfind(x)
def scan(self):
	env=self.env
	gruik=d_parser(env,self.generator.includes_nodes)
	node=self.inputs[0]
	gruik.start(node)
	nodes=gruik.nodes
	names=gruik.names
	if Logs.verbose:
		Logs.debug('deps: deps for %s: %r; unresolved %r'%(str(node),nodes,names))
	return(nodes,names)
