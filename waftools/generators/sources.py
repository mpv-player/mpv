from waflib.Build import BuildContext
from waflib import TaskGen
from io import StringIO
from TOOLS.matroska import generate_C_header, generate_C_definitions
from TOOLS.file2string import file2string
import os

def __zshcomp_cmd__(ctx, argument):
    return '"${{BIN_PERL}}" "{0}/TOOLS/zsh.pl" "{1}" > "${{TGT}}"' \
                .format(ctx.srcnode.abspath(), argument)

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

BuildContext.file2string = __file2string__
BuildContext.zshcomp = __zshcomp__
