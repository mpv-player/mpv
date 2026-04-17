#!/usr/bin/env python3

import json
import os
import re
import shutil
_sp = __import__("subprocess")
call = _sp.call
check_output = _sp.check_output
DEVNULL = _sp.DEVNULL
import sys
from functools import partial

sys_re = re.compile("^/System")
usr_re = re.compile("^/usr/lib/")
exe_re = re.compile("@executable_path")

def is_user_lib(objfile, libname):
    return not sys_re.match(libname) and \
           not usr_re.match(libname) and \
           not exe_re.match(libname) and \
           "libobjc." not in libname and \
           "libSystem." not in libname and \
           "libc." not in libname and \
           "libgcc." not in libname and \
           os.path.basename(libname) != "Python" and \
           os.path.basename(objfile) not in libname and \
           "libswift" not in libname

def otool(objfile, rapths):
    output = check_output(
        ["otool", "-L", objfile],
        universal_newlines=True,
    )
    # Replicate: grep -e '\t' | awk '{ print $1 }'
    lines = [line.split()[0] for line in output.splitlines() if line.startswith('\t')]
    libs = set(filter(partial(is_user_lib, objfile), lines))

    libs_resolved = set()
    libs_relative = set()
    for lib in libs:
        lib_path = resolve_lib_path(objfile, lib, rapths)
        libs_resolved.add(lib_path)
        if lib_path != lib:
            libs_relative.add(lib)

    return libs_resolved, libs_relative

def get_rapths(objfile):
    rpaths: list[str] = []
    path_re = re.compile(r"^\s*path (.*) \(offset \d*\)$")

    try:
        result = check_output(
            ["otool", "-l", objfile],
            universal_newlines=True,
        )
    except Exception:
        return rpaths

    # Replicate: grep -A2 LC_RPATH | grep path
    lines = result.splitlines()
    for i, line in enumerate(lines):
        if "LC_RPATH" not in line:
            continue
        for candidate in lines[i + 1:i + 3]:
            if "path" not in candidate:
                continue
            match = path_re.search(candidate)
            if match is None:
                continue
            line_clean = match.group(1).strip()
            # resolve @loader_path
            if line_clean.startswith("@loader_path/"):
                line_clean = line_clean[len("@loader_path/"):]
                line_clean = os.path.join(os.path.dirname(objfile), line_clean)
                line_clean = os.path.normpath(line_clean)
            rpaths.append(line_clean)

    return rpaths

def get_rpaths_dev_tools(binary):
    path_re = re.compile(r"^\s*path (.*) \(offset \d*\)$")
    result = check_output(
        ["otool", "-l", binary],
        universal_newlines=True,
    )
    # Replicate: grep -A2 LC_RPATH | grep path | grep "Xcode\|CommandLineTools"
    rpaths = []
    lines = result.splitlines()
    for i, line in enumerate(lines):
        if "LC_RPATH" not in line:
            continue
        for candidate in lines[i + 1:i + 3]:
            if "path" not in candidate:
                continue
            match = path_re.search(candidate)
            if match is None:
                continue
            value = match.group(1).strip()
            if "Xcode" in value or "CommandLineTools" in value:
                rpaths.append(value)
    return rpaths

def resolve_lib_path(objfile, lib, rapths):
    if os.path.exists(lib):
        return lib

    if lib.startswith("@rpath/"):
        lib = lib[len("@rpath/"):]
        for rpath in rapths:
            lib_path = os.path.join(rpath, lib)
            if os.path.exists(lib_path):
                return lib_path
    elif lib.startswith("@loader_path/"):
        lib = lib[len("@loader_path/"):]
        lib_path = os.path.normpath(os.path.join(objfile, lib))
        if os.path.exists(lib_path):
            return lib_path

    raise Exception("Could not resolve library: " + lib)

def check_vulkan_max_version(version):
    try:
        check_output(
            ["pkg-config", "vulkan", f"--max-version={version}"],
            stderr=DEVNULL,
        )
        return True
    except Exception:
        return False

def get_homebrew_prefix():
    # set default to standard ARM path, intel path is already in the vulkan
    # loader search array
    result = "/opt/homebrew"
    try:
        result = check_output(
            ["brew", "--prefix"],
            universal_newlines=True,
            stderr=DEVNULL,
        ).strip()
    except Exception:
        pass

    return result

def install_name_tool_change(old, new, objfile):
    call(
        ["install_name_tool", "-change", old, new, objfile],
        stderr=DEVNULL,
    )

def install_name_tool_id(name, objfile):
    call(
        ["install_name_tool", "-id", name, objfile],
        stderr=DEVNULL,
    )

def install_name_tool_add_rpath(rpath, binary):
    call(
        ["install_name_tool", "-add_rpath", rpath, binary],
        stderr=DEVNULL,
    )

def install_name_tool_delete_rpath(rpath, binary):
    call(
        ["install_name_tool", "-delete_rpath", rpath, binary],
        stderr=DEVNULL,
    )

def libraries(objfile, result=None, result_relative=None, rapths=None):
    if result is None:
        result = {}

    if result_relative is None:
        result_relative = set()

    if rapths is None:
        rapths = []

    rapths = get_rapths(objfile) + rapths
    libs_list, libs_relative = otool(objfile, rapths)
    result[objfile] = libs_list
    result_relative |= libs_relative

    for lib in libs_list:
        if lib not in result:
            libraries(lib, result, result_relative, rapths)

    return result, result_relative

def lib_path(binary):
    return os.path.join(os.path.dirname(binary), "lib")

def resources_path(binary):
    return os.path.join(os.path.dirname(binary), "../Resources")

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
        os.chmod(dst, 0o644)
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
    swift_stdlib_tool = check_output(
        ["xcrun", "--find", "swift-stdlib-tool"],
        universal_newlines=True,
    ).strip()
    # from xcode11 on the dynamic swift libs reside in a separate directory from
    # the std one, might need versioned paths for future swift versions
    swift_lib_path = os.path.join(swift_stdlib_tool, "../../lib/swift-5.0/macosx")
    swift_lib_path = os.path.abspath(swift_lib_path)

    command = [
        swift_stdlib_tool, "--copy", "--platform", "macosx",
        "--scan-executable", binary, "--destination", lib_path(binary),
    ]

    if os.path.exists(swift_lib_path):
        command.extend(["--source-libraries", swift_lib_path])

    check_output(command, universal_newlines=True)

    print(">> setting additional rpath for swift libraries")
    install_name_tool_add_rpath("@executable_path/lib", binary)

def process_vulkan_loader(binary, loader_name, loader_relative_folder, library_node):
    # https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderDriverInterface.md#example-macos-driver-search-path
    # https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md#macos-layer-discovery
    loader_system_search_folders = [
        os.path.join(os.path.expanduser("~"), ".config", loader_relative_folder),
        os.path.join("/etc/xdg", loader_relative_folder),
        os.path.join("/usr/local/etc", loader_relative_folder),
        os.path.join("/etc", loader_relative_folder),
        os.path.join(os.path.expanduser("~"), ".local/share", loader_relative_folder),
        os.path.join("/usr/local/share", loader_relative_folder),
        os.path.join("/usr/share/vulkan", loader_relative_folder),
        os.path.join(get_homebrew_prefix(), "etc", loader_relative_folder),
        os.path.join(get_homebrew_prefix(), "share", loader_relative_folder), # old location
    ]

    loader_system_folder = ""
    for loader_system_search_folder in loader_system_search_folders:
        if os.path.exists(loader_system_search_folder):
            loader_system_folder = loader_system_search_folder
            break

    if not loader_system_folder:
        print(">>> could not find loader folder " + loader_relative_folder)
        return

    loader_bundle_folder = os.path.join(resources_path(binary), loader_relative_folder)
    loader_system_path = os.path.join(loader_system_folder, loader_name)
    loader_bundle_path = os.path.join(loader_bundle_folder, loader_name)
    library_relative_folder = "../../../Frameworks/"

    if not os.path.exists(loader_system_path):
        print(">>> could not find loader " + loader_name)
        return

    if not os.path.exists(loader_bundle_folder):
        os.makedirs(loader_bundle_folder)

    loader_system_file = open(loader_system_path)
    loader_json_data = json.load(loader_system_file)
    library_path = loader_json_data[library_node]["library_path"]
    library_system_path = os.path.join(loader_system_folder, library_path)

    if not os.path.exists(library_system_path):
        print(">>> could not find loader library " + library_system_path)
        return

    print(">>> modifying and writing loader json " + loader_name)
    loader_bundle_file = open(loader_bundle_path, "w")
    loader_library_name = os.path.basename(library_system_path)
    library_path = os.path.join(library_relative_folder, loader_library_name)
    loader_json_data[library_node]["library_path"] = library_path
    json.dump(loader_json_data, loader_bundle_file, indent=4)

    print(">>> copying loader library " + loader_library_name)
    framework_bundle_folder = os.path.join(
        loader_bundle_folder, library_relative_folder,
    )
    if not os.path.exists(framework_bundle_folder):
        os.makedirs(framework_bundle_folder)
    library_target_path = os.path.join(framework_bundle_folder, loader_library_name)
    shutil.copy(library_system_path, library_target_path)

def remove_dev_tools_rapths(binary):
    for path in get_rpaths_dev_tools(binary):
        install_name_tool_delete_rpath(path, binary)

def process(binary):
    binary = os.path.abspath(binary)
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
        process_vulkan_loader(
            binary,
            "VkLayer_khronos_synchronization2.json",
            "vulkan/explicit_layer.d",
            "layer",
        )

if __name__ == "__main__":
    process(sys.argv[1])
