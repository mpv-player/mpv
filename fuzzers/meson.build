incdir = include_directories('../')

executable('fuzzer_loadfile_direct', 'fuzzer_loadfile_direct.c',
           include_directories: incdir, link_with: libmpv)

protocols = ['edl', 'file', 'hex', 'lavf', 'memory', 'mf', 'slice', 'smb']
demuxers = ['disc', 'edl', 'cue', 'rawaudio', 'rawvideo', 'mkv', 'lavf', 'mf']

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
  executable('fuzzer_protocol_' + p,
             'fuzzer_loadfile_direct.c',
             c_args: ['-DMPV_PROTO=' + p, '-DMPV_PROTO_' + p.to_upper()],
             include_directories: incdir,
             link_with: libmpv)
endforeach

foreach demuxer : demuxers
    cmd = 'file'
    cmd_ = cmd.underscorify()
    demuxer_ = demuxer.underscorify()
    executable('fuzzer_load' + cmd_ + '_' + demuxer_,
                'fuzzer_load.c',
                c_args: ['-DMPV_LOAD="' + cmd + '"', '-DMPV_LOAD' + cmd_.to_upper(),
                         '-DMPV_DEMUXER="' + demuxer + '"', '-DMPV_DEMUXER' + demuxer_.to_upper()],
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

foreach cmd : ['file', '-config-file', '-input-conf']
    executable('fuzzer_load' + cmd.underscorify(),
                'fuzzer_load.c',
                c_args: ['-DMPV_LOAD="' + cmd + '"', '-DMPV_LOAD' + cmd.underscorify().to_upper()],
                include_directories: incdir,
                link_with: libmpv)
endforeach
