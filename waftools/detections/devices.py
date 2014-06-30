__cdrom_devices_map__ = {
    'win32':   'D:',
    'cygwin':  'D:',
    'darwin':  '/dev/disk1',
    'freebsd': '/dev/cd0',
    'openbsd': '/dev/rcd0c',
    'linux':   '/dev/sr0',
    'default': '/dev/cdrom'
}

__dvd_devices_map__ = {
    'win32':   'D:',
    'cygwin':  'D:',
    'darwin':  '/dev/rdiskN',
    'freebsd': '/dev/cd0',
    'openbsd': '/dev/rcd0c',
    'linux':   '/dev/sr0',
    'default': '/dev/dvd'
}

def __default_cdrom_device__(ctx):
    default = __cdrom_devices_map__['default']
    return __cdrom_devices_map__.get(ctx.env.DEST_OS, default)

def __default_dvd_device__(ctx):
    default = __dvd_devices_map__['default']
    return __dvd_devices_map__.get(ctx.env.DEST_OS, default)

def configure(ctx):
    ctx.define('DEFAULT_DVD_DEVICE', __default_dvd_device__(ctx))
    ctx.define('DEFAULT_CDROM_DEVICE', __default_cdrom_device__(ctx))
