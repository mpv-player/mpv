#! /usr/bin/env python
# encoding: utf-8
# WARNING! Do not edit! https://waf.io/book/index.html#_obtaining_the_waf_file

from waflib import Utils
from waflib.Configure import conf
@conf
def d_platform_flags(self):
	v=self.env
	if not v.DEST_OS:
		v.DEST_OS=Utils.unversioned_sys_platform()
	binfmt=Utils.destos_to_binfmt(self.env.DEST_OS)
	if binfmt=='pe':
		v['dprogram_PATTERN']='%s.exe'
		v['dshlib_PATTERN']='lib%s.dll'
		v['dstlib_PATTERN']='lib%s.a'
	elif binfmt=='mac-o':
		v['dprogram_PATTERN']='%s'
		v['dshlib_PATTERN']='lib%s.dylib'
		v['dstlib_PATTERN']='lib%s.a'
	else:
		v['dprogram_PATTERN']='%s'
		v['dshlib_PATTERN']='lib%s.so'
		v['dstlib_PATTERN']='lib%s.a'
DLIB='''
version(D_Version2) {
	import std.stdio;
	int main() {
		writefln("phobos2");
		return 0;
	}
} else {
	version(Tango) {
		import tango.stdc.stdio;
		int main() {
			printf("tango");
			return 0;
		}
	} else {
		import std.stdio;
		int main() {
			writefln("phobos1");
			return 0;
		}
	}
}
'''
@conf
def check_dlibrary(self,execute=True):
	ret=self.check_cc(features='d dprogram',fragment=DLIB,compile_filename='test.d',execute=execute,define_ret=True)
	if execute:
		self.env.DLIBRARY=ret.strip()
