python_files = ['defaults.py', 'mpv_main_event_loop.py']
foreach file: python_files
    python_file = custom_target(file,
        input: join_paths(source_root, 'player', 'python', file),
        output: file + '.inc',
        command: [file2string, '@INPUT@', '@OUTPUT@', '@SOURCE_ROOT@'],
    )
    sources += python_file
endforeach
