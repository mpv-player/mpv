#!/usr/bin/env python3

# This checks for the sdk path, the sdk version, and
# the sdk build version.

import os
import subprocess
import sys
from shutil import which
from subprocess import check_output


def find_macos_sdk():
    sdk = os.environ.get("MACOS_SDK", "")
    sdk_version = os.environ.get("MACOS_SDK_VERSION", "0.0")
    xcrun = which("xcrun")
    xcodebuild = which("xcodebuild")

    if not xcrun:
        return sdk,sdk_version

    if not sdk:
        sdk = check_output([xcrun, "--sdk", "macosx", "--show-sdk-path"],
                            encoding="UTF-8")

    # find macOS SDK paths and version
    if sdk_version == "0.0":
        sdk_version = check_output([xcrun, "--sdk", "macosx", "--show-sdk-version"],
                                    encoding="UTF-8")

        # use xcode tools when installed, still necessary for xcode versions <12.0
        try:
            sdk_version = check_output(
                [xcodebuild, "-sdk", "macosx", "-version", "ProductVersion"],
                encoding="UTF-8",
                stderr=subprocess.DEVNULL
            )
        except Exception:
            pass

    if not isinstance(sdk_version, str):
        sdk_version = "10.10.0"

    return sdk.strip(),sdk_version.strip()

if __name__ == "__main__":
    sdk_info = find_macos_sdk()
    sys.stdout.write(",".join(sdk_info))
