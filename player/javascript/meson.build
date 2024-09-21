defaults_js = custom_target('defaults.js',
    input: 'defaults.js',
    output: 'defaults.js.inc',
    command: [file2string, '@INPUT@', '@OUTPUT@', '@SOURCE_ROOT@'],
)
sources += defaults_js
