python_files = ['defaults.py']
foreach file: python_files
    python_file = custom_target(file,
        input: join_paths(source_root, 'player', 'python', file),
        output: file + '.inc',
        command: [file2string, '@INPUT@', '@OUTPUT@', '@SOURCE_ROOT@'],
    )
    sources += python_file
endforeach
