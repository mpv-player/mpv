#!/usr/bin/env python3
#
# This file is part of mpv.
#
# mpv is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# mpv is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
#
"""
Tool to generate a small shim library from header files.
The shim library will dynamically dlopen() a backing library and forward the
declarations from the header files to it.
"""

import argparse
import sys

import pycparser
from pycparser import c_ast, c_generator
from pycparser.c_ast import (
    ID,
    Return,
    FuncDecl,
    FuncCall,
    Compound,
    ExprList,
    FileAST,
    FuncDef,
    PtrDecl,
    Decl,
    NodeVisitor,
    TypeDecl,
    IdentifierType,
    ParamList,
    Typename,
    Constant,
    BinaryOp,
    Assignment,
)


def get_name(decl):
    t = decl.type

    if isinstance(t, PtrDecl):
        return t.type.declname
    else:
        return t.declname


def get_type_decl(decl):
    if isinstance(decl, PtrDecl):
        return decl.type
    else:
        return decl


def has_args(decl):
    if len(decl.args.params) == 0:
        return False

    if len(decl.args.params) > 1:
        return True

    param = decl.args.params[0].type
    type_decl = get_type_decl(param)
    return not (type_decl.declname is None and param.type.names == ["void"])


class DeclarationExtractor(NodeVisitor):
    def __init__(self):
        self.decls = []

    def visit_FuncDecl(self, node):
        self.decls.append(node)

    def visit_Typedef(self, node):
        pass


def find_declarations(compiler, includedir, f):
    ast = pycparser.parse_file(
        f,
        use_cpp=True,
        cpp_path=compiler,
        cpp_args=[
            r"-E",

            # undefine symbols that are used to define MPV_EXPORT
            # pycparser does not accept attribute syntax
            r"-U_WIN32",
            r"-U__GNUC__",
            r"-U__clang__",

            r"-nostdinc",
            r"-I" + includedir,
        ],
    )
    e = DeclarationExtractor()
    e.visit(ast)
    return e.decls


def unique_elements(l, keyextractor):
    seen = set()
    for x in l:
        k = keyextractor(x)
        if k not in seen:
            seen.add(k)
            yield x


def generate_shim_function(decl):
    orig_name = "orig_" + get_name(decl)
    ha = has_args(decl)
    return [
        # Pointer to the real function:
        #
        # static unsigned int (*orig_decl_name)(int arg);
        Decl(
            orig_name,
            [],
            [],
            ["static"],
            [],
            PtrDecl(
                [],
                FuncDecl(decl.args, TypeDecl(orig_name, [], None, decl.type)),
            ),
            None,
            None,
        ),
        # Shim function:
        #
        # unsigned int decl_name(int arg)
        # {
        #   return orig_decl_name(arg);
        # }
        FuncDef(
            decl,
            None,
            Compound(
                [
                    Return(
                        FuncCall(
                            ID(orig_name),
                            ExprList([ID(p.name) for p in decl.args.params if ha]),
                        )
                    )
                ]
            ),
        ),
    ]


def generate_init_function(decls):
    body = [
        Decl(
            "handle",
            [],
            [],
            [],
            [],
            PtrDecl([], TypeDecl("handle", [], None, IdentifierType(["void"]))),
            FuncCall(
                ID("dlopen"),
                ExprList(
                    [
                        Constant("string", '"{}"'.format(args.library)),
                        BinaryOp("|", ID("RTLD_LAZY"), ID("RTLD_GLOBAL")),
                    ]
                ),
            ),
            None,
        ),
        FuncCall(ID("assert"), ExprList([ID("handle")])),
    ]

    for decl in decls:
        orig_name = "orig_" + get_name(decl)
        body.append(
            Assignment(
                "=",
                ID(orig_name),
                FuncCall(
                    ID("dlsym"),
                    ExprList(
                        [
                            ID("handle"),
                            Constant("string", '"{}"'.format(get_name(decl))),
                        ]
                    ),
                ),
            )
        )
        body.append(
            FuncCall(ID("assert"), ExprList([ID(orig_name)])),
        )

    return FuncDef(
        FuncDecl(
            ParamList(
                [
                    Typename(
                        None,
                        [],
                        None,
                        TypeDecl(None, [], None, IdentifierType(["void"])),
                    )
                ]
            ),
            TypeDecl(
                "init",
                ["__attribute__((constructor))", "static"],
                None,
                IdentifierType(["void"]),
            ),
        ),
        None,
        Compound(body),
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("headers", metavar="HEADER", nargs="+")
    parser.add_argument("--output", required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--includedir", required=True)
    parser.add_argument("--library", required=True)
    args = parser.parse_args()

    decls = []

    for i in args.headers:
        decls.extend(find_declarations(args.compiler, args.includedir, i))

    decls = list(unique_elements(decls, get_name))

    statements = []
    for decl in decls:
        statements.extend(generate_shim_function(decl))

    statements.append(generate_init_function(decls))

    ast = FileAST(statements)
    generator = c_generator.CGenerator()
    source = generator.visit(ast)

    with open(args.output, "w") as f:
        # pycparser does not want to write #includes
        print("#include <dlfcn.h>", file=f)
        print("#include <assert.h>", file=f)
        for i in args.headers:
            print('#include "{}"'.format(i), file=f)
        print(source, file=f)
