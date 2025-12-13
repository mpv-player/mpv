if xcrun.found()
    actool = find_program(run_command(xcrun, '-find', 'actool', check: true).stdout().strip())
    icon_directory = join_paths(tools_directory, 'osxbundle', 'icon.icon')

    mac_icon = custom_target('mac_icon',
        output: ['icon.icns', 'Assets.car'],
        command: [actool, icon_directory, '--compile', '@BUILD_ROOT@/TOOLS/osxbundle/',
            '--app-icon', 'icon', '--enable-on-demand-resources', 'NO',
            '--development-region', 'en', '--target-device', 'mac', '--platform', 'macosx',
            '--include-all-app-icons', '--minimum-deployment-target', '10.15',
            '--output-partial-info-plist', '/dev/null', '--standalone-icon-behavior', 'all'],
    )
    sources += mac_icon
endif

mac_bundle_icon = custom_target('mac_bundle_icon',
    input: mac_icon,
    output: 'icon.icns.inc',
    command: [file2string, '@INPUT0@', '@OUTPUT@', '@SOURCE_ROOT@'],
)
sources += mac_bundle_icon
