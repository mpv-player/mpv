#!/usr/bin/env python3

import re
import os
import sys
import shutil
import subprocess
from functools import partial

sys_re = re.compile("^/System")
usr_re = re.compile("^/usr/lib/")
exe_re = re.compile("@executable_path")

def is_user_lib(objfile, libname):
    return not sys_re.match(libname) and \
           not usr_re.match(libname) and \
           not exe_re.match(libname) and \
           not "libobjc." in libname and \
           not "libSystem." in libname and \
           not "libc." in libname and \
           not "libgcc." in libname and \
           not os.path.basename(libname) == 'Python' and \
           not os.path.basename(objfile) in libname and \
           not "libswift" in libname

def otool(objfile):
    command = "otool -L '%s' | grep -e '\t' | awk '{ print $1 }'" % objfile
    output  = subprocess.check_output(command, shell = True, universal_newlines=True)
    return set(filter(partial(is_user_lib, objfile), output.split()))

def get_rpaths_dev_tools(binary):
    command = "otool -l '%s' | grep -A2 LC_RPATH | grep path | grep \"Xcode\|CommandLineTools\"" % binary
    result  = subprocess.check_output(command, shell = True, universal_newlines=True)
    pathRe = re.compile("^\s*path (.*) \(offset \d*\)$")
    output = []

    for line in result.splitlines():
        output.append(pathRe.search(line).group(1).strip())

    return output

def install_name_tool_change(old, new, objfile):
    subprocess.call(["install_name_tool", "-change", old, new, objfile])

def install_name_tool_id(name, objfile):
    subprocess.call(["install_name_tool", "-id", name, objfile])

def install_name_tool_add_rpath(rpath, binary):
    subprocess.call(["install_name_tool", "-add_rpath", rpath, binary])

def install_name_tool_delete_rpath(rpath, binary):
    subprocess.call(["install_name_tool", "-delete_rpath", rpath, binary])

def libraries(objfile, result = dict()):
    libs_list       = otool(objfile)
    result[objfile] = libs_list

    for lib in libs_list:
        if lib not in result:
            libraries(lib, result)

    return result

def lib_path(binary):
    return os.path.join(os.path.dirname(binary), 'lib')

def lib_name(lib):
    return os.path.join("@executable_path", "lib", os.path.basename(lib))

def process_libraries(libs_dict, binary):
    libs_set = set(libs_dict)
    # Remove binary from libs_set to prevent a duplicate of the binary being 
    # added to the libs directory.
    libs_set.remove(binary)

    for src in libs_set:
        name = lib_name(src)
        dst  = os.path.join(lib_path(binary), os.path.basename(src))

        shutil.copy(src, dst)
        os.chmod(dst, 0o755)
        install_name_tool_id(name, dst)

        if src in libs_dict[binary]:
            install_name_tool_change(src, name, binary)

        for p in libs_set:
            if p in libs_dict[src]:
                install_name_tool_change(p, lib_name(p), dst)

def process_swift_libraries(binary):
    command = ['xcrun', '--find', 'swift-stdlib-tool']
    swiftStdlibTool = subprocess.check_output(command, universal_newlines=True).strip()
    # from xcode11 on the dynamic swift libs reside in a separate directory from
    # the std one, might need versioned paths for future swift versions
    swiftLibPath = os.path.join(swiftStdlibTool, '../../lib/swift-5.0/macosx')
    swiftLibPath = os.path.abspath(swiftLibPath)

    command = [swiftStdlibTool, '--copy', '--platform', 'macosx', '--scan-executable', binary, '--destination', lib_path(binary)]

    if os.path.exists(swiftLibPath):
        command.extend(['--source-libraries', swiftLibPath])

    subprocess.check_output(command, universal_newlines=True)

    print(">> setting additional rpath for swift libraries")
    install_name_tool_add_rpath("@executable_path/lib", binary)

def remove_dev_tools_rapths(binary):
    for path in get_rpaths_dev_tools(binary):
        install_name_tool_delete_rpath(path, binary)

def main():
    binary = os.path.abspath(sys.argv[1])
    if not os.path.exists(lib_path(binary)):
        os.makedirs(lib_path(binary))
    print(">> gathering all linked libraries")
    libs = libraries(binary)

    print(">> copying and processing all linked libraries")
    process_libraries(libs, binary)

    print(">> removing rpath definitions towards dev tools")
    remove_dev_tools_rapths(binary)

    print(">> copying and processing swift libraries")
    process_swift_libraries(binary)

if __name__ == "__main__":
    main()
