icons = ['16', '32', '64', '128']
foreach size: icons
    name = 'mpv-icon-8bit-'+size+'x'+size+'.png'
    icon = custom_target(name,
        input: name,
        output: name + '.inc',
        command: [file2string, '@INPUT@', '@OUTPUT@', '@SOURCE_ROOT@'],
    )
    sources += icon
endforeach

etc_files = ['input.conf', 'builtin.conf']
foreach file: etc_files
    etc_file = custom_target(file,
        input: file,
        output: file + '.inc',
        command: [file2string, '@INPUT@', '@OUTPUT@', '@SOURCE_ROOT@'],
    )
    sources += etc_file
endforeach
