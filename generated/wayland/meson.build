wl_protocol_dir = wayland['deps'][2].get_variable(pkgconfig: 'pkgdatadir')
protocols = [[wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
             [wl_protocol_dir, 'stable/presentation-time/presentation-time.xml'],
             [wl_protocol_dir, 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml'],
             [wl_protocol_dir, 'unstable/xdg-decoration/xdg-decoration-unstable-v1.xml']]
wl_protocols_source = []
wl_protocols_headers = []

foreach p: protocols
    xml = join_paths(p)
    wl_protocols_source += custom_target(xml.underscorify() + '_c',
        input: xml,
        output: '@BASENAME@.c',
        command: [wayland['scanner'], 'private-code', '@INPUT@', '@OUTPUT@'],
    )
    wl_protocols_headers += custom_target(xml.underscorify() + '_h',
        input: xml,
        output: '@BASENAME@.h',
        command: [wayland['scanner'], 'client-header', '@INPUT@', '@OUTPUT@'],
    )
endforeach

lib_client_protocols = static_library('protocols',
                                      wl_protocols_source + wl_protocols_headers,
                                      dependencies: wayland['deps'][0])

client_protocols = declare_dependency(link_with: lib_client_protocols,
                                      sources: wl_protocols_headers)

dependencies += [client_protocols, wayland['deps']]

sources += ['video/out/wayland_common.c']
