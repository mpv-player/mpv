#!/usr/bin/env python3
import os
import shutil
import fileinput
import dylib_unhell
import subprocess
from optparse import OptionParser

def bundle_path(binary_name):
    return "%s.app" % binary_name

def bundle_name(binary_name):
    return os.path.basename(bundle_path(binary_name))

def target_plist(binary_name):
    return os.path.join(bundle_path(binary_name), 'Contents', 'Info.plist')

def target_directory(binary_name):
    return os.path.join(bundle_path(binary_name), 'Contents', 'MacOS')

def target_binary(binary_name):
    return os.path.join(target_directory(binary_name),
                        os.path.basename(binary_name))

def copy_bundle(binary_name, src_path):
    if os.path.isdir(bundle_path(binary_name)):
        shutil.rmtree(bundle_path(binary_name))
    shutil.copytree(
        os.path.join(src_path, 'TOOLS', 'osxbundle', bundle_name(binary_name)),
        bundle_path(binary_name))

def copy_binary(binary_name):
    shutil.copy(binary_name, target_binary(binary_name))

def apply_plist_template(plist_file, version):
    for line in fileinput.input(plist_file, inplace=1):
        print(line.rstrip().replace('${VERSION}', version))

def sign_bundle(binary_name):
    sign_directories = ['Contents/Frameworks', 'Contents/MacOS']
    for dir in sign_directories:
        resolved_dir = os.path.join(bundle_path(binary_name), dir)
        for root, _dirs, files in os.walk(resolved_dir):
            for f in files:
                subprocess.run(['codesign', '--force', '-s', '-', os.path.join(root, f)])
    subprocess.run(['codesign', '--force', '-s', '-', bundle_path(binary_name)])

def bundle_version(src_path):
    version = 'UNKNOWN'
    version_path = os.path.join(src_path, 'VERSION')
    if os.path.exists(version_path):
        x = open(version_path)
        version = x.read()
        x.close()
    return version

def main():
    usage = "usage: %prog [options] arg"
    parser = OptionParser(usage)
    parser.add_option("-s", "--skip-deps", action="store_false", dest="deps",
                      default=True,
                      help="don't bundle the dependencies")

    (options, args) = parser.parse_args()

    if len(args) < 1 or len(args) > 2:
        parser.error("incorrect number of arguments")
    else:
        binary_name = args[0]
        src_path = args[1] if len(args) > 1 else "."

    version = bundle_version(src_path).rstrip()

    print("Creating macOS application bundle (version: %s)..." % version)
    print("> copying bundle skeleton")
    copy_bundle(binary_name, src_path)
    print("> copying binary")
    copy_binary(binary_name)
    print("> generating Info.plist")
    apply_plist_template(target_plist(binary_name), version)

    if options.deps:
        print("> bundling dependencies")
        dylib_unhell.process(target_binary(binary_name))

    print("> signing bundle with ad-hoc pseudo identity")
    sign_bundle(binary_name)

    print("done.")

if __name__ == "__main__":
    main()
