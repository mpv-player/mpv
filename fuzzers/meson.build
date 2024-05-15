incdir = include_directories('../')

executable('fuzzer_loadfile', 'fuzzer_loadfile.c',
           include_directories: incdir, link_with: libmpv)
executable('fuzzer_loadfile_direct', 'fuzzer_loadfile_direct.c',
           include_directories: incdir, link_with: libmpv)

protocols = ['edl', 'file', 'hex', 'lavf', 'memory', 'mf', 'slice', 'smb']
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
endif

foreach p : protocols
  executable('fuzzer_protocol_' + p,
             'fuzzer_loadfile_direct.c',
             c_args: ['-DMPV_PROTO=' + p, '-DMPV_PROTO_' + p.to_upper()],
             include_directories: incdir,
             link_with: libmpv)
endforeach


foreach f : ['MPV_FORMAT_STRING', 'MPV_FORMAT_FLAG', 'MPV_FORMAT_INT64', 'MPV_FORMAT_DOUBLE']
  foreach i : ['0', '1']
    executable('fuzzer_set_property_' + f + '_' + i,
                'fuzzer_set_property.c',
                c_args: ['-DMPV_FORMAT=' + f, '-DMPV_RUN=' + i],
                include_directories: incdir,
                link_with: libmpv)
  endforeach
endforeach

foreach cmd : ['config-file', 'input-conf']
    executable('fuzzer_load_' + cmd.underscorify(),
                'fuzzer_load.c',
                c_args: ['-DMPV_LOAD="' + cmd + '"'],
                include_directories: incdir,
                link_with: libmpv)
endforeach
