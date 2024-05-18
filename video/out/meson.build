protocols = ['wayland.xml',
             'presentation-time.xml',
             'viewporter.xml',
             'xdg-shell.xml',
             'idle-inhibit-unstable-v1.xml',
             'linux-dmabuf-v1.xml',
             'xdg-decoration-unstable-v1.xml',
             'content-type-v1.xml',
             'single-pixel-buffer-v1.xml',
             'fractional-scale-v1.xml',
             'cursor-shape-v1.xml',
             'tablet-v2.xml']
wl_protocols_source = []
wl_protocols_headers = []

foreach p: protocols
    xml = join_paths(['wayland-protocols', p])
    wl_protocols_source += custom_target(xml.underscorify() + '_c',
        input: xml,
        output: '@BASENAME@.c',
        command: [wayland['scanner'], 'private-code', '@INPUT@', '@OUTPUT@'],
    )
    wl_protocols_headers += custom_target(xml.underscorify() + '_h',
        input: xml,
        output: '@BASENAME@.h',
        command: [wayland['scanner'], 'client-header', '-c', '@INPUT@', '@OUTPUT@'],
    )
endforeach

lib_client_protocols = static_library('protocols',
                                      wl_protocols_source + wl_protocols_headers,
                                      dependencies: wayland['deps'][0])

client_protocols = declare_dependency(link_with: lib_client_protocols,
                                      sources: wl_protocols_headers)

dependencies += [client_protocols, wayland['deps']]

sources += files('wayland_common.c')
