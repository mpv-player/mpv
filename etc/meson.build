icons = ['16', '32', '64', '128']
foreach size: icons
    name = 'mpv-icon-8bit-'+size+'x'+size+'.png'
    icon = custom_target(name,
        input: join_paths(source_root, 'etc', name),
        output: name + '.inc',
        command: [file2string, '@INPUT@', '@OUTPUT@'],
    )
    sources += icon
endforeach

etc_files = ['input.conf', 'builtin.conf']
foreach file: etc_files
    etc_file = custom_target(file,
        input: join_paths(source_root, 'etc', file),
        output: file + '.inc',
        command: [file2string, '@INPUT@', '@OUTPUT@'],
    )
    sources += etc_file
endforeach
