executable('fuzzer_loadfile', 'fuzzer_loadfile.c', link_with: libmpv)
executable('fuzzer_loadfile_direct', 'fuzzer_loadfile_direct.c', link_with: libmpv)

foreach p : ['mf', 'memory']
  executable('fuzzer_protocol_' + p,
             'fuzzer_loadfile_direct.c',
             c_args: ['-DMPV_PROTO=' + p],
             link_with: libmpv)
endforeach


foreach f : ['MPV_FORMAT_STRING', 'MPV_FORMAT_FLAG', 'MPV_FORMAT_INT64', 'MPV_FORMAT_DOUBLE']
  foreach i : ['0', '1']
    executable('fuzzer_set_property_' + f + '_' + i,
                'fuzzer_set_property.c',
                c_args: ['-DMPV_FORMAT=' + f, '-DMPV_RUN=' + i],
                link_with: libmpv)
  endforeach
endforeach
