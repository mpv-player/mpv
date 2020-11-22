from waflib.Build import BuildContext
from waflib import TaskGen, Utils
from io import StringIO
from TOOLS.matroska import generate_C_header, generate_C_definitions
from TOOLS.file2string import file2string
import os

def __wayland_scanner_cmd__(ctx, mode, dir, src, vendored_file):
    return "${{WAYSCAN}} {0} < {1} > ${{TGT}}".format(
        mode,
        "${SRC}" if vendored_file else "{}/{}".format(dir, src)
    )

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
        file2string(source, iter(src.read('rb').splitlines(True)), out)
    execf(self, fn)

@TaskGen.feature('ebml_header')
def ebml_header(self):
    execf(self, generate_C_header)

@TaskGen.feature('ebml_definitions')
def ebml_definitions(self):
    execf(self, generate_C_definitions)

def __wayland_protocol_code__(ctx, **kwargs):
    protocol_is_vendored = kwargs.get("vendored_protocol", False)
    file_name = kwargs['protocol'] + '.xml'

    if protocol_is_vendored:
        del kwargs['vendored_protocol']
        kwargs['source'] = '{}/{}'.format(kwargs['proto_dir'], file_name)

    ctx(
        rule   = __wayland_scanner_cmd__(ctx, 'private-code', kwargs['proto_dir'],
                                         file_name,
                                         protocol_is_vendored),
        name   = os.path.basename(kwargs['target']),
        **kwargs
    )

def __wayland_protocol_header__(ctx, **kwargs):
    protocol_is_vendored = kwargs.get("vendored_protocol", False)
    file_name = kwargs['protocol'] + '.xml'

    if protocol_is_vendored:
        del kwargs['vendored_protocol']
        kwargs['source'] = '{}/{}'.format(kwargs['proto_dir'], file_name)

    ctx(
        rule   = __wayland_scanner_cmd__(ctx, 'client-header', kwargs['proto_dir'],
                                         file_name,
                                         protocol_is_vendored),
        before = ('c',),
        name   = os.path.basename(kwargs['target']),
        **kwargs
    )

@TaskGen.feature('cprogram')
@TaskGen.feature('cshlib')
@TaskGen.feature('cstlib')
@TaskGen.feature('apply_link')
@TaskGen.after_method('process_source', 'process_use', 'apply_link', 'process_uselib_local', 'propagate_uselib_vars', 'do_the_symbol_stuff')
def handle_add_object(tgen):
    if getattr(tgen, 'add_objects', None):
        for input in tgen.add_objects:
            input_node = tgen.path.find_resource(input)
            if input_node is not None:
                tgen.link_task.inputs.append(input_node)

BuildContext.file2string             = __file2string__
BuildContext.wayland_protocol_code   = __wayland_protocol_code__
BuildContext.wayland_protocol_header = __wayland_protocol_header__
