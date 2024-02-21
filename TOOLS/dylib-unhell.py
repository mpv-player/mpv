#!/usr/bin/env python3

import re
import os
import sys
import shutil
import subprocess
import json
from distutils.dir_util import copy_tree
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

def otool(objfile, rapths):
    command = "otool -L '%s' | grep -e '\t' | awk '{ print $1 }'" % objfile
    output  = subprocess.check_output(command, shell = True, universal_newlines=True)
    libs = set(filter(partial(is_user_lib, objfile), output.split()))

    libs_resolved = set()
    libs_relative = set()
    for lib in libs:
        lib_path = resolve_lib_path(objfile, lib, rapths)
        libs_resolved.add(lib_path)
        if lib_path != lib:
            libs_relative.add(lib)

    return libs_resolved, libs_relative

def get_rapths(objfile):
    rpaths = []
    command = "otool -l '%s' | grep -A2 LC_RPATH | grep path" % objfile
    pathRe = re.compile(r"^\s*path (.*) \(offset \d*\)$")

    try:
        result = subprocess.check_output(command, shell = True, universal_newlines=True)
    except:
        return rpaths

    for line in result.splitlines():
        rpaths.append(pathRe.search(line).group(1).strip())

    return rpaths

def get_rpaths_dev_tools(binary):
    command = "otool -l '%s' | grep -A2 LC_RPATH | grep path | grep \"Xcode\\|CommandLineTools\"" % binary
    result  = subprocess.check_output(command, shell = True, universal_newlines=True)
    pathRe = re.compile(r"^\s*path (.*) \(offset \d*\)$")
    output = []

    for line in result.splitlines():
        output.append(pathRe.search(line).group(1).strip())

    return output

def resolve_lib_path(objfile, lib, rapths):
    if os.path.exists(lib):
        return lib

    if lib.startswith('@rpath/'):
        lib = lib[len('@rpath/'):]
        for rpath in rapths:
            lib_path = os.path.join(rpath, lib)
            if os.path.exists(lib_path):
                return lib_path
    elif lib.startswith('@loader_path/'):
        lib = lib[len('@loader_path/'):]
        lib_path = os.path.normpath(os.path.join(objfile, lib))
        if os.path.exists(lib_path):
            return lib_path

    raise Exception('Could not resolve library: ' + lib)

def check_vulkan_max_version(version):
    try:
        result = subprocess.check_output("pkg-config vulkan --max-version=" + version, shell = True)
        return True
    except:
        return False

def install_name_tool_change(old, new, objfile):
    subprocess.call(["install_name_tool", "-change", old, new, objfile], stderr=subprocess.DEVNULL)

def install_name_tool_id(name, objfile):
    subprocess.call(["install_name_tool", "-id", name, objfile], stderr=subprocess.DEVNULL)

def install_name_tool_add_rpath(rpath, binary):
    subprocess.call(["install_name_tool", "-add_rpath", rpath, binary])

def install_name_tool_delete_rpath(rpath, binary):
    subprocess.call(["install_name_tool", "-delete_rpath", rpath, binary])

def libraries(objfile, result = dict(), result_relative = set(), rapths = []):
    rapths = get_rapths(objfile) + rapths
    libs_list, libs_relative = otool(objfile, rapths)
    result[objfile] = libs_list
    result_relative |= libs_relative

    for lib in libs_list:
        if lib not in result:
            libraries(lib, result, result_relative, rapths)

    return result, result_relative

def lib_path(binary):
    return os.path.join(os.path.dirname(binary), 'lib')

def resources_path(binary):
    return os.path.join(os.path.dirname(binary), '../Resources')

def lib_name(lib):
    return os.path.join("@executable_path", "lib", os.path.basename(lib))

def process_libraries(libs_dict, libs_dyn, binary):
    libs_set = set(libs_dict)
    # Remove binary from libs_set to prevent a duplicate of the binary being
    # added to the libs directory.
    libs_set.remove(binary)

    for src in libs_set:
        name = lib_name(src)
        dst = os.path.join(lib_path(binary), os.path.basename(src))

        shutil.copy(src, dst)
        os.chmod(dst, 0o755)
        install_name_tool_id(name, dst)

        if src in libs_dict[binary]:
            install_name_tool_change(src, name, binary)

        for p in libs_set:
            if p in libs_dict[src]:
                install_name_tool_change(p, lib_name(p), dst)

        for lib in libs_dyn:
            install_name_tool_change(lib, lib_name(lib), dst)

    for lib in libs_dyn:
        install_name_tool_change(lib, lib_name(lib), binary)

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

def process_vulkan_loader(binary, loaderName, loaderRelativeFolder, libraryNode):
    # https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderDriverInterface.md#example-macos-driver-search-path
    # https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md#macos-layer-discovery
    loaderSystemSearchFolders = [
        os.path.join(os.path.expanduser("~"), ".config", loaderRelativeFolder),
        os.path.join("/etc/xdg", loaderRelativeFolder),
        os.path.join("/usr/local/etc", loaderRelativeFolder),
        os.path.join("/etc", loaderRelativeFolder),
        os.path.join(os.path.expanduser("~"), ".local/share", loaderRelativeFolder),
        os.path.join("/usr/local/share", loaderRelativeFolder),
        os.path.join("/usr/share/vulkan", loaderRelativeFolder),
    ]

    loaderSystemFolder = ""
    for loaderSystemSearchFolder in loaderSystemSearchFolders:
        if os.path.exists(loaderSystemSearchFolder):
            loaderSystemFolder = loaderSystemSearchFolder
            break

    if not loaderSystemFolder:
        print(">>> could not find loader folder " + loaderRelativeFolder)
        return

    loaderBundleFolder = os.path.join(resources_path(binary), loaderRelativeFolder)
    loaderSystemPath = os.path.join(loaderSystemFolder, loaderName)
    loaderBundlePath = os.path.join(loaderBundleFolder, loaderName)
    libraryRelativeFolder = "../../../Frameworks/"

    if not os.path.exists(loaderSystemPath):
        print(">>> could not find loader " + loaderName)
        return

    if not os.path.exists(loaderBundleFolder):
        os.makedirs(loaderBundleFolder)

    loaderSystemFile = open(loaderSystemPath, 'r')
    loaderJsonData = json.load(loaderSystemFile)
    librarySystemPath = os.path.join(loaderSystemFolder, loaderJsonData[libraryNode]["library_path"])

    if not os.path.exists(librarySystemPath):
        print(">>> could not find loader library " + librarySystemPath)
        return

    print(">>> modifiying and writing loader json " + loaderName)
    loaderBundleFile = open(loaderBundlePath, 'w')
    loaderLibraryName = os.path.basename(librarySystemPath)
    loaderJsonData[libraryNode]["library_path"] = os.path.join(libraryRelativeFolder, loaderLibraryName)
    json.dump(loaderJsonData, loaderBundleFile, indent=4)

    print(">>> copying loader library " + loaderLibraryName)
    frameworkBundleFolder = os.path.join(loaderBundleFolder, libraryRelativeFolder)
    if not os.path.exists(frameworkBundleFolder):
        os.makedirs(frameworkBundleFolder)
    shutil.copy(librarySystemPath, os.path.join(frameworkBundleFolder, loaderLibraryName))

def remove_dev_tools_rapths(binary):
    for path in get_rpaths_dev_tools(binary):
        install_name_tool_delete_rpath(path, binary)

def main():
    binary = os.path.abspath(sys.argv[1])
    if not os.path.exists(lib_path(binary)):
        os.makedirs(lib_path(binary))
    print(">> gathering all linked libraries")
    libs, libs_rel = libraries(binary)

    print(">> copying and processing all linked libraries")
    process_libraries(libs, libs_rel, binary)

    print(">> removing rpath definitions towards dev tools")
    remove_dev_tools_rapths(binary)

    print(">> copying and processing swift libraries")
    process_swift_libraries(binary)

    print(">> copying and processing vulkan loader")
    process_vulkan_loader(binary, "MoltenVK_icd.json", "vulkan/icd.d", "ICD")
    if check_vulkan_max_version("1.3.261.1"):
        process_vulkan_loader(binary, "VkLayer_khronos_synchronization2.json", "vulkan/explicit_layer.d", "layer")

if __name__ == "__main__":
    main()
