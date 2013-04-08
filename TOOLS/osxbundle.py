#!/usr/bin/env python

import os
import re
import shutil
import sys
from optparse import OptionParser

def sh(command):
    return os.popen(command).read()

def dylib_lst(input_file):
    return sh("otool -L %s | grep -e '\t' | awk '{ print $1 }'" % input_file)

sys_re = re.compile("/System")
exe_re = re.compile("@executable_path")
binary_name = sys.argv[1]

def is_user_lib(libname, input_file):
    return not sys_re.match(libname) and \
           not exe_re.match(libname) and \
           not "libobjc" in libname and \
           not "libSystem" in libname and \
           not "libgcc" in libname and \
           not os.path.basename(input_file) in libname and \
           not libname == ''

def user_dylib_lst(input_file):
    return [lib for lib in dylib_lst(input_file).split("\n") if
            is_user_lib(lib, input_file)]

def bundle_name(binary_name):
    return "%s.app" % binary_name

def target_plist(binary_name):
    return os.path.join(bundle_name(binary_name), 'Contents', 'Info.plist')

def target_directory(binary_name):
    return os.path.join(bundle_name(binary_name), 'Contents', 'MacOS')

def target_binary(binary_name):
    return os.path.join(target_directory(binary_name), binary_name)

def copy_bundle(binary_name):
    if os.path.isdir(bundle_name(binary_name)):
        shutil.rmtree(bundle_name(binary_name))
    shutil.copytree(
        os.path.join('TOOLS', 'osxbundle', bundle_name(binary_name)),
        bundle_name(binary_name))

def copy_binary(binary_name):
    shutil.copy(binary_name, target_binary(binary_name))

def run_install_name_tool(target_file, dylib_path, dest_dir, root=True):
    new_dylib_path = os.path.join("@executable_path", "lib",
        os.path.basename(dylib_path))

    sh("install_name_tool -change %s %s %s" % \
        (dylib_path, new_dylib_path, target_file))
    if root:
        sh("install_name_tool -id %s %s" % \
            (new_dylib_path, os.path.join(dest_dir,
                os.path.basename(dylib_path))))

def cp_dylibs(target_file, dest_dir):
    for dylib_path in user_dylib_lst(target_file):
        dylib_dest_path = os.path.join(dest_dir, os.path.basename(dylib_path))

        try:
            shutil.copy(dylib_path, dylib_dest_path)
        except IOError:
            sys.exit("%s uses library %s which is not available anymore" % \
                     (target_file, dylib_path) )

        os.chmod(dylib_dest_path, 0o755)
        cp_dylibs(dylib_dest_path, dest_dir)

def fix_dylibs_paths(target_file, dest_dir, root=True):
    for dylib_path in user_dylib_lst(target_file):
        dylib_dest_path = os.path.join(dest_dir, os.path.basename(dylib_path))
        run_install_name_tool(target_file, dylib_path, dest_dir, root)
        fix_dylibs_paths(dylib_dest_path, dest_dir, False)

def apply_plist_template(plist_file, version):
    sh("sed -i -e 's/{{VERSION}}/%s/g' %s" % (version, plist_file))

def bundle_dependencies(binary_name):
    lib_bundle_directory = os.path.join(target_directory(binary_name), "lib")
    cp_dylibs(binary_name, lib_bundle_directory)
    fix_dylibs_paths(target_binary(binary_name), lib_bundle_directory)

def main():
    version = sh("TOOLS/osxbundle/version.sh").strip()

    usage = "usage: %prog [options] arg"
    parser = OptionParser(usage)
    parser.add_option("-s", "--skip-deps", action="store_false", dest="deps",
                      default=True,
                      help="don't bundle the dependencies")

    (options, args) = parser.parse_args()

    if len(args) != 1:
        parser.error("incorrect number of arguments")
    else:
        binary_name = args[0]

    print("Creating Mac OS X application bundle (version: %s)..." % version)
    print("> copying bundle skeleton")
    copy_bundle(binary_name)
    print("> copying binary")
    copy_binary(binary_name)
    print("> generating Info.plist")
    apply_plist_template(target_plist(binary_name), version)

    if options.deps:
        print("> bundling dependencies")
        bundle_dependencies(binary_name)

    print("done.")

if __name__ == "__main__":
    main()
