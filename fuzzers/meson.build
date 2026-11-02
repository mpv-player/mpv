incdir = include_directories('../')

add_languages('cpp', native: false)

fuzzers = []
dicts = {}

fuzzers += executable('fuzzer_loadfile_direct', 'fuzzer_loadfile_direct.c',
                      link_language: 'cpp', dependencies: libmpv_dep)

protocols = ['edl', 'file', 'lavf', 'memory']
demuxers = ['mkv']

if features['cdda']
    protocols += ['cdda']
endif
if features['dvbin']
    protocols += ['dvb']
endif
if features['dvdnav']
    # TODO: This needs folder structure to load.
    protocols += ['dvd']
endif
if features['libbluray']
    # TODO: This needs folder structure to load.
    protocols += ['bd']
endif
if features['libarchive']
    protocols += ['archive']
    demuxers += ['libarchive']
endif

foreach p : protocols
  fuzzers += executable('fuzzer_protocol_' + p,
                        'fuzzer_loadfile_direct.c',
                        link_language: 'cpp',
                        c_args: ['-DMPV_PROTO=' + p, '-DMPV_PROTO_' + p.to_upper()],
                        dependencies: libmpv_dep)
endforeach

foreach demuxer : demuxers
    cmd = 'file'
    cmd_ = cmd.underscorify()
    demuxer_ = demuxer.underscorify()
    fuzzers += executable('fuzzer_load' + cmd_ + '_' + demuxer_,
                          'fuzzer_load.c',
                          link_language: 'cpp',
                          c_args: ['-DMPV_LOAD="' + cmd + '"', '-DMPV_LOAD' + cmd_.to_upper(),
                                   '-DMPV_DEMUXER="' + demuxer + '"', '-DMPV_DEMUXER' + demuxer_.to_upper()],
                          dependencies: libmpv_dep)
endforeach

foreach f : ['MPV_FORMAT_STRING', 'MPV_FORMAT_FLAG', 'MPV_FORMAT_INT64', 'MPV_FORMAT_DOUBLE']
  foreach i : ['0', '1']
    fuzzers += executable('fuzzer_set_property_' + f + '_' + i,
                          'fuzzer_set_property.c',
                          link_language: 'cpp',
                          c_args: ['-DMPV_FORMAT=' + f, '-DMPV_RUN=' + i],
                          dependencies: libmpv_dep)
    dicts += {'fuzzer_set_property_' + f + '_' + i: 'options,properties'}
  endforeach
endforeach

foreach cmd : ['file', '-config-file', '-input-conf']
    fuzzers += executable('fuzzer_load' + cmd.underscorify(),
                          'fuzzer_load.c',
                          link_language: 'cpp',
                          c_args: ['-DMPV_LOAD="' + cmd + '"', '-DMPV_LOAD' + cmd.underscorify().to_upper()],
                          dependencies: libmpv_dep)
endforeach

fuzzers += executable('fuzzer_sub_file', 'fuzzer_load.c',
                      link_language: 'cpp',
                      c_args: ['-DMPV_LOAD="file"', '-DMPV_LOADFILE', '-DMPV_SUB_FILE'],
                      dependencies: libmpv_dep)

fuzzers += executable('fuzzer_options_parser', 'fuzzer_options_parser.c',
                      link_language: 'cpp', include_directories: incdir_public,
                      objects: libmpv.extract_all_objects(recursive: true),
                      dependencies: dependencies)

fuzzers += executable('fuzzer_json', 'fuzzer_json.c',
                      link_language: 'cpp', include_directories: [incdir, incdir_public],
                      objects: libmpv.extract_all_objects(recursive: true),
                      dependencies: dependencies)

fuzzers += executable('fuzzer_ipc', 'fuzzer_ipc.c',
                      link_language: 'cpp', include_directories: [incdir, incdir_public],
                      objects: libmpv.extract_all_objects(recursive: true),
                      dependencies: dependencies)

dicts += {
    'fuzzer_ipc': 'commands,ipc,keys,options,properties,protocols',
    'fuzzer_load_config_file': 'options',
    'fuzzer_load_input_conf': 'commands,keys',
    'fuzzer_loadfile_direct': 'protocols',
    'fuzzer_loadfile_mkv': 'ebml',
    'fuzzer_options_parser': 'cli-options',
    'fuzzer_sub_file': 'subtitles',
}

dict_args = []
dict_outputs = []
foreach name, kinds : dicts
    dict_args += ['--dict', name + '=' + kinds]
    dict_outputs += name + '.dict'
endforeach
dict_names = executable('dict_names', 'dict_names.c',
                        link_language: 'cpp', link_args: ['-fno-sanitize=fuzzer'],
                        dependencies: libmpv_dep)
gen_fuzzer_dict = find_program(join_paths(tools_directory, 'gen-fuzzer-dict.py'))
fuzzers += custom_target('fuzzer_dicts', output: dict_outputs,
                         command: [gen_fuzzer_dict, '--names', dict_names,
                                   '--outdir', '@OUTDIR@', dict_args])

alias_target('fuzzers', fuzzers)
