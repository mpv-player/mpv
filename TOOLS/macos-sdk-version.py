#!/usr/bin/env python3

# Logic copied from compiler_swift.py in the waf build. This checks for the sdk
# path, the sdk version, and the sdk build version. The sdk path is returned
# along with what is selected as the sdk version.

import re
import os
import string
import sys
from packaging import version
from shutil import which
from subprocess import check_output

def find_macos_sdk():
    sdk = os.environ.get('MACOS_SDK', '')
    sdk_version = os.environ.get('MACOS_SDK_VERSION', '0.0')
    xcrun = which('xcrun')

    if not xcrun:
        return sdk,sdk_version

    if not sdk:
        sdk = check_output([xcrun, '--sdk', 'macosx', '--show-sdk-path'],
                            encoding="UTF-8")

    # find macOS SDK paths and version
    if sdk_version == '0.0':
        # show-sdk-build-version: is not available on older command line tools, but returns a build version (eg 17A360)
        # show-sdk-version: is always available, but on older dev tools it's only the major version
        sdk_build_version = check_output([xcrun, '--sdk', 'macosx',
                                          '--show-sdk-build-version'], encoding="UTF-8")

        sdk_version = check_output([xcrun, '--sdk', 'macosx', '--show-sdk-version'],
                                    encoding="UTF-8")

    if sdk:
        build_version = '10.10.0'

    # convert build version to a version string
    # first 2 two digits are the major version, starting with 15 which is 10.11 (offset of 4)
    # 1 char is the minor version, A => 0, B => 1 and ongoing
    # last digits are bugfix version, which are not relevant for us
    # eg 16E185 => 10.12.4, 17A360 => 10.13, 18B71 => 10.14.1
    if sdk_build_version and isinstance(sdk_build_version, str):
        verRe = re.compile("(\d+)(\D+)(\d+)")
        version_parts = verRe.search(sdk_build_version)
        major = int(version_parts.group(1)) - 4
        minor = string.ascii_lowercase.index(version_parts.group(2).lower())
        build_version = '10.' + str(major) + '.' + str(minor)
        # from 20 onwards macOS 11.0 starts
        if int(version_parts.group(1)) >= 20:
            build_version = '11.' + str(minor)

    if not isinstance(sdk_version, str):
        sdk_version = '10.10.0'

    # pick the higher version, always pick sdk over build if newer
    if version.parse(build_version) > version.parse(sdk_version):
        return sdk,build_version
    else:
        return sdk,sdk_version

    return sdk,sdk_version

if __name__ == "__main__":
    sdk_info = find_macos_sdk()
    sys.stdout.write(','.join(sdk_info))
