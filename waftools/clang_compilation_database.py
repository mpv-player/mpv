#!/usr/bin/env python
# encoding: utf-8
# Christoph Koke, 2013
# Original source: waflib/extras/clang_compilation_database.py from
# waf git 15d14c7bdf2e (New BSD License)

"""
Writes the c and cpp compile commands into build/compile_commands.json
see http://clang.llvm.org/docs/JSONCompilationDatabase.html

Usage:

    def configure(conf):
        conf.load('compiler_cxx')
        ...
        conf.load('clang_compilation_database')
"""

import sys, os, json, shlex, pipes
from waflib import Logs, TaskGen
from waflib.Tools import c, cxx

if sys.hexversion >= 0x3030000:
        quote = shlex.quote
else:
        quote = pipes.quote

@TaskGen.feature('*')
@TaskGen.after_method('process_use')
def collect_compilation_db_tasks(self):
        "Add a compilation database entry for compiled tasks"
        try:
                clang_db = self.bld.clang_compilation_database_tasks
        except AttributeError:
                clang_db = self.bld.clang_compilation_database_tasks = []
                self.bld.add_post_fun(write_compilation_database)

        for task in getattr(self, 'compiled_tasks', []):
                if isinstance(task, (c.c, cxx.cxx)):
                        clang_db.append(task)

def write_compilation_database(ctx):
        "Write the clang compilation database as JSON"
        database_file = ctx.bldnode.make_node('compile_commands.json')
        Logs.info("Build commands will be stored in %s" % database_file.path_from(ctx.path))
        try:
                root = json.load(database_file)
        except IOError:
                root = []
        clang_db = dict((x["file"], x) for x in root)
        for task in getattr(ctx, 'clang_compilation_database_tasks', []):
                try:
                        cmd = task.last_cmd
                except AttributeError:
                        continue
                directory = getattr(task, 'cwd', ctx.variant_dir)
                f_node = task.inputs[0]
                filename = os.path.relpath(f_node.abspath(), directory)
                cmd = " ".join(map(quote, cmd))
                entry = {
                        "directory": directory,
                        "command": cmd,
                        "file": filename,
                }
                clang_db[filename] = entry
        root = list(clang_db.values())
        database_file.write(json.dumps(root, indent=2))
