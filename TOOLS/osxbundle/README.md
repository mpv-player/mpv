# macOS Bundle

This directory contains the skeleton structure for the macOS application bundle.

## Wrapper Script

The `mpv-wrapper` script in `mpv.app/Contents/MacOS/` serves as the entry point for the mpv application. This allows for:

1. **Specifying default command line arguments**: Edit the `DEFAULT_ARGS` variable in the wrapper script to add default arguments that will be passed to mpv on every launch.

2. **Maintaining compatibility**: The wrapper ensures that arguments passed when opening files (e.g., via double-click or drag-and-drop) are still properly forwarded to mpv.

### Usage

To customize default arguments for your mpv.app bundle:

1. Before running `osxbundle.py`, edit `TOOLS/osxbundle/mpv.app/Contents/MacOS/mpv-wrapper`
2. Set the `DEFAULT_ARGS` variable to your desired command line options
3. Example: `DEFAULT_ARGS="--force-window --keep-open"`
4. Run `osxbundle.py` to create the bundle with your customized wrapper

The `Info.plist` is configured to use `mpv-wrapper` as the `CFBundleExecutable`, which means the wrapper script will be executed when the app is launched.
