osd_font = custom_target('osd_font.otf',
    input: join_paths(source_root, 'sub', 'osd_font.otf'),
    output: 'osd_font.otf.inc',
    command: [file2string, '@INPUT@', '@OUTPUT@'],
)
sources += osd_font
