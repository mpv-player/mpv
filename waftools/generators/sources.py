from waflib.Build import BuildContext
from waflib import TaskGen
from io import StringIO
from TOOLS.matroska import generate_C_header, generate_C_definitions
from TOOLS.file2string import file2string
import os

def __zshcomp_cmd__(ctx, argument):
    return '"${{BIN_PERL}}" "{0}/TOOLS/zsh.pl" "{1}" > "${{TGT}}"' \
                .format(ctx.srcnode.abspath(), argument)

def __wayland_scanner_cmd__(ctx, mode, dir, src):
    return "${{WAYSCAN}} {0} < {1}/{2} > ${{TGT}}".format(mode, dir, src)

def __file2string__(ctx, **kwargs):
    ctx(
        rule   = __file2string_cmd__(ctx),
        before = ("c",),
        name   = os.path.basename(kwargs['target']),
        **kwargs
    )

def execf(self, fn):
    setattr(self, 'before', ['c'])
    setattr(self, 'rule', ' ') # waf doesn't print the task with no rule
    target = getattr(self, 'target', None)
    out = self.path.find_or_declare(target)
    tmp = StringIO()
    fn(tmp)
    out.write(tmp.getvalue())
    tmp.close()

@TaskGen.feature('file2string')
def f2s(self):
    def fn(out):
        source = getattr(self, 'source', None)
        src = self.path.find_resource(source)
        file2string(source, iter(src.read().splitlines(True)), out)
    execf(self, fn)

@TaskGen.feature('ebml_header')
def ebml_header(self):
    execf(self, generate_C_header)

@TaskGen.feature('ebml_definitions')
def ebml_definitions(self):
    execf(self, generate_C_definitions)

def __zshcomp__(ctx, **kwargs):
    ctx(
        rule   = __zshcomp_cmd__(ctx, ctx.bldnode.abspath() + '/mpv'),
        after = ("c", "cprogram",),
        name   = os.path.basename(kwargs['target']),
        **kwargs
    )

def __wayland_protocol_code__(ctx, **kwargs):
    ctx(
        rule   = __wayland_scanner_cmd__(ctx, 'code', kwargs['proto_dir'],
                                         kwargs['protocol'] + '.xml'),
        name   = os.path.basename(kwargs['target']),
        **kwargs
    )

def __wayland_protocol_header__(ctx, **kwargs):
    ctx(
        rule   = __wayland_scanner_cmd__(ctx, 'client-header', kwargs['proto_dir'],
                                         kwargs['protocol'] + '.xml'),
        before = ('c',),
        name   = os.path.basename(kwargs['target']),
        **kwargs
    )

BuildContext.file2string             = __file2string__
BuildContext.wayland_protocol_code   = __wayland_protocol_code__
BuildContext.wayland_protocol_header = __wayland_protocol_header__
BuildContext.zshcomp                 = __zshcomp__
