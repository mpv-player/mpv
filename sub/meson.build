osd_font = custom_target('osd_font.otf',
    input: 'osd_font.otf',
    output: 'osd_font.otf.inc',
    command: [file2string, '@INPUT@', '@OUTPUT@', '@SOURCE_ROOT@'],
)
sources += osd_font
