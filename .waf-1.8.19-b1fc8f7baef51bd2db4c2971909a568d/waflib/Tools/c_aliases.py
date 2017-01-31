#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

from waflib import Utils,Errors
from waflib.Configure import conf
def get_extensions(lst):
	ret=[]
	for x in Utils.to_list(lst):
		try:
			if not isinstance(x,str):
				x=x.name
			ret.append(x[x.rfind('.')+1:])
		except Exception:
			pass
	return ret
def sniff_features(**kw):
	exts=get_extensions(kw['source'])
	type=kw['_type']
	feats=[]
	for x in'cxx cpp c++ cc C'.split():
		if x in exts:
			feats.append('cxx')
			break
	if'c'in exts or'vala'in exts or'gs'in exts:
		feats.append('c')
	for x in'f f90 F F90 for FOR'.split():
		if x in exts:
			feats.append('fc')
			break
	if'd'in exts:
		feats.append('d')
	if'java'in exts:
		feats.append('java')
		return'java'
	if type in('program','shlib','stlib'):
		will_link=False
		for x in feats:
			if x in('cxx','d','fc','c'):
				feats.append(x+type)
				will_link=True
		if not will_link and not kw.get('features',[]):
			raise Errors.WafError('Cannot link from %r, try passing eg: features="c cprogram"?'%kw)
	return feats
def set_features(kw,_type):
	kw['_type']=_type
	kw['features']=Utils.to_list(kw.get('features',[]))+Utils.to_list(sniff_features(**kw))
@conf
def program(bld,*k,**kw):
	set_features(kw,'program')
	return bld(*k,**kw)
@conf
def shlib(bld,*k,**kw):
	set_features(kw,'shlib')
	return bld(*k,**kw)
@conf
def stlib(bld,*k,**kw):
	set_features(kw,'stlib')
	return bld(*k,**kw)
@conf
def objects(bld,*k,**kw):
	set_features(kw,'objects')
	return bld(*k,**kw)
