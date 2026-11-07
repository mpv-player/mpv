# Windows Spatial Audio hardware validation

This test is for the native Windows SDK build that contains the optional
`wasapi-spatial` audio output. It separates three checks:

1. The binary registers the new audio output driver.
2. Windows activates a Spatial Audio stream for the selected endpoint.
3. A real multichannel PCM source plays through that stream and is heard through
   the intended headphone/speaker setup.

The GitHub Actions runner can validate the first two build-time properties and
compile/link the program. It cannot validate your physical endpoint, Windows
Spatial Sound setting, headphone chain, or what you hear. The final listening
check must be run on the target Windows PC.

## Requirements

- Windows 10 version 1703 or newer (Windows 11 recommended).
- A Windows audio endpoint that exposes the Spatial Audio renderer.
- For a baseline test, select **Windows Sonic for Headphones** under the
  endpoint's Spatial sound setting in Windows.
- A decoded multichannel PCM source, preferably a known 5.1 or 7.1 FLAC/WAV
  test file. A stereo file can verify activation, but it cannot verify
  multichannel placement.
- Do not use WASAPI exclusive mode for this driver.

The driver accepts decoded PCM and maps channels to Windows static spatial
objects. It does not bitstream Dolby TrueHD/DTS-HD or pass through native Dolby
Atmos/DTS:X dynamic-object metadata. Decode those formats to PCM before testing.

## 1. Check the endpoint list

Put `mpv.com` and `wasapi-spatial-test.ps1` in the same directory, then open
PowerShell in that directory:

```powershell
.\wasapi-spatial-test.ps1 -ListDevices
```

This asks mpv to enumerate endpoints through the Spatial Audio output driver.
Use `-AudioDevice "device-name-or-id"` in the playback command if you need to
select a particular endpoint. The default is `auto`.

## 2. Run a strict Spatial Audio playback test

Use a multichannel PCM file:

```powershell
.\wasapi-spatial-test.ps1 -MediaPath "D:\Audio\surround-5.1.flac"
```

To select a specific endpoint and log to a known location:

```powershell
$testArgs = @{
    MediaPath = "D:\Audio\surround-7.1.flac"
    AudioDevice = "Your Windows output device"
    LogPath = "$PWD\wasapi-spatial-test.log"
}
.\wasapi-spatial-test.ps1 @testArgs
```

The script deliberately uses `--ao=wasapi-spatial` without a fallback driver.
This is important: a successful ordinary WASAPI fallback must not be mistaken
for a successful Spatial Audio test.

A successful stream activation should produce a line similar to:

```text
Using Windows Spatial Audio: ... at ... Hz (... static objects).
```

That confirms stream activation, not by itself correct speaker mapping or
subjective headphone virtualization. Listen to the channel-identification
material and confirm that left/right, center, surrounds and (where present)
height channels are rendered as expected.

## 3. Compare with ordinary WASAPI

After the strict test, compare the same PCM file through regular WASAPI:

```powershell
.\mpv.com --no-config --ao=wasapi --audio-device=auto "D:\Audio\surround-5.1.flac"
```

For normal use, the optional fallback form is:

```powershell
.\mpv.com --no-config --ao=wasapi-spatial,wasapi "D:\Audio\surround-5.1.flac"
```

The fallback form is convenient for playback but is not suitable for proving
that Spatial Audio initialized. Use the strict command/script above for that.

## 4. What to attach when the test fails

Keep the complete `wasapi-spatial-test.log` and report:

- Windows version and build.
- The exact output endpoint selected.
- Whether Windows Sonic for Headphones was selected.
- The source file's channel layout (for example, 5.1 or 7.1 PCM).
- Whether ordinary `--ao=wasapi` plays the same file.
- Whether the log contains `Using Windows Spatial Audio:` and, if not, the
  HRESULT/error immediately preceding the failure.

Do not post private media files or personal device identifiers. A redacted
device name is sufficient for initial diagnosis.
