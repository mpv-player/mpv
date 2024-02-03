incdir = include_directories('../')

executable('fuzzer_loadfile', 'fuzzer_loadfile.c',
           include_directories: incdir, link_with: libmpv)
executable('fuzzer_loadfile_direct', 'fuzzer_loadfile_direct.c',
           include_directories: incdir, link_with: libmpv)

foreach p : ['bd', 'cdda', 'dvb', 'dvd', 'edl', 'file', 'hex', 'lavf', 'memory',
             'mf', 'slice', 'smb']
  executable('fuzzer_protocol_' + p,
             'fuzzer_loadfile_direct.c',
             c_args: ['-DMPV_PROTO=' + p],
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
