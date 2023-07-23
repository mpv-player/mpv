defaults_js = custom_target('defaults.js',
    input: join_paths(source_root, 'player', 'javascript', 'defaults.js'),
    output: 'defaults.js.inc',
    command: [file2string, '@INPUT@', '@OUTPUT@'],
)
sources += defaults_js
