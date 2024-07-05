wl_protocol_dir = wayland['deps'][2].get_variable(pkgconfig: 'pkgdatadir', internal: 'pkgdatadir')
protocols = [[wl_protocol_dir, 'stable/presentation-time/presentation-time.xml'],
             [wl_protocol_dir, 'stable/viewporter/viewporter.xml'],
             [wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
             [wl_protocol_dir, 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml'],
             [wl_protocol_dir, 'unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml'],
             [wl_protocol_dir, 'unstable/xdg-decoration/xdg-decoration-unstable-v1.xml'],
             [wl_protocol_dir, 'staging/content-type/content-type-v1.xml'],
             [wl_protocol_dir, 'staging/single-pixel-buffer/single-pixel-buffer-v1.xml'],
             [wl_protocol_dir, 'staging/fractional-scale/fractional-scale-v1.xml']]
wl_protocols_source = []
wl_protocols_headers = []

foreach v: ['1.32']
    features += {'wayland-protocols-' + v.replace('.', '-'):
        wayland['deps'][2].version().version_compare('>=' + v)}
endforeach

if features['wayland-protocols-1-32']
    protocols += [[wl_protocol_dir, 'staging/cursor-shape/cursor-shape-v1.xml'],
                  [wl_protocol_dir, 'unstable/tablet/tablet-unstable-v2.xml']] # required by cursor-shape
endif

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

sources += files('wayland_common.c')
