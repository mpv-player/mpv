lua_files = ['defaults.lua', 'assdraw.lua', 'options.lua', 'osc.lua',
             'ytdl_hook.lua', 'stats.lua', 'console.lua', 'auto_profiles.lua',
             'input.lua']
foreach file: lua_files
    lua_file = custom_target(file,
        input: join_paths(source_root, 'player', 'lua', file),
        output: file + '.inc',
        command: [file2string, '@INPUT@', '@OUTPUT@'],
    )
    sources += lua_file
endforeach
